// Spindeck - verification sous Linux : analyse BPM, moteur audio (lecture, cue, scratch MIDI), capture de l'interface.
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <iostream>

static void writeTestTrack (const juce::File& f, double bpm, double seconds, double phaseSec, float tone)
{
    const double sr = 44100.0;
    const int n = (int) (sr * seconds);
    juce::AudioBuffer<float> b (2, n);
    b.clear();
    const double beat = 60.0 / bpm;
    juce::Random rnd (42);
    for (int i = 0; i < n; ++i)
    {
        const double t = i / sr;
        const double tb = std::fmod (t - phaseSec + 1000 * beat, beat);
        double x = 0;
        if (tb < 0.25) x += 0.8 * std::exp (-tb * 18.0) * std::sin (2 * M_PI * (50.0 + 90.0 * std::exp (-tb * 40.0)) * tb);   // grosse caisse
        const double th = std::fmod (tb + beat * 0.5, beat);
        if (th < 0.05) x += 0.15 * std::exp (-th * 90.0) * (rnd.nextFloat() * 2 - 1);                                 // charleston
        x += 0.12 * std::sin (2 * M_PI * tone * t) * (0.6 + 0.4 * std::sin (2 * M_PI * t / (beat * 4)));              // nappe
        b.setSample (0, i, (float) x); b.setSample (1, i, (float) x);
    }
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, 2, 16, {}, 0));
    if (w) { os.release(); w->writeFromAudioSampleBuffer (b, 0, n); }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File out (argc > 1 ? argv[1] : "/tmp/spin");
    out.createDirectory();
    const auto music = out.getChildFile ("music");
    music.createDirectory();
    writeTestTrack (music.getChildFile ("Nine Hands - Groove 124.wav"), 124.0, 90.0, 0.37, 220.0f);
    writeTestTrack (music.getChildFile ("Test Artist - Beat 128.wav"), 128.0, 90.0, 0.12, 330.0f);
    writeTestTrack (music.getChildFile ("Crate Digger - Slow Jam 92.wav"), 92.0, 40.0, 0.0, 165.0f);

    // MIDI Learn par defaut : sauvegarde, rappel dans une nouvelle instance, priorite au projet
    {
        auto a = std::make_unique<SpinProcessor>();
        for (auto& c : a->ccMap) c = -1;
        a->ccMap[21] = spin::pXf; a->ccMap[22] = spin::pVolA;
        a->saveDefaultMapping();
        auto b = std::make_unique<SpinProcessor>();
        std::cout << "default midi learn: new instance has " << b->mappingCount() << " (expected 2), cc21->" << b->ccMap[21].load() << std::endl;
        juce::MemoryBlock st;
        b->ccMap[22] = -1; b->ccMap[30] = spin::pMaster;               // le projet a ses propres assignations
        b->getStateInformation (st);
        auto c = std::make_unique<SpinProcessor>();
        c->setStateInformation (st.getData(), (int) st.getSize());
        std::cout << "project mapping wins: " << c->mappingToString() << std::endl;
        a->clearDefaultMapping();
        auto d = std::make_unique<SpinProcessor>();
        std::cout << "after delete default: " << d->mappingCount() << " (expected 0)" << std::endl;
    }

    auto proc = std::make_unique<SpinProcessor>();
    proc->setLibraryFolder (music);
    proc->prepareToPlay (44100.0, 256);
    auto* mm = juce::MessageManager::getInstance();
    for (int i = 0; i < 100 && proc->library.size() < 3; ++i) mm->runDispatchLoopUntil (30);
    std::cout << "library: " << proc->library.size() << " tracks" << std::endl;

    proc->loadFile (0, music.getChildFile ("Nine Hands - Groove 124.wav"));
    for (int i = 0; i < 400 && (proc->track (0) == nullptr); ++i) mm->runDispatchLoopUntil (30);
    proc->loadFile (1, music.getChildFile ("Test Artist - Beat 128.wav"));
    for (int i = 0; i < 400 && (proc->track (1) == nullptr); ++i) mm->runDispatchLoopUntil (30);
    for (int d = 0; d < 2; ++d)
        if (auto t = proc->track (d))
            std::cout << "deck " << d << ": bpm=" << t->bpm << " firstBeat=" << t->firstBeat / t->sampleRate << " s  cue=" << t->initialCue << std::endl;

    // --- moteur audio : PLAY A + B via les notes de la Discover DJ, puis sync, scratch
    juce::AudioBuffer<float> buf (2, 256);
    juce::MidiBuffer midi;
    double maxAbs = 0, sum = 0; long long cnt = 0; bool bad = false;
    double worstMs = 0;
    auto run = [&] (int blocks, std::vector<juce::MidiMessage> msgs = {})
    {
        for (int b = 0; b < blocks; ++b)
        {
            midi.clear();
            if (b == 0) for (auto& m : msgs) midi.addEvent (m, 10);
            const auto t0 = juce::Time::getHighResolutionTicks();
            proc->processBlock (buf, midi);
            const double ms = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0) * 1000.0;
            worstMs = std::max (worstMs, ms);
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 256; ++i)
                {
                    const float v = buf.getSample (c, i);
                    if (! std::isfinite (v)) bad = true;
                    maxAbs = std::max (maxAbs, (double) std::abs (v)); sum += v * v; ++cnt;
                }
            if ((b & 31) == 0) mm->runDispatchLoopUntil (1);
        }
    };
    run (1, { juce::MidiMessage::noteOn (1, 0x4A, (juce::uint8) 127) });            // play A
    run (400);
    std::cout << "A playing=" << proc->view[0].playing.load() << " pos=" << proc->view[0].pos.load() / 44100.0 << " s" << std::endl;
    run (1, { juce::MidiMessage::noteOn (1, 0x4C, (juce::uint8) 127) });            // play B
    run (200);
    run (1, { juce::MidiMessage::noteOn (1, 0x47, (juce::uint8) 127), juce::MidiMessage::noteOn (1, 0x47, (juce::uint8) 0) });   // sync B
    run (100);
    std::cout << "B rate after sync=" << proc->view[1].rate.load() << " (expected " << 124.0 / 128.0 << ")" << std::endl;

    // scratch : mode scratch, toucher A, ticks avant/arriere
    run (1, { juce::MidiMessage::noteOn (1, 0x48, (juce::uint8) 127) });
    run (1, { juce::MidiMessage::noteOn (1, 0x4D, (juce::uint8) 127) });
    const double p0 = proc->view[0].pos.load();
    for (int k = 0; k < 40; ++k) run (1, { juce::MidiMessage::controllerEvent (1, 0x19, 6) });
    run (20);
    const double p1 = proc->view[0].pos.load();
    for (int k = 0; k < 40; ++k) run (1, { juce::MidiMessage::controllerEvent (1, 0x19, 0x7A) });
    run (20);
    const double p2 = proc->view[0].pos.load();
    run (1, { juce::MidiMessage::noteOn (1, 0x4D, (juce::uint8) 0) });
    run (60);
    std::cout << "scratch: forward " << (p1 - p0) / 44100.0 << " s, back " << (p2 - p1) / 44100.0 << " s, playing after release="
              << proc->view[0].playing.load() << " scratching=" << proc->view[0].scratching.load() << std::endl;

    // cue A (a l'arret)
    run (1, { juce::MidiMessage::noteOn (1, 0x3B, (juce::uint8) 127) });
    run (40);
    std::cout << "cue while playing -> playing=" << proc->view[0].playing.load() << " pos=" << proc->view[0].pos.load() << " cue=" << proc->view[0].cue.load() << std::endl;
    run (1, { juce::MidiMessage::noteOn (1, 0x3B, (juce::uint8) 0), juce::MidiMessage::controllerEvent (1, 0x0A, 30) });
    run (200);
    mm->runDispatchLoopUntil (50);
    {   // les deux platines a l'arret : le master doit etre silencieux
        run (1, { juce::MidiMessage::noteOn (1, 0x4C, (juce::uint8) 127) });   // stop B
        run (300);
        maxAbs = 0; run (50);
        std::cout << "stopped decks: master peak=" << maxAbs << " (expected 0)" << std::endl;
    }
    std::cout << "crossfader param=" << proc->apvts.getRawParameterValue ("xfader")->load() << std::endl;
    std::cout << "audio: peak=" << maxAbs << " rms=" << std::sqrt (sum / std::max (1LL, cnt)) << " nan=" << bad
              << " worst block=" << worstMs << " ms (budget 5.8 ms)" << std::endl;

    // --- capture de l'interface (platines en lecture)
    run (1, { juce::MidiMessage::noteOn (1, 0x4A, (juce::uint8) 127) });
    run (300);
    proc->post (0, spin::cmdScratchMode, 0.0);
    run (2);
    for (int theme = 0; theme < 4; ++theme)
    {
        proc->theme = theme;
        proc->libraryToggle = false;
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditor());
        ed->setSize (1600, 900);
        for (int i = 0; i < 12; ++i) { run (4); mm->runDispatchLoopUntil (40); }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        auto f = out.getChildFile ("ui_theme_" + juce::String (theme) + ".png");
        f.deleteFile();
        if (auto os = f.createOutputStream()) png.writeImageToStream (img, *os);
        // gros plan de la platine A en 2x
        auto zoom = ed->createComponentSnapshot ({ 0, 210, 520, 380 }, true, 2.0f);
        auto fz = out.getChildFile ("deck_zoom_" + juce::String (theme) + ".png");
        fz.deleteFile();
        if (auto os = fz.createOutputStream()) png.writeImageToStream (zoom, *os);
        auto armz = ed->createComponentSnapshot ({ 260, 300, 190, 230 }, true, 4.0f);
        auto fa = out.getChildFile ("arm_zoom_" + juce::String (theme) + ".png");
        fa.deleteFile();
        if (auto os = fa.createOutputStream()) png.writeImageToStream (armz, *os);
    }
    // animation : 36 images de la platine A en lecture (1/30 s entre deux images)
    {
        proc->theme = 0;
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditor());
        ed->setSize (1600, 900);
        for (int i = 0; i < 8; ++i) { run (4); mm->runDispatchLoopUntil (40); }
        double worst = 0;
        for (int fr = 0; fr < 36; ++fr)
        {
            run (6);
            mm->runDispatchLoopUntil (5);
            const auto t0 = juce::Time::getHighResolutionTicks();
            auto img = ed->createComponentSnapshot ({ 70, 300, 300, 240 }, true, 2.0f);
            worst = std::max (worst, juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0) * 1000.0);
            juce::PNGImageFormat png;
            auto f = out.getChildFile ("anim_" + juce::String (fr).paddedLeft ('0', 2) + ".png");
            f.deleteFile();
            if (auto os = f.createOutputStream()) png.writeImageToStream (img, *os);
        }
        std::cout << "platter frame (2x) worst " << worst << " ms" << std::endl;
    }
    // icone du mode scratch (main pleine)
    {
        proc->theme = 0;
        proc->post (0, spin::cmdScratchMode, 1.0);
        run (4);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditor());
        ed->setSize (1600, 900);
        for (int i = 0; i < 6; ++i) { run (2); mm->runDispatchLoopUntil (40); }
        juce::PNGImageFormat png;
        auto img = ed->createComponentSnapshot ({ 70, 360, 330, 270 }, true, 2.0f);
        auto f = out.getChildFile ("icon_final.png");
        f.deleteFile();
        if (auto os = f.createOutputStream()) png.writeImageToStream (img, *os);
        auto full = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto ff = out.getChildFile ("ui_final.png");
        ff.deleteFile();
        if (auto os = ff.createOutputStream()) png.writeImageToStream (full, *os);
        // captures 2x pour la notice : interface en mode scratch, puis bibliotheque agrandie
        auto big = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        auto fb = out.getChildFile ("manual_ui_2x.png");
        fb.deleteFile();
        if (auto os = fb.createOutputStream()) png.writeImageToStream (big, *os);
        proc->post (0, spin::cmdScratchMode, 0.0);
        proc->libraryToggle = true;
        for (int i = 0; i < 6; ++i) { run (2); mm->runDispatchLoopUntil (40); }
        auto lib = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        auto fl = out.getChildFile ("manual_library.png");
        fl.deleteFile();
        if (auto os = fl.createOutputStream()) png.writeImageToStream (lib, *os);
        proc->libraryToggle = false;
    }
    // fin de morceau : le bras retourne au repos
    {
        proc->theme = 0;
        proc->post (0, spin::cmdScratchMode, 0.0);
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditor());
        ed->setSize (1600, 900);
        if (! proc->view[0].playing.load()) run (1, { juce::MidiMessage::noteOn (1, 0x4A, (juce::uint8) 127) });
        proc->post (0, spin::cmdSeekNorm, 0.993);
        for (int i = 0; i < 4; ++i) { run (4); mm->runDispatchLoopUntil (35); }
        const auto tStart = juce::Time::getMillisecondCounterHiRes();
        for (int fr = 0; fr < 160; ++fr)                         // ~1 image d'interface (30 Hz) par capture : GIF en temps reel
        {
            run (6);
            mm->runDispatchLoopUntil (34);
            auto img = ed->createComponentSnapshot ({ 70, 360, 330, 270 }, true, 1.5f);
            juce::PNGImageFormat png;
            auto f = out.getChildFile ("end_" + juce::String (fr).paddedLeft ('0', 2) + ".png");
            f.deleteFile();
            if (auto os = f.createOutputStream()) png.writeImageToStream (img, *os);
        }
        std::cout << "end: playing=" << proc->view[0].playing.load() << " frame_ms=" << (juce::Time::getMillisecondCounterHiRes() - tStart) / 160.0 << std::endl;
    }
    // souris sur la forme d'onde defilante de la platine A : clic puis glisser
    {
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditor());
        ed->setSize (1600, 900);
        for (int i = 0; i < 4; ++i) { run (2); mm->runDispatchLoopUntil (30); }
        if (proc->view[0].playing.load()) run (1, { juce::MidiMessage::noteOn (1, 0x4A, (juce::uint8) 127) });
        proc->post (0, spin::cmdSeekNorm, 0.5);
        run (40);
        ed->setVisible (true);
        auto* lane = ed->getComponentAt (400, 100);
        if (lane == nullptr) { std::cout << "lane not found" << std::endl; return 1; }
        auto src = juce::Desktop::getInstance().getMainMouseSource();
        auto ev = [&] (float x, float y, bool drag)
        {
            return juce::MouseEvent (src, { x, y }, juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                     lane, lane, juce::Time::getCurrentTime(), { 1200.0f, 40.0f }, juce::Time::getCurrentTime(), 1, drag);
        };
        const double p0 = proc->view[0].pos.load();
        // clic simple a droite de la tete de lecture (centre de la voie)
        const float cx = lane->getWidth() * 0.5f;
        lane->mouseDown (ev (cx + 200.0f, 40.0f, false)); lane->mouseUp (ev (cx + 200.0f, 40.0f, false));
        run (40);
        const double p1 = proc->view[0].pos.load();
        // glisser de 300 px vers la droite : retour en arriere
        lane->mouseDown (ev (cx, 40.0f, false));
        for (int k = 1; k <= 10; ++k) { lane->mouseDrag (ev (cx + 30.0f * k, 40.0f, true)); run (4); }
        lane->mouseUp (ev (cx + 300.0f, 40.0f, true));
        run (40);
        const double p2 = proc->view[0].pos.load();
        const double fpp = 10.0 * 44100.0 / lane->getWidth();
        std::cout << "lane click: +" << (p1 - p0) / fpp << " px (expected 200)  drag: " << (p2 - p1) / fpp << " px (expected -300)" << std::endl;
    }
    // pre-ecoute : ligne selectionnee, lecture, deplacement, suivi de la selection, arret
    {
        while (proc->view[0].playing.load()) run (1, { juce::MidiMessage::noteOn (1, 0x4A, (juce::uint8) 127) });
        if (proc->view[1].playing.load()) run (1, { juce::MidiMessage::noteOn (1, 0x4C, (juce::uint8) 127) });
        run (400);
        proc->selected = 0;
        proc->togglePreviewSelected();
        maxAbs = 0; run (200);
        std::cout << "preview: active=" << proc->isPreviewing() << " pos=" << proc->previewPosSec.load() << " s peak=" << maxAbs
                  << " file=" << proc->previewFile().getFileName() << std::endl;
        proc->seekPreview (0.5);
        run (40);
        std::cout << "preview seek 50%: pos=" << proc->previewPosSec.load() << " / " << proc->previewLenSec.load() << std::endl;
        proc->selected = 1;
        for (int i = 0; i < 6; ++i) { mm->runDispatchLoopUntil (20); run (4); }
        std::cout << "preview follows selection: " << proc->previewFile().getFileName() << " pos=" << proc->previewPosSec.load() << std::endl;
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc->createEditor());
        ed->setSize (1600, 900);
        for (int i = 0; i < 6; ++i) { run (4); mm->runDispatchLoopUntil (30); }
        auto img = ed->createComponentSnapshot ({ 0, 630, 1600, 270 }, true, 1.5f);
        juce::PNGImageFormat png;
        auto f = out.getChildFile ("preview_strip.png");
        f.deleteFile();
        if (auto os = f.createOutputStream()) png.writeImageToStream (img, *os);
        proc->stopPreview();
        maxAbs = 0; run (60); maxAbs = 0; run (40);
        std::cout << "preview stopped: active=" << proc->isPreviewing() << " peak=" << maxAbs << std::endl;

        // plein ecran sur l'ecran virtuel
        auto* se = dynamic_cast<SpinEditor*> (ed.get());
        se->toggleFullScreen();
        for (int i = 0; i < 4; ++i) { run (2); mm->runDispatchLoopUntil (30); }
        const auto* disp = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
        std::cout << "fullscreen: screen " << disp->userArea.toString() << " -> editor " << ed->getWidth() << "x" << ed->getHeight() << std::endl;
        auto fsImg = ed->createComponentSnapshot (ed->getLocalBounds(), true, 0.5f);
        auto ff = out.getChildFile ("fullscreen.png");
        ff.deleteFile();
        if (auto os = ff.createOutputStream()) png.writeImageToStream (fsImg, *os);
        se->toggleFullScreen();
        std::cout << "fullscreen off -> " << ed->getWidth() << "x" << ed->getHeight() << std::endl;
    }
    proc.reset();
    return 0;
}
