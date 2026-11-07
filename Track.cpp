#include "Track.h"
#include <cmath>
#include <algorithm>

namespace spin
{
void splitName (const juce::String& stem, juce::String& artist, juce::String& title)
{
    auto s = stem.replaceCharacter ('_', ' ').trim();
    const int dash = s.indexOf (" - ");
    if (dash > 0)
    {
        artist = s.substring (0, dash).trim();
        title  = s.substring (dash + 3).trim();
    }
    else
    {
        artist = {};
        title  = s;
    }
    // "01 Artiste - Titre" : on enleve un numero de piste en tete
    auto stripNum = [] (juce::String x)
    {
        int i = 0;
        while (i < x.length() && juce::CharacterFunctions::isDigit (x[i])) ++i;
        if (i > 0 && i <= 3 && i < x.length() && (x[i] == ' ' || x[i] == '.' || x[i] == '-'))
            return x.substring (i + 1).trimCharactersAtStart (" .-");
        return x;
    };
    if (artist.isNotEmpty()) artist = stripNum (artist); else title = stripNum (title);
}

juce::String formatTime (double sec, bool tenths)
{
    if (! std::isfinite (sec) || sec < 0) sec = 0;
    const int m = (int) (sec / 60.0);
    const double s = sec - m * 60.0;
    if (tenths)
        return juce::String (m).paddedLeft ('0', 2) + ":" + juce::String ((int) s).paddedLeft ('0', 2)
               + "." + juce::String ((int) ((s - std::floor (s)) * 10.0));
    return juce::String (m).paddedLeft ('0', 2) + ":" + juce::String ((int) s).paddedLeft ('0', 2);
}

//==============================================================================
// Analyse du tempo : fonction d'attaque (basses + large bande), autocorrelation
// avec un a priori autour de 120 BPM, puis affinage fin par peigne de temps sur
// tout le morceau (precision ~0,01 BPM) qui donne aussi la phase de la grille.
static void analyseTempo (Track& t, const std::vector<float>& mono, const std::function<bool()>& shouldExit)
{
    const double sr = t.sampleRate;
    const int hop = sr > 60000.0 ? 1024 : 512;
    const double fps = sr / hop;
    const int nFrames = (int) (mono.size() / (size_t) hop);
    if (nFrames < (int) (fps * 8.0)) return;                    // moins de 8 s : pas d'analyse

    // energies par trame (basses < 150 Hz, et large bande)
    std::vector<float> eLow ((size_t) nFrames), eAll ((size_t) nFrames);
    const float aLp = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * 150.0 / sr);
    float lp = 0, lp2 = 0;
    for (int f = 0; f < nFrames; ++f)
    {
        double sl = 0, sa = 0;
        for (int i = 0; i < hop; ++i)
        {
            const float x = mono[(size_t) (f * hop + i)];
            lp  = x + aLp * (lp - x);
            lp2 = lp + aLp * (lp2 - lp);
            sl += (double) lp2 * lp2;
            sa += (double) x * x;
        }
        eLow[(size_t) f] = (float) std::log1p (1000.0 * sl / hop);
        eAll[(size_t) f] = (float) std::log1p (1000.0 * sa / hop);
    }

    std::vector<float> onset ((size_t) nFrames, 0.0f);
    for (int f = 1; f < nFrames; ++f)
        onset[(size_t) f] = std::max (0.0f, eLow[(size_t) f] - eLow[(size_t) f - 1])
                          + 0.5f * std::max (0.0f, eAll[(size_t) f] - eAll[(size_t) f - 1]);

    // retrait de la moyenne locale (~0,5 s)
    {
        const int w = std::max (2, (int) (fps * 0.25));
        std::vector<double> cum ((size_t) nFrames + 1, 0.0);
        for (int f = 0; f < nFrames; ++f) cum[(size_t) f + 1] = cum[(size_t) f] + onset[(size_t) f];
        std::vector<float> o2 ((size_t) nFrames);
        for (int f = 0; f < nFrames; ++f)
        {
            const int a = std::max (0, f - w), b = std::min (nFrames, f + w + 1);
            const float mean = (float) ((cum[(size_t) b] - cum[(size_t) a]) / (b - a));
            o2[(size_t) f] = std::max (0.0f, onset[(size_t) f] - mean);
        }
        onset.swap (o2);
    }
    if (shouldExit()) return;

    // autocorrelation sur les retards utiles (jusqu'a 4 temps a 60 BPM)
    const int maxLag = std::min (nFrames - 1, (int) (fps * 4.0) + 4);
    std::vector<double> R ((size_t) maxLag + 1, 0.0);
    for (int L = 1; L <= maxLag; ++L)
    {
        double s = 0;
        for (int f = 0; f + L < nFrames; ++f) s += (double) onset[(size_t) f] * onset[(size_t) (f + L)];
        R[(size_t) L] = s / (nFrames - L);
        if ((L & 63) == 0 && shouldExit()) return;
    }
    auto Ri = [&] (double lag)
    {
        if (lag < 1.0 || lag >= maxLag) return 0.0;
        const int i = (int) lag; const double fr = lag - i;
        return R[(size_t) i] * (1.0 - fr) + R[(size_t) i + 1] * fr;
    };

    double bestBpm = 0, bestScore = -1;
    for (double bpm = 60.0; bpm <= 200.0; bpm += 0.05)
    {
        const double lag = 60.0 * fps / bpm;
        double s = Ri (lag) + 0.5 * Ri (2 * lag) + 0.25 * Ri (4 * lag) + 0.25 * Ri (0.5 * lag);
        const double z = std::log2 (bpm / 120.0) / 0.7;
        s *= std::exp (-0.5 * z * z);
        if (s > bestScore) { bestScore = s; bestBpm = bpm; }
    }
    if (bestBpm <= 0) return;
    while (bestBpm < 75.0)  bestBpm *= 2.0;
    while (bestBpm > 180.0) bestBpm *= 0.5;

    // affinage par peigne sur tout le morceau : tempo + phase
    double fineBpm = bestBpm, finePhase = 0, fineScore = -1;
    for (double bpm = bestBpm * 0.985; bpm <= bestBpm * 1.015; bpm += 0.01)
    {
        const double period = 60.0 * fps / bpm;
        for (double ph = 0; ph < period; ph += 0.5)
        {
            double s = 0;
            for (double x = ph; x < nFrames - 1; x += period)
            {
                const int i = (int) x; const float fr = (float) (x - i);
                s += onset[(size_t) i] * (1.0f - fr) + onset[(size_t) i + 1] * fr;
            }
            if (s > fineScore) { fineScore = s; fineBpm = bpm; finePhase = ph; }
        }
        if (shouldExit()) return;
    }
    t.bpm = (float) (std::round (fineBpm * 100.0) / 100.0);
    const double beatFrames = 60.0 * sr / t.bpm;
    double fb = finePhase * hop + hop * 0.5;               // centre de la trame d'attaque
    fb = std::fmod (fb, beatFrames);
    t.firstBeat = fb;
}

//==============================================================================
TrackPtr loadTrack (const juce::File& file, juce::AudioFormatManager& fm,
                    const std::function<bool()>& shouldExit, juce::String& error)
{
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr) { error = "Unsupported format"; return nullptr; }

    const double sr = reader->sampleRate;
    const auto len = reader->lengthInSamples;
    if (sr <= 0 || len <= 0) { error = "Empty file"; return nullptr; }
    if ((double) len / sr > 60.0 * 60.0) { error = "Track too long (60 min max)"; return nullptr; }

    auto t = std::make_shared<Track>();
    t->path = file.getFullPathName();
    t->format = file.getFileExtension().trimCharactersAtStart (".").toUpperCase();
    t->sampleRate = sr;
    t->numFrames = len;
    splitName (file.getFileNameWithoutExtension(), t->artist, t->title);

    try { t->pcm.resize ((size_t) len * 2); }
    catch (...) { error = "Not enough memory"; return nullptr; }

    std::vector<float> mono ((size_t) len);

    const int chunk = 65536;
    juce::AudioBuffer<float> buf ((int) juce::jmax (2u, reader->numChannels), chunk);
    juce::int64 pos = 0;
    while (pos < len)
    {
        if (shouldExit()) return nullptr;
        const int n = (int) std::min<juce::int64> (chunk, len - pos);
        buf.clear();
        reader->read (&buf, 0, n, pos, true, true);
        const float* L = buf.getReadPointer (0);
        const float* Rr = reader->numChannels > 1 ? buf.getReadPointer (1) : L;
        for (int i = 0; i < n; ++i)
        {
            const float l = juce::jlimit (-1.0f, 1.0f, L[i]);
            const float r = juce::jlimit (-1.0f, 1.0f, Rr[i]);
            const auto k = (size_t) (pos + i);
            t->pcm[2 * k]     = (juce::int16) std::lrint (l * 32767.0f);
            t->pcm[2 * k + 1] = (juce::int16) std::lrint (r * 32767.0f);
            mono[k] = 0.5f * (l + r);
        }
        pos += n;
    }

    // forme d'onde (crete + 3 bandes)
    const size_t cols = (size_t) ((len + kWaveHop - 1) / kWaveHop);
    std::vector<float> pk (cols), lo (cols), mi (cols), hi (cols);
    {
        const float a200  = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * 200.0 / sr);
        const float a2500 = (float) std::exp (-2.0 * juce::MathConstants<double>::pi * 2500.0 / sr);
        float l1 = 0, l2 = 0;
        for (size_t c = 0; c < cols; ++c)
        {
            float p = 0; double sl = 0, sm = 0, sh = 0; int n = 0;
            for (int i = 0; i < kWaveHop; ++i)
            {
                const size_t k = c * kWaveHop + (size_t) i;
                if (k >= (size_t) len) break;
                const float x = mono[k];
                l1 = x + a200 * (l1 - x);
                l2 = x + a2500 * (l2 - x);
                const float low = l1, mid = l2 - l1, high = x - l2;
                p = std::max (p, std::abs (x));
                sl += low * low; sm += mid * mid; sh += high * high; ++n;
            }
            n = std::max (1, n);
            pk[c] = p; lo[c] = (float) std::sqrt (sl / n); mi[c] = (float) std::sqrt (sm / n); hi[c] = (float) std::sqrt (sh / n);
        }
        float mp = 1e-6f, mb = 1e-6f;
        for (size_t c = 0; c < cols; ++c) { mp = std::max (mp, pk[c]); mb = std::max ({ mb, lo[c], mi[c] * 1.6f, hi[c] * 2.6f }); }
        t->wPeak.resize (cols); t->wLow.resize (cols); t->wMid.resize (cols); t->wHigh.resize (cols);
        auto q = [] (float v) { return (juce::uint8) juce::jlimit (0, 255, (int) std::lrint (v * 255.0f)); };
        for (size_t c = 0; c < cols; ++c)
        {
            t->wPeak[c] = q (pk[c] / mp);
            t->wLow[c]  = q (lo[c] / mb);
            t->wMid[c]  = q (mi[c] * 1.6f / mb);
            t->wHigh[c] = q (hi[c] * 2.6f / mb);
        }
    }
    if (shouldExit()) return nullptr;

    analyseTempo (*t, mono, shouldExit);
    if (shouldExit()) return nullptr;
    return t;
}
}
