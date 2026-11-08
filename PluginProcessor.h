#pragma once
// Spindeck (NINE HANDS | DSP) - logiciel DJ 2 platines dans un VST3 instrument, pilote par l'ION Discover DJ.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <mutex>
#include <map>
#include "Track.h"

namespace spin
{
    // ---------------------------------------------------------------- commandes interface -> audio
    enum CmdType
    {
        cmdPlay, cmdCueDown, cmdCueUp, cmdSync, cmdRevDown, cmdRevUp,
        cmdPitchDown, cmdPitchUp, cmdPitchRelease, cmdPitchReset,
        cmdSeekNorm, cmdTouchDown, cmdTouchUp, cmdTicks, cmdNudge, cmdScratchMode, cmdEject
    };
    struct Cmd { int deck = 0; int type = 0; double value = 0; };

    // parametres (index fixes, ids sans espace)
    enum ParamIndex
    {
        pVolA, pVolB, pHiA, pHiB, pMidA, pMidB, pLowA, pLowB, pFiltA, pFiltB,
        pPitchA, pPitchB, pRangeA, pRangeB, pXf, pMaster, pXfCurve, numParams
    };
    extern const char* const kParamIds[numParams];
    extern const char* const kParamNames[numParams];
    inline int deckParam (int base, int deck) { return base + deck; }   // pVolA + 1 = pVolB, etc.

    // ce que l'interface lit de l'audio (atomiques uniquement)
    struct DeckView
    {
        std::atomic<double> pos { 0 }, cue { 0 }, speed { 0 };
        std::atomic<float>  rate { 1 }, level { 0 };
        std::atomic<bool>   playing { false }, scratching { false }, reverse { false }, cueHeld { false };
        std::atomic<int>    appliedGen { 0 };
    };

    // une entree de la bibliotheque
    struct LibEntry
    {
        juce::File file;
        juce::String artist, title, format, folder;
        float bpm = 0;
    };
}

class SpinProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    SpinProcessor();
    ~SpinProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // ---------------------------------------------------------------- interface (thread de messages)
    void post (int deck, int type, double value = 0);
    void loadFile (int deck, const juce::File& f);
    void loadSelected (int deck);
    juce::String deckStatus (int deck) const;                  // "Chargement…", erreur, ou vide
    bool isLoading (int deck) const { return loading[(size_t) deck]; }
    spin::TrackPtr track (int deck) const { return current[(size_t) deck]; }
    spin::DeckView view[2];
    std::atomic<float> masterLevelL { 0 }, masterLevelR { 0 };
    std::atomic<bool> scratchMode { false };
    void refreshLeds() { ledRefresh = true; }
    std::atomic<int>  midiActivity { 0 };                      // incremente a chaque message recu

    // bibliotheque
    void setLibraryFolder (const juce::File& dir);
    juce::File getLibraryFolder() const { return libraryFolder; }
    void rescanLibrary();
    bool isScanning() const { return scanning.load(); }
    std::vector<spin::LibEntry> library;                       // message thread
    std::vector<int> visible;                                  // indices filtres
    juce::String filter;
    void setFilter (const juce::String& f);
    int selected = -1;                                         // index dans visible
    int libraryVersion = 0;                                    // change a chaque modification
    std::atomic<bool> libraryToggle { false };                 // bouton bibliotheque de la platine

    // pre-ecoute de la bibliotheque (lecture en continu depuis le disque, sans analyse)
    void startPreview (const juce::File& f);
    void stopPreview();
    void togglePreviewSelected();
    void seekPreview (double norm);
    bool isPreviewing() const { return previewActive; }
    juce::File previewFile() const { return previewPath; }
    std::atomic<double> previewPosSec { 0 }, previewLenSec { 0 };
    std::atomic<bool>   previewPlaying { false };
    std::atomic<float>  previewVolume { 0.7f };
    std::atomic<int>    previewOutput { 0 };                   // 0 = melangee au master, 1 = sorties 3-4 (bus Preview)
    std::atomic<float>  previewLevel { 0 };

    // reglages
    std::atomic<int>  controller { 0 };                        // 0 = ION Discover DJ, 1 = aucun (MIDI Learn seul)
    std::atomic<bool> invertXf { true };                       // l'ION Discover DJ envoie son crossfader a l'envers
    std::atomic<int>  jogSens { 1 };                           // sensibilite du jog de la platine : 25 / 50 / 75 / 100 %
    std::atomic<int>  theme { 0 };                             // design de l'interface
    std::atomic<int>  learnTarget { -1 };                      // parametre en attente de MIDI Learn
    std::atomic<int>  ccMap[128];                              // CC appris -> parametre (-1)
    int uiScaleIdx = -1;
    bool uiFullScreen = false;                                 // plein ecran memorise avec le projet

    static juce::String settingsName() { return "Spindeck"; }

private:
    void timerCallback() override;
    void handleMidi (const juce::MidiMessage& m);
    void setParamFromAudio (int idx, float v01);
    float paramValue (int idx) const;                          // valeur 0..1 (en attente comprise)
    void applyCmd (const spin::Cmd& c);
    void renderDecks (float* outL, float* outR, int start, int n);
    void doSync (int deck);
    void jumpTo (int deck, double pos, bool stop);
    void sendLeds (juce::MidiBuffer& out);
    void publishLoaded();
    void rebuildVisible();
    void loadLibraryCache();
    void saveLibraryCache();

    // ---------------------------------------------------------------- parametres
    std::atomic<float>* raw[spin::numParams] {};
    juce::RangedAudioParameter* params[spin::numParams] {};
    // valeurs ecrites par le thread audio, transmises a l'hote par le timer (jamais d'appel hote en audio)
    std::atomic<float> pendingValue[spin::numParams];
    std::atomic<juce::uint32> pendingSeq[spin::numParams];     // incremente a chaque ecriture audio
    std::atomic<juce::uint32> appliedSeq[spin::numParams];     // derniere ecriture transmise a l'hote

    // ---------------------------------------------------------------- file de commandes
    juce::AbstractFifo fifo { 512 };
    std::array<spin::Cmd, 512> cmds;
    std::mutex postLock;                                       // plusieurs ecrivains cote messages seulement

    // ---------------------------------------------------------------- etat des platines (thread audio)
    struct Deck
    {
        spin::Track* t = nullptr;
        double pos = 0, speed = 0, cue = 0;
        bool playing = false, cueHeld = false, playDuringCue = false, reverse = false;
        bool touched = false, scratch = false, mouseScratch = false;
        int search = 0;                                        // jog a l'arret : deplacement audible (echantillons restants)
        double target = 0, vel = 0;                            // ressort du scratch (images, images/echantillon)
        double nudge = 0;
        int pitchDir = 0, pitchHold = 0;
        int fade = 0; double fadeTarget = -1; bool fadeStop = false;   // saut sans clic
        float gainEnv = 1.0f;
        float motionEnv = 0.0f;                                // silence quand le disque est immobile
        float dcX[2] {}, dcY[2] {};                            // coupe-continu
        juce::dsp::LinkwitzRileyFilter<float> xLow, xHigh;    // 250 Hz / 2,5 kHz
        juce::dsp::StateVariableTPTFilter<float> filt;
        int filtMode = 0;
        juce::SmoothedValue<float> gVol, gHi, gMid, gLow, gXf, filtCut;
        float peak = 0;
    } deck[2];

    std::atomic<spin::Track*> pendingTrack[2] { { nullptr }, { nullptr } };
    std::atomic<int> requestedGen[2] { { 0 }, { 0 } };
    double hostRate = 44100.0;
    juce::SmoothedValue<float> gMaster;
    int ledState[16] {};
    std::atomic<bool> ledRefresh { true };
    int pitchRepeatDelay = 0, pitchRepeatEvery = 0;

    // ---------------------------------------------------------------- chargement (thread de fond)
    juce::AudioFormatManager formats;
    juce::ThreadPool pool { juce::ThreadPoolOptions{}.withNumberOfThreads (1).withThreadName ("Spindeck loader") };
    std::mutex loadLock;
    spin::TrackPtr loadedResult[2];
    juce::String loadedError[2];
    int loadedGen[2] {}, loadGen[2] {};
    bool loadedReady[2] {};
    bool loading[2] {};
    juce::String status[2];
    spin::TrackPtr current[2];
    std::vector<spin::TrackPtr> held[2];                        // pistes encore utilisables par l'audio
    double restoreCue[2] { -1, -1 };
    int trackIdCounter = 0;
    std::atomic<int> loadRequest[2] { { 0 }, { 0 } };          // boutons LOAD de la platine
    std::atomic<int> browseDelta { 0 };                        // encodeur BROWSE

    // bibliotheque
    juce::File libraryFolder;
    std::atomic<bool> scanning { false };
    std::mutex scanLock;
    std::vector<spin::LibEntry> scanResult;
    bool scanReady = false;
    std::map<juce::String, float> bpmCache;
    bool bpmCacheDirty = false;
    int cacheSaveCountdown = 0;

    // pre-ecoute : lecteur cree sur le thread de messages, transmis au thread audio par pointeur atomique
    struct Preview
    {
        std::unique_ptr<juce::AudioFormatReader> reader;     // BufferingAudioReader (lit en avance sur previewThread)
        double sampleRate = 44100.0;
        juce::int64 length = 0;
        int id = 0;
    };
    juce::TimeSliceThread previewThread { "Spindeck preview" };
    std::vector<std::unique_ptr<Preview>> previewHeld;           // lecteurs encore utilisables par l'audio
    std::atomic<Preview*> pendingPreview { nullptr };
    std::atomic<int> previewAppliedId { 0 };
    std::atomic<bool> previewStopReq { false };
    std::atomic<double> previewSeekReq { -1.0 };
    Preview* pv = nullptr;                                      // thread audio
    double pvPos = 0;
    bool pvPlaying = false;
    float pvGain = 0, pvFade = 0;
    juce::AudioBuffer<float> pvScratch;
    int previewIdCounter = 0, previewFollowSel = -2;
    bool previewActive = false;
    juce::File previewPath;
    void renderPreview (juce::AudioBuffer<float>& buffer, int n);

    friend class LoadJob;
    friend class ScanJob;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpinProcessor)
};
