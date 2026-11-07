#pragma once
// Spindeck (NINE HANDS | DSP) - piste chargee en memoire + analyse (forme d'onde, BPM, grille).
// Le decodage et l'analyse se font sur un thread de fond, jamais dans le thread audio.
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <memory>
#include <vector>

namespace spin
{
    // une colonne de forme d'onde = kWaveHop images du fichier source
    constexpr int kWaveHop = 128;

    struct Track
    {
        juce::String path, title, artist, format;
        double sampleRate = 44100.0;
        juce::int64 numFrames = 0;
        std::vector<juce::int16> pcm;            // stereo entrelace, 16 bits (memoire divisee par 2)

        // forme d'onde : amplitude crete + energie basse / medium / aigue (0..255)
        std::vector<juce::uint8> wPeak, wLow, wMid, wHigh;

        float bpm = 0.0f;                         // 0 = inconnu
        double firstBeat = 0.0;                   // position du premier temps (images)
        double initialCue = 0.0;                  // cue a l'arrivee sur la platine (premier son ou cue sauve)
        int id = 0;                               // croissant : sert aussi de generation pour le thread audio

        double lengthSeconds() const { return sampleRate > 0 ? (double) numFrames / sampleRate : 0.0; }
        double beatLength() const     { return bpm > 1.0f ? sampleRate * 60.0 / (double) bpm : 0.0; }

        // lecture interpolee (Hermite 4 points), hors limites = silence
        inline void read (double pos, float& l, float& r) const noexcept
        {
            const auto i = (juce::int64) std::floor (pos);
            const float t = (float) (pos - (double) i);
            float xl[4], xr[4];
            for (int k = 0; k < 4; ++k)
            {
                const auto j = i - 1 + k;
                if (j < 0 || j >= numFrames) { xl[k] = xr[k] = 0.0f; continue; }
                xl[k] = (float) pcm[(size_t) (2 * j)]     * (1.0f / 32768.0f);
                xr[k] = (float) pcm[(size_t) (2 * j + 1)] * (1.0f / 32768.0f);
            }
            l = hermite (xl, t);
            r = hermite (xr, t);
        }

        static inline float hermite (const float* x, float t) noexcept
        {
            const float c0 = x[1];
            const float c1 = 0.5f * (x[2] - x[0]);
            const float c2 = x[0] - 2.5f * x[1] + 2.0f * x[2] - 0.5f * x[3];
            const float c3 = 0.5f * (x[3] - x[0]) + 1.5f * (x[1] - x[2]);
            return ((c3 * t + c2) * t + c1) * t + c0;
        }
    };

    using TrackPtr = std::shared_ptr<Track>;

    // Decode + analyse. Renvoie nullptr si le fichier est illisible ou trop long.
    // shouldExit : permet d'abandonner (fermeture du plugin).
    TrackPtr loadTrack (const juce::File& file, juce::AudioFormatManager& fm,
                        const std::function<bool()>& shouldExit, juce::String& error);

    // "Artiste - Titre.mp3" -> artiste / titre
    void splitName (const juce::String& fileStem, juce::String& artist, juce::String& title);

    juce::String formatTime (double seconds, bool tenths = false);
}
