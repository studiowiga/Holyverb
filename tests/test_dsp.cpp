// Banc de test hors DAW pour le moteur Holyverb
#include "../Source/HolyverbDSP.h"
#include <cstdio>
#include <vector>
#include <random>
#include <string>
#include <xmmintrin.h>

using namespace holyverb;

static void writeWav (const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int fs)
{
    FILE* f = fopen (path.c_str(), "wb");
    const int n = (int) L.size(), ch = 2, bps = 16, dataBytes = n * ch * 2;
    auto w32 = [&] (int v) { fwrite (&v, 4, 1, f); };
    auto w16 = [&] (short v) { fwrite (&v, 2, 1, f); };
    fwrite ("RIFF", 1, 4, f); w32 (36 + dataBytes); fwrite ("WAVEfmt ", 1, 8, f);
    w32 (16); w16 (1); w16 (ch); w32 (fs); w32 (fs * ch * 2); w16 (ch * 2); w16 (bps);
    fwrite ("data", 1, 4, f); w32 (dataBytes);
    for (int i = 0; i < n; ++i)
    {
        short a = (short) std::lround (clampf (L[(size_t) i], -1, 1) * 32767);
        short b = (short) std::lround (clampf (R[(size_t) i], -1, 1) * 32767);
        w16 (a); w16 (b);
    }
    fclose (f);
}

// Estimation du T60 par intégration de Schroeder (pente -5 à -25 dB)
static float estimateT60 (const std::vector<float>& wet, int fs)
{
    std::vector<double> e (wet.size());
    double acc = 0;
    for (int i = (int) wet.size() - 1; i >= 0; --i) { acc += (double) wet[(size_t) i] * wet[(size_t) i]; e[(size_t) i] = acc; }
    if (acc <= 0) return 0;
    int i5 = -1, i25 = -1;
    for (size_t i = 0; i < e.size(); ++i)
    {
        const double db = 10 * std::log10 (e[i] / acc + 1e-30);
        if (i5 < 0 && db <= -5) i5 = (int) i;
        if (i25 < 0 && db <= -25) { i25 = (int) i; break; }
    }
    if (i5 < 0 || i25 < 0) return -1;
    return 3.0f * (float) (i25 - i5) / (float) fs;
}

int main()
{
    _MM_SET_FLUSH_ZERO_MODE (_MM_FLUSH_ZERO_ON);
    const int fs = 48000;
    const int block = 256;
    bool allOk = true;

    for (int m = 0; m < 8; ++m)
    {
        for (float decay : { 0.0f, 0.5f, 1.0f })
        {
            Engine eng;
            Params p; p.mode = m; p.decay = decay; p.tone = 0.5f; p.level = 0.5f;
            eng.setParams (p);
            eng.prepare (fs);

            const ModeSpec& s = modeSpec (m);
            const float expected = s.tMin * std::pow (s.tMax / s.tMin, decay);
            const int len = (int) ((expected * 1.6f + 1.0f) * fs);

            std::vector<float> L ((size_t) len, 0.0f), R ((size_t) len, 0.0f);
            L[0] = R[0] = 1.0f;
            for (int i = 0; i < len; i += block) eng.process (&L[(size_t) i], &R[(size_t) i], std::min (block, len - i));
            L[0] -= 1.0f; R[0] -= 1.0f; // retirer le signal sec

            std::vector<float> mono ((size_t) len);
            bool finite = true; float peak = 0;
            for (int i = 0; i < len; ++i)
            {
                mono[(size_t) i] = 0.5f * (L[(size_t) i] + R[(size_t) i]);
                if (! std::isfinite (L[(size_t) i])) finite = false;
                peak = std::max (peak, std::fabs (L[(size_t) i]));
            }
            const float t = estimateT60 (mono, fs);
            printf ("%-8s decay=%.1f  T60 attendu %5.2fs  mesuré %5.2fs  crête %.3f %s\n",
                    modeName (m), decay, expected, t, peak, finite ? "" : "NaN!");
            if (! finite) allOk = false;
        }
    }

    // Calibrage de niveau : bruit rose approximatif (guitare) -> RMS du wet
    printf ("\nNiveau wet (bruit filtré, LEVEL 0.5, DECAY 0.5) :\n");
    for (int m = 0; m < 8; ++m)
    {
        Engine eng; Params p; p.mode = m; eng.setParams (p); eng.prepare (fs);
        std::mt19937 rng (1); std::normal_distribution<float> nd (0, 0.1f);
        const int len = fs * 4;
        std::vector<float> L ((size_t) len), R ((size_t) len), dry ((size_t) len);
        float lp = 0;
        for (int i = 0; i < len; ++i) { lp += 0.15f * (nd (rng) - lp); dry[(size_t) i] = L[(size_t) i] = R[(size_t) i] = lp * 3.0f; }
        for (int i = 0; i < len; i += block) eng.process (&L[(size_t) i], &R[(size_t) i], block);
        double ed = 0, ew = 0;
        for (int i = fs * 2; i < len; ++i) { ed += dry[(size_t) i] * dry[(size_t) i]; const double w = L[(size_t) i] - dry[(size_t) i]; ew += w * w; }
        printf ("  %-8s wet/dry = %+.1f dB\n", modeName (m), 10 * std::log10 (ew / ed));
    }

    // Stabilité extrême : shimmer, church, mod à fond + expression à fond, signal soutenu 20 s
    printf ("\nStabilité (decay=1, expression=1, tone=1, 20 s de signal soutenu) :\n");
    for (int m = 0; m < 8; ++m)
    {
        Engine eng; Params p; p.mode = m; p.decay = 1; p.expression = 1; p.tone = 1; p.level = 1;
        eng.setParams (p); eng.prepare (fs);
        const int len = fs * 20;
        float peak = 0; bool finite = true;
        std::vector<float> L (block), R (block);
        int t = 0;
        for (int i = 0; i < len; i += block)
        {
            for (int j = 0; j < block; ++j, ++t)
            {
                const float x = 0.5f * std::sin (kTwoPi * 98.0f * t / fs) + 0.3f * std::sin (kTwoPi * 147.0f * t / fs);
                L[(size_t) j] = R[(size_t) j] = x;
            }
            eng.process (L.data(), R.data(), block);
            for (int j = 0; j < block; ++j) { if (! std::isfinite (L[(size_t) j])) finite = false; peak = std::max (peak, std::fabs (L[(size_t) j])); }
        }
        printf ("  %-8s crête %.2f %s\n", modeName (m), peak, finite ? "OK" : "NaN!");
        if (! finite || peak > 8.0f) allOk = false;
    }

    // Démo audio : arpège synthétique pincé, chaque mode, fichiers WAV
    printf ("\nRendu des démos WAV...\n");
    for (int m = 0; m < 8; ++m)
    {
        Engine eng; Params p; p.mode = m; p.decay = 0.6f; p.level = 0.6f; eng.setParams (p); eng.prepare (fs);
        const int len = fs * 7;
        std::vector<float> L ((size_t) len, 0.0f), R ((size_t) len, 0.0f);
        const float notes[] = { 98.0f, 146.8f, 196.0f, 246.9f, 293.7f, 392.0f };
        for (int k = 0; k < 6; ++k)
        {
            const int start = (int) (k * 0.35f * fs);
            for (int i = 0; i < fs * 2 && start + i < len; ++i)
            {
                const float t = (float) i / fs;
                float v = 0;
                for (int h = 1; h <= 6; ++h) v += std::sin (kTwoPi * notes[k] * h * t) / (float) (h * h);
                v *= 0.25f * std::exp (-t * 3.0f);
                L[(size_t) (start + i)] += v; R[(size_t) (start + i)] += v;
            }
        }
        for (int i = 0; i < len; i += block) eng.process (&L[(size_t) i], &R[(size_t) i], std::min (block, len - i));
        writeWav (std::string ("demo_") + modeName (m) + ".wav", L, R, fs);
    }

    // Footswitch + spillover : la queue doit continuer, puis couper sans spillover
    {
        Engine eng; Params p; p.mode = 1; p.decay = 0.8f; eng.setParams (p); eng.prepare (fs);
        std::vector<float> L (fs, 0.0f), R (fs, 0.0f); L[0] = R[0] = 1;
        eng.process (L.data(), R.data(), 4800);
        p.on = false; p.spillover = true; eng.setParams (p);
        eng.process (L.data() + 4800, R.data() + 4800, 9600);
        float tail = 0; for (int i = 9600; i < 14400; ++i) tail = std::max (tail, std::fabs (L[(size_t) i]));
        p.spillover = false; eng.setParams (p);
        eng.process (L.data() + 14400, R.data() + 14400, 9600);
        float after = 0; for (int i = 19200; i < 24000; ++i) after = std::max (after, std::fabs (L[(size_t) i]));
        printf ("\nSpillover : queue après coupure %.4f (doit être > 0), sans spillover %.6f (doit être 0)\n", tail, after);
        if (tail <= 0 || after > 1e-6f) allOk = false;
    }

    printf ("\n%s\n", allOk ? "TOUS LES TESTS OK" : "ECHEC");
    return allOk ? 0 : 1;
}
