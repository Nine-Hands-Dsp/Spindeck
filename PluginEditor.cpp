#include "PluginEditor.h"
#include "WigaWindowSize.h"
#include "BinaryData.h"
#include <cmath>

using namespace spin;

namespace
{
    // toile de dessin (coordonnees logiques), mise a l'echelle de la fenetre par transformation
    constexpr int kW = 1600, kH = 900;
    constexpr int kRefW = 1600;                                    // 100 % = 1600 x 900 (taille par defaut demandee)
    const float kScales[3] = { 1.0f, 1.25f, 1.5f };                // 100 % (defaut, taille minimale), 125 %, 150 %
    constexpr float kDefaultScale = 1.0f;
    constexpr float kRanges[4] = { 0.04f, 0.08f, 0.16f, 0.50f };

    // themes de l'interface (choix du design)
    namespace col
    {
        juce::Colour bg, panel, panel2, line, text, dim, faint, deckA, deckB, play, cue, red, learn (0xffff3dbb);
        juce::Colour screen, knobTop, knobBottom, capTop, capBottom, padTop, padBottom, padText, vinylTop, vinylBottom;
        juce::Colour waveLow, waveMid, waveHigh;
        float radius = 8.0f, glow = 0.18f;
        inline juce::Colour deck (int d) { return d == 0 ? deckA : deckB; }
        int current = -1;

        void apply (int t)
        {
            if (t == current) return;
            current = t;
            auto C = [] (juce::uint32 v) { return juce::Colour (v); };
            switch (t)
            {
                default:
                case 0:   // Midnight : graphite, platines cyan / orange
                    bg = C (0xff0e1013); panel = C (0xff171a1f); panel2 = C (0xff1e2229); line = C (0xff2a2f38);
                    text = C (0xffe8ebf0); dim = C (0xff8a919c); faint = C (0xff565d68);
                    deckA = C (0xff36c5f0); deckB = C (0xffff8a3d); play = C (0xff3ddc84); cue = C (0xffffc23d); red = C (0xffff4d5e);
                    screen = C (0xff0b0d10); knobTop = C (0xff3a404b); knobBottom = C (0xff15181d); capTop = C (0xff4a515d); capBottom = C (0xff1b1f25);
                    padTop = C (0xff262b33); padBottom = C (0xff1a1d22); padText = C (0xff000000); vinylTop = C (0xff23262c); vinylBottom = C (0xff07080a);
                    waveLow = C (0xff2e6bff); waveMid = C (0xff3ddc97); waveHigh = C (0xffe8f4ff); radius = 8; glow = 0.18f;
                    break;
                case 1:   // Neon Club : nuit violette, magenta / vert acide, halos
                    bg = C (0xff08050f); panel = C (0xff120b1f); panel2 = C (0xff1b112d); line = C (0xff36245a);
                    text = C (0xfff5edff); dim = C (0xffa58fcb); faint = C (0xff604c82);
                    deckA = C (0xffff2bd6); deckB = C (0xff3dff8f); play = C (0xff21e6ff); cue = C (0xffffd23f); red = C (0xffff3b5c);
                    screen = C (0xff05030a); knobTop = C (0xff3a2a57); knobBottom = C (0xff0f0819); capTop = C (0xff4a3870); capBottom = C (0xff160d26);
                    padTop = C (0xff221639); padBottom = C (0xff140c23); padText = C (0xff000000); vinylTop = C (0xff241838); vinylBottom = C (0xff050309);
                    waveLow = C (0xffff2bd6); waveMid = C (0xff8b5cff); waveHigh = C (0xff21e6ff); radius = 14; glow = 0.35f;
                    break;
                case 2:   // Amber Mono : noir mat, une seule couleur ambre, angles vifs (sobre, studio)
                    bg = C (0xff0d0d0d); panel = C (0xff171717); panel2 = C (0xff1f1f1f); line = C (0xff2f2f2f);
                    text = C (0xfff2e8d5); dim = C (0xff9d9484); faint = C (0xff5a554d);
                    deckA = C (0xffffb000); deckB = C (0xffe9dfc9); play = C (0xffb8dc6a); cue = C (0xffffb000); red = C (0xffff5a36);
                    screen = C (0xff080808); knobTop = C (0xff2e2e2e); knobBottom = C (0xff111111); capTop = C (0xff3a3a3a); capBottom = C (0xff141414);
                    padTop = C (0xff1e1e1e); padBottom = C (0xff181818); padText = C (0xff000000); vinylTop = C (0xff1e1d1b); vinylBottom = C (0xff060606);
                    waveLow = C (0xffff7a00); waveMid = C (0xffffb000); waveHigh = C (0xfffff1cc); radius = 2; glow = 0.12f;
                    break;
                case 3:   // Pro Red / Blue : esprit logiciel DJ classique, A rouge / B bleu, potards chromes
                    bg = C (0xff14161b); panel = C (0xff22262e); panel2 = C (0xff2c313b); line = C (0xff3c434f);
                    text = C (0xffffffff); dim = C (0xffa8b0bd); faint = C (0xff656d7a);
                    deckA = C (0xffe5383b); deckB = C (0xff2d8cff); play = C (0xff2ecc71); cue = C (0xfff5a623); red = C (0xffff4d4d);
                    screen = C (0xff0c0e12); knobTop = C (0xffe9ecf1); knobBottom = C (0xff6f7782); capTop = C (0xffdfe3e8); capBottom = C (0xff7d8591);
                    padTop = C (0xff353b46); padBottom = C (0xff262b33); padText = C (0xffffffff); vinylTop = C (0xff25282e); vinylBottom = C (0xff08090b);
                    waveLow = C (0xff1f5fff); waveMid = C (0xff7ec8ff); waveHigh = C (0xffffffff); radius = 4; glow = 0.22f;
                    break;
            }
        }
    }

    juce::Font fnt (float h, bool bold = false)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    }

    // niveau crete -> position du VU (echelle en dB, -60 dB = eteint)
    float meterPos (float peak)
    {
        if (peak < 0.001f) return 0.0f;
        return juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (peak) + 60.0f) / 60.0f);
    }

    float effBpm (const Track* t, float rate) { return t != nullptr && t->bpm > 0 ? t->bpm * rate : 0.0f; }
}

//==============================================================================
// composant vivant : ses demandes de dessin passent par le chien de garde de l'editeur
struct Live : public juce::Component
{
    void ask()
    {
        if (auto* e = findParentComponentOfClass<SpinEditor>()) e->askRepaint (*this);
        else repaint();
    }
    void painted()
    {
        if (auto* e = findParentComponentOfClass<SpinEditor>()) e->notePainted();
    }
};

//==============================================================================
class SpinLook : public juce::LookAndFeel_V4
{
public:
    SpinLook()
    {
        setColour (juce::PopupMenu::backgroundColourId, col::panel2);
        setColour (juce::PopupMenu::textColourId, col::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::deckA.withAlpha (0.35f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::TextEditor::backgroundColourId, col::bg);
        setColour (juce::TextEditor::textColourId, col::text);
        setColour (juce::TextEditor::outlineColourId, col::line);
        setColour (juce::TextEditor::focusedOutlineColourId, col::deckA);
        setColour (juce::CaretComponent::caretColourId, col::deckA);
        setColour (juce::ListBox::backgroundColourId, col::panel);
        setColour (juce::TableHeaderComponent::backgroundColourId, col::panel2);
        setColour (juce::TableHeaderComponent::textColourId, col::dim);
        setColour (juce::TableHeaderComponent::outlineColourId, col::line);
        setColour (juce::ScrollBar::thumbColourId, col::faint);
        setColour (juce::AlertWindow::backgroundColourId, col::panel2);
        setColour (juce::AlertWindow::textColourId, col::text);
        setColour (juce::TextButton::buttonColourId, col::panel2);
        setColour (juce::TextButton::textColourOffId, col::text);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        const auto accent = juce::Colour ((juce::uint32) (juce::int64) s.getProperties().getWithDefault ("accent", (juce::int64) col::deckA.getARGB()));
        const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
        const bool learning = (bool) s.getProperties().getWithDefault ("learning", false);
        const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
        const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float ang = a0 + pos * (a1 - a0);

        // piste + arc de valeur
        juce::Path track; track.addCentredArc (c.x, c.y, r - 2, r - 2, 0, a0, a1, true);
        g.setColour (col::line); g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float from = bipolar ? (a0 + a1) * 0.5f : a0;
        if (std::abs (ang - from) > 0.01f)
        {
            juce::Path arc; arc.addCentredArc (c.x, c.y, r - 2, r - 2, 0, juce::jmin (from, ang), juce::jmax (from, ang), true);
            g.setColour (learning ? col::learn : accent);
            g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else if (bipolar)                                           // au centre : petit point de repere
        {
            g.setColour (learning ? col::learn : accent);
            g.fillEllipse (c.x - 1.8f, c.y - (r - 2) - 1.8f, 3.6f, 3.6f);
        }
        if (learning)
        {
            g.setColour (col::learn.withAlpha (0.8f));
            g.drawEllipse (c.x - r, c.y - r, 2 * r, 2 * r, 1.5f);
        }

        // capuchon
        const float kr = r - 7.0f;
        juce::ColourGradient grad (col::knobTop, c.x - kr * 0.6f, c.y - kr * 0.7f, col::knobBottom, c.x + kr * 0.6f, c.y + kr * 0.8f, false);
        g.setGradientFill (grad);
        g.fillEllipse (c.x - kr, c.y - kr, 2 * kr, 2 * kr);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (c.x - kr, c.y - kr, 2 * kr, 2 * kr, 1.0f);
        // repere
        const juce::Point<float> p1 (c.x + std::sin (ang) * kr * 0.35f, c.y - std::cos (ang) * kr * 0.35f);
        const juce::Point<float> p2 (c.x + std::sin (ang) * kr * 0.92f, c.y - std::cos (ang) * kr * 0.92f);
        g.setColour (col::knobTop.getBrightness() > 0.6f ? juce::Colour (0xff15171b) : col::text);
        g.drawLine ({ p1, p2 }, 2.2f);
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float, juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        const auto accent = juce::Colour ((juce::uint32) (juce::int64) s.getProperties().getWithDefault ("accent", (juce::int64) col::deckA.getARGB()));
        const bool centreMark = (bool) s.getProperties().getWithDefault ("centre", false);
        const bool learning = (bool) s.getProperties().getWithDefault ("learning", false);
        const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);

        if (style == juce::Slider::LinearVertical)
        {
            const float cx = b.getCentreX();
            // graduations
            g.setColour (col::faint);
            for (int i = 0; i <= 10; ++i)
            {
                const float yy = b.getY() + b.getHeight() * i / 10.0f;
                const float len = (i == 5 && centreMark) ? 12.0f : 6.0f;
                g.fillRect (cx - 9 - len, yy - 0.5f, len, 1.0f);
                g.fillRect (cx + 9, yy - 0.5f, len, 1.0f);
            }
            g.setColour (juce::Colours::black);
            g.fillRoundedRectangle (cx - 3, b.getY() - 4, 6, b.getHeight() + 8, 3);
            g.setColour (learning ? col::learn : col::line);
            g.drawRoundedRectangle (cx - 3, b.getY() - 4, 6, b.getHeight() + 8, 3, 1);
            // capuchon
            const float yy = pos;
            auto cap = juce::Rectangle<float> (cx - 15, yy - 11, 30, 22);
            juce::ColourGradient grad (col::capTop, cap.getX(), cap.getY(), col::capBottom, cap.getX(), cap.getBottom(), false);
            g.setGradientFill (grad); g.fillRoundedRectangle (cap, 3);
            g.setColour (juce::Colours::black); g.drawRoundedRectangle (cap, 3, 1);
            g.setColour (accent); g.fillRect (cap.getX() + 3, yy - 1, cap.getWidth() - 6, 2.0f);
        }
        else
        {
            const float cy = b.getCentreY();
            g.setColour (col::faint);
            for (int i = 0; i <= 10; ++i)
            {
                const float xx = b.getX() + b.getWidth() * i / 10.0f;
                g.fillRect (xx - 0.5f, cy - 16, 1.0f, i == 5 ? 7.0f : 4.0f);
            }
            g.setColour (juce::Colours::black);
            g.fillRoundedRectangle (b.getX() - 4, cy - 3, b.getWidth() + 8, 6, 3);
            g.setColour (learning ? col::learn : col::line);
            g.drawRoundedRectangle (b.getX() - 4, cy - 3, b.getWidth() + 8, 6, 3, 1);
            auto cap = juce::Rectangle<float> (pos - 13, cy - 16, 26, 32);
            juce::ColourGradient grad (col::capTop, cap.getX(), cap.getY(), col::capBottom, cap.getX(), cap.getBottom(), false);
            g.setGradientFill (grad); g.fillRoundedRectangle (cap, 3);
            g.setColour (juce::Colours::black); g.drawRoundedRectangle (cap, 3, 1);
            g.setColour (col::text); g.fillRect (pos - 1, cap.getY() + 3, 2.0f, cap.getHeight() - 6);
        }
    }

    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (col::panel2);
        g.setColour (col::line); g.drawRect (0, 0, w, h);
    }

    juce::Font getPopupMenuFont() override { return fnt (15.0f); }
};

//==============================================================================
// potard / fader relie a un parametre, avec MIDI Learn au clic droit
class ParamSlider : public juce::Slider
{
public:
    ParamSlider (SpinProcessor& p, int idx, juce::Slider::SliderStyle st, juce::Colour accent, bool bipolar = false)
        : juce::Slider (st, juce::Slider::NoTextBox), proc (p), param (idx),
          att (p.apvts, kParamIds[idx], *this)
    {
        getProperties().set ("accent", (juce::int64) accent.getARGB());
        getProperties().set ("bipolar", bipolar);
        getProperties().set ("centre", bipolar);
        setPopupMenuEnabled (false);
        setScrollWheelEnabled (true);
        if (st == juce::Slider::RotaryHorizontalVerticalDrag)
            setRotaryParameters (juce::degreesToRadians (-140.0f), juce::degreesToRadians (140.0f), true);
        setDoubleClickReturnValue (true, (double) p.apvts.getParameterRange (kParamIds[idx]).convertFrom0to1 (p.apvts.getParameter (kParamIds[idx])->getDefaultValue()));
        setVelocityModeParameters (1.0, 1, 0.0, false);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! e.mods.isPopupMenu()) { juce::Slider::mouseDown (e); return; }
        juce::PopupMenu m;
        int cc = -1;
        for (int i = 0; i < 128; ++i) if (proc.ccMap[i].load() == param) cc = i;
        m.addSectionHeader (kParamNames[param]);
        m.addItem (1, "MIDI Learn", true, proc.learnTarget.load() == param);
        m.addItem (2, cc >= 0 ? "Forget MIDI (CC " + juce::String (cc) + ")" : juce::String ("Forget MIDI"), cc >= 0);
        m.addItem (3, "Reset");
        juce::Component::SafePointer<ParamSlider> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int r)
        {
            if (safe == nullptr) return;
            auto& p = safe->proc;
            if (r == 1) p.learnTarget = p.learnTarget.load() == safe->param ? -1 : safe->param;
            if (r == 2) for (auto& x : p.ccMap) if (x.load() == safe->param) x = -1;
            if (r == 3) safe->setValue (safe->getDoubleClickReturnValue(), juce::sendNotificationSync);
        });
    }

    void tick()
    {
        const bool l = proc.learnTarget.load() == param;
        if (l != (bool) getProperties().getWithDefault ("learning", false)) { getProperties().set ("learning", l); repaint(); }
    }

    SpinProcessor& proc;
    const int param;
private:
    juce::AudioProcessorValueTreeState::SliderAttachment att;
};

//==============================================================================
// bouton de platine (CUE, PLAY, SYNC...) : appui / relachement, allume selon l'etat audio
class Pad : public Live
{
public:
    Pad (juce::String t, juce::Colour c) : text (std::move (t)), colour (c) { setRepaintsOnMouseActivity (false); }

    std::function<void()> onDown, onUp;
    std::function<bool()> isLit;
    std::function<void()> onRightClick;

    void setText (const juce::String& t) { if (t != text) { text = t; ask(); } }
    void setBig (bool b) { big = b; }

    void tick()
    {
        const bool l = isLit ? isLit() : false;
        if (l != lit) { lit = l; ask(); }
    }

    void paint (juce::Graphics& g) override
    {
        painted();
        auto b = getLocalBounds().toFloat().reduced (1.5f);
        const float rr = juce::jmin (col::radius, 10.0f);
        if (lit)
        {
            g.setColour (colour.withAlpha (col::glow)); g.fillRoundedRectangle (b.expanded (1.5f + col::glow * 8.0f), rr + 1 + col::glow * 8.0f);
            juce::ColourGradient grad (colour.brighter (0.25f), b.getX(), b.getY(), colour.darker (0.25f), b.getX(), b.getBottom(), false);
            g.setGradientFill (grad); g.fillRoundedRectangle (b, rr);
            g.setColour (colour.getPerceivedBrightness() > 0.55f ? juce::Colours::black.withAlpha (0.85f) : juce::Colours::white);
        }
        else
        {
            juce::ColourGradient grad (col::padTop, b.getX(), b.getY(), col::padBottom, b.getX(), b.getBottom(), false);
            g.setGradientFill (grad); g.fillRoundedRectangle (b, rr);
            g.setColour (colour.withAlpha (down ? 0.9f : 0.55f)); g.drawRoundedRectangle (b, rr, 1.4f);
            g.setColour (colour);
        }
        if (down && ! lit) { g.setColour (colour.withAlpha (0.25f)); g.fillRoundedRectangle (b, rr); g.setColour (colour); }
        g.setFont (fnt (big ? 22.0f : 15.0f, true));
        g.drawFittedText (text, getLocalBounds().reduced (4), juce::Justification::centred, 1);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }
        down = true; ask();
        if (onDown) onDown();
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu()) return;
        down = false; ask();
        if (onUp) onUp();
    }

private:
    juce::String text;
    juce::Colour colour;
    bool lit = false, down = false, big = false;
};

//==============================================================================
// forme d'onde defilante (10 s visibles, tete de lecture au centre), image construite par le timer
class WaveLane : public Live
{
public:
    WaveLane (SpinProcessor& p, int d) : proc (p), deck (d) {}

    void tick()
    {
        const auto t = proc.track (deck);
        const double pos = proc.view[deck].pos.load();
        const int w = getWidth(), h = getHeight();
        if (w <= 0 || h <= 0) return;
        if (t == lastTrack && std::abs (pos - lastPos) < 1.0 && img.getWidth() == w && img.getHeight() == h) return;
        lastTrack = t; lastPos = pos;

        // image logicielle : format de pixel connu sur toutes les plateformes (ecriture directe, rapide)
        if (img.getWidth() != w || img.getHeight() != h) img = juce::Image (juce::Image::ARGB, w, h, true, juce::SoftwareImageType());
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        const bool argb = bd.pixelFormat == juce::Image::ARGB;
        auto px = [&bd, argb] (int x, int y, int r, int g, int b)
        {
            if (argb) reinterpret_cast<juce::PixelARGB*> (bd.getPixelPointer (x, y))->setARGB (255, (juce::uint8) r, (juce::uint8) g, (juce::uint8) b);
            else      bd.setPixelColour (x, y, juce::Colour ((juce::uint8) r, (juce::uint8) g, (juce::uint8) b));
        };
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) px (x, y, col::screen.getRed(), col::screen.getGreen(), col::screen.getBlue());

        if (t != nullptr && ! t->wPeak.empty())
        {
            const double fpp = 10.0 * t->sampleRate / w;               // images par pixel
            const int cols = (int) t->wPeak.size();
            const float half = h * 0.5f - 2.0f;
            for (int x = 0; x < w; ++x)
            {
                const double f0 = pos + (x - w * 0.5) * fpp;
                const double f1 = f0 + fpp;
                int c0 = (int) std::floor (f0 / kWaveHop), c1 = (int) std::floor (f1 / kWaveHop);
                if (c1 < 0 || c0 >= cols) continue;
                c0 = juce::jlimit (0, cols - 1, c0); c1 = juce::jlimit (c0, cols - 1, c1);
                int pk = 0, lo = 0, mi = 0, hi = 0;
                for (int c = c0; c <= c1; ++c)
                {
                    pk = std::max (pk, (int) t->wPeak[(size_t) c]);
                    lo = std::max (lo, (int) t->wLow[(size_t) c]);
                    mi = std::max (mi, (int) t->wMid[(size_t) c]);
                    hi = std::max (hi, (int) t->wHigh[(size_t) c]);
                }
                // couleur par bande : basses bleues, mediums verts, aigus blancs
                const float s = (float) (lo + mi + hi) + 1.0f;
                float r  = (lo * col::waveLow.getFloatRed()   + mi * col::waveMid.getFloatRed()   + hi * col::waveHigh.getFloatRed())   / s;
                float gg = (lo * col::waveLow.getFloatGreen() + mi * col::waveMid.getFloatGreen() + hi * col::waveHigh.getFloatGreen()) / s;
                float b  = (lo * col::waveLow.getFloatBlue()  + mi * col::waveMid.getFloatBlue()  + hi * col::waveHigh.getFloatBlue())  / s;
                const float played = x < w / 2 ? 0.45f : 1.0f;
                const int a = (int) (pk / 255.0f * half);
                const int mid = h / 2;
                for (int y = -a; y <= a; ++y)
                {
                    const float k = (1.0f - 0.45f * std::abs ((float) y) / (half + 1.0f)) * played * 255.0f;
                    px (x, mid + y, (int) (r * k), (int) (gg * k), (int) (b * k));
                }
            }
        }
        ask();
    }

    void paint (juce::Graphics& g) override
    {
        painted();
        g.drawImageAt (img, 0, 0);
        const auto t = lastTrack;
        if (img.isNull()) g.fillAll (col::screen);
        const int w = getWidth(), h = getHeight();
        if (t != nullptr && t->bpm > 0)
        {
            // grille de temps (mesures plus marquees)
            const double fpp = 10.0 * t->sampleRate / w;
            const double bl = t->beatLength();
            const double left = lastPos - w * 0.5 * fpp;
            double k = std::ceil ((left - t->firstBeat) / bl);
            for (double f = t->firstBeat + k * bl; f < left + w * fpp; f += bl, k += 1.0)
            {
                const float x = (float) ((f - left) / fpp);
                const bool bar = ((long long) std::llround (k) % 4) == 0;
                g.setColour (juce::Colours::white.withAlpha (bar ? 0.35f : 0.12f));
                g.fillRect (x - (bar ? 1.0f : 0.5f), 0.0f, bar ? 2.0f : 1.0f, (float) h);
            }
            // cue
            const float cx = (float) ((proc.view[deck].cue.load() - left) / fpp);
            if (cx > -10 && cx < w + 10)
            {
                g.setColour (col::cue);
                g.fillRect (cx - 0.5f, 0.0f, 1.0f, (float) h);
                juce::Path tri; tri.addTriangle (cx - 6, 0, cx + 6, 0, cx, 8);
                g.fillPath (tri);
            }
        }
        // tete de lecture
        g.setColour (juce::Colours::white);
        g.fillRect (w * 0.5f - 1.0f, 0.0f, 2.0f, (float) h);
        g.setColour (col::deck (deck));
        g.fillRect (0, 0, 3, h);
        g.setColour (col::line);
        g.drawRect (getLocalBounds());
    }

private:
    SpinProcessor& proc;
    const int deck;
    juce::Image img;
    TrackPtr lastTrack;
    double lastPos = -1;
};

//==============================================================================
// vue d'ensemble du morceau (clic = deplacement)
class Overview : public Live
{
public:
    Overview (SpinProcessor& p, int d) : proc (p), deck (d) {}

    void tick()
    {
        const auto t = proc.track (deck);
        if (t != track || img.getWidth() != getWidth() || img.getHeight() != getHeight())
        {
            track = t;
            rebuild();
            ask();
        }
        if (t == nullptr) return;
        const int x = posX();
        if (x != lastX) { lastX = x; ask(); }
    }

    void paint (juce::Graphics& g) override
    {
        painted();
        g.fillAll (col::screen);
        if (img.isValid()) g.drawImageAt (img, 0, 0);
        if (track != nullptr)
        {
            const int x = posX();
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRect (0, 0, x, getHeight());
            const float cx = (float) (proc.view[deck].cue.load() / (double) track->numFrames * getWidth());
            g.setColour (col::cue); g.fillRect (cx - 0.5f, 0.0f, 1.0f, (float) getHeight());
            g.setColour (juce::Colours::white); g.fillRect (x - 1, 0, 2, getHeight());
        }
        g.setColour (col::line); g.drawRect (getLocalBounds());
    }

    void mouseDown (const juce::MouseEvent& e) override { seek (e); }
    void mouseDrag (const juce::MouseEvent& e) override { seek (e); }

private:
    void seek (const juce::MouseEvent& e)
    {
        if (track != nullptr) proc.post (deck, cmdSeekNorm, juce::jlimit (0.0, 1.0, (double) e.x / juce::jmax (1, getWidth())));
    }
    int posX() const
    {
        if (track == nullptr || track->numFrames <= 0) return 0;
        return (int) std::lround (proc.view[deck].pos.load() / (double) track->numFrames * getWidth());
    }
    void rebuild()
    {
        const int w = getWidth(), h = getHeight();
        if (w <= 0 || h <= 0 || track == nullptr || track->wPeak.empty()) { img = {}; return; }
        img = juce::Image (juce::Image::ARGB, w, h, true);
        juce::Graphics g (img);
        const int cols = (int) track->wPeak.size();
        const auto c = col::deck (deck);
        for (int x = 0; x < w; ++x)
        {
            const int c0 = (int) ((long long) x * cols / w), c1 = juce::jmax (c0 + 1, (int) ((long long) (x + 1) * cols / w));
            int pk = 0, lo = 0;
            for (int k = c0; k < c1 && k < cols; ++k) { pk = std::max (pk, (int) track->wPeak[(size_t) k]); lo = std::max (lo, (int) track->wLow[(size_t) k]); }
            const float a = juce::jmax (1.0f, pk / 255.0f * (h * 0.5f - 1.0f));
            g.setColour (c.interpolatedWith (juce::Colours::white, 0.12f * (1.0f - lo / 255.0f)));
            g.fillRect ((float) x, h * 0.5f - a, 1.0f, 2.0f * a);
        }
    }

    SpinProcessor& proc;
    const int deck;
    TrackPtr track;
    juce::Image img;
    int lastX = -1;
};

//==============================================================================
// platine : plateau metal, vinyle realiste (sillons, plages, reflet en noeud papillon fixe sous la lumiere),
// etiquette qui tourne avec la lecture, bras de lecture qui avance du bord vers le centre au fil du morceau.
// Scratch a la souris sur le disque.
class Platter : public Live
{
public:
    Platter (SpinProcessor& p, int d) : proc (p), deck (d) {}

    void tick()
    {
        const auto t = proc.track (deck);
        const double sr = t != nullptr ? t->sampleRate : 44100.0;
        const double pos = proc.view[deck].pos.load();
        const float a = (float) std::fmod (pos / (sr * 1.8) * juce::MathConstants<double>::twoPi, juce::MathConstants<double>::twoPi);
        const bool pl = proc.view[deck].playing.load(), sc = proc.view[deck].scratching.load(), sm = proc.scratchMode.load();

        // bras : du sillon d'entree (bord) au sillon de sortie (centre) selon la progression
        const bool hasTrack = t != nullptr && t->numFrames > 0;
        const float prog = hasTrack ? (float) juce::jlimit (0.0, 1.0, pos / (double) t->numFrames) : 0.0f;
        const float rho = hasTrack ? 0.95f - (0.95f - 0.47f) * prog : -1.0f;

        if (vinyl.getWidth() != getWidth() * kOver || vinyl.getHeight() != getHeight() * kOver) { buildImages(); ask(); }

        if (sc != scratching || pl != playing || sm != scratchMode || std::abs (rho - armRho) > 0.0015f)
        {
            angle = a; playing = pl; scratching = sc; scratchMode = sm; armRho = rho;
            ask();
            return;
        }
        if (std::abs (a - angle) > 0.002f)
        {
            angle = a;
            if (auto* e = findParentComponentOfClass<SpinEditor>()) e->askRepaint (*this, discArea());    // le disque tourne : poussieres, rayures, etiquette
            else repaint (discArea());
        }
    }

    void paint (juce::Graphics& g) override
    {
        painted();
        const auto accent = col::deck (deck);
        const float R = discR(), cx = centre().x, cy = centre().y;

        // plateau : bord metal sombre avec rangee de points stroboscopiques argentes qui tournent avec le disque
        {
            const float pr = R + 8.0f;                                             // bord argente un peu plus large : deux rangees de points
            g.setColour (juce::Colours::black.withAlpha (0.45f));                  // ombre portee du plateau
            g.fillEllipse (cx - pr + 3, cy - pr + 5, 2 * pr, 2 * pr);
            juce::ColourGradient rim (juce::Colour (0xffd3d8df), cx - pr, cy - pr, juce::Colour (0xff3a3e45), cx + pr, cy + pr, false);
            rim.addColour (0.45, juce::Colour (0xff80868f));
            g.setGradientFill (rim);
            g.fillEllipse (cx - pr, cy - pr, 2 * pr, 2 * pr);
            g.setColour (juce::Colour (0xff1c1f23).withAlpha (0.7f));             // fine gorge entre les deux rangees
            g.drawEllipse (cx - (R + 4.2f), cy - (R + 4.2f), 2 * (R + 4.2f), 2 * (R + 4.2f), 0.6f);
            g.setColour (juce::Colour (0xff8a9099).withAlpha (0.6f));
            g.drawEllipse (cx - pr + 0.5f, cy - pr + 0.5f, 2 * pr - 1, 2 * pr - 1, 0.8f);

            // deux rangees de points stroboscopiques, comme sur une platine Technics :
            // la rangee interieure tourne avec le disque ; la rangee exterieure, plus espacee, donne
            // l'impression de tourner a l'envers (effet stroboscopique), d'autant plus vite que le disque tourne.
            auto row = [&] (float radius, int count, float rot, float size)
            {
                for (int i = 0; i < count; ++i)
                {
                    const float a = rot + i * juce::MathConstants<float>::twoPi / count;
                    const float x = cx + std::sin (a) * radius, y = cy - std::cos (a) * radius;
                    const float lit = 0.45f + 0.55f * juce::jmax (0.0f, -(std::sin (a) - std::cos (a)) * 0.7071f);   // cote lumiere plus clair
                    g.setColour (juce::Colour (0xffeef1f5).withAlpha (0.40f + 0.55f * lit));
                    g.fillRect (x - size * 0.5f, y - size * 0.5f, size, size);
                }
            };
            // (a 30 images/s la rangee interieure avance de ~1/3 d'intervalle par image : elle suit le disque ;
            //  la rangee exterieure recule lentement, sans repliement, quel que soit le pitch)
            row (R + 2.3f, 180, angle, 1.2f);
            row (R + 6.2f, 72, -angle * 0.12f, 1.7f);
        }

        // vinyle (image fixe : les sillons sont des cercles, le reflet ne tourne pas avec le disque)
        g.setOpacity (1.0f);
        if (vinyl.isValid())
            g.drawImageTransformed (vinyl, juce::AffineTransform::scale (1.0f / kOver));

        drawSurface (g);
        g.setOpacity (1.0f);                     // l'etiquette ne doit pas prendre l'alpha de la derniere couleur

        // etiquette qui tourne
        if (label.isValid())
        {
            const float lw = (float) label.getWidth();
            g.drawImageTransformed (label, juce::AffineTransform::translation (-lw * 0.5f, -lw * 0.5f)
                                              .rotated (angle).scaled (1.0f / kOver).translated (cx, cy));
        }
        // brillance fixe du papier glace (la lumiere ne tourne pas avec le disque)
        {
            const float lr = R * 0.40f;
            juce::ColourGradient gl (juce::Colours::white.withAlpha (0.14f), cx - lr * 0.6f, cy - lr * 0.7f, juce::Colours::transparentWhite, cx, cy, true);
            g.setGradientFill (gl);
            g.fillEllipse (cx - lr, cy - lr, 2 * lr, 2 * lr);
        }
        // axe chrome
        {
            const float sr = juce::jmax (3.0f, R * 0.035f);
            juce::ColourGradient spin (juce::Colours::white, cx - sr, cy - sr, juce::Colour (0xff50555e), cx + sr, cy + sr, false);
            g.setGradientFill (spin);
            g.fillEllipse (cx - sr, cy - sr, 2 * sr, 2 * sr);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.drawEllipse (cx - sr, cy - sr, 2 * sr, 2 * sr, 0.8f);
        }


        drawArm (g);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        lastAngle = angleAt (e.position);
        proc.post (deck, cmdTouchDown);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const float a = angleAt (e.position);
        float d = a - lastAngle;
        if (d > juce::MathConstants<float>::pi)  d -= juce::MathConstants<float>::twoPi;
        if (d < -juce::MathConstants<float>::pi) d += juce::MathConstants<float>::twoPi;
        lastAngle = a;
        proc.post (deck, cmdTicks, d / juce::MathConstants<float>::twoPi * 128.0);
    }
    void mouseUp (const juce::MouseEvent&) override { proc.post (deck, cmdTouchUp); }

private:
    static constexpr int kOver = 2;                       // images calculees en 2x : nettes jusqu'a la taille maximale

    // le disque a gauche, la place du bras a droite
    float discR() const { return juce::jmax (20.0f, juce::jmin (getHeight() * 0.5f - 9.0f, (getWidth() - 46.0f) * 0.5f - 7.0f)); }
    juce::Point<float> centre() const { return { discR() + 9.0f, getHeight() * 0.5f }; }
    juce::Point<float> pivot() const  { const float R = discR(); return { centre().x + R + 20.0f, centre().y - R * 0.60f }; }
    float armLen() const              { return discR() * 1.12f; }

    juce::Rectangle<int> discArea() const
    {
        const float r = discR() + 13.0f;
        return juce::Rectangle<float> (centre().x - r, centre().y - r, 2 * r, 2 * r).getSmallestIntegerContainer();
    }

    juce::Rectangle<int> labelArea() const
    {
        const float lr = discR() * 0.40f + 2.0f;
        return juce::Rectangle<float> (centre().x - lr, centre().y - lr, 2 * lr, 2 * lr).getSmallestIntegerContainer();
    }

    float angleAt (juce::Point<float> p) const
    {
        const auto c = centre();
        return std::atan2 (p.x - c.x, -(p.y - c.y));
    }

    // pointe de lecture posee au rayon rho (fraction du rayon du disque), ou sur le repose-bras
    juce::Point<float> stylus() const
    {
        const auto P = pivot();
        const float L = armLen();
        if (armRho < 0) return { P.x + 2.0f, P.y + L };                       // au repos, a droite du disque
        const auto C = centre();
        const float rho = armRho * discR();
        const auto D = C - P;
        const float d = D.getDistanceFromOrigin();
        const float a = (L * L - rho * rho + d * d) / (2 * d);
        const float h = std::sqrt (juce::jmax (0.0f, L * L - a * a));
        const auto u = D / d;
        const juce::Point<float> perp (-u.y, u.x);
        const auto s1 = P + u * a + perp * h, s2 = P + u * a - perp * h;
        return s1.y > s2.y ? s1 : s2;
    }

    void drawArm (juce::Graphics& g)
    {
        const float R = discR();
        const auto P = pivot();
        const auto S = stylus();
        auto u = (S - P); u /= juce::jmax (1.0f, u.getDistanceFromOrigin());
        // porte-cellule incline (comme une platine reelle)
        const float off = juce::degreesToRadians (24.0f);
        const juce::Point<float> v (u.x * std::cos (off) - u.y * std::sin (off), u.x * std::sin (off) + u.y * std::cos (off));
        const auto K = S - v * (R * 0.20f);                     // jonction tube / porte-cellule
        const auto W = P - u * (R * 0.22f);                     // contrepoids
        const float tube = juce::jmax (2.5f, R * 0.045f);

        juce::Path arm;
        arm.startNewSubPath (W);
        arm.lineTo (P);
        arm.lineTo (P + u * (armLen() * 0.70f));
        arm.lineTo (K);
        juce::Path shell;
        {
            const juce::Point<float> n (-v.y, v.x);
            const float hw = R * 0.055f;
            const auto a0 = K - v * (R * 0.02f), a1 = S + v * (R * 0.05f);
            shell.startNewSubPath (a0 + n * hw * 0.6f);
            shell.lineTo (a1 + n * hw);
            shell.lineTo (a1 - n * hw);
            shell.lineTo (a0 - n * hw * 0.6f);
            shell.closeSubPath();
        }

        // ombre portee (lumiere en haut a gauche)
        {
            const auto sh = juce::AffineTransform::translation (5.0f, 7.0f);
            g.setColour (juce::Colours::black.withAlpha (0.38f));
            g.strokePath (arm, juce::PathStrokeType (tube + 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), sh);
            g.fillPath (shell, sh);
            g.fillEllipse (P.x - R * 0.13f + 5, P.y - R * 0.13f + 7, R * 0.26f, R * 0.26f);
        }

        // socle du bras : alu avec reflet et grain fin
        {
            const float br = R * 0.15f;
            juce::Path disc; disc.addEllipse (P.x - br, P.y - br, 2 * br, 2 * br);
            juce::ColourGradient base (juce::Colour (0xffc3c9d1), P.x - br, P.y - br, juce::Colour (0xff1f2226), P.x + br, P.y + br, false);
            base.addColour (0.42, juce::Colour (0xff8a919b));
            base.addColour (0.55, juce::Colour (0xffd8dde3));
            base.addColour (0.70, juce::Colour (0xff50565f));
            g.setGradientFill (base);
            g.fillPath (disc);
            grain (g, disc, 0.6f, P);
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.drawEllipse (P.x - br, P.y - br, 2 * br, 2 * br, 1.0f);
        }
        // repose-bras
        {
            const juce::Point<float> rest (P.x + 2.0f, P.y + armLen() - R * 0.10f);
            g.setColour (juce::Colour (0xff2a2d33));
            g.fillEllipse (rest.x - R * 0.05f, rest.y - R * 0.05f, R * 0.10f, R * 0.10f);
        }

        // tube chrome : bandes de reflet en travers du tube + grain fin brosse dans l'axe
        const auto Bp = P + u * (armLen() * 0.70f);              // coude du bras (forme en S des platines)
        chromeRod (g, W, P, tube * 0.9f);
        chromeRod (g, P, Bp, tube);
        chromeRod (g, Bp, K, tube);
        {
            const float jr = tube * 0.75f;                     // jonction tube / porte-cellule
            juce::ColourGradient jg (juce::Colours::white, K.x - jr, K.y - jr, juce::Colour (0xff3a3e45), K.x + jr, K.y + jr, false);
            g.setGradientFill (jg);
            g.fillEllipse (K.x - jr, K.y - jr, 2 * jr, 2 * jr);
        }

        // contrepoids : gros cylindre chrome avec une gorge sombre
        {
            const float cl = R * 0.13f;
            chromeRod (g, W + u * (cl * 0.5f), W - u * (cl * 0.5f), R * 0.20f, false);
            const juce::Point<float> n (-u.y, u.x);
            const float cw = R * 0.10f;
            const auto gA = W + u * (cl * 0.12f);
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.drawLine ({ gA + n * cw, gA - n * cw }, 1.2f);
        }

        // pivot
        {
            const float pr = R * 0.065f;
            juce::ColourGradient pg (juce::Colours::white, P.x - pr, P.y - pr, juce::Colour (0xff5a606a), P.x + pr, P.y + pr, false);
            g.setGradientFill (pg);
            g.fillEllipse (P.x - pr, P.y - pr, 2 * pr, 2 * pr);
        }

        // porte-cellule + cellule
        g.setColour (juce::Colour (0xff1c1e22));
        g.fillPath (shell);
        g.setColour (juce::Colour (0xff6b717b));
        g.strokePath (shell, juce::PathStrokeType (0.9f));
        {
            const juce::Point<float> n (-v.y, v.x);
            const float cw = R * 0.035f;
            juce::Path cart;
            const auto c0 = S - v * (R * 0.07f), c1 = S + v * (R * 0.03f);
            cart.startNewSubPath (c0 + n * cw); cart.lineTo (c1 + n * cw); cart.lineTo (c1 - n * cw); cart.lineTo (c0 - n * cw); cart.closeSubPath();
            g.setColour (col::deck (deck).darker (0.2f));
            g.fillPath (cart);
            // leve-bras du porte-cellule
            const auto f0 = K + n * (R * 0.05f), f1 = f0 + n * (R * 0.07f) - v * (R * 0.02f);
            g.setColour (juce::Colour (0xffb9bfc8));
            g.drawLine ({ f0, f1 }, 1.4f);
        }
    }

    // texture de grain chrome : stries fines et irregulieres (le metal poli/brosse), tuilable, calculee une fois
    static const juce::Image& grainTexture()
    {
        static juce::Image tex = []
        {
            const int N = 128;
            juce::Image im (juce::Image::ARGB, N, N, true, juce::SoftwareImageType());
            juce::Random rnd (777);
            std::vector<float> row ((size_t) N), phase ((size_t) N);
            for (int y = 0; y < N; ++y) { row[(size_t) y] = rnd.nextFloat() * 2 - 1; phase[(size_t) y] = rnd.nextFloat() * 6.2832f; }
            for (int y = 0; y < N; ++y)
                for (int x = 0; x < N; ++x)
                {
                    // stries le long de x (bruit par ligne, tres etire) + pointilles tres fins
                    const float along = std::sin (x * 6.2832f / N * 3 + phase[(size_t) y]) * 0.35f;
                    const float v = row[(size_t) y] * 0.65f + along * row[(size_t) ((y + 37) % N)] + (rnd.nextFloat() * 2 - 1) * 0.35f;
                    const float a = juce::jlimit (0.0f, 1.0f, std::abs (v)) * 0.30f;
                    im.setPixelAt (x, y, (v > 0 ? juce::Colours::white : juce::Colours::black).withAlpha (a));
                }
            return im;
        }();
        return tex;
    }

    // applique le grain a une forme, stries orientees selon dir (axe du tube)
    static void grain (juce::Graphics& g, const juce::Path& shape, float strength, juce::Point<float> origin, float dirAngle = 0.785f)
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (shape);
        g.setFillType (juce::FillType (grainTexture(), juce::AffineTransform::scale (0.35f, 0.35f).rotated (dirAngle).translated (origin)));
        g.setOpacity (strength);
        g.fillPath (shape);
    }

    // tige chrome de a a b : degrade en travers (bandes claires/sombres comme un vrai cylindre poli), grain dans l'axe
    static void chromeRod (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b, float width, bool roundCaps = true)
    {
        auto d = b - a;
        const float len = d.getDistanceFromOrigin();
        if (len < 0.5f) return;
        d /= len;
        juce::Point<float> n (-d.y, d.x);
        if (n.x + n.y > 0) n = -n;                                // n regarde vers la lumiere (haut gauche)
        juce::Path rod;
        rod.startNewSubPath (a); rod.lineTo (b);
        juce::Path body;
        juce::PathStrokeType (width, juce::PathStrokeType::curved, roundCaps ? juce::PathStrokeType::rounded : juce::PathStrokeType::butt)
            .createStrokedPath (body, rod);

        const auto hi = a + n * (width * 0.5f), lo = a - n * (width * 0.5f);
        juce::ColourGradient cg (juce::Colour (0xff2c2f35), hi.x, hi.y, juce::Colour (0xff1d1f23), lo.x, lo.y, false);
        cg.addColour (0.10, juce::Colour (0xff9aa1ab));
        cg.addColour (0.22, juce::Colour (0xfff7f9fc));          // reflet principal
        cg.addColour (0.34, juce::Colour (0xffb7bec8));
        cg.addColour (0.52, juce::Colour (0xff5b616b));          // bande sombre (reflet du sol)
        cg.addColour (0.70, juce::Colour (0xffaab1bb));          // contre-reflet
        cg.addColour (0.86, juce::Colour (0xff6c727c));
        g.setGradientFill (cg);
        g.fillPath (body);
        grain (g, body, 0.9f, a, std::atan2 (d.y, d.x));
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.strokePath (body, juce::PathStrokeType (0.6f));
    }

    // ------------------------------------------------------------ surface du disque qui tourne
    // Les sillons etant des cercles, un disque parfait ne montrerait pas qu'il tourne : comme sur un vrai vinyle,
    // ce sont les poussieres, les micro-rayures et les marques de pressage qui trahissent la rotation.
    // Les poussieres scintillent en passant dans le reflet de la lampe.
    struct Speck { float rho, phi, size, alpha; };
    struct Mark  { float rho, phi, span, alpha, width; };
    std::vector<Speck> specks;
    std::vector<Mark> marks;

    void buildSurface()
    {
        specks.clear(); marks.clear();
        juce::Random rnd (4242 + deck * 31);
        for (int i = 0; i < 45; ++i)
            specks.push_back ({ 0.38f + 0.59f * std::sqrt (rnd.nextFloat()), rnd.nextFloat() * juce::MathConstants<float>::twoPi,
                                0.35f + rnd.nextFloat() * (rnd.nextFloat() < 0.1f ? 0.9f : 0.4f), 0.05f + rnd.nextFloat() * 0.12f });
        // micro-rayures dans le sens des sillons
        for (int i = 0; i < 7; ++i)
            marks.push_back ({ 0.42f + 0.52f * rnd.nextFloat(), rnd.nextFloat() * juce::MathConstants<float>::twoPi,
                               0.08f + rnd.nextFloat() * 0.35f, 0.07f + rnd.nextFloat() * 0.08f, 0.5f });
        // marques de pressage : bandes de sillons un peu plus claires sur un secteur
        for (int i = 0; i < 3; ++i)
        {
            const float r0 = 0.45f + 0.45f * rnd.nextFloat(), p0 = rnd.nextFloat() * juce::MathConstants<float>::twoPi, sp = 0.6f + rnd.nextFloat() * 0.9f;
            for (int k = 0; k < 6; ++k)
                marks.push_back ({ r0 + k * 0.006f, p0 + k * 0.03f, sp, 0.022f, 1.2f });
        }
    }

    void drawSurface (juce::Graphics& g)
    {
        if (specks.empty()) buildSurface();
        const float R = discR() - 1.0f;
        const auto c = centre();
        const float lightA = juce::degreesToRadians (-135.0f);
        auto sheenAt = [lightA] (float x, float y)
        {
            float th = std::atan2 (y, x) - lightA;
            th = std::fmod (std::abs (th), juce::MathConstants<float>::pi);
            th = juce::jmin (th, juce::MathConstants<float>::pi - th);
            return std::exp (-(th * th) / (0.17f * 0.17f));
        };

        for (const auto& m : marks)
        {
            juce::Path arc;
            const float r = m.rho * R;
            arc.addCentredArc (c.x, c.y, r, r, 0.0f, m.phi + angle, m.phi + angle + m.span, true);
            g.setColour (juce::Colours::white.withAlpha (m.alpha));
            g.strokePath (arc, juce::PathStrokeType (m.width));
        }
        for (const auto& sp : specks)
        {
            const float a = sp.phi + angle;
            const float x = std::sin (a) * sp.rho * R, y = -std::cos (a) * sp.rho * R;
            const float glint = sheenAt (x, y);
            g.setColour (juce::Colours::white.withAlpha (juce::jlimit (0.0f, 1.0f, sp.alpha + 0.45f * glint)));
            const float s = sp.size * (1.0f + 0.6f * glint);
            g.fillEllipse (c.x + x - s * 0.5f, c.y + y - s * 0.5f, s, s);
        }
    }

    // ------------------------------------------------------------ images (construites par le timer, jamais dans paint)
    static float hash (int n)
    {
        n = (n << 13) ^ n;
        return 1.0f - (float) ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f * 0.5f;
    }

    void buildImages()
    {
        const int W = getWidth() * kOver, H = getHeight() * kOver;
        if (W <= 0 || H <= 0) return;
        const float R = (discR() - 1.0f) * kOver;              // vinyle un peu plus petit que le plateau
        const float cx = centre().x * kOver, cy = centre().y * kOver;
        const float lightA = juce::degreesToRadians (-135.0f);  // lumiere en haut a gauche

        vinyl = juce::Image (juce::Image::ARGB, W, H, true, juce::SoftwareImageType());
        {
            juce::Image::BitmapData bd (vinyl, juce::Image::BitmapData::readWrite);
            const int x0 = juce::jmax (0, (int) (cx - R - 2)), x1 = juce::jmin (W, (int) (cx + R + 2));
            const int y0 = juce::jmax (0, (int) (cy - R - 2)), y1 = juce::jmin (H, (int) (cy + R + 2));
            const float gaps[5] = { 0.895f, 0.805f, 0.715f, 0.625f, 0.535f };   // plages entre les morceaux
            for (int y = y0; y < y1; ++y)
                for (int x = x0; x < x1; ++x)
                {
                    const float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
                    const float dist = std::sqrt (dx * dx + dy * dy);
                    const float rho = dist / R;
                    if (rho > 1.0f + 1.0f / R) continue;
                    const float cover = juce::jlimit (0.0f, 1.0f, (R - dist) + 0.5f);

                    // reflet en noeud papillon (deux secteurs opposes, axe vers la lumiere)
                    float th = std::atan2 (dy, dx) - lightA;
                    th = std::fmod (std::abs (th), juce::MathConstants<float>::pi);
                    th = juce::jmin (th, juce::MathConstants<float>::pi - th);
                    const float ambient = 0.035f * (1.0f - (dx + dy) / (2.8f * R));

                    float lum = 0.050f + ambient;
                    float r = 1.0f, gr = 1.0f, b = 1.0f;
                    const bool grooved = rho > 0.43f && rho < 0.972f;
                    bool smooth = ! grooved;
                    for (float gp : gaps) if (std::abs (rho - gp) < 0.0042f) smooth = true;

                    if (rho >= 0.985f)                              // bord arrondi du disque
                    {
                        const float e = (rho - 0.985f) / 0.015f;
                        lum += 0.10f * (1.0f - e) * (0.5f + 0.5f * std::cos (th * 2.0f));
                    }
                    else if (smooth)                                // surfaces lisses : reflet net et brillant
                    {
                        lum += 0.42f * std::exp (-(th * th) / (0.05f * 0.05f)) * (0.4f + 0.6f * rho);
                        lum += 0.055f;                              // fin cercle plus clair visible tout autour
                    }
                    else                                            // sillons : fine modulation + reflet granuleux et large
                    {
                        const float ring = dist * 0.55f;
                        const int ringIdx = (int) (dist * 0.18f);
                        const float amp = 0.55f + 0.45f * hash (ringIdx + deck * 7919);
                        const float groove = 0.5f + 0.5f * std::sin (ring * juce::MathConstants<float>::twoPi);
                        lum += 0.012f * groove;
                        const float sheen = std::exp (-(th * th) / (0.19f * 0.19f)) * (0.35f + 0.65f * rho);
                        const float spec = sheen * (0.21f + 0.30f * groove * (0.75f + 0.25f * amp));
                        // diffraction : la couleur change avec le rayon et l'ecart au centre du reflet (arc-en-ciel des vinyles)
                        float hue = std::fmod (0.55f + rho * 2.6f + th * 1.6f, 1.0f);
                        if (hue < 0) hue += 1.0f;
                        const auto rainbow = juce::Colour::fromHSV (hue, 0.48f, 1.0f, 1.0f);
                        const float w = juce::jlimit (0.0f, 0.9f, spec * 2.0f);
                        lum += spec;
                        r  = 1.0f + w * (rainbow.getFloatRed()   * 1.6f - 1.0f);
                        gr = 1.0f + w * (rainbow.getFloatGreen() * 1.6f - 1.0f);
                        b  = 1.0f + w * (rainbow.getFloatBlue()  * 1.6f - 1.0f);
                    }
                    lum = juce::jlimit (0.0f, 1.0f, lum);
                    const auto px = juce::Colour::fromFloatRGBA (juce::jlimit (0.0f, 1.0f, lum * r * 0.96f),
                                                                 juce::jlimit (0.0f, 1.0f, lum * gr * 0.98f),
                                                                 juce::jlimit (0.0f, 1.0f, lum * b * 1.03f), cover);
                    bd.setPixelColour (x, y, px);
                }
        }

        // etiquette : papier teinte a la couleur de la platine, imprimee, avec grain
        const int LW = (int) std::ceil (R * 0.80f) | 1;
        label = juce::Image (juce::Image::ARGB, LW, LW, true, juce::SoftwareImageType());
        {
            juce::Graphics lg (label);
            const float lr = LW * 0.5f;
            const auto paper = col::deck (deck);
            juce::ColourGradient pg (paper.brighter (0.12f), 0, 0, paper.darker (0.18f), (float) LW, (float) LW, false);
            lg.setGradientFill (pg);
            lg.fillEllipse (0, 0, (float) LW, (float) LW);
            lg.setColour (juce::Colours::black.withAlpha (0.25f));
            lg.drawEllipse (0.5f, 0.5f, LW - 1.0f, LW - 1.0f, 1.0f);
            const auto ink = juce::Colour (0xff0d1014);
            lg.setColour (ink.withAlpha (0.35f));
            lg.drawEllipse (lr * 0.08f, lr * 0.08f, LW - lr * 0.16f, LW - lr * 0.16f, juce::jmax (1.0f, lr * 0.012f));
            lg.setColour (ink);
            lg.setFont (fnt (lr * 0.27f, true));
            lg.drawText ("SPINDECK", juce::Rectangle<float> (0, lr * 0.38f, (float) LW, lr * 0.32f), juce::Justification::centred);
            lg.setFont (fnt (lr * 0.12f, true));
            lg.drawText ("NINE HANDS | DSP", juce::Rectangle<float> (0, lr * 0.70f, (float) LW, lr * 0.16f), juce::Justification::centred);
            lg.setFont (fnt (lr * 0.15f, true));
            lg.drawText (deck == 0 ? "SIDE A" : "SIDE B", juce::Rectangle<float> (0, lr * 1.22f, (float) LW, lr * 0.20f), juce::Justification::centred);
            lg.setFont (fnt (lr * 0.10f, true));
            lg.drawText ("33 1/3 RPM", juce::Rectangle<float> (0, lr * 1.44f, (float) LW, lr * 0.15f), juce::Justification::centred);
            lg.fillRect (lr * 0.52f, lr * 1.515f, lr * 0.16f, juce::jmax (1.0f, lr * 0.012f));
            lg.fillRect (lr * 1.32f, lr * 1.515f, lr * 0.16f, juce::jmax (1.0f, lr * 0.012f));
            // grain du papier
            juce::Random rnd (1234 + deck);
            for (int i = 0; i < LW * LW / 6; ++i)
            {
                const float x = rnd.nextFloat() * LW, y = rnd.nextFloat() * LW;
                if ((x - lr) * (x - lr) + (y - lr) * (y - lr) > lr * lr) continue;
                lg.setColour ((rnd.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (0.05f));
                lg.fillRect (x, y, 1.0f, 1.0f);
            }
        }
    }

    SpinProcessor& proc;
    const int deck;
    juce::Image vinyl, label;
    float angle = 0, lastAngle = 0, armRho = -2.0f;
    bool playing = false, scratching = false, scratchMode = false;
};

//==============================================================================
// titre, artiste, temps, BPM
class DeckHeader : public Live
{
public:
    DeckHeader (SpinProcessor& p, int d) : proc (p), deck (d) {}

    void tick()
    {
        const auto t = proc.track (deck);
        const float rate = proc.view[deck].rate.load();
        const double pos = proc.view[deck].pos.load();
        const int tenth = t != nullptr ? (int) (pos / t->sampleRate * 10.0) : 0;
        const auto st = proc.deckStatus (deck);
        if (t != track || tenth != lastTenth || std::abs (rate - lastRate) > 1e-5f || st != status)
        {
            track = t; lastTenth = tenth; lastRate = rate; status = st;
            ask();
        }
    }

    void paint (juce::Graphics& g) override
    {
        painted();
        const auto accent = col::deck (deck);
        auto b = getLocalBounds();
        const bool right = deck == 1;

        // lettre de la platine
        auto badge = (right ? b.removeFromRight (44) : b.removeFromLeft (44)).reduced (4, 6);
        g.setColour (accent); g.fillRoundedRectangle (badge.toFloat(), 6);
        g.setColour (juce::Colours::black); g.setFont (fnt (26.0f, true));
        g.drawText (deck == 0 ? "A" : "B", badge, juce::Justification::centred);
        if (right) b.removeFromRight (10); else b.removeFromLeft (10);

        // BPM + pitch
        auto bpmBox = right ? b.removeFromLeft (110) : b.removeFromRight (110);
        const float bpm = effBpm (track.get(), lastRate);
        g.setColour (col::text); g.setFont (fnt (30.0f, true));
        g.drawText (bpm > 0 ? juce::String (bpm, 1) : juce::String ("--.-"), bpmBox.removeFromTop (34), juce::Justification::centredRight);
        g.setColour (col::dim); g.setFont (fnt (13.0f));
        const float pct = (lastRate - 1.0f) * 100.0f;
        g.drawText ("BPM  " + juce::String (pct >= 0 ? "+" : "") + juce::String (pct, 2) + " %", bpmBox, juce::Justification::centredRight);

        // temps
        auto timeBox = right ? b.removeFromLeft (150) : b.removeFromRight (150);
        if (track != nullptr)
        {
            const double el = proc.view[deck].pos.load() / track->sampleRate;
            const double rem = juce::jmax (0.0, track->lengthSeconds() - el);
            g.setColour (rem < 30.0 && proc.view[deck].playing.load() ? col::red : col::text);
            g.setFont (fnt (26.0f, true));
            g.drawText ("-" + formatTime (rem, true), timeBox.removeFromTop (34), juce::Justification::centredRight);
            g.setColour (col::dim); g.setFont (fnt (13.0f));
            g.drawText (formatTime (el) + "  /  " + formatTime (track->lengthSeconds()), timeBox, juce::Justification::centredRight);
        }

        // titre / artiste / etat
        if (right) b.removeFromLeft (24); else b.removeFromRight (8);
        const auto just = right ? juce::Justification::centredRight : juce::Justification::centredLeft;
        if (status.isNotEmpty())
        {
            g.setColour (proc.isLoading (deck) ? accent : col::red);
            g.setFont (fnt (18.0f, true));
            g.drawText (status, b, just);
        }
        else if (track != nullptr)
        {
            g.setColour (col::text); g.setFont (fnt (20.0f, true));
            g.drawText (track->title, b.removeFromTop (30), right ? juce::Justification::bottomRight : juce::Justification::bottomLeft, true);
            g.setColour (col::dim); g.setFont (fnt (15.0f));
            g.drawText (track->artist.isNotEmpty() ? track->artist + "   " + track->format : track->format, b, right ? juce::Justification::topRight : juce::Justification::topLeft, true);
        }
        else
        {
            g.setColour (col::faint); g.setFont (fnt (16.0f));
            g.drawText ("Drop a track here, or press LOAD", b, just);
        }
    }

private:
    SpinProcessor& proc;
    const int deck;
    TrackPtr track;
    int lastTenth = -1;
    float lastRate = 0;
    juce::String status;
};

//==============================================================================
class Vu : public Live
{
public:
    explicit Vu (std::function<float()> src) : source (std::move (src)) {}
    void tick()
    {
        const float v = source ? source() : 0.0f;
        level = juce::jmax (v, level * 0.82f);
        if (level < 0.0005f) level = 0.0f;
        const int px = (int) (meterPos (level) * getHeight());
        if (px != lastPx) { lastPx = px; ask(); }
    }
    void paint (juce::Graphics& g) override
    {
        painted();
        auto b = getLocalBounds().toFloat();
        g.setColour (juce::Colours::black); g.fillRoundedRectangle (b, 2);
        const int segs = 16;
        const float sh = b.getHeight() / segs;
        const int lit = (int) std::round ((float) lastPx / getHeight() * segs);
        for (int i = 0; i < segs; ++i)
        {
            const auto c = i >= 14 ? col::red : (i >= 11 ? col::cue : col::play);
            g.setColour (i < lit ? c : c.withAlpha (0.10f));
            g.fillRect (b.getX() + 1, b.getBottom() - (i + 1) * sh + 1, b.getWidth() - 2, sh - 2);
        }
    }
private:
    std::function<float()> source;
    float level = 0;
    int lastPx = -1;
};

//==============================================================================
// position dans la mesure (4 temps), pour caler les deux platines a l'oreille et a l'oeil
class BeatPhase : public Live
{
public:
    BeatPhase (SpinProcessor& p, int d) : proc (p), deck (d) {}
    void tick()
    {
        const auto t = proc.track (deck);
        int s = -1;
        if (t != nullptr && t->bpm > 0)
        {
            const double beats = (proc.view[deck].pos.load() - t->firstBeat) / t->beatLength();
            const double b = std::floor (beats);
            s = (int) (((long long) b % 4 + 4) % 4) * 8 + (int) ((beats - b) * 8.0);
        }
        if (s != state) { state = s; ask(); }
    }
    void paint (juce::Graphics& g) override
    {
        painted();
        auto b = getLocalBounds().toFloat();
        const float w = (b.getWidth() - 3 * 6.0f) / 4.0f;
        const auto c = col::deck (deck);
        for (int i = 0; i < 4; ++i)
        {
            auto r = juce::Rectangle<float> (b.getX() + i * (w + 6.0f), b.getY(), w, b.getHeight());
            g.setColour (col::screen); g.fillRoundedRectangle (r, 3);
            if (state >= 0 && state / 8 == i)
            {
                const float k = 1.0f - (state % 8) / 8.0f;
                g.setColour ((i == 0 ? juce::Colours::white : c).withAlpha (0.35f + 0.65f * k));
                g.fillRoundedRectangle (r, 3);
            }
            g.setColour (col::line); g.drawRoundedRectangle (r, 3, 1);
        }
    }
private:
    SpinProcessor& proc;
    const int deck;
    int state = -2;
};

//==============================================================================
// une platine complete (tete, vue d'ensemble, disque, pitch, boutons)
class DeckPanel : public juce::Component, public juce::FileDragAndDropTarget, public juce::DragAndDropTarget
{
public:
    DeckPanel (SpinProcessor& p, int d)
        : proc (p), deck (d), header (p, d), overview (p, d), platter (p, d),
          pitch (p, deckParam (pPitchA, d), juce::Slider::LinearVertical, col::deck (d), true),
          cue ("CUE", col::cue), play ("PLAY", col::play), sync ("SYNC", col::deck (d)), rev ("REV", col::red),
          pDown ("PITCH -", col::dim), pUp ("PITCH +", col::dim), range ("8 %", col::dim), beat (p, d)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &header, &overview, &platter, &pitch, &cue, &play, &sync, &rev, &pDown, &pUp, &range, &beat })
            addAndMakeVisible (c);
        cue.setBig (true); play.setBig (true);

        auto& v = proc.view[d];
        cue.onDown  = [this] { proc.post (deck, cmdCueDown); };
        cue.onUp    = [this] { proc.post (deck, cmdCueUp); };
        cue.isLit   = [this, &v] { const auto t = proc.track (deck); return t != nullptr && (v.cueHeld.load() || (! v.playing.load() && std::abs (v.pos.load() - v.cue.load()) < 2.0)); };
        play.onDown = [this] { proc.post (deck, cmdPlay); };
        play.isLit  = [&v] { return v.playing.load(); };
        sync.onDown = [this] { proc.post (deck, cmdSync); };
        sync.isLit  = [this, &v]
        {
            const auto t = proc.track (deck);
            if (t == nullptr || t->bpm <= 0 || ! v.playing.load()) return false;
            const double ph = (v.pos.load() - t->firstBeat) / t->beatLength();
            return ph - std::floor (ph) < 0.18;
        };
        rev.onDown  = [this] { proc.post (deck, cmdRevDown); };
        rev.onUp    = [this] { proc.post (deck, cmdRevUp); };
        rev.isLit   = [&v] { return v.reverse.load(); };
        pDown.onDown = [this] { proc.post (deck, cmdPitchDown); };
        pDown.onUp   = [this] { proc.post (deck, cmdPitchRelease); };
        pUp.onDown   = [this] { proc.post (deck, cmdPitchUp); };
        pUp.onUp     = [this] { proc.post (deck, cmdPitchRelease); };
        range.onDown = [this]
        {
            auto* prm = proc.apvts.getParameter (kParamIds[deckParam (pRangeA, deck)]);
            const int cur = (int) proc.apvts.getRawParameterValue (kParamIds[deckParam (pRangeA, deck)])->load();
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) ((cur + 1) % 4)));
        };
    }

    void tick()
    {
        header.tick(); overview.tick(); platter.tick(); pitch.tick(); beat.tick();
        for (auto* p : { &cue, &play, &sync, &rev, &pDown, &pUp, &range }) p->tick();
        const int r = (int) proc.apvts.getRawParameterValue (kParamIds[deckParam (pRangeA, deck)])->load();
        range.setText (juce::String (juce::roundToInt (kRanges[juce::jlimit (0, 3, r)] * 100.0f)) + " %");
        const bool hot = dragOver;
        if (hot != drawnHot) { drawnHot = hot; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (col::panel); g.fillRoundedRectangle (b, col::radius);
        g.setColour (drawnHot ? col::deck (deck) : col::line); g.drawRoundedRectangle (b.reduced (0.5f), col::radius, drawnHot ? 2.0f : 1.0f);
        // legendes du pitch
        g.setColour (col::dim); g.setFont (fnt (12.0f, true));
        g.drawText ("PITCH", pitchLabel, juce::Justification::centred);
        g.setColour (col::text); g.setFont (fnt (15.0f, true));
        const float pct = (proc.view[deck].rate.load() - 1.0f) * 100.0f;
        g.drawText ((pct >= 0 ? "+" : "") + juce::String (pct, 2), pitchValue, juce::Justification::centred);
        g.setColour (col::dim); g.setFont (fnt (12.0f, true));
        g.drawText ("BEAT", beatLabel.reduced (4, 0), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (10);
        header.setBounds (b.removeFromTop (64));
        b.removeFromTop (6);
        overview.setBounds (b.removeFromTop (40));
        b.removeFromTop (10);
        const bool right = deck == 1;

        auto pitchCol = right ? b.removeFromRight (64) : b.removeFromLeft (64);
        pitchLabel = pitchCol.removeFromTop (16);
        pitchValue = pitchCol.removeFromTop (20);
        range.setBounds (pitchCol.removeFromBottom (26).reduced (4, 0));
        pitchCol.removeFromBottom (4);
        pitch.setBounds (pitchCol.reduced (0, 10));

        auto btns = right ? b.removeFromLeft (200) : b.removeFromRight (200);
        platter.setBounds (right ? b.withTrimmedLeft (6) : b.withTrimmedRight (6));

        btns.removeFromTop (4);
        auto row1 = btns.removeFromTop (44);
        sync.setBounds (row1.removeFromLeft (row1.getWidth() / 2).reduced (4, 0));
        rev.setBounds (row1.reduced (4, 0));
        btns.removeFromTop (8);
        auto row2 = btns.removeFromTop (40);
        pDown.setBounds (row2.removeFromLeft (row2.getWidth() / 2).reduced (4, 0));
        pUp.setBounds (row2.reduced (4, 0));
        btns.removeFromTop (18);
        beatLabel = btns.removeFromTop (16);
        beat.setBounds (btns.removeFromTop (22).reduced (4, 0));
        auto row3 = btns.removeFromBottom (88);
        cue.setBounds (row3.removeFromLeft (row3.getWidth() / 2).reduced (4, 0));
        play.setBounds (row3.reduced (4, 0));
    }

    // glisser-deposer : fichiers de l'explorateur, ou ligne de la bibliotheque
    bool isInterestedInFileDrag (const juce::StringArray& files) override { return files.size() == 1; }
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; }
    void filesDropped (const juce::StringArray& files, int, int) override { dragOver = false; proc.loadFile (deck, juce::File (files[0])); }
    bool isInterestedInDragSource (const SourceDetails& d) override { return d.description.toString().startsWith ("lib:"); }
    void itemDragEnter (const SourceDetails&) override { dragOver = true; }
    void itemDragExit (const SourceDetails&) override { dragOver = false; }
    void itemDropped (const SourceDetails& d) override
    {
        dragOver = false;
        const int row = d.description.toString().fromFirstOccurrenceOf ("lib:", false, false).getIntValue();
        if (row >= 0 && row < (int) proc.visible.size()) proc.loadFile (deck, proc.library[(size_t) proc.visible[(size_t) row]].file);
    }

private:
    SpinProcessor& proc;
    const int deck;
    DeckHeader header;
    Overview overview;
    Platter platter;
    ParamSlider pitch;
    Pad cue, play, sync, rev, pDown, pUp, range;
    BeatPhase beat;
    juce::Rectangle<int> pitchLabel, pitchValue, beatLabel;
    bool dragOver = false, drawnHot = false;
};

//==============================================================================
// mixer central : EQ 3 bandes, filtre, volume, VU, crossfader, master
class MixerPanel : public juce::Component
{
public:
    explicit MixerPanel (SpinProcessor& p)
        : proc (p),
          hiA (p, pHiA, juce::Slider::RotaryHorizontalVerticalDrag, col::deckA, true),
          midA (p, pMidA, juce::Slider::RotaryHorizontalVerticalDrag, col::deckA, true),
          lowA (p, pLowA, juce::Slider::RotaryHorizontalVerticalDrag, col::deckA, true),
          filtA (p, pFiltA, juce::Slider::RotaryHorizontalVerticalDrag, col::cue, true),
          hiB (p, pHiB, juce::Slider::RotaryHorizontalVerticalDrag, col::deckB, true),
          midB (p, pMidB, juce::Slider::RotaryHorizontalVerticalDrag, col::deckB, true),
          lowB (p, pLowB, juce::Slider::RotaryHorizontalVerticalDrag, col::deckB, true),
          filtB (p, pFiltB, juce::Slider::RotaryHorizontalVerticalDrag, col::cue, true),
          volA (p, pVolA, juce::Slider::LinearVertical, col::deckA),
          volB (p, pVolB, juce::Slider::LinearVertical, col::deckB),
          xf (p, pXf, juce::Slider::LinearHorizontal, col::text),
          master (p, pMaster, juce::Slider::RotaryHorizontalVerticalDrag, col::text),
          vuA ([&p] { return p.view[0].level.exchange (0.0f); }),
          vuB ([&p] { return p.view[1].level.exchange (0.0f); }),
          loadA ("LOAD A", col::deckA), loadB ("LOAD B", col::deckB), curve ("MIX", col::dim)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &hiA, &midA, &lowA, &filtA, &hiB, &midB, &lowB, &filtB,
                                                                 &volA, &volB, &xf, &master, &vuA, &vuB, &loadA, &loadB, &curve })
            addAndMakeVisible (c);
        loadA.onDown = [this] { proc.loadSelected (0); };
        loadB.onDown = [this] { proc.loadSelected (1); };
        curve.onDown = [this]
        {
            auto* prm = proc.apvts.getParameter (kParamIds[pXfCurve]);
            prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
        };
        curve.isLit = [this] { return proc.apvts.getRawParameterValue (kParamIds[pXfCurve])->load() > 0.5f; };
    }

    void tick()
    {
        for (auto* s : { &hiA, &midA, &lowA, &filtA, &hiB, &midB, &lowB, &filtB, &volA, &volB, &xf, &master }) s->tick();
        vuA.tick(); vuB.tick();
        loadA.tick(); loadB.tick(); curve.tick();
        curve.setText (proc.apvts.getRawParameterValue (kParamIds[pXfCurve])->load() > 0.5f ? "SCRATCH" : "MIX");
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (col::panel); g.fillRoundedRectangle (b, col::radius);
        g.setColour (col::line); g.drawRoundedRectangle (b.reduced (0.5f), col::radius, 1.0f);
        g.setFont (fnt (11.0f, true));
        for (const auto& l : labels) { g.setColour (l.c); g.drawText (l.t, l.r, juce::Justification::centred); }
    }

    void resized() override
    {
        labels.clear();
        auto b = getLocalBounds().reduced (10);
        auto top = b.removeFromTop (40);
        loadA.setBounds (top.removeFromLeft (110));
        loadB.setBounds (top.removeFromRight (110));
        master.setBounds (top.withSizeKeepingCentre (40, 40));
        addLabel ("MASTER", master.getBounds().translated (0, 34).withHeight (14).expanded (20, 0), col::dim);

        auto bottom = b.removeFromBottom (74);
        auto xfRow = bottom.removeFromTop (44);
        xf.setBounds (xfRow.reduced (40, 0));
        auto curveRow = bottom.reduced (0, 2);
        curve.setBounds (curveRow.withSizeKeepingCentre (110, 26));
        addLabel ("A", { xfRow.getX(), xfRow.getY(), 30, xfRow.getHeight() }, col::deckA);
        addLabel ("B", { xfRow.getRight() - 30, xfRow.getY(), 30, xfRow.getHeight() }, col::deckB);

        b.removeFromTop (18);
        auto colA = b.removeFromLeft (b.getWidth() / 2);
        auto colB = b;
        layoutStrip (colA, false, hiA, midA, lowA, filtA, volA, vuA, col::deckA);
        layoutStrip (colB, true,  hiB, midB, lowB, filtB, volB, vuB, col::deckB);
    }

private:
    struct Label { juce::String t; juce::Rectangle<int> r; juce::Colour c; };
    std::vector<Label> labels;
    void addLabel (const juce::String& t, juce::Rectangle<int> r, juce::Colour c) { labels.push_back ({ t, r, c }); }

    void layoutStrip (juce::Rectangle<int> b, bool right, ParamSlider& hi, ParamSlider& mid, ParamSlider& lo, ParamSlider& fl,
                      ParamSlider& vol, Vu& vu, juce::Colour c)
    {
        auto knobs = right ? b.removeFromRight (64) : b.removeFromLeft (64);
        auto vuR = right ? b.removeFromLeft (14) : b.removeFromRight (14);
        const int kh = knobs.getHeight() / 4;
        int i = 0;
        for (auto* k : { &hi, &mid, &lo, &fl })
        {
            auto cell = knobs.withY (knobs.getY() + i * kh).withHeight (kh);
            const char* names[4] = { "HIGH", "MID", "LOW", "FILTER" };
            addLabel (names[i], cell.removeFromBottom (14), i == 3 ? col::cue : col::dim);
            k->setBounds (cell.withSizeKeepingCentre (juce::jmin (52, cell.getHeight()), juce::jmin (52, cell.getHeight())));
            ++i;
        }
        vu.setBounds (vuR.reduced (2, 10));
        auto fader = b.reduced (6, 0);
        addLabel ("VOL", fader.removeFromBottom (14), c);
        vol.setBounds (fader.reduced (0, 14));
    }

    SpinProcessor& proc;
    ParamSlider hiA, midA, lowA, filtA, hiB, midB, lowB, filtB, volA, volB, xf, master;
    Vu vuA, vuB;
    Pad loadA, loadB, curve;
};

//==============================================================================
// bibliotheque : dossier, recherche, liste des morceaux
class LibraryPanel : public juce::Component, private juce::TableListBoxModel
{
public:
    explicit LibraryPanel (SpinProcessor& p) : proc (p), folderBtn ("FOLDER", col::dim), rescanBtn ("RESCAN", col::dim),
                                               loadA ("< LOAD A", col::deckA), loadB ("LOAD B >", col::deckB)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &table, &search, &folderBtn, &rescanBtn, &loadA, &loadB })
            addAndMakeVisible (c);
        search.setTextToShowWhenEmpty ("Search artist, title, folder...", col::faint);
        search.setFont (fnt (15.0f));
        search.onTextChange = [this] { proc.setFilter (search.getText()); };
        search.setIndents (8, 6);

        folderBtn.onDown = [this] { chooseFolder(); };
        rescanBtn.onDown = [this] { proc.rescanLibrary(); };
        loadA.onDown = [this] { proc.loadSelected (0); };
        loadB.onDown = [this] { proc.loadSelected (1); };

        table.setModel (this);
        table.setRowHeight (24);
        table.setMultipleSelectionEnabled (false);
        table.setColour (juce::ListBox::outlineColourId, col::line);
        table.setOutlineThickness (1);
        auto& h = table.getHeader();
        h.addColumn ("#", 1, 50, 40, 70, juce::TableHeaderComponent::notSortable);
        h.addColumn ("Artist", 2, 260);
        h.addColumn ("Title", 3, 420);
        h.addColumn ("BPM", 4, 70, 50, 90);
        h.addColumn ("Format", 5, 70, 50, 90);
        h.addColumn ("Folder", 6, 420);
        h.setStretchToFitActive (true);
    }

    void tick()
    {
        folderBtn.tick(); rescanBtn.tick(); loadA.tick(); loadB.tick();
        if (proc.libraryVersion != shownVersion)
        {
            shownVersion = proc.libraryVersion;
            table.updateContent();
            if (proc.selected >= 0)
            {
                table.selectRow (proc.selected, true, true);
                table.scrollToEnsureRowIsOnscreen (proc.selected);
            }
            repaint();
        }
        const bool sc = proc.isScanning();
        if (sc != scanning) { scanning = sc; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (col::panel); g.fillRoundedRectangle (b, col::radius);
        g.setColour (col::line); g.drawRoundedRectangle (b.reduced (0.5f), col::radius, 1.0f);
        g.setColour (col::dim); g.setFont (fnt (13.0f));
        const auto dir = proc.getLibraryFolder();
        g.drawText (dir.getFullPathName(), pathArea, juce::Justification::centredLeft, true);
        g.setColour (scanning ? col::deckA : col::faint);
        g.drawText (scanning ? juce::String ("Scanning...") : juce::String ((int) proc.visible.size()) + " / " + juce::String ((int) proc.library.size()) + " tracks",
                    countArea, juce::Justification::centredRight);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (10);
        auto bar = b.removeFromTop (32);
        folderBtn.setBounds (bar.removeFromLeft (90));
        bar.removeFromLeft (6);
        rescanBtn.setBounds (bar.removeFromLeft (90));
        bar.removeFromLeft (10);
        loadB.setBounds (bar.removeFromRight (110));
        bar.removeFromRight (6);
        loadA.setBounds (bar.removeFromRight (110));
        bar.removeFromRight (10);
        countArea = bar.removeFromRight (130);
        search.setBounds (bar.removeFromRight (360));
        bar.removeFromRight (10);
        pathArea = bar;
        b.removeFromTop (8);
        table.setBounds (b);
    }

private:
    int getNumRows() override { return (int) proc.visible.size(); }

    void paintRowBackground (juce::Graphics& g, int row, int, int, bool selected) override
    {
        if (selected) g.fillAll (col::deckA.withAlpha (0.28f));
        else if (row % 2) g.fillAll (juce::Colours::white.withAlpha (0.025f));
    }

    void paintCell (juce::Graphics& g, int row, int colId, int w, int h, bool selected) override
    {
        if (row < 0 || row >= (int) proc.visible.size()) return;
        const auto& e = proc.library[(size_t) proc.visible[(size_t) row]];
        juce::String s;
        switch (colId)
        {
            case 1: s = juce::String (row + 1); break;
            case 2: s = e.artist; break;
            case 3: s = e.title; break;
            case 4: s = e.bpm > 0 ? juce::String (e.bpm, 1) : juce::String(); break;
            case 5: s = e.format; break;
            case 6: s = e.folder; break;
            default: break;
        }
        g.setColour (colId == 3 || selected ? col::text : col::dim);
        g.setFont (fnt (14.0f, colId == 3));
        g.drawText (s, 6, 0, w - 10, h, colId == 1 || colId == 4 ? juce::Justification::centredRight : juce::Justification::centredLeft, true);
    }

    void selectedRowsChanged (int row) override { proc.selected = row; }

    void cellDoubleClicked (int row, int, const juce::MouseEvent&) override
    {
        proc.selected = row;
        const int d = ! proc.view[0].playing.load() ? 0 : (! proc.view[1].playing.load() ? 1 : -1);
        if (d >= 0) proc.loadSelected (d);
    }

    juce::var getDragSourceDescription (const juce::SparseSet<int>& rows) override
    {
        return rows.size() > 0 ? juce::var ("lib:" + juce::String (rows[0])) : juce::var();
    }

public:
    void chooseFolder()
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose your music folder", proc.getLibraryFolder());
        juce::Component::SafePointer<LibraryPanel> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [safe] (const juce::FileChooser& fc)
        {
            if (safe == nullptr) return;
            const auto r = fc.getResult();
            if (r.isDirectory()) { safe->proc.setLibraryFolder (r); safe->repaint(); }
        });
    }

private:
    SpinProcessor& proc;
    juce::TableListBox table;
    juce::TextEditor search;
    Pad folderBtn, rescanBtn, loadA, loadB;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Rectangle<int> pathArea, countArea;
    int shownVersion = -1;
    bool scanning = false;
};

//==============================================================================
// barre du haut : logo, platine detectee, mode scratch, VU master, menu
class TopBar : public Live
{
public:
    explicit TopBar (SpinProcessor& p) : proc (p), scratch ("SCRATCH", col::deckA), menu ("MENU", col::dim), libBtn ("LIBRARY", col::dim)
    {
        logo = col::bg.getBrightness() > 0.5f   // logo noir sur fond clair, blanc sur fond sombre
             ? juce::ImageCache::getFromMemory (BinaryData::nine_hands_logo_noir_png, BinaryData::nine_hands_logo_noir_pngSize)
             : juce::ImageCache::getFromMemory (BinaryData::nine_hands_logo_blanc_png, BinaryData::nine_hands_logo_blanc_pngSize);
        for (auto* c : { &scratch, &menu, &libBtn }) addAndMakeVisible (c);
        scratch.onDown = [this] { proc.post (0, cmdScratchMode, proc.scratchMode.load() ? 0.0 : 1.0); };
        scratch.isLit  = [this] { return proc.scratchMode.load(); };
        libBtn.onDown  = [this] { proc.libraryToggle = ! proc.libraryToggle.load(); };
        libBtn.isLit   = [this] { return proc.libraryToggle.load(); };
    }

    std::function<void()> onMenu;

    void tick()
    {
        scratch.tick(); menu.tick(); libBtn.tick();
        const int act = proc.midiActivity.load();
        if (act != lastActivity) { lastActivity = act; blink = 6; }
        const bool on = blink > 0;
        if (blink > 0) --blink;
        if (on != drawnOn) { drawnOn = on; ask(); }
        level[0] = juce::jmax (proc.masterLevelL.exchange (0.0f), level[0] * 0.82f);
        level[1] = juce::jmax (proc.masterLevelR.exchange (0.0f), level[1] * 0.82f);
        for (auto& l : level) if (l < 0.0005f) l = 0.0f;
        const int px = (int) (meterPos (level[0]) * 200) * 1000 + (int) (meterPos (level[1]) * 200);
        const int ctl = proc.controller.load();
        if (px != lastPx || ctl != lastCtl) { lastPx = px; lastCtl = ctl; ask(); }
    }

    void paint (juce::Graphics& g) override
    {
        painted();
        g.fillAll (col::bg);
        g.setColour (col::line); g.fillRect (0, getHeight() - 1, getWidth(), 1);
        if (logo.isValid())
        {
            const float h = 18.0f, w = h * logo.getWidth() / logo.getHeight();
            g.drawImage (logo, juce::Rectangle<float> (16, (getHeight() - h) * 0.5f, w, h), juce::RectanglePlacement::centred);
            g.setColour (col::faint); g.fillRect (16 + w + 16, 12.0f, 1.0f, getHeight() - 24.0f);
            g.setColour (col::text); g.setFont (fnt (20.0f, true));
            g.drawText ("SPINDECK", juce::Rectangle<float> (16 + w + 32, 0, 200, (float) getHeight()), juce::Justification::centredLeft);
        }
        // controleur
        auto pill = ctlArea.toFloat();
        g.setColour (col::panel2); g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);
        g.setColour (drawnOn ? col::play : col::faint);
        g.fillEllipse (pill.getX() + 12, pill.getCentreY() - 5, 10, 10);
        g.setColour (col::text); g.setFont (fnt (14.0f, true));
        g.drawText (lastCtl == 0 ? "ION Discover DJ" : "MIDI Learn only", pill.withTrimmedLeft (30), juce::Justification::centredLeft);
        // VU master
        for (int c = 0; c < 2; ++c)
        {
            auto r = vuArea.toFloat().withHeight (6).translated (0, c * 10.0f);
            g.setColour (juce::Colours::black); g.fillRoundedRectangle (r, 2);
            const float v = meterPos (level[c]);
            juce::ColourGradient grad (col::play, r.getX(), 0, col::red, r.getRight(), 0, false);
            grad.addColour (0.75, col::cue);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (r.withWidth (r.getWidth() * v), 2);
        }
        g.setColour (col::dim); g.setFont (fnt (11.0f, true));
        g.drawText ("MASTER", vuArea.withY (vuArea.getY() - 16).withHeight (14), juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced (10, 8);
        menu.setBounds (b.removeFromRight (80));
        b.removeFromRight (16);
        vuArea = b.removeFromRight (180).withSizeKeepingCentre (180, 16).translated (0, 6);
        b.removeFromRight (20);
        libBtn.setBounds (b.removeFromRight (100));
        b.removeFromRight (8);
        scratch.setBounds (b.removeFromRight (100));
        ctlArea = b.withSizeKeepingCentre (200, b.getHeight());
        ctlArea.setX (b.getCentreX() - 60);
        menu.onDown = [this] { if (onMenu) onMenu(); };
    }

private:
    SpinProcessor& proc;
    Pad scratch, menu, libBtn;
    juce::Image logo;
    juce::Rectangle<int> ctlArea, vuArea;
    int lastActivity = 0, blink = 0, lastPx = -1, lastCtl = -1;
    bool drawnOn = false;
    float level[2] {};
};

//==============================================================================
class SpinContent : public juce::Component, public juce::DragAndDropContainer
{
public:
    explicit SpinContent (SpinProcessor& p)
        : proc (p), themeInit ((col::apply (p.theme.load()), 0)), top (p), laneA (p, 0), laneB (p, 1), deckA (p, 0), deckB (p, 1), mixer (p), library (p)
    {
        setLookAndFeel (&look);
        for (auto* c : std::initializer_list<juce::Component*> { &top, &laneA, &laneB, &deckA, &deckB, &mixer, &library })
            addAndMakeVisible (c);
        top.onMenu = [this] { showMenu(); };
    }
    ~SpinContent() override { setLookAndFeel (nullptr); }

    void tick()
    {
        top.tick(); laneA.tick(); laneB.tick();
        const bool libMax = proc.libraryToggle.load();
        if (libMax != libraryMax) { libraryMax = libMax; resized(); }
        if (! libraryMax) { deckA.tick(); deckB.tick(); mixer.tick(); }
        library.tick();
    }

    void paint (juce::Graphics& g) override { g.fillAll (col::bg); }

    void resized() override
    {
        auto b = getLocalBounds();
        top.setBounds (b.removeFromTop (52));
        b.reduce (12, 8);
        laneA.setBounds (b.removeFromTop (84));
        b.removeFromTop (4);
        laneB.setBounds (b.removeFromTop (84));
        b.removeFromTop (10);

        deckA.setVisible (! libraryMax); deckB.setVisible (! libraryMax); mixer.setVisible (! libraryMax);
        if (! libraryMax)
        {
            auto mid = b.removeFromTop (390);
            deckA.setBounds (mid.removeFromLeft (590));
            deckB.setBounds (mid.removeFromRight (590));
            mixer.setBounds (mid.reduced (10, 0));
            b.removeFromTop (10);
        }
        library.setBounds (b);
    }

    void mouseDown (const juce::MouseEvent& e) override { if (e.mods.isPopupMenu()) showMenu(); }

    void showMenu();

private:
    SpinProcessor& proc;
    int themeInit;
    SpinLook look;
    TopBar top;
    WaveLane laneA, laneB;
    DeckPanel deckA, deckB;
    MixerPanel mixer;
    LibraryPanel library;
    bool libraryMax = false;
};

//==============================================================================
// menu : controleur, crossfader, jog, taille de la fenetre (comme les autres plugins NINE HANDS | DSP)
static bool isDefaultSize (const SpinEditor* ed)
{
    return ed != nullptr && std::abs ((float) ed->getWidth() / (float) kRefW - kDefaultScale) < 0.02f;
}

void SpinContent::showMenu()
{
    auto* ed = findParentComponentOfClass<SpinEditor>();
    juce::PopupMenu m, ctl, jog, size, defSizes;
    ctl.addItem (10, "ION Discover DJ", true, proc.controller.load() == 0);
    ctl.addItem (11, "None (MIDI Learn only)", true, proc.controller.load() == 1);
    m.addSubMenu ("Controller", ctl);
    m.addItem (12, "Invert crossfader (controller)", true, proc.invertXf.load());
    const char* sens[4] = { "25 %", "50 %  (default)", "75 %", "100 %" };
    for (int i = 0; i < 4; ++i) jog.addItem (40 + i, sens[i], true, proc.jogSens.load() == i);
    m.addSubMenu ("Scratch sensitivity (controller)", jog);
    m.addItem (15, "Refresh controller LEDs");
    juce::PopupMenu skins;
    const char* skinNames[4] = { "Midnight", "Neon Club", "Amber Mono", "Pro Red / Blue" };
    for (int i = 0; i < 4; ++i) skins.addItem (30 + i, skinNames[i], true, proc.theme.load() == i);
    m.addSubMenu ("Skin", skins);
    m.addSeparator();
    m.addItem (16, "Choose music folder...");
    m.addItem (17, "Reaper setup help");

    const int cur = ed != nullptr ? ed->getScaleIndex() : -1;
    const int def = WigaWindowSize::defaultIndex (SpinProcessor::settingsName(), SpinEditor::numSizes);
    const char* names[SpinEditor::numSizes] = { "100 %", "125 %", "150 %" };
    const int ids[SpinEditor::numSizes] = { 104, 102, 103 };
    for (int i = 0; i < SpinEditor::numSizes; ++i)
    {
        size.addItem (ids[i], juce::String (names[i]) + (i == def ? "  (default)" : ""), true, i == 0 ? isDefaultSize (ed) : cur == i);
        defSizes.addItem (120 + i, names[i], true, i == def);
    }
    size.addSeparator();
    size.addSubMenu ("Default size (new windows)", defSizes);
    m.addSeparator();
    m.addSubMenu ("Plugin size", size);

    juce::Component::SafePointer<SpinContent> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options(), [safe] (int r)
    {
        if (safe == nullptr || r == 0) return;
        auto& p = safe->proc;
        auto saveSetting = [] (const char* key, int v)
        {
            auto f = WigaWindowSize::file (SpinProcessor::settingsName());
            juce::XmlElement x ("StudioWigaSettings");
            if (auto old = juce::XmlDocument::parse (f)) x = *old;
            x.setAttribute (key, v);
            f.getParentDirectory().createDirectory();
            x.writeTo (f);
        };
        auto* e = safe->findParentComponentOfClass<SpinEditor>();
        switch (r)
        {
            case 10: case 11: p.controller = r - 10; saveSetting ("controller", r - 10); break;
            case 12: p.invertXf = ! p.invertXf.load(); saveSetting ("invertXf", p.invertXf.load() ? 1 : 0); break;
            case 40: case 41: case 42: case 43: p.jogSens = r - 40; saveSetting ("jogSens", r - 40); break;
            case 15: p.refreshLeds(); break;
            case 16: safe->library.chooseFolder(); break;
            case 17:
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Spindeck in Reaper",
                    "1. Insert Spindeck on a track (it is an instrument).\n"
                    "2. Record-arm the track and turn record monitoring ON.\n"
                    "3. Track input: MIDI > ION Discover DJ > All channels.\n"
                    "4. For the controller LEDs: track Routing > MIDI Hardware Output > ION Discover DJ.\n"
                    "5. For scratching, use an ASIO buffer of 128 or 256 samples.\n\n"
                    "Music folder: FOLDER button above the track list.");
                break;
            case 30: case 31: case 32: case 33:
                p.theme = r - 30;
                saveSetting ("skin", r - 30);
                if (e) e->rebuildContent();
                break;
            case 102: if (e) e->setScaleIndex (1); break;
            case 103: if (e) e->setScaleIndex (2); break;
            case 104: if (e) e->setScaleIndex (0); break;
            case 120: case 121: case 122:
                WigaWindowSize::setDefaultIndex (SpinProcessor::settingsName(), r - 120);
                if (e) e->setScaleIndex (-1);
                break;
            default: break;
        }
    });
}

//==============================================================================
SpinEditor::SpinEditor (SpinProcessor& p) : juce::AudioProcessorEditor (&p), proc (p)
{
    setOpaque (true);
    content = std::make_unique<SpinContent> (proc);
    addAndMakeVisible (*content);
    setResizable (true, true);
    const int minW = (int) std::lround (kRefW * kDefaultScale);          // jamais plus petite que la taille par defaut
    const int maxW = 2400;
    setResizeLimits (minW, (int) std::lround ((double) minW * kH / kW), maxW, (int) std::lround ((double) maxW * kH / kW));
    getConstrainer()->setFixedAspectRatio ((double) kW / (double) kH);
    const int saved = juce::jlimit (-1, 2, proc.uiScaleIdx);
    if (saved >= 0) setScaleIndex (saved);
    else            applyScaleIndex (WigaWindowSize::defaultIndex (SpinProcessor::settingsName(), numSizes));
    startTimerHz (30);
}

SpinEditor::~SpinEditor() { stopTimer(); }

// changement de skin : on reconstruit toute l'interface (couleurs, images en cache) apres la fermeture du menu
void SpinEditor::rebuildContent()
{
    juce::Component::SafePointer<SpinEditor> safe (this);
    juce::MessageManager::callAsync ([safe]
    {
        if (safe == nullptr) return;
        safe->removeChildComponent (safe->content.get());
        safe->content = std::make_unique<SpinContent> (safe->proc);
        safe->addAndMakeVisible (*safe->content);
        safe->resized();
        safe->repaint();
    });
}

void SpinEditor::askRepaint (juce::Component& c, juce::Rectangle<int> area)
{
    if (repaintAskedMs == 0) repaintAskedMs = juce::Time::getMillisecondCounter();
    c.repaint (area);
}

void SpinEditor::useSoftwareRenderer()
{
    auto* peer = getPeer();
    if (peer == nullptr) return;
   #if JUCE_WINDOWS
    int sw = peer->getAvailableRenderingEngines().indexOf ("Software Renderer");
    if (sw < 0) sw = 0;
    if (peer->getCurrentRenderingEngine() != sw)
        peer->setCurrentRenderingEngine (sw);
   #endif
    if ((void*) peer != lastPeer) { lastPeer = (void*) peer; repaint(); }
    lastRendererCheck = juce::Time::getMillisecondCounter();
}

void SpinEditor::resetRenderer()
{
    lastPeer = nullptr;
    useSoftwareRenderer();
    repaint();
}

void SpinEditor::askRepaint (juce::Component& c)
{
    if (repaintAskedMs == 0) repaintAskedMs = juce::Time::getMillisecondCounter();
    c.repaint();
}

void SpinEditor::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounter();
    if ((void*) getPeer() != lastPeer || now - lastRendererCheck >= 1000) useSoftwareRenderer();
    content->tick();
    // chien de garde : une demande de dessin non servie depuis 1,5 s -> on relance le rendu
    if (repaintAskedMs != 0 && isShowing() && now - repaintAskedMs > 1500)
    {
        repaintAskedMs = 0;
        resetRenderer();
    }
}

void SpinEditor::paint (juce::Graphics& g)
{
    g.fillAll (col::bg);
    notePainted();
}

void SpinEditor::parentHierarchyChanged() { useSoftwareRenderer(); }

void SpinEditor::visibilityChanged()
{
    useSoftwareRenderer();
    if (isShowing()) repaint();
}

void SpinEditor::resized()
{
    const float s = (float) getWidth() / (float) kW;
    content->setTransform (juce::AffineTransform::scale (s));
    content->setBounds (0, 0, kW, kH);           // jamais de setSize ici (boucle avec l'hote a 125 / 150 %)
}

int SpinEditor::getScaleIndex() const
{
    const float s = (float) getWidth() / (float) kRefW;
    for (int i = 1; i < numSizes; ++i) if (std::abs (s - kScales[i]) < 0.02f) return i;
    return -1;
}

void SpinEditor::setScaleIndex (int idx)
{
    idx = juce::jlimit (-1, numSizes - 1, idx);
    proc.uiScaleIdx = idx;
    applyScaleIndex (idx < 0 ? WigaWindowSize::defaultIndex (SpinProcessor::settingsName(), numSizes) : idx);
}

void SpinEditor::applyScaleIndex (int idx)
{
    idx = juce::jlimit (0, numSizes - 1, idx);
    const float s = idx == 0 ? kDefaultScale : kScales[idx];
    const int w = (int) std::lround (kRefW * s);
    setSize (w, (int) std::lround ((double) w * kH / kW));
}
