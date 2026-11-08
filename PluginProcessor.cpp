#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "NineHandsFolder.h"
#include "WigaWindowSize.h"
#include <cmath>

using namespace spin;

namespace spin
{
    const char* const kParamIds[numParams] = {
        "volA", "volB", "hiA", "hiB", "midA", "midB", "lowA", "lowB", "filtA", "filtB",
        "pitchA", "pitchB", "rangeA", "rangeB", "xfader", "master", "xfCurve" };
    const char* const kParamNames[numParams] = {
        "Volume A", "Volume B", "High A", "High B", "Mid A", "Mid B", "Low A", "Low B", "Filter A", "Filter B",
        "Pitch A", "Pitch B", "Pitch Range A", "Pitch Range B", "Crossfader", "Master", "Crossfader Curve" };
}

namespace
{
    constexpr float kRanges[4] = { 0.04f, 0.08f, 0.16f, 0.50f };

    // Discover DJ : notes et CC (canal indifferent) - d'apres le mapping Mixxx de l'ION Discover DJ
    namespace dj
    {
        constexpr int playA = 0x4A, playB = 0x4C, cueA = 0x3B, cueB = 0x42, syncA = 0x40, syncB = 0x47;
        constexpr int revA = 0x33, revB = 0x3C, pitchDnA = 0x43, pitchUpA = 0x44, pitchDnB = 0x45, pitchUpB = 0x46;
        constexpr int scratch = 0x48, touchA = 0x4D, touchB = 0x4E, loadA = 0x4B, loadB = 0x34, library = 0x4F;
        constexpr int jogA = 0x19, jogB = 0x18, xfader = 0x0A, master = 0x17, browse = 0x1A;
        constexpr int volA = 0x08, volB = 0x09, bassA = 0x14, bassB = 0x15, trebleA = 0x10, trebleB = 0x11;
    }

    // volume / master : 0,8 = 0 dB, 1 = +3,9 dB, 0 = silence (courbe quadratique, continue)
    inline float volGain (float v) { const float x = v / 0.8f; return x * x; }

    // EQ DJ : meme pente (6 dB par demi-course) des deux cotes de midi, -26 dB puis coupure franche a gauche, +6 dB a droite
    inline float eqGain (float v)
    {
        const float x = juce::jlimit (-1.0f, 1.0f, (v - 0.5f) * 2.0f);
        if (x >= 0.0f) return juce::Decibels::decibelsToGain (6.0f * x);
        const float db = 6.0f * x - 20.0f * x * x;
        return juce::Decibels::decibelsToGain (db) * juce::jlimit (0.0f, 1.0f, (x + 1.0f) / 0.08f);
    }

    // crossfader : courbe "mix" (pleine puissance jusqu'au centre) ou "scratch" (coupure franche aux bords)
    inline void xfGains (float x, bool scratchCurve, float& a, float& b)
    {
        if (scratchCurve)
        {
            a = juce::jlimit (0.0f, 1.0f, (1.0f - x) / 0.04f);
            b = juce::jlimit (0.0f, 1.0f, x / 0.04f);
        }
        else
        {
            a = x <= 0.5f ? 1.0f : std::cos ((x - 0.5f) * juce::MathConstants<float>::pi);
            b = x >= 0.5f ? 1.0f : std::cos ((0.5f - x) * juce::MathConstants<float>::pi);
        }
    }

    juce::File settingsFile() { return WigaWindowSize::file (SpinProcessor::settingsName()); }
    juce::File cacheFile()    { return settingsFile().getSiblingFile ("library.xml"); }
}

//==============================================================================
class LoadJob : public juce::ThreadPoolJob
{
public:
    LoadJob (SpinProcessor& p, int d, juce::File f, int g) : juce::ThreadPoolJob ("load"), proc (p), deckIdx (d), file (std::move (f)), gen (g) {}
    JobStatus runJob() override
    {
        juce::String err;
        auto t = spin::loadTrack (file, proc.formats, [this] { return shouldExit(); }, err);
        if (shouldExit()) return jobHasFinished;
        std::lock_guard<std::mutex> lk (proc.loadLock);
        proc.loadedResult[deckIdx] = t;
        proc.loadedError[deckIdx] = t ? juce::String() : (err.isEmpty() ? juce::String ("Cannot read this file") : err);
        proc.loadedGen[deckIdx] = gen;
        proc.loadedReady[deckIdx] = true;
        return jobHasFinished;
    }
private:
    SpinProcessor& proc;
    int deckIdx;
    juce::File file;
    int gen;
};

class ScanJob : public juce::ThreadPoolJob
{
public:
    ScanJob (SpinProcessor& p, juce::File d, juce::String wc) : juce::ThreadPoolJob ("scan"), proc (p), dir (std::move (d)), wildcard (std::move (wc)) {}
    JobStatus runJob() override
    {
        std::vector<spin::LibEntry> out;
        if (dir.isDirectory())
        {
            for (const auto& e : juce::RangedDirectoryIterator (dir, true, wildcard, juce::File::findFiles))
            {
                if (shouldExit()) return jobHasFinished;
                const auto f = e.getFile();
                if (f.getFileName().startsWithChar ('.')) continue;
                spin::LibEntry le;
                le.file = f;
                spin::splitName (f.getFileNameWithoutExtension(), le.artist, le.title);
                le.format = f.getFileExtension().trimCharactersAtStart (".").toUpperCase();
                le.folder = f.getParentDirectory().getRelativePathFrom (dir);
                if (le.folder == ".") le.folder = {};
                out.push_back (std::move (le));
                if (out.size() >= 20000) break;
            }
            std::sort (out.begin(), out.end(), [] (const spin::LibEntry& a, const spin::LibEntry& b)
            {
                const int c = a.folder.compareNatural (b.folder);
                return c != 0 ? c < 0 : a.file.getFileName().compareNatural (b.file.getFileName()) < 0;
            });
        }
        std::lock_guard<std::mutex> lk (proc.scanLock);
        proc.scanResult = std::move (out);
        proc.scanReady = true;
        proc.scanning = false;
        return jobHasFinished;
    }
private:
    SpinProcessor& proc;
    juce::File dir;
    juce::String wildcard;
};

//==============================================================================
static juce::AudioProcessorValueTreeState::ParameterLayout makeLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto knob = [&] (int i, float def)
    {
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { kParamIds[i], 1 }, kParamNames[i],
                                                      NormalisableRange<float> (0.0f, 1.0f), def));
    };
    knob (pVolA, 0.8f); knob (pVolB, 0.8f);
    knob (pHiA, 0.5f);  knob (pHiB, 0.5f);
    knob (pMidA, 0.5f); knob (pMidB, 0.5f);
    knob (pLowA, 0.5f); knob (pLowB, 0.5f);
    knob (pFiltA, 0.5f); knob (pFiltB, 0.5f);
    for (int i : { (int) pPitchA, (int) pPitchB })
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { kParamIds[i], 1 }, kParamNames[i],
                                                      NormalisableRange<float> (-1.0f, 1.0f), 0.0f));
    for (int i : { (int) pRangeA, (int) pRangeB })
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { kParamIds[i], 1 }, kParamNames[i],
                                                       StringArray { "4 %", "8 %", "16 %", "50 %" }, 1));
    knob (pXf, 0.5f);
    knob (pMaster, 0.8f);
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { kParamIds[pXfCurve], 1 }, kParamNames[pXfCurve],
                                                   StringArray { "Mix", "Scratch" }, 0));
    return l;
}

SpinProcessor::SpinProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Master", juce::AudioChannelSet::stereo(), true)
                                         .withOutput ("Preview", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "SPINDECK", makeLayout())
{
    formats.registerBasicFormats();
    for (int i = 0; i < numParams; ++i)
    {
        raw[i] = apvts.getRawParameterValue (kParamIds[i]);
        params[i] = apvts.getParameter (kParamIds[i]);
        pendingValue[i] = 0.0f;
        pendingSeq[i] = 0;
        appliedSeq[i] = 0;
    }
    for (auto& c : ccMap) c = -1;

    // dossier de musique choisi precedemment (commun a toutes les instances)
    if (auto x = juce::XmlDocument::parse (settingsFile()))
    {
        const juce::File dir (x->getStringAttribute ("libraryFolder"));
        if (dir.isDirectory()) libraryFolder = dir;
        controller = x->getIntAttribute ("controller", 0);
        invertXf = x->getBoolAttribute ("invertXf", true);
        jogSens = juce::jlimit (0, 3, x->getIntAttribute ("jogSens", 1));
        previewOutput = juce::jlimit (0, 1, x->getIntAttribute ("previewOutput", 0));
        previewVolume = (float) juce::jlimit (0.0, 1.0, x->getDoubleAttribute ("previewVolume", 0.7));
        theme = juce::jlimit (0, 3, x->getIntAttribute ("skin", 0));
    }
    if (! libraryFolder.isDirectory())
        libraryFolder = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
    loadLibraryCache();
    previewThread.startThread (juce::Thread::Priority::normal);
    rescanLibrary();
    startTimerHz (60);
}

SpinProcessor::~SpinProcessor()
{
    stopTimer();
    pool.removeAllJobs (true, 4000);
    pendingPreview = nullptr;
    pv = nullptr;
    previewHeld.clear();
    previewThread.stopThread (2000);
    if (bpmCacheDirty) saveLibraryCache();
}

bool SpinProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    if (! layouts.getMainInputChannelSet().isDisabled()) return false;
    if (layouts.outputBuses.size() > 1)
    {
        const auto& pb = layouts.outputBuses.getReference (1);
        if (! pb.isDisabled() && pb != juce::AudioChannelSet::stereo()) return false;
    }
    return true;
}

void SpinProcessor::prepareToPlay (double sr, int block)
{
    hostRate = sr;
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) juce::jmax (1, block), 2 };
    for (auto& d : deck)
    {
        d.xLow.prepare (spec);  d.xLow.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);  d.xLow.setCutoffFrequency (250.0f);
        d.xHigh.prepare (spec); d.xHigh.setType (juce::dsp::LinkwitzRileyFilterType::lowpass); d.xHigh.setCutoffFrequency (2500.0f);
        d.filt.prepare (spec);  d.filt.setResonance (0.9f);
        d.filtMode = 0;
        for (auto* s : { &d.gVol, &d.gHi, &d.gMid, &d.gLow, &d.filtCut }) s->reset (sr, 0.02);
        d.gXf.reset (sr, 0.002);                                    // coupures de crossfader nettes pour le scratch
        d.gVol.setCurrentAndTargetValue (1.0f); d.gHi.setCurrentAndTargetValue (1.0f);
        d.gMid.setCurrentAndTargetValue (1.0f); d.gLow.setCurrentAndTargetValue (1.0f);
        d.gXf.setCurrentAndTargetValue (1.0f);  d.filtCut.setCurrentAndTargetValue (0.0f);
    }
    gMaster.reset (sr, 0.02);
    gMaster.setCurrentAndTargetValue (volGain (paramValue (pMaster)));
    pitchRepeatDelay = (int) (sr * 0.35);
    pitchRepeatEvery = (int) (sr * 0.05);
    pvScratch.setSize (2, 512 * 5 + 16);                      // jusqu'a 192 kHz lu a 44,1 kHz, par tranches de 512
    ledRefresh = true;
}

//==============================================================================
float SpinProcessor::paramValue (int i) const
{
    if (pendingSeq[i].load (std::memory_order_acquire) != appliedSeq[i].load (std::memory_order_acquire))
        return pendingValue[i].load (std::memory_order_relaxed);
    return raw[i]->load (std::memory_order_relaxed);
}

void SpinProcessor::setParamFromAudio (int i, float v)
{
    pendingValue[i].store (v, std::memory_order_relaxed);
    pendingSeq[i].fetch_add (1, std::memory_order_release);
}

void SpinProcessor::post (int d, int type, double value)
{
    std::lock_guard<std::mutex> lk (postLock);
    const auto scope = fifo.write (1);
    if (scope.blockSize1 > 0)      cmds[(size_t) scope.startIndex1] = { d, type, value };
    else if (scope.blockSize2 > 0) cmds[(size_t) scope.startIndex2] = { d, type, value };
}

//==============================================================================
void SpinProcessor::jumpTo (int di, double p, bool stop)
{
    auto& d = deck[di];
    d.fadeTarget = p;
    d.fadeStop = stop;
    d.fade = 1;                                                  // fondu de sortie puis saut, puis fondu d'entree
}

void SpinProcessor::doSync (int di)
{
    auto& d = deck[di];
    auto& o = deck[1 - di];
    if (d.t == nullptr || o.t == nullptr || d.t->bpm <= 0 || o.t->bpm <= 0) return;

    const float oRate = 1.0f + paramValue (deckParam (pPitchA, 1 - di)) * kRanges[juce::jlimit (0, 3, (int) paramValue (deckParam (pRangeA, 1 - di)))];
    double need = (double) o.t->bpm * oRate / (double) d.t->bpm;
    for (double alt : { need * 2.0, need * 0.5 })                // 70 BPM contre 140 BPM : on cale sur le double / la moitie
        if (std::abs (alt - 1.0) < std::abs (need - 1.0)) need = alt;

    int range = juce::jlimit (0, 3, (int) paramValue (deckParam (pRangeA, di)));
    while (range < 3 && std::abs (need - 1.0) > kRanges[range]) ++range;
    if (range != (int) paramValue (deckParam (pRangeA, di)))
        setParamFromAudio (deckParam (pRangeA, di), (float) range);
    setParamFromAudio (deckParam (pPitchA, di), (float) juce::jlimit (-1.0, 1.0, (need - 1.0) / kRanges[range]));

    // phase : seulement si les deux platines tournent
    if (d.playing && o.playing)
    {
        const double bl = d.t->beatLength(), obl = o.t->beatLength();
        auto frac = [] (double x) { return x - std::floor (x); };
        const double fo = frac ((o.pos - o.t->firstBeat) / obl);
        const double fd = frac ((d.pos - d.t->firstBeat) / bl);
        double delta = fo - fd;
        if (delta > 0.5) delta -= 1.0;
        if (delta < -0.5) delta += 1.0;
        if (std::abs (delta) > 0.01) jumpTo (di, d.pos + delta * bl, false);
    }
}

void SpinProcessor::applyCmd (const Cmd& c)
{
    const int di = juce::jlimit (0, 1, c.deck);
    auto& d = deck[di];
    const double framesPerTick = d.t != nullptr ? d.t->sampleRate * 1.8 / 128.0 : 0.0;

    switch (c.type)
    {
        case cmdPlay:
            if (d.t == nullptr) break;
            if (d.cueHeld) { d.cueHeld = false; d.playDuringCue = true; d.playing = true; break; }
            d.playing = ! d.playing;
            break;

        case cmdCueDown:
            if (d.t == nullptr) break;
            if (d.playing) { jumpTo (di, d.cue, true); }
            else if (std::abs (d.pos - d.cue) < 2.0) { d.cueHeld = true; d.playDuringCue = false; d.playing = true; d.speed = 0; }
            else d.cue = d.pos;                                       // a l'arret ailleurs : nouveau point de cue
            break;

        case cmdCueUp:
            if (d.cueHeld) { d.cueHeld = false; if (! d.playDuringCue) jumpTo (di, d.cue, true); }
            break;

        case cmdSync:     doSync (di); break;
        case cmdRevDown:  d.reverse = true; break;
        case cmdRevUp:    d.reverse = false; break;

        case cmdPitchDown:
        case cmdPitchUp:
        {
            d.pitchDir = c.type == cmdPitchUp ? 1 : -1;
            d.pitchHold = 0;
            const int range = juce::jlimit (0, 3, (int) paramValue (deckParam (pRangeA, di)));
            const float step = 0.001f / kRanges[range];             // 0,1 % par appui
            setParamFromAudio (deckParam (pPitchA, di), juce::jlimit (-1.0f, 1.0f, paramValue (deckParam (pPitchA, di)) + d.pitchDir * step));
            break;
        }
        case cmdPitchRelease: d.pitchDir = 0; break;
        case cmdPitchReset:   setParamFromAudio (deckParam (pPitchA, di), 0.0f); break;

        case cmdSeekNorm:
            if (d.t != nullptr) jumpTo (di, juce::jlimit (0.0, 1.0, c.value) * (double) d.t->numFrames, false);
            break;

        case cmdTouchDown:                                          // souris sur la platine : scratch toujours actif
            d.mouseScratch = true;
            break;
        case cmdTouchUp:
            d.mouseScratch = false;
            break;

        case cmdTicks:
            if (d.t == nullptr) break;
            if (d.mouseScratch || (d.touched && scratchMode.load()))
                d.target = juce::jlimit (0.0, (double) d.t->numFrames, d.target + c.value * framesPerTick);
            else if (d.playing)
                d.nudge = juce::jlimit (-0.3, 0.3, d.nudge + c.value * 0.006);   // bord du jog : on pousse / freine le disque
            else
            {
                if (d.search <= 0) { d.target = d.pos; d.vel = d.speed; }
                d.search = (int) (hostRate * 0.15);
                d.target = juce::jlimit (0.0, (double) d.t->numFrames, d.target + c.value * framesPerTick);
            }
            break;

        case cmdNudge:
            d.nudge = juce::jlimit (-0.3, 0.3, d.nudge + c.value);
            break;

        case cmdScratchMode:
            scratchMode = c.value > 0.5;
            break;

        default: break;
    }
}

//==============================================================================
void SpinProcessor::handleMidi (const juce::MidiMessage& m)
{
    midiActivity.fetch_add (1, std::memory_order_relaxed);

    if (m.isController())
    {
        const int cc = m.getControllerNumber(), v = m.getControllerValue();

        // MIDI Learn : le prochain CC est attribue au parametre choisi
        const int learn = learnTarget.load();
        if (learn >= 0 && learn < numParams)
        {
            for (auto& x : ccMap) if (x.load() == learn) x = -1;
            ccMap[cc] = learn;
            learnTarget = -1;
        }
        const int mapped = ccMap[cc].load();
        if (mapped >= 0 && mapped < numParams)
        {
            const float v01 = (float) v / 127.0f;
            setParamFromAudio (mapped, params[mapped]->convertFrom0to1 (v01));
            return;
        }
        if (controller.load() != 0) return;

        auto rel = [] (int x) { return x >= 0x40 ? x - 0x80 : x; };   // 1..63 horaire, 127..65 anti-horaire
        const double sens[4] = { 0.25, 0.5, 0.75, 1.0 };
        const double jogK = sens[juce::jlimit (0, 3, jogSens.load())];
        switch (cc)
        {
            case dj::jogA:    applyCmd ({ 0, cmdTicks, rel (v) * jogK }); break;
            case dj::jogB:    applyCmd ({ 1, cmdTicks, rel (v) * jogK }); break;
            case dj::browse:  browseDelta.fetch_add (rel (v)); break;
            case dj::xfader:  setParamFromAudio (pXf, invertXf.load() ? 1.0f - v / 127.0f : v / 127.0f); break;
            case dj::master:  setParamFromAudio (pMaster, v / 127.0f); break;
            case dj::volA:    setParamFromAudio (pVolA, v / 127.0f); break;
            case dj::volB:    setParamFromAudio (pVolB, v / 127.0f); break;
            case dj::bassA:   setParamFromAudio (pLowA, v / 127.0f); break;
            case dj::bassB:   setParamFromAudio (pLowB, v / 127.0f); break;
            case dj::trebleA: setParamFromAudio (pHiA, v / 127.0f); break;
            case dj::trebleB: setParamFromAudio (pHiB, v / 127.0f); break;
            default: break;
        }
        return;
    }

    if (controller.load() != 0) return;
    if (! (m.isNoteOn() || m.isNoteOff())) return;
    const bool down = m.isNoteOn();                               // note on vel 0 = note off pour JUCE
    switch (m.getNoteNumber())
    {
        case dj::playA:    if (down) applyCmd ({ 0, cmdPlay }); break;
        case dj::playB:    if (down) applyCmd ({ 1, cmdPlay }); break;
        case dj::cueA:     applyCmd ({ 0, down ? cmdCueDown : cmdCueUp }); break;
        case dj::cueB:     applyCmd ({ 1, down ? cmdCueDown : cmdCueUp }); break;
        case dj::syncA:    if (down) applyCmd ({ 0, cmdSync }); break;
        case dj::syncB:    if (down) applyCmd ({ 1, cmdSync }); break;
        case dj::revA:     applyCmd ({ 0, down ? cmdRevDown : cmdRevUp }); break;
        case dj::revB:     applyCmd ({ 1, down ? cmdRevDown : cmdRevUp }); break;
        case dj::pitchDnA: applyCmd ({ 0, down ? cmdPitchDown : cmdPitchRelease }); break;
        case dj::pitchUpA: applyCmd ({ 0, down ? cmdPitchUp : cmdPitchRelease }); break;
        case dj::pitchDnB: applyCmd ({ 1, down ? cmdPitchDown : cmdPitchRelease }); break;
        case dj::pitchUpB: applyCmd ({ 1, down ? cmdPitchUp : cmdPitchRelease }); break;
        case dj::scratch:  if (down) scratchMode = ! scratchMode.load(); break;
        case dj::touchA:   deck[0].touched = down; break;
        case dj::touchB:   deck[1].touched = down; break;
        case dj::loadA:    if (down) loadRequest[0] = 1; break;
        case dj::loadB:    if (down) loadRequest[1] = 1; break;
        case dj::library:  if (down) libraryToggle = ! libraryToggle.load(); break;
        default: break;
    }
}

//==============================================================================
void SpinProcessor::renderDecks (float* outL, float* outR, int start, int n)
{
    const bool scratchCurve = paramValue (pXfCurve) > 0.5f;
    float xa, xb;
    xfGains (paramValue (pXf), scratchCurve, xa, xb);
    gMaster.setTargetValue (volGain (paramValue (pMaster)));

    for (int di = 0; di < 2; ++di)
    {
        auto& d = deck[di];
        d.gVol.setTargetValue (volGain (paramValue (deckParam (pVolA, di))));
        d.gHi.setTargetValue  (eqGain (paramValue (deckParam (pHiA, di))));
        d.gMid.setTargetValue (eqGain (paramValue (deckParam (pMidA, di))));
        d.gLow.setTargetValue (eqGain (paramValue (deckParam (pLowA, di))));
        d.gXf.setTargetValue  (di == 0 ? xa : xb);

        // filtre : gauche = passe-bas, droite = passe-haut, zone morte au centre
        const float f = paramValue (deckParam (pFiltA, di)) - 0.5f;
        const int mode = f < -0.03f ? -1 : (f > 0.03f ? 1 : 0);
        if (mode != d.filtMode && mode != 0)
        {
            d.filt.setType (mode < 0 ? juce::dsp::StateVariableTPTFilterType::lowpass : juce::dsp::StateVariableTPTFilterType::highpass);
            d.filt.reset();
        }
        d.filtMode = mode;
        d.filtCut.setTargetValue (mode == 0 ? 0.0f : (std::abs (f) - 0.03f) / 0.47f);

        if (d.t == nullptr)
        {
            for (auto* s : { &d.gVol, &d.gHi, &d.gMid, &d.gLow, &d.gXf, &d.filtCut }) s->skip (n);
            continue;
        }

        const double ratio = d.t->sampleRate / hostRate;
        const float rate = 1.0f + paramValue (deckParam (pPitchA, di)) * kRanges[juce::jlimit (0, 3, (int) paramValue (deckParam (pRangeA, di)))];
        const double accel = ratio / (0.06 * hostRate);           // demarrage du moteur ~60 ms
        const double brake = ratio / (0.20 * hostRate);           // freinage ~200 ms (plus long s'il faut ralentir de plus haut)
        const double w = 2.0 * juce::MathConstants<double>::pi * 18.0 / hostRate;   // ressort du scratch
        const double nudgeDecay = std::exp (-1.0 / (0.12 * hostRate));
        const double maxPos = (double) d.t->numFrames;

        for (int i = 0; i < n; ++i)
        {
            // --- mouvement
            const bool scr = d.mouseScratch || (d.touched && scratchMode.load (std::memory_order_relaxed)) || d.search > 0;
            if (scr != d.scratch)
            {
                d.scratch = scr;
                if (scr) { d.target = d.pos; d.vel = d.speed; }
                else     d.speed = d.vel;
            }
            if (d.search > 0) --d.search;

            if (d.scratch)
            {
                const double acc = w * w * (d.target - d.pos) - 2.0 * w * d.vel;
                d.vel += acc;
                d.pos += d.vel;
                d.speed = d.vel;
            }
            else
            {
                d.nudge *= nudgeDecay;
                const double tgt = d.playing ? ((double) rate * (d.reverse ? -1.0 : 1.0) + d.nudge) * ratio : 0.0;
                if (d.speed < tgt)      d.speed = std::min (tgt, d.speed + (d.speed < 0 ? brake : accel));
                else if (d.speed > tgt) d.speed = std::max (tgt, d.speed - (tgt >= 0 && d.playing ? accel : brake));
                d.pos += d.speed;
            }

            if (d.pos >= maxPos) { d.pos = maxPos; d.playing = false; d.cueHeld = false; d.speed = 0; d.vel = 0; }
            if (d.pos < 0.0)     { d.pos = 0.0; if (d.speed < 0) d.speed = 0; if (d.vel < 0) d.vel = 0; }

            // --- saut sans clic (cue, sync, clic sur la vue d'ensemble)
            if (d.fade > 0)
            {
                d.gainEnv -= 1.0f / 64.0f;
                if (d.gainEnv <= 0.0f)
                {
                    d.gainEnv = 0.0f;
                    d.pos = juce::jlimit (0.0, maxPos, d.fadeTarget);
                    d.target = d.pos;
                    if (d.fadeStop) { d.playing = false; d.speed = 0; d.vel = 0; }
                    d.fade = 0;
                }
            }
            else if (d.gainEnv < 1.0f) d.gainEnv = std::min (1.0f, d.gainEnv + 1.0f / 64.0f);

            // --- pitch +/- maintenu : repetition
            if (d.pitchDir != 0 && ++d.pitchHold >= pitchRepeatDelay && ((d.pitchHold - pitchRepeatDelay) % juce::jmax (1, pitchRepeatEvery)) == 0)
            {
                const int range = juce::jlimit (0, 3, (int) paramValue (deckParam (pRangeA, di)));
                setParamFromAudio (deckParam (pPitchA, di), juce::jlimit (-1.0f, 1.0f, paramValue (deckParam (pPitchA, di)) + d.pitchDir * 0.001f / kRanges[range]));
            }

            // --- lecture
            float l, r;
            d.t->read (d.pos, l, r);
            // disque immobile = silence (sinon l'echantillon sous la tete donne une tension continue)
            const float moving = (d.scratch || std::abs (d.speed) > 1e-5) ? 1.0f : 0.0f;
            d.motionEnv += (moving - d.motionEnv) * 0.004f;
            l *= d.gainEnv * d.motionEnv; r *= d.gainEnv * d.motionEnv;
            // coupe-continu ~5 Hz
            {
                const float k = 1.0f - (float) (juce::MathConstants<double>::twoPi * 5.0 / hostRate);
                const float yl = l - d.dcX[0] + k * d.dcY[0]; d.dcX[0] = l; d.dcY[0] = yl; l = yl;
                const float yr = r - d.dcX[1] + k * d.dcY[1]; d.dcX[1] = r; d.dcY[1] = yr; r = yr;
            }

            // --- EQ 3 bandes (Linkwitz-Riley 4e ordre : la somme reste a plat)
            const float gl = d.gLow.getNextValue(), gm = d.gMid.getNextValue(), gh = d.gHi.getNextValue();
            float lo, rest, mid, hi;
            d.xLow.processSample (0, l, lo, rest);  d.xHigh.processSample (0, rest, mid, hi);  l = lo * gl + mid * gm + hi * gh;
            d.xLow.processSample (1, r, lo, rest);  d.xHigh.processSample (1, rest, mid, hi);  r = lo * gl + mid * gm + hi * gh;

            // --- filtre
            const float fc = d.filtCut.getNextValue();
            if (d.filtMode != 0 || fc > 0.001f)
            {
                if ((i & 15) == 0)
                {
                    const float hz = d.filtMode < 0 ? 20000.0f * std::pow (80.0f / 20000.0f, fc)
                                                    : 20.0f * std::pow (8000.0f / 20.0f, fc);
                    d.filt.setCutoffFrequency (juce::jlimit (20.0f, (float) (hostRate * 0.45), hz));
                }
                l = d.filt.processSample (0, l);
                r = d.filt.processSample (1, r);
            }

            const float gv = d.gVol.getNextValue();
            l *= gv; r *= gv;
            d.peak = std::max (d.peak, std::max (std::abs (l), std::abs (r)));
            const float gx = d.gXf.getNextValue();
            outL[start + i] += l * gx;
            outR[start + i] += r * gx;
        }
    }

    // master + limiteur doux (jamais d'ecretage dur)
    for (int i = 0; i < n; ++i)
    {
        const float g = gMaster.getNextValue();
        for (float* o : { outL, outR })
        {
            float x = o[start + i] * g;
            const float ax = std::abs (x);
            if (ax > 0.9f) x = std::copysign (0.9f + 0.1f * std::tanh ((ax - 0.9f) / 0.1f), x);
            o[start + i] = x;
        }
    }
}

void SpinProcessor::sendLeds (juce::MidiBuffer& out)
{
    if (controller.load() != 0) return;
    const bool refresh = ledRefresh.exchange (false);
    auto beatOn = [this] (int di)
    {
        const auto& d = deck[di];
        if (d.t == nullptr || d.t->bpm <= 0 || ! d.playing) return false;
        const double ph = (d.pos - d.t->firstBeat) / d.t->beatLength();
        return ph - std::floor (ph) < 0.18;
    };
    const int notes[9] = { dj::playA, dj::playB, dj::cueA, dj::cueB, dj::syncA, dj::syncB, dj::revA, dj::revB, dj::scratch };
    const bool on[9] = {
        deck[0].playing, deck[1].playing,
        deck[0].t != nullptr && (deck[0].cueHeld || (! deck[0].playing && std::abs (deck[0].pos - deck[0].cue) < 2.0)),
        deck[1].t != nullptr && (deck[1].cueHeld || (! deck[1].playing && std::abs (deck[1].pos - deck[1].cue) < 2.0)),
        beatOn (0), beatOn (1), deck[0].reverse, deck[1].reverse, scratchMode.load() };
    for (int i = 0; i < 9; ++i)
    {
        const int s = on[i] ? 1 : 0;
        if (refresh || s != ledState[i])
        {
            ledState[i] = s;
            out.addEvent (juce::MidiMessage::noteOn (1, notes[i], (juce::uint8) (s ? 0x7F : 0x00)), 0);
        }
    }
}

void SpinProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (buffer.getNumChannels() < 2) return;
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);

    // nouvelles pistes publiees par le thread de messages
    for (int di = 0; di < 2; ++di)
        if (auto* p = pendingTrack[di].exchange (nullptr))
        {
            auto& d = deck[di];
            d.t = p;
            d.pos = d.cue = d.target = p->initialCue;
            d.speed = d.vel = d.nudge = 0;
            d.playing = d.cueHeld = d.reverse = false;
            d.fade = 0; d.gainEnv = 1.0f; d.motionEnv = 0.0f;
            view[di].appliedGen.store (p->id);
        }

    // commandes de l'interface
    {
        const auto scope = fifo.read (fifo.getNumReady());
        for (int i = 0; i < scope.blockSize1; ++i) applyCmd (cmds[(size_t) (scope.startIndex1 + i)]);
        for (int i = 0; i < scope.blockSize2; ++i) applyCmd (cmds[(size_t) (scope.startIndex2 + i)]);
    }

    // MIDI a l'echantillon pres : on rend jusqu'a chaque evenement
    int done = 0;
    for (const auto meta : midi)
    {
        const int at = juce::jlimit (0, n, meta.samplePosition);
        if (at > done) { renderDecks (L, R, done, at - done); done = at; }
        handleMidi (meta.getMessage());
    }
    if (done < n) renderDecks (L, R, done, n - done);

    renderPreview (buffer, n);

    midi.clear();                    // pas de renvoi des messages de la platine (boucle)
    sendLeds (midi);                 // LED de la Discover DJ (sortie MIDI de la piste vers la platine)

    // valeurs lues par l'interface
    float ml = 0, mr = 0;
    for (int i = 0; i < n; ++i) { ml = std::max (ml, std::abs (L[i])); mr = std::max (mr, std::abs (R[i])); }
    masterLevelL.store (std::max (ml, masterLevelL.load (std::memory_order_relaxed)), std::memory_order_relaxed);
    masterLevelR.store (std::max (mr, masterLevelR.load (std::memory_order_relaxed)), std::memory_order_relaxed);
    for (int di = 0; di < 2; ++di)
    {
        auto& d = deck[di];
        auto& v = view[di];
        v.pos.store (d.pos, std::memory_order_relaxed);
        v.cue.store (d.cue, std::memory_order_relaxed);
        v.speed.store (d.speed, std::memory_order_relaxed);
        v.playing.store (d.playing, std::memory_order_relaxed);
        v.scratching.store (d.scratch, std::memory_order_relaxed);
        v.reverse.store (d.reverse, std::memory_order_relaxed);
        v.cueHeld.store (d.cueHeld, std::memory_order_relaxed);
        v.rate.store (1.0f + paramValue (deckParam (pPitchA, di)) * kRanges[juce::jlimit (0, 3, (int) paramValue (deckParam (pRangeA, di)))], std::memory_order_relaxed);
        v.level.store (std::max (d.peak, v.level.load (std::memory_order_relaxed)), std::memory_order_relaxed);
        d.peak = 0;
    }
}

//==============================================================================
// thread de messages
void SpinProcessor::timerCallback()
{
    // valeurs venues du thread audio (MIDI, sync, pitch) -> hote
    for (int i = 0; i < numParams; ++i)
    {
        const auto seq = pendingSeq[i].load (std::memory_order_acquire);
        if (seq == appliedSeq[i].load (std::memory_order_relaxed)) continue;
        const float v = pendingValue[i].load (std::memory_order_relaxed);
        params[i]->setValueNotifyingHost (params[i]->convertTo0to1 (v));
        // si l'audio a reecrit entre-temps, seq a change : on retransmettra au prochain tour
        appliedSeq[i].store (seq, std::memory_order_release);
    }

    publishLoaded();

    // pre-ecoute : lecteurs remplaces, fin du morceau, suivi de la selection (souris ou encodeur BROWSE)
    {
        const int applied = previewAppliedId.load();
        while (previewHeld.size() > 1 && previewHeld.front()->id < applied && pendingPreview.load() != previewHeld.front().get())
            previewHeld.erase (previewHeld.begin());
        if (previewActive && ! previewPlaying.load() && pendingPreview.load() == nullptr && applied == previewIdCounter
            && previewPosSec.load() >= previewLenSec.load() - 0.05)
            previewActive = false;
        if (previewActive && selected != previewFollowSel && selected >= 0 && selected < (int) visible.size())
        {
            previewFollowSel = selected;
            const auto f = library[(size_t) visible[(size_t) selected]].file;
            if (f != previewPath) startPreview (f);
        }
    }

    // pistes qui ne servent plus a l'audio
    for (int di = 0; di < 2; ++di)
    {
        const int applied = view[di].appliedGen.load();
        auto& h = held[di];
        h.erase (std::remove_if (h.begin(), h.end(), [&] (const TrackPtr& t) { return t != current[di] && t->id < applied; }), h.end());
    }

    // boutons LOAD et encodeur BROWSE de la platine
    for (int di = 0; di < 2; ++di)
        if (loadRequest[di].exchange (0) != 0) loadSelected (di);
    if (const int delta = browseDelta.exchange (0); delta != 0 && ! visible.empty())
    {
        selected = juce::jlimit (0, (int) visible.size() - 1, (selected < 0 ? 0 : selected + delta));
        ++libraryVersion;
    }

    // resultat d'un scan de dossier
    {
        std::unique_lock<std::mutex> lk (scanLock);
        if (scanReady)
        {
            scanReady = false;
            library = std::move (scanResult);
            scanResult.clear();
            lk.unlock();
            for (auto& e : library)
                if (auto it = bpmCache.find (e.file.getFullPathName()); it != bpmCache.end()) e.bpm = it->second;
            rebuildVisible();
        }
    }

    if (bpmCacheDirty && --cacheSaveCountdown <= 0) { saveLibraryCache(); bpmCacheDirty = false; }
}

void SpinProcessor::publishLoaded()
{
    for (int di = 0; di < 2; ++di)
    {
        TrackPtr t; juce::String err; bool ready = false; int gen = 0;
        {
            std::lock_guard<std::mutex> lk (loadLock);
            if (loadedReady[di])
            {
                ready = true; t = std::move (loadedResult[di]); err = loadedError[di]; gen = loadedGen[di];
                loadedReady[di] = false;
            }
        }
        if (! ready || gen != loadGen[di]) continue;
        loading[di] = false;
        if (t == nullptr) { status[di] = err; continue; }
        status[di] = {};

        t->id = ++trackIdCounter;
        double cue = 0;                                           // premier son audible
        for (size_t c = 0; c < t->wPeak.size(); ++c) if (t->wPeak[c] > 6) { cue = (double) c * kWaveHop; break; }
        if (restoreCue[di] >= 0 && restoreCue[di] < (double) t->numFrames) cue = restoreCue[di];
        restoreCue[di] = -1;
        t->initialCue = cue;

        current[di] = t;
        held[di].push_back (t);
        pendingTrack[di].store (t.get());

        if (t->bpm > 0)
        {
            bpmCache[t->path] = t->bpm;
            bpmCacheDirty = true; cacheSaveCountdown = 120;
            for (auto& e : library) if (e.file.getFullPathName() == t->path) e.bpm = t->bpm;
            ++libraryVersion;
        }
    }
}

void SpinProcessor::loadFile (int di, const juce::File& f)
{
    di = juce::jlimit (0, 1, di);
    if (! f.existsAsFile()) { status[di] = "File not found"; return; }
    if (view[di].playing.load()) { status[di] = "Stop the deck before loading"; return; }
    loading[di] = true;
    status[di] = {};
    pool.addJob (new LoadJob (*this, di, f, ++loadGen[di]), true);
}

void SpinProcessor::loadSelected (int di)
{
    if (selected >= 0 && selected < (int) visible.size())
        loadFile (di, library[(size_t) visible[(size_t) selected]].file);
}

juce::String SpinProcessor::deckStatus (int di) const
{
    if (loading[di]) return "Loading and analysing...";
    return status[di];
}

//==============================================================================
void SpinProcessor::setLibraryFolder (const juce::File& dir)
{
    libraryFolder = dir;
    auto f = settingsFile();
    juce::XmlElement x ("StudioWigaSettings");
    if (auto old = juce::XmlDocument::parse (f)) x = *old;
    x.setAttribute ("libraryFolder", dir.getFullPathName());
    f.getParentDirectory().createDirectory();
    x.writeTo (f);
    rescanLibrary();
}

void SpinProcessor::rescanLibrary()
{
    if (! libraryFolder.isDirectory()) return;
    scanning = true;
    pool.addJob (new ScanJob (*this, libraryFolder, formats.getWildcardForAllFormats()), true);
}

void SpinProcessor::setFilter (const juce::String& f)
{
    filter = f;
    rebuildVisible();
}

void SpinProcessor::rebuildVisible()
{
    juce::File keep;
    if (selected >= 0 && selected < (int) visible.size()) keep = library[(size_t) visible[(size_t) selected]].file;

    visible.clear();
    juce::StringArray words;
    words.addTokens (filter.trim(), " ", "\"");
    words.removeEmptyStrings();
    for (int i = 0; i < (int) library.size(); ++i)
    {
        const auto& e = library[(size_t) i];
        const auto hay = e.artist + " " + e.title + " " + e.folder;
        bool ok = true;
        for (const auto& w : words) if (! hay.containsIgnoreCase (w)) { ok = false; break; }
        if (ok) visible.push_back (i);
    }
    selected = visible.empty() ? -1 : 0;
    for (int i = 0; i < (int) visible.size(); ++i) if (library[(size_t) visible[(size_t) i]].file == keep) { selected = i; break; }
    ++libraryVersion;
}

void SpinProcessor::loadLibraryCache()
{
    if (auto x = juce::XmlDocument::parse (cacheFile()))
        for (auto* e : x->getChildIterator())
            if (e->hasTagName ("T")) bpmCache[e->getStringAttribute ("p")] = (float) e->getDoubleAttribute ("bpm");
}

void SpinProcessor::saveLibraryCache()
{
    juce::XmlElement x ("SpindeckLibrary");
    for (const auto& [p, b] : bpmCache)
    {
        auto* e = x.createNewChildElement ("T");
        e->setAttribute ("p", p);
        e->setAttribute ("bpm", (double) b);
    }
    cacheFile().getParentDirectory().createDirectory();
    x.writeTo (cacheFile());
}

//==============================================================================
// pre-ecoute
void SpinProcessor::startPreview (const juce::File& f)
{
    std::unique_ptr<juce::AudioFormatReader> r (formats.createReaderFor (f));
    if (r == nullptr || r->lengthInSamples <= 0 || r->sampleRate <= 0) return;
    auto p = std::make_unique<Preview>();
    p->sampleRate = r->sampleRate;
    p->length = r->lengthInSamples;
    p->id = ++previewIdCounter;
    auto* buffered = new juce::BufferingAudioReader (r.release(), previewThread, (int) (p->sampleRate * 8.0));
    buffered->setReadTimeout (0);                              // jamais d'attente dans le thread audio : silence si pas encore lu
    p->reader.reset (buffered);
    previewLenSec = (double) p->length / p->sampleRate;
    previewPosSec = 0;
    previewPath = f;
    previewActive = true;
    previewPlaying = true;
    previewStopReq = false;
    pendingPreview.store (p.get());
    previewHeld.push_back (std::move (p));
}

void SpinProcessor::stopPreview()
{
    previewStopReq = true;
    previewActive = false;
}

void SpinProcessor::togglePreviewSelected()
{
    if (selected < 0 || selected >= (int) visible.size()) return;
    const auto f = library[(size_t) visible[(size_t) selected]].file;
    if (previewActive && previewPath == f) { stopPreview(); return; }
    previewFollowSel = selected;
    startPreview (f);
}

void SpinProcessor::seekPreview (double norm)
{
    if (previewActive) previewSeekReq = juce::jlimit (0.0, 1.0, norm);
}

void SpinProcessor::renderPreview (juce::AudioBuffer<float>& buffer, int n)
{
    if (auto* p = pendingPreview.exchange (nullptr))
    {
        pv = p; pvPos = 0; pvPlaying = true; pvFade = 0;
        previewAppliedId.store (p->id);
    }
    if (previewStopReq.exchange (false)) pvPlaying = false;
    if (pv == nullptr) { previewLevel = 0; return; }

    if (const double sk = previewSeekReq.exchange (-1.0); sk >= 0.0)
    {
        pvPos = sk * (double) pv->length;
        pvPlaying = true; pvFade = 0;
    }

    // destination : melangee au master, ou bus Preview (sorties 3-4) s'il est actif
    float* dl = buffer.getWritePointer (0);
    float* dr = buffer.getWritePointer (1);
    bool mixed = true;
    if (previewOutput.load() == 1 && getBusCount (false) > 1)
    {
        auto pbus = getBusBuffer (buffer, false, 1);
        if (pbus.getNumChannels() >= 2) { dl = pbus.getWritePointer (0); dr = pbus.getWritePointer (1); mixed = false; }
    }

    const float target = pvPlaying ? previewVolume.load() * previewVolume.load() : 0.0f;
    if (! pvPlaying && pvGain < 1e-4f) { pvGain = 0; previewPlaying = false; previewLevel = 0; return; }

    const double ratio = pv->sampleRate / hostRate;
    float peak = 0;
    int done = 0;
    while (done < n)
    {
        const int chunk = juce::jmin (512, n - done);
        const auto start = (juce::int64) std::floor (pvPos);
        const int need = juce::jmin (pvScratch.getNumSamples(), (int) std::ceil (chunk * ratio) + 3);
        pvScratch.clear();
        if (start < pv->length)
            pv->reader->read (&pvScratch, 0, need, start, true, true);
        const float* sl = pvScratch.getReadPointer (0);
        const float* sr = pv->reader->numChannels > 1 ? pvScratch.getReadPointer (1) : sl;
        double frac = pvPos - (double) start;
        for (int i = 0; i < chunk; ++i)
        {
            const int k = (int) frac;
            const float t = (float) (frac - k);
            float l = 0, r = 0;
            if (k + 1 < need)
            {
                l = sl[k] + (sl[k + 1] - sl[k]) * t;
                r = sr[k] + (sr[k + 1] - sr[k]) * t;
            }
            pvGain += (target - pvGain) * 0.002f;                 // fondu ~10 ms (marche, arret, volume)
            pvFade = std::min (1.0f, pvFade + 1.0f / 256.0f);     // entree sans clic apres un saut
            const float g = pvGain * pvFade;
            l *= g; r *= g;
            peak = std::max (peak, std::max (std::abs (l), std::abs (r)));
            if (mixed)
            {
                dl[done + i] = juce::jlimit (-1.0f, 1.0f, dl[done + i] + l);
                dr[done + i] = juce::jlimit (-1.0f, 1.0f, dr[done + i] + r);
            }
            else { dl[done + i] += l; dr[done + i] += r; }
            frac += ratio;
        }
        pvPos += chunk * ratio;
        done += chunk;
        if (pvPos >= (double) pv->length) { pvPos = (double) pv->length; pvPlaying = false; break; }
    }
    previewPosSec.store (pvPos / pv->sampleRate, std::memory_order_relaxed);
    previewPlaying.store (pvPlaying, std::memory_order_relaxed);
    previewLevel.store (std::max (peak, previewLevel.load (std::memory_order_relaxed)), std::memory_order_relaxed);
}

//==============================================================================
void SpinProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    for (int di = 0; di < 2; ++di)
    {
        const juce::String s (di == 0 ? "A" : "B");
        xml->setAttribute ("track" + s, current[di] ? current[di]->path : juce::String());
        xml->setAttribute ("cue" + s, view[di].cue.load());
    }
    xml->setAttribute ("uiScale", uiScaleIdx);
    xml->setAttribute ("uiFullScreen", uiFullScreen);
    xml->setAttribute ("scratchMode", scratchMode.load());
    juce::String map;
    for (int cc = 0; cc < 128; ++cc) if (ccMap[cc].load() >= 0) map << cc << ":" << kParamIds[ccMap[cc].load()] << ";";
    xml->setAttribute ("midiLearn", map);
    copyXmlToBinary (*xml, dest);
}

void SpinProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    uiScaleIdx = xml->getIntAttribute ("uiScale", -1);
    uiFullScreen = xml->getBoolAttribute ("uiFullScreen", false);
    scratchMode = xml->getBoolAttribute ("scratchMode", false);

    for (auto& c : ccMap) c = -1;
    juce::StringArray pairs;
    pairs.addTokens (xml->getStringAttribute ("midiLearn"), ";", "");
    for (const auto& p : pairs)
    {
        const int cc = p.upToFirstOccurrenceOf (":", false, false).getIntValue();
        const auto id = p.fromFirstOccurrenceOf (":", false, false);
        for (int i = 0; i < numParams; ++i)
            if (id == kParamIds[i] && cc >= 0 && cc < 128) ccMap[cc] = i;
    }

    for (int di = 0; di < 2; ++di)
    {
        const juce::String s (di == 0 ? "A" : "B");
        const juce::File f (xml->getStringAttribute ("track" + s));
        if (f.existsAsFile() && (current[di] == nullptr || current[di]->path != f.getFullPathName()))
        {
            restoreCue[di] = xml->getDoubleAttribute ("cue" + s, -1);
            loadFile (di, f);
        }
    }
}

juce::AudioProcessorEditor* SpinProcessor::createEditor() { return new SpinEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SpinProcessor(); }
