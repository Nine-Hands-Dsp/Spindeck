#pragma once
// Spindeck (NINE HANDS | DSP) - interface facon logiciel DJ : formes d'onde, 2 platines, mixer, bibliotheque.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

class SpinContent;

class SpinEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SpinEditor (SpinProcessor&);
    ~SpinEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override;
    void visibilityChanged() override;

    static constexpr int numSizes = 3;
    int  getScaleIndex() const;
    void setScaleIndex (int idx);           // -1 = taille par defaut choisie
    void applyScaleIndex (int idx);

    // anti-gel v2 : demandes de dessin suivies par le chien de garde
    void askRepaint (juce::Component& c);
    void askRepaint (juce::Component& c, juce::Rectangle<int> area);
    void rebuildContent();                  // nouveau skin
    void notePainted() { lastPaintMs = juce::Time::getMillisecondCounter(); repaintAskedMs = 0; }

private:
    void timerCallback() override;
    void useSoftwareRenderer();
    void resetRenderer();

    SpinProcessor& proc;
    std::unique_ptr<SpinContent> content;
    void* lastPeer = nullptr;
    juce::uint32 lastRendererCheck = 0, lastPaintMs = 0, repaintAskedMs = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpinEditor)
};
