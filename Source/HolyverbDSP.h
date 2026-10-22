/*
    Holyverb — Studio Wiga
    Moteur de reverb (sans dépendance JUCE) inspiré d'une pédale reverb compacte
    à 8 modes : Room, Hall, Spring, Plate, Church, Lofi, Mod, Shimmer.

    Contrôles : DECAY, TONE, LEVEL, MODE, PRE-DELAY (short/long),
                EXPRESSION (équivalent du footswitch sensible à la pression),
                ON (footswitch), SPILLOVER (les queues continuent après coupure).

    Le signal sec passe intact : seule la reverb est ajoutée.
*/
#pragma once

#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace holyverb
{

constexpr float kPi    = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

inline float clampf (float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }

//==============================================================================
// Ligne à retard circulaire (taille puissance de 2).
// Convention : on lit AVANT d'écrire. read(d) renvoie l'échantillon écrit il y a d pas (d >= 1).
class DelayLine
{
public:
    void prepare (int maxDelaySamples)
    {
        int n = 1;
        while (n < maxDelaySamples + 8) n <<= 1;
        buf.assign ((size_t) n, 0.0f);
        mask = n - 1;
        w = 0;
    }
    void clear()                       { std::fill (buf.begin(), buf.end(), 0.0f); }
    inline void push (float x)         { buf[(size_t) w] = x; w = (w + 1) & mask; }
    inline float read (int d) const    { return buf[(size_t) ((w - d) & mask)]; }

    // Lecture fractionnaire (interpolation d'Hermite), d >= 2
    inline float readFrac (float d) const
    {
        const int   i = (int) d;
        const float f = d - (float) i;
        const float xm1 = read (i - 1), x0 = read (i), x1 = read (i + 1), x2 = read (i + 2);
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * f + c2) * f + c1) * f + x0;
    }
    int size() const { return mask + 1; }

private:
    std::vector<float> buf;
    int mask = 0, w = 0;
};

//==============================================================================
// Allpass de Schroeder (retard fixe ou modulé)
class Allpass
{
public:
    void prepare (int maxDelay) { dl.prepare (maxDelay + 4); }
    void clear()                { dl.clear(); }
    void setDelay (int d)       { delay = std::max (1, d); }

    inline float process (float x, float g)
    {
        const float d = dl.read (delay);
        const float u = x + g * d;
        dl.push (u);
        return d - g * u;
    }
    inline float processFrac (float x, float g, float delayF)
    {
        const float d = dl.readFrac (std::max (2.0f, delayF));
        const float u = x + g * d;
        dl.push (u);
        return d - g * u;
    }
    inline float tap (int d) const { return dl.read (d); }

private:
    DelayLine dl;
    int delay = 1;
};

//==============================================================================
struct OnePole
{
    float a = 1.0f, z = 0.0f;
    void setLowpass (float fc, float fs) { a = 1.0f - std::exp (-kTwoPi * fc / fs); }
    inline float lp (float x) { z += a * (x - z); return z; }
    inline float hp (float x) { z += a * (x - z); return x - z; }
    void clear() { z = 0.0f; }
};

// Biquad RBJ (forme transposée II)
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void setLowpass (float fc, float fs, float q = 0.7071f)
    {
        fc = clampf (fc, 10.0f, 0.45f * fs);
        const float w0 = kTwoPi * fc / fs, c = std::cos (w0), al = std::sin (w0) / (2.0f * q);
        const float a0 = 1.0f + al;
        b0 = (1.0f - c) * 0.5f / a0; b1 = (1.0f - c) / a0; b2 = b0;
        a1 = -2.0f * c / a0;         a2 = (1.0f - al) / a0;
    }
    void setHighpass (float fc, float fs, float q = 0.7071f)
    {
        fc = clampf (fc, 10.0f, 0.45f * fs);
        const float w0 = kTwoPi * fc / fs, c = std::cos (w0), al = std::sin (w0) / (2.0f * q);
        const float a0 = 1.0f + al;
        b0 = (1.0f + c) * 0.5f / a0; b1 = -(1.0f + c) / a0; b2 = b0;
        a1 = -2.0f * c / a0;         a2 = (1.0f - al) / a0;
    }
    inline float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void clear() { z1 = z2 = 0.0f; }
};

//==============================================================================
// Réseau de lignes à retard réinjectées (FDN 8 lignes, matrice de Hadamard)
// utilisé par Room, Hall, Church, Lofi, Mod et Shimmer.
class FDN8
{
public:
    static constexpr int N = 8;

    void prepare (float sampleRate, float maxSize, float maxModDepthMs)
    {
        fs = sampleRate;
        const float k = fs / 48000.0f;
        const int maxMod = (int) std::ceil (maxModDepthMs * 0.001f * fs) + 4;
        for (int i = 0; i < N; ++i)
            lines[(size_t) i].prepare ((int) std::ceil (kBase[i] * maxSize * k) + maxMod + 8);
        for (int i = 0; i < 4; ++i)
            diff[(size_t) i].prepare ((int) std::ceil (kDiff[i] * 2.0f * k) + 8);
        clear();
    }

    void clear()
    {
        for (auto& l : lines) l.clear();
        for (auto& d : diff)  d.clear();
        z.fill (0.0f);
    }

    // size : 1 = Hall ; diffScale : taille des diffuseurs d'entrée
    void setGeometry (float size, float diffScale, float diffGain, int numDiffusers)
    {
        const float k = fs / 48000.0f;
        for (int i = 0; i < N; ++i)  len[(size_t) i] = kBase[i] * size * k;
        for (int i = 0; i < 4; ++i)  diff[(size_t) i].setDelay ((int) std::round (kDiff[i] * diffScale * k));
        diffG = diffGain;
        nDiff = numDiffusers;
    }

    void setModulation (float depthMs, float rateHz)
    {
        depth = depthMs * 0.001f * fs;
        for (int i = 0; i < N; ++i)  inc[(size_t) i] = kTwoPi * rateHz * kRateMul[i] / fs;
    }

    // T60 en secondes aux basses fréquences ; hfRatio = T60(aigus)/T60(graves)
    void setDecay (float t60, float hfRatio)
    {
        t60 = std::max (0.05f, t60);
        hfRatio = clampf (hfRatio, 0.05f, 1.0f);
        for (int i = 0; i < N; ++i)
        {
            const float L    = len[(size_t) i];
            const float g0   = std::pow (10.0f, -3.0f * L / (fs * t60));
            const float gpi  = std::pow (10.0f, -3.0f * L / (fs * t60 * hfRatio));
            const float b    = (g0 - gpi) / (g0 + gpi);
            gain[(size_t) i] = g0 * (1.0f - b);
            pole[(size_t) i] = b;
        }
    }

    inline void process (float in, float& outL, float& outR)
    {
        // Diffusion d'entrée
        float x = in;
        for (int i = 0; i < nDiff; ++i)  x = diff[(size_t) i].process (x, diffG);

        std::array<float, N> v;
        for (int i = 0; i < N; ++i)
        {
            float d = len[(size_t) i];
            if (depth > 0.0f)
            {
                phase[(size_t) i] += inc[(size_t) i];
                if (phase[(size_t) i] > kTwoPi) phase[(size_t) i] -= kTwoPi;
                d += depth * (1.0f + std::sin (phase[(size_t) i])) * 0.5f;
            }
            const float r = lines[(size_t) i].readFrac (std::max (3.0f, d));
            // Absorption (filtre passe-bas d'atténuation dépendant de la fréquence)
            z[(size_t) i] = gain[(size_t) i] * r + pole[(size_t) i] * z[(size_t) i];
            v[(size_t) i] = z[(size_t) i];
        }

        outL = (v[0] - v[1] + v[2] - v[3] + v[4] - v[5] + v[6] - v[7]) * 0.35f;
        outR = (v[0] + v[1] - v[2] - v[3] + v[4] + v[5] - v[6] - v[7]) * 0.35f;

        // Hadamard 8 rapide, normalisé
        hadamard (v);
        static constexpr float inSign[N] = { 1, -1, 1, 1, -1, 1, -1, -1 };
        for (int i = 0; i < N; ++i)
            lines[(size_t) i].push (v[(size_t) i] + x * inSign[i] * 0.5f);
    }

private:
    static inline void hadamard (std::array<float, N>& v)
    {
        for (int h = 1; h < N; h <<= 1)
            for (int i = 0; i < N; i += h << 1)
                for (int j = i; j < i + h; ++j)
                {
                    const float a = v[(size_t) j], b = v[(size_t) (j + h)];
                    v[(size_t) j] = a + b;
                    v[(size_t) (j + h)] = a - b;
                }
        const float s = 0.35355339f; // 1/sqrt(8)
        for (auto& e : v) e *= s;
    }

    static constexpr float kBase[N]    = { 1931, 2213, 2539, 2851, 3163, 3491, 3803, 4127 };
    static constexpr float kDiff[4]    = { 142, 107, 379, 277 };
    static constexpr float kRateMul[N] = { 1.0f, 1.13f, 0.87f, 1.27f, 0.79f, 1.41f, 0.93f, 1.07f };

    float fs = 48000.0f;
    std::array<DelayLine, N> lines;
    std::array<Allpass, 4> diff;
    std::array<float, N> len {}, gain {}, pole {}, z {}, phase { 0.0f, 0.8f, 1.6f, 2.4f, 3.2f, 4.0f, 4.8f, 5.6f }, inc {};
    float depth = 0.0f, diffG = 0.7f;
    int nDiff = 4;
};

//==============================================================================
// Réflexions précoces (Room)
class EarlyReflections
{
public:
    void prepare (float sampleRate) { fs = sampleRate; dl.prepare ((int) (0.03f * fs) + 8); }
    void clear() { dl.clear(); }
    inline void process (float x, float& l, float& r)
    {
        static constexpr float ms[6] = { 4.3f, 7.9f, 11.2f, 13.7f, 17.9f, 23.1f };
        static constexpr float gl[6] = { 0.62f, 0.0f, 0.48f, 0.0f, 0.33f, 0.21f };
        static constexpr float gr[6] = { 0.0f, 0.58f, 0.0f, 0.41f, 0.29f, -0.2f };
        l = r = 0.0f;
        for (int i = 0; i < 6; ++i)
        {
            const float s = dl.read ((int) (ms[i] * 0.001f * fs));
            l += gl[i] * s; r += gr[i] * s;
        }
        dl.push (x);
    }
private:
    DelayLine dl;
    float fs = 48000.0f;
};

//==============================================================================
// Plaque (topologie de Dattorro)
class Plate
{
public:
    void prepare (float sampleRate)
    {
        fs = sampleRate;
        k  = fs / 29761.0f;
        const int inLen[4] = { 142, 107, 379, 277 };
        for (int i = 0; i < 4; ++i) { inAP[(size_t) i].prepare (sc (inLen[i]) + 2); inAP[(size_t) i].setDelay (sc (inLen[i])); }
        modL.prepare (sc (672 + 32)); modR.prepare (sc (908 + 32));
        apL2.prepare (sc (1800)); apL2.setDelay (sc (1800));
        apR2.prepare (sc (2656)); apR2.setDelay (sc (2656));
        dL1.prepare (sc (4453)); dL2.prepare (sc (3720));
        dR1.prepare (sc (4217)); dR2.prepare (sc (3163));
        clear();
    }
    void clear()
    {
        for (auto& a : inAP) a.clear();
        modL.clear(); modR.clear(); apL2.clear(); apR2.clear();
        dL1.clear(); dL2.clear(); dR1.clear(); dR2.clear();
        bw = dampL = dampR = 0.0f;
    }
    void setDecay (float t60)
    {
        const float loop = 21589.0f / 29761.0f; // durée d'un tour complet du réservoir
        decay = clampf (std::pow (10.0f, -3.0f * loop / (4.0f * std::max (0.1f, t60))), 0.0f, 0.9995f);
    }
    void setDamping (float d) { damping = clampf (d, 0.0f, 0.95f); }

    inline void process (float in, float& outL, float& outR)
    {
        bw += 0.9995f * (in - bw);
        float x = bw;
        x = inAP[0].process (x, 0.75f);
        x = inAP[1].process (x, 0.75f);
        x = inAP[2].process (x, 0.625f);
        x = inAP[3].process (x, 0.625f);

        lfo += kTwoPi * 1.0f / fs;
        if (lfo > kTwoPi) lfo -= kTwoPi;
        const float exc = 16.0f * k;

        const float eL = dL2.read (sc (3720));
        const float eR = dR2.read (sc (3163));

        // Moitié gauche
        const float aL = modL.processFrac (x + decay * eR, -0.7f, (float) sc (672) + exc * std::sin (lfo));
        const float bL = dL1.read (sc (4453)); dL1.push (aL);
        dampL = (1.0f - damping) * bL + damping * dampL;
        const float fL = apL2.process (dampL * decay, 0.5f);
        dL2.push (fL);

        // Moitié droite
        const float aR = modR.processFrac (x + decay * eL, -0.7f, (float) sc (908) + exc * std::cos (lfo));
        const float bR = dR1.read (sc (4217)); dR1.push (aR);
        dampR = (1.0f - damping) * bR + damping * dampR;
        const float fR = apR2.process (dampR * decay, 0.5f);
        dR2.push (fR);

        outL = 0.6f * (dR1.read (sc (266)) + dR1.read (sc (2974)) - apR2.tap (sc (1913)) + dR2.read (sc (1996))
                       - dL1.read (sc (1990)) - apL2.tap (sc (187)) - dL2.read (sc (1066)));
        outR = 0.6f * (dL1.read (sc (353)) + dL1.read (sc (3627)) - apL2.tap (sc (1228)) + dL2.read (sc (2673))
                       - dR1.read (sc (2111)) - apR2.tap (sc (335)) - dR2.read (sc (121)));
    }

private:
    inline int sc (int n) const { return std::max (1, (int) std::lround ((float) n * k)); }

    float fs = 48000.0f, k = 1.0f, decay = 0.5f, damping = 0.3f;
    float bw = 0.0f, dampL = 0.0f, dampR = 0.0f, lfo = 0.0f;
    std::array<Allpass, 4> inAP;
    Allpass modL, modR, apL2, apR2;
    DelayLine dL1, dL2, dR1, dR2;
};

//==============================================================================
// Ressort : chaîne d'allpass dispersifs (chirp) dans une boucle réinjectée
class SpringTank
{
public:
    static constexpr int M = 48;

    void prepare (float sampleRate, float loopSeconds, float lfoHz)
    {
        fs = sampleRate;
        K = std::max (1, (int) std::lround (2.0f * fs / 48000.0f));
        xh.assign ((size_t) (M * K), 0.0f);
        yh.assign ((size_t) (M * K), 0.0f);
        loopLen = loopSeconds * fs;
        loop.prepare ((int) (loopLen + 0.002f * fs) + 8);
        lp.setLowpass (4200.0f, fs);
        hp.setHighpass (110.0f, fs);
        lfoInc = kTwoPi * lfoHz / fs;
        clear();
    }
    void clear()
    {
        std::fill (xh.begin(), xh.end(), 0.0f);
        std::fill (yh.begin(), yh.end(), 0.0f);
        loop.clear(); lp.clear(); hp.clear();
        p = 0;
    }
    void setDecay (float t60)
    {
        // Durée de boucle + retard moyen de la dispersion (≈ 4 ms)
        const float loopT = loopLen / fs + 0.004f;
        fb = clampf (std::pow (10.0f, -3.0f * loopT / std::max (0.1f, t60)), 0.0f, 0.97f);
    }

    inline float process (float in)
    {
        lfo += lfoInc; if (lfo > kTwoPi) lfo -= kTwoPi;
        const float back = loop.readFrac (loopLen + 0.0006f * fs * std::sin (lfo));
        float x = in + fb * back;

        constexpr float a = -0.6f;
        for (int s = 0; s < M; ++s)
        {
            const size_t idx = (size_t) (s * K + p);
            const float xd = xh[idx], yd = yh[idx];
            const float y  = a * x + xd - a * yd;
            xh[idx] = x; yh[idx] = y;
            x = y;
        }
        p = (p + 1) % K;

        const float y = hp.process (lp.process (x));
        loop.push (std::tanh (y));
        return y;
    }

private:
    float fs = 48000.0f, loopLen = 1000.0f, fb = 0.5f, lfo = 0.0f, lfoInc = 0.0f;
    int K = 2, p = 0;
    std::vector<float> xh, yh;
    DelayLine loop;
    Biquad lp, hp;
};

//==============================================================================
// Transposition à l'octave supérieure (deux têtes de lecture en fondu)
class OctaveUp
{
public:
    void prepare (float sampleRate)
    {
        W = 0.045f * sampleRate;
        dl.prepare ((int) W + 16);
        phase = 0.0f;
    }
    void clear() { dl.clear(); phase = 0.0f; }

    inline float process (float x)
    {
        phase += 1.0f / W;
        if (phase >= 1.0f) phase -= 1.0f;
        float ph2 = phase + 0.5f; if (ph2 >= 1.0f) ph2 -= 1.0f;
        const float sa = std::sin (kPi * phase), sb = std::sin (kPi * ph2);
        const float y = sa * sa * dl.readFrac (3.0f + (1.0f - phase) * W)
                      + sb * sb * dl.readFrac (3.0f + (1.0f - ph2) * W);
        dl.push (x);
        return y;
    }

private:
    DelayLine dl;
    float W = 2048.0f, phase = 0.0f;
};

//==============================================================================
enum class Mode : int { Room = 0, Hall, Spring, Plate, Church, Lofi, Mod, Shimmer, Count };

inline const char* modeName (int m)
{
    static const char* names[] = { "ROOM", "HALL", "SPRING", "PLATE", "CHURCH", "LOFI", "MOD", "SHIMMER" };
    return names[std::clamp (m, 0, 7)];
}

struct Params
{
    float decay = 0.5f;       // 0..1
    float tone = 0.5f;        // 0..1
    float level = 0.5f;       // 0..1
    int   mode = 1;           // 0..7
    bool  preLong = false;    // mini-switch pré-délai
    float expression = 0.0f;  // 0..1 (pédale d'expression / MASH)
    bool  on = true;          // footswitch
    bool  spillover = true;   // queues conservées à la coupure
};

struct ModeSpec
{
    float preShortMs, preLongMs, tMin, tMax, hfLo, hfHi, gain;
};

inline const ModeSpec& modeSpec (int m)
{
    static const ModeSpec specs[8] = {
        //  pre S  pre L   Tmin  Tmax   hfLo  hfHi  gain
        {   2.0f,  40.0f, 0.25f,  2.2f, 0.30f, 0.85f, 1.70f }, // Room
        {  10.0f,  80.0f, 0.80f,  6.0f, 0.25f, 0.80f, 1.66f }, // Hall
        {   0.0f,  50.0f, 0.60f,  4.5f, 0.30f, 0.90f, 0.73f }, // Spring
        {   5.0f,  70.0f, 0.60f,  7.0f, 0.30f, 0.90f, 0.87f }, // Plate
        {  25.0f, 120.0f, 2.50f, 12.0f, 0.18f, 0.60f, 1.46f }, // Church
        {   8.0f,  60.0f, 0.60f,  6.0f, 0.25f, 0.60f, 1.75f }, // Lofi
        {  12.0f,  80.0f, 0.80f,  8.0f, 0.25f, 0.80f, 1.57f }, // Mod
        {  15.0f,  90.0f, 1.50f, 15.0f, 0.30f, 0.85f, 1.17f }, // Shimmer
    };
    return specs[std::clamp (m, 0, 7)];
}

//==============================================================================
class Engine
{
public:
    void prepare (double sampleRate)
    {
        fs = (float) sampleRate;
        fdn.prepare (fs, 2.2f, 8.0f);
        er.prepare (fs);
        plate.prepare (fs);
        springA.prepare (fs, 0.0331f, 0.43f);
        springB.prepare (fs, 0.0407f, 0.37f);
        octave.prepare (fs);
        preDelay.prepare ((int) (0.13f * fs) + 16);

        ctrlInterval = std::max (16, (int) (fs / 1500.0f));
        smoothCoef   = 1.0f - std::exp (-1.0f / (0.05f * fs / (float) ctrlInterval)); // ~50 ms
        rampStep     = 1.0f / (0.012f * fs);                                           // ~12 ms
        preGlide     = 1.0f - std::exp (-1.0f / (0.08f * fs));

        activeMode = target.mode;
        reset();
        snapSmoothers();
        configureMode (activeMode);
        updateControl (true);
    }

    void reset()
    {
        fdn.clear(); er.clear(); plate.clear(); springA.clear(); springB.clear(); octave.clear();
        preDelay.clear();
        toneL.clear(); toneR.clear(); hpL.clear(); hpR.clear();
        lfoBpL.clear(); lfoBpH.clear(); lofiLpL.clear(); lofiLpR.clear();
        shimHp.clear(); shimLp.clear();
        lastWetMono = holdL = holdR = 0.0f; holdCount = 0;
        meter = 0.0f;
    }

    void setParams (const Params& p) { target = p; }
    float getMeter() const { return meter; }      // enveloppe du signal traité (pour la LED)
    int   getActiveMode() const { return activeMode; }

    // Traitement stéréo en place. Mono : passer le même pointeur pour L et R.
    void process (float* L, float* R, int numSamples)
    {
        const bool mono = (L == R);
        for (int n = 0; n < numSamples; ++n)
        {
            if (++ctrlCounter >= ctrlInterval) { ctrlCounter = 0; updateControl (false); }

            // Gestion du changement de mode (fondu sortant -> réinitialisation -> fondu entrant)
            if (target.mode != activeMode)
            {
                modeFade -= rampStep;
                if (modeFade <= 0.0f)
                {
                    modeFade = 0.0f;
                    activeMode = target.mode;
                    reset();
                    configureMode (activeMode);
                    updateControl (true);
                }
            }
            else if (modeFade < 1.0f) modeFade = std::min (1.0f, modeFade + rampStep);

            // Footswitch + spillover
            const float inTarget  = target.on ? 1.0f : 0.0f;
            const float outTarget = (target.on || target.spillover) ? 1.0f : 0.0f;
            inGain  += clampf (inTarget  - inGain,  -rampStep, rampStep);
            outGain += clampf (outTarget - outGain, -rampStep, rampStep);
            if (! target.on && ! target.spillover && outGain <= 0.0f && ! cleared) { reset(); cleared = true; }
            if (target.on) cleared = false;

            levelS += 0.0015f * (levelT - levelS);
            preS   += preGlide * (preT - preS);

            const float dryL = L[n];
            const float dryR = mono ? dryL : R[n];
            const float in   = 0.5f * (dryL + dryR) * inGain * modeFade;

            const float pre = preS > 2.0f ? preDelay.readFrac (preS) : in;
            preDelay.push (in);

            float wl = 0.0f, wr = 0.0f;
            processMode (pre, wl, wr);

            wl = toneL.process (hpL.process (wl));
            wr = toneR.process (hpR.process (wr));

            const float g = levelS * outGain * modeFade * modeGain;
            wl = softLimit (wl * g); wr = softLimit (wr * g);

            if (! std::isfinite (wl) || ! std::isfinite (wr)) { reset(); wl = wr = 0.0f; }

            const float env = 0.5f * (std::fabs (wl) + std::fabs (wr));
            meter = env > meter ? env : meter * meterRel;

            L[n] = dryL + (mono ? 0.5f * (wl + wr) : wl);
            if (! mono) R[n] = dryR + wr;
        }
    }

private:
    void snapSmoothers()
    {
        decayS = target.decay; toneS = target.tone; exprS = target.expression;
        const ModeSpec& s = modeSpec (target.mode);
        preS = preT = (target.preLong ? s.preLongMs : s.preShortMs) * 0.001f * fs;
        inGain = outGain = target.on ? 1.0f : 0.0f;
        modeFade = 1.0f;
    }

    void configureMode (int m)
    {
        switch ((Mode) m)
        {
            case Mode::Room:    fdn.setGeometry (0.42f, 0.55f, 0.60f, 3); fdn.setModulation (0.25f, 0.6f); break;
            case Mode::Hall:    fdn.setGeometry (1.00f, 1.00f, 0.72f, 4); fdn.setModulation (0.45f, 0.5f); break;
            case Mode::Church:  fdn.setGeometry (2.00f, 1.80f, 0.75f, 4); fdn.setModulation (0.70f, 0.3f); break;
            case Mode::Lofi:    fdn.setGeometry (0.75f, 0.90f, 0.70f, 4); fdn.setModulation (2.50f, 0.7f); break;
            case Mode::Mod:     fdn.setGeometry (1.10f, 1.00f, 0.70f, 4); fdn.setModulation (3.00f, 1.1f); break;
            case Mode::Shimmer: fdn.setGeometry (1.30f, 1.20f, 0.72f, 4); fdn.setModulation (0.60f, 0.4f); break;
            default: break;
        }
        const ModeSpec& s = modeSpec (m);
        modeGain = s.gain;
        hpL.setHighpass (m == (int) Mode::Spring ? 160.0f : 70.0f, fs);
        hpR.setHighpass (m == (int) Mode::Spring ? 160.0f : 70.0f, fs);
        lfoBpL.setHighpass (280.0f, fs);
        lfoBpH.setLowpass (3400.0f, fs);
        lofiLpL.setLowpass (4200.0f, fs);
        lofiLpR.setLowpass (4200.0f, fs);
        shimHp.setHighpass (180.0f, fs);
        shimLp.setLowpass (6500.0f, fs);
        lastTone = -1.0f;
    }

    // Calcul des coefficients (taux de contrôle)
    void updateControl (bool snap)
    {
        const float c = snap ? 1.0f : smoothCoef;
        decayS += c * (target.decay - decayS);
        toneS  += c * (target.tone  - toneS);
        exprS  += c * (target.expression - exprS);

        const int m = activeMode;
        const ModeSpec& s = modeSpec (m);
        float t60 = s.tMin * std::pow (s.tMax / s.tMin, clampf (decayS, 0.0f, 1.0f));
        float lvl = target.level;
        const float e = clampf (exprS, 0.0f, 1.0f);

        shimAmount = 0.0f;
        float modDepthMul = 1.0f;

        // Expression : même logique que le footswitch sensible à la pression
        switch ((Mode) m)
        {
            case Mode::Room: case Mode::Hall: case Mode::Spring: case Mode::Plate:
                lvl = lvl + e * (1.0f - lvl); break;
            case Mode::Church: case Mode::Lofi:
                t60 = t60 + e * (20.0f - t60); break;
            case Mode::Mod:
                modDepthMul = 1.0f + 2.5f * e;
                t60 = t60 + e * (s.tMax * 1.6f - t60); break;
            case Mode::Shimmer:
                t60 = t60 + e * (20.0f - t60); break;
            default: break;
        }

        const float hf = s.hfLo + (s.hfHi - s.hfLo) * toneS;
        switch ((Mode) m)
        {
            case Mode::Plate:  plate.setDecay (t60); plate.setDamping (0.75f - 0.65f * toneS); break;
            case Mode::Spring: springA.setDecay (t60); springB.setDecay (t60); break;
            default:           fdn.setDecay (t60, hf); break;
        }
        if (m == (int) Mode::Mod)     fdn.setModulation (3.0f * modDepthMul, 1.1f);
        if (m == (int) Mode::Shimmer) shimAmount = 0.45f + e * 0.37f;

        if (std::fabs (toneS - lastTone) > 0.002f)
        {
            lastTone = toneS;
            const float fc = 1400.0f * std::pow (12.0f, toneS);
            toneL.setLowpass (fc, fs); toneR.setLowpass (fc, fs);
        }

        // LEVEL : 0 -> muet, 0.5 -> -6 dB, 1 -> +6 dB (courbe douce)
        levelT = 2.0f * lvl * lvl;
        preT   = (target.preLong ? s.preLongMs : s.preShortMs) * 0.001f * fs;
        if (snap) { levelS = levelT; preS = preT; }
    }

    inline void processMode (float x, float& l, float& r)
    {
        switch ((Mode) activeMode)
        {
            case Mode::Room:
            {
                float el, er2;
                er.process (x, el, er2);
                fdn.process (x * 0.8f, l, r);
                l += 0.55f * el; r += 0.55f * er2;
                break;
            }
            case Mode::Hall:
            case Mode::Church:
            case Mode::Mod:
                fdn.process (x, l, r);
                break;

            case Mode::Plate:
                plate.process (x, l, r);
                break;

            case Mode::Spring:
            {
                const float a = springA.process (x);
                const float b = springB.process (x);
                l = 0.8f * a + 0.3f * b;
                r = 0.3f * a + 0.8f * b;
                break;
            }

            case Mode::Lofi:
            {
                const float band = lfoBpH.process (lfoBpL.process (x));
                float tl, tr;
                fdn.process (band, tl, tr);
                // Sous-échantillonnage (~11 kHz) + réduction de résolution
                const int hold = std::max (1, (int) std::lround (fs / 11025.0f));
                if (++holdCount >= hold) { holdCount = 0; holdL = crush (tl); holdR = crush (tr); }
                l = lofiLpL.process (holdL);
                r = lofiLpR.process (holdR);
                break;
            }

            case Mode::Shimmer:
            {
                const float sh = std::tanh (octave.process (shimLp.process (shimHp.process (lastWetMono))) * shimAmount * 1.4f);
                fdn.process (x + sh, l, r);
                lastWetMono = 0.5f * (l + r);
                break;
            }
            default: l = r = 0.0f; break;
        }
    }

    // Linéaire jusqu'à 1, puis compression douce (crête max 2)
    static inline float softLimit (float x)
    {
        const float a = std::fabs (x);
        return a <= 1.0f ? x : std::copysign (1.0f + std::tanh (a - 1.0f), x);
    }

    static inline float crush (float v)
    {
        // Remise à l'échelle avant quantification pour garder la queue audible
        constexpr float pre = 24.0f, q = 1.0f / 256.0f;
        return std::round (v * pre / q) * q / pre;
    }

    float fs = 48000.0f;
    Params target;
    int activeMode = 1;

    FDN8 fdn;
    EarlyReflections er;
    Plate plate;
    SpringTank springA, springB;
    OctaveUp octave;
    DelayLine preDelay;

    Biquad toneL, toneR, hpL, hpR, lfoBpL, lfoBpH, lofiLpL, lofiLpR, shimHp, shimLp;

    int   ctrlInterval = 32, ctrlCounter = 0, holdCount = 0;
    float smoothCoef = 0.1f, rampStep = 0.001f, preGlide = 0.001f;
    float decayS = 0.5f, toneS = 0.5f, exprS = 0.0f, lastTone = -1.0f;
    float levelT = 0.5f, levelS = 0.5f, preT = 0.0f, preS = 0.0f;
    float inGain = 1.0f, outGain = 1.0f, modeFade = 1.0f, modeGain = 1.0f;
    float shimAmount = 0.0f, lastWetMono = 0.0f, holdL = 0.0f, holdR = 0.0f;
    float meter = 0.0f, meterRel = 0.9995f;
    bool  cleared = false;
};

} // namespace holyverb
