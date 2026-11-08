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
    void openColourPicker (int which);      // 0 = platine A, 1 = platine B, 2 = lampe stroboscopique

    // plein ecran : la fenetre prend tout l'ecran ou elle se trouve (barre des taches exclue) et suit
    // l'ecran si on la deplace ; elle peut alors descendre sous la taille par defaut sur un petit ecran
    void toggleFullScreen();
    bool isFullScreen() const { return fullScreen; }
    void notePainted() { lastPaintMs = juce::Time::getMillisecondCounter(); repaintAskedMs = 0; }

private:
    void timerCallback() override;
    void useSoftwareRenderer();
    void resetRenderer();

    SpinProcessor& proc;
    std::unique_ptr<SpinContent> content;
    void fitToScreen();
    juce::Component::SafePointer<juce::Component> pickerLayer;
    int colourRebuildTicks = 0;
    void normalLimits();
    void* lastPeer = nullptr;
    bool fullScreen = false, fsCanMove = false;
    int savedW = 0, placeTicks = 0, restoreTicks = 0, openTicks = 0;
    juce::Rectangle<int> fsArea;
    juce::Point<int> savedRootPhys;
    bool haveSavedRoot = false;
    juce::uint32 lastFsCheck = 0;
    juce::uint32 lastRendererCheck = 0, lastPaintMs = 0, repaintAskedMs = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpinEditor)
};
