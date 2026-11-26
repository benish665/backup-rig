#pragma once
// DSP building blocks that mirror the Web Audio nodes the page used
// (BiquadFilterNode formulas, WaveShaperNode, DelayNode, ConvolverNode, setTargetAtTime smoothing).
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <vector>
#include <memory>
#include <random>

namespace rig
{
constexpr double kPi = 3.14159265358979323846;
inline float dbToG (float d) { return std::pow (10.0f, d / 20.0f); }

//==============================================================================
// setTargetAtTime-style one-pole smoother
struct Smooth
{
    float v = 0, t = 0, c = 1;
    void prepare (double sr, double tau) { c = (float) (1.0 - std::exp (-1.0 / (tau * sr))); }
    void set (float x) { t = x; }
    void snap (float x) { v = t = x; }
    inline float next() { v += (t - v) * c; return v; }
    inline void advance (int n) { v += (t - v) * (1.0f - std::pow (1.0f - c, (float) n)); }
    bool settled() const { return std::abs (t - v) <= 1e-5f * (1.0f + std::abs (t)); }
};

//==============================================================================
struct Gain
{
    Smooth s;
    void prepare (double sr, double tau = 0.015) { s.prepare (sr, tau); }
    void set (float g) { s.set (g); }
    void snap (float g) { s.snap (g); }
    void process (float* x, int n) { for (int i = 0; i < n; ++i) x[i] *= s.next(); }
    void process (float* l, float* r, int n) { for (int i = 0; i < n; ++i) { const float g = s.next(); l[i] *= g; r[i] *= g; } }
    // dst += src * g
    void mixInto (const float* src, float* dst, int n) { for (int i = 0; i < n; ++i) dst[i] += src[i] * s.next(); }
    void mixInto (const float* sl, const float* sr, float* dl, float* dr, int n)
    { for (int i = 0; i < n; ++i) { const float g = s.next(); dl[i] += sl[i] * g; dr[i] += sr[i] * g; } }
};

//==============================================================================
// Biquad using the exact Web Audio coefficient formulas.
// LP/HP: Q is in dB (as in the spec). Peaking: linear Q. Shelves: S = 1.
enum class FT { LP, HP, Peak, LS, HS };

struct Biquad
{
    FT type = FT::LP;
    double sr = 48000;
    float q = 1;
    Smooth sf, sg;
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1[2] {}, z2[2] {};
    int cnt = 0;
    static constexpr int kUpd = 16;

    void init (FT t, float f, float Q, float g) { type = t; q = Q; sf.snap (f); sg.snap (g); }
    void prepare (double s) { sr = s; sf.prepare (sr, 0.015); sg.prepare (sr, 0.015); calc(); reset(); }
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; cnt = 0; }
    void setFreq (float f) { sf.set (f); }
    void setGain (float g) { sg.set (g); }
    void snapFreq (float f) { sf.snap (f); calc(); }
    void snapGain (float g) { sg.snap (g); calc(); }

    void calc()
    {
        const double f = juce::jlimit (1.0, sr * 0.4999, (double) sf.v);
        const double w0 = 2.0 * kPi * f / sr, cw = std::cos (w0), sw = std::sin (w0);
        double B0, B1, B2, A0, A1, A2;
        switch (type)
        {
            case FT::LP: { const double al = sw / (2.0 * std::pow (10.0, q / 20.0));
                B0 = (1 - cw) / 2; B1 = 1 - cw; B2 = (1 - cw) / 2; A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al; break; }
            case FT::HP: { const double al = sw / (2.0 * std::pow (10.0, q / 20.0));
                B0 = (1 + cw) / 2; B1 = -(1 + cw); B2 = (1 + cw) / 2; A0 = 1 + al; A1 = -2 * cw; A2 = 1 - al; break; }
            case FT::Peak: { const double A = std::pow (10.0, sg.v / 40.0), al = sw / (2.0 * q);
                B0 = 1 + al * A; B1 = -2 * cw; B2 = 1 - al * A; A0 = 1 + al / A; A1 = -2 * cw; A2 = 1 - al / A; break; }
            case FT::LS: { const double A = std::pow (10.0, sg.v / 40.0), al = sw / 2.0 * std::sqrt (2.0), sA = 2 * al * std::sqrt (A);
                B0 = A * ((A + 1) - (A - 1) * cw + sA); B1 = 2 * A * ((A - 1) - (A + 1) * cw); B2 = A * ((A + 1) - (A - 1) * cw - sA);
                A0 = (A + 1) + (A - 1) * cw + sA; A1 = -2 * ((A - 1) + (A + 1) * cw); A2 = (A + 1) + (A - 1) * cw - sA; break; }
            case FT::HS: default: { const double A = std::pow (10.0, sg.v / 40.0), al = sw / 2.0 * std::sqrt (2.0), sA = 2 * al * std::sqrt (A);
                B0 = A * ((A + 1) + (A - 1) * cw + sA); B1 = -2 * A * ((A - 1) + (A + 1) * cw); B2 = A * ((A + 1) + (A - 1) * cw - sA);
                A0 = (A + 1) - (A - 1) * cw + sA; A1 = 2 * ((A - 1) - (A + 1) * cw); A2 = (A + 1) - (A - 1) * cw - sA; break; }
        }
        b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0;
    }

    inline void tickUpdate()
    {
        if (cnt-- <= 0)
        {
            cnt = kUpd - 1;
            if (! sf.settled() || ! sg.settled()) { sf.advance (kUpd); sg.advance (kUpd); calc(); }
        }
    }
    inline float tick (float x, int ch = 0)
    {
        const double y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return (float) y;
    }
    void process (float* x, int n) { for (int i = 0; i < n; ++i) { tickUpdate(); x[i] = tick (x[i]); } }
    void process (float* l, float* r, int n) { for (int i = 0; i < n; ++i) { tickUpdate(); l[i] = tick (l[i], 0); r[i] = tick (r[i], 1); } }
};

struct FilterChain
{
    std::vector<Biquad> f;
    void add (FT t, float fr, float q, float g = 0) { Biquad b; b.init (t, fr, q, g); f.push_back (b); }
    void prepare (double sr) { for (auto& b : f) b.prepare (sr); }
    void reset() { for (auto& b : f) b.reset(); }
    void process (float* x, int n) { for (auto& b : f) b.process (x, n); }
};

//==============================================================================
// Waveshaper stage: g = drive/R into curve fn(x*R)  =>  y = fn(clamp(x*drive, -R, R)), optional oversampling.
enum class Curve { Tube, Soft };

struct Shaper
{
    Curve fn = Curve::Tube;
    float R = 1;
    int hq = 2, lq = 0; // oversampling exponents: 0 none, 1 = 2x, 2 = 4x
    int cur = -1;
    Smooth d;
    std::unique_ptr<juce::dsp::Oversampling<float>> os[3];
    std::vector<float> dbuf;

    void init (Curve c, float r, int hqF = 2, int lqF = 0) { fn = c; R = r; hq = hqF; lq = lqF; d.snap (1.0f); }
    void prepare (double sr, int maxBlock, bool hiq)
    {
        d.prepare (sr, 0.015);
        dbuf.assign ((size_t) maxBlock, 1.0f);
        for (int k = 1; k <= 2; ++k)
        {
            os[k] = std::make_unique<juce::dsp::Oversampling<float>> (1, (size_t) k,
                        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false, false);
            os[k]->initProcessing ((size_t) maxBlock);
        }
        setQuality (hiq);
    }
    void setQuality (bool hiq)
    {
        const int want = hiq ? hq : lq;
        if (want != cur) { cur = want; if (cur > 0) os[cur]->reset(); }
    }
    void reset() { for (int k = 1; k <= 2; ++k) if (os[k]) os[k]->reset(); }
    void drive (float x) { d.set (x); }
    void snapDrive (float x) { d.snap (x); }

    inline float shape (float u) const
    {
        u = juce::jlimit (-R, R, u);
        if (fn == Curve::Soft) return std::tanh (u);
        return u >= 0 ? std::tanh (u) : std::tanh (1.4f * u) / 1.4f;
    }

    void process (float* x, int n)
    {
        for (int i = 0; i < n; ++i) dbuf[(size_t) i] = d.next();
        if (cur <= 0)
        {
            for (int i = 0; i < n; ++i) x[i] = shape (x[i] * dbuf[(size_t) i]);
            return;
        }
        float* chans[1] = { x };
        juce::dsp::AudioBlock<float> blk (chans, 1, (size_t) n);
        auto up = os[cur]->processSamplesUp (blk);
        float* u = up.getChannelPointer (0);
        const int m = (int) up.getNumSamples();
        for (int j = 0; j < m; ++j) u[j] = shape (u[j] * dbuf[(size_t) (j >> cur)]);
        os[cur]->processSamplesDown (blk);
    }
};

//==============================================================================
// Delay line with linear interpolation (delay given in samples)
struct Delay
{
    std::vector<float> buf;
    int w = 0, mask = 0;
    void prepare (int maxSamples)
    {
        int sz = 1; while (sz < maxSamples + 4) sz <<= 1;
        buf.assign ((size_t) sz, 0.0f); mask = sz - 1; w = 0;
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); }
    inline void write (float x) { w = (w + 1) & mask; buf[(size_t) w] = x; }
    inline float read (float d) const
    {
        d = juce::jlimit (0.0f, (float) (mask - 2), d);
        const int i = (int) d; const float fr = d - (float) i;
        const float a = buf[(size_t) ((w - i) & mask)], b = buf[(size_t) ((w - i - 1) & mask)];
        return a + (b - a) * fr;
    }
    inline float tick (float x, float d) { write (x); return read (d); }
};

//==============================================================================
// stereo chorus: two delays, LFO +depth on the left and -depth on the right (wet only)
struct Chorus
{
    double sr = 48000, ph = 0; float base = 0.011f;
    Delay dl[2]; Smooth rate, depth;
    void init (float b) { base = b; rate.snap (0.5f); depth.snap (0.0f); }
    void prepare (double s) { sr = s; dl[0].prepare ((int) (0.1 * sr)); dl[1].prepare ((int) (0.1 * sr)); rate.prepare (sr, 0.015); depth.prepare (sr, 0.015); }
    void reset() { dl[0].reset(); dl[1].reset(); }
    void set (float r, float d) { rate.set (r); depth.set (d); }
    void process (const float* il, const float* ir, float* ol, float* orr, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            const float r = rate.next(), dp = depth.next();
            const float s = (float) std::sin (2.0 * kPi * ph);
            ph += r / sr; if (ph >= 1.0) ph -= 1.0;
            ol[i]  = dl[0].tick (il[i], (base + dp * s) * (float) sr);
            orr[i] = dl[1].tick (ir[i], (base - dp * s) * (float) sr);
        }
    }
};

//==============================================================================
// ping-pong delay: mono in, d1 -> L, d1 -> d2 -> R, d2 -> LP -> HP -> fb -> d1, slight LFO wow
struct PingPong
{
    double sr = 48000, ph = 0;
    Delay d1, d2; Biquad lp, hp; Smooth time, fb; float fbState = 0;
    void init (float lpF) { lp.init (FT::LP, lpF, -3, 0); hp.init (FT::HP, 120, -3, 0); time.snap (0.45f); fb.snap (0.35f); }
    void prepare (double s)
    {
        sr = s; d1.prepare ((int) (2.1 * sr)); d2.prepare ((int) (2.1 * sr));
        lp.prepare (sr); hp.prepare (sr); time.prepare (sr, 0.08); fb.prepare (sr, 0.015);
    }
    void reset() { d1.reset(); d2.reset(); lp.reset(); hp.reset(); fbState = 0; }
    void set (float sec, float f) { time.set (sec); fb.set (f); }
    void process (const float* in, float* ol, float* orr, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            const float t = time.next(), f = fb.next();
            const float m = 0.0006f * (float) std::sin (2.0 * kPi * ph);
            ph += 0.4 / sr; if (ph >= 1.0) ph -= 1.0;
            const float dt = (t + m) * (float) sr;
            const float y1 = d1.tick (in[i] + fbState, dt);
            const float y2 = d2.tick (y1, dt);
            ol[i] = y1; orr[i] = y2;
            fbState = f * hp.tick (lp.tick (y2));
        }
    }
};

//==============================================================================
// octave-up shifter: two delay taps on a looped ramp, sin^2 crossfade (stereo)
struct Octave
{
    double sr = 48000; int L = 2880, pos = 0; float W = 0.06f, off = 0.004f;
    Delay dl[2];
    void prepare (double s) { sr = s; L = (int) std::round (0.06 * sr); W = (float) L / (float) sr; dl[0].prepare ((int) (0.2 * sr)); dl[1].prepare ((int) (0.2 * sr)); pos = 0; }
    void reset() { dl[0].reset(); dl[1].reset(); pos = 0; }
    void process (const float* il, const float* ir, float* ol, float* orr, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            dl[0].write (il[i]); dl[1].write (ir[i]);
            float a = 0, b = 0;
            for (int k = 0; k < 2; ++k)
            {
                float ph = (float) pos / (float) L + (float) k * 0.5f; if (ph >= 1.0f) ph -= 1.0f;
                const float d = (off + W * (1.0f - ph)) * (float) sr;
                const float s = std::sin ((float) kPi * ph), g = s * s;
                a += g * dl[0].read (d); b += g * dl[1].read (d);
            }
            ol[i] = a; orr[i] = b;
            if (++pos >= L) pos = 0;
        }
    }
};

//==============================================================================
// zero-latency convolver (stereo IR: L->L, R->R)
struct Conv
{
    juce::dsp::Convolution c { juce::dsp::Convolution::NonUniform { 4096 } };
    void prepare (double sr, int maxBlock) { c.prepare ({ sr, (juce::uint32) maxBlock, 2 }); }
    void reset() { c.reset(); }
    void load (juce::AudioBuffer<float>&& ir, double sr)
    {
        c.loadImpulseResponse (std::move (ir), sr, juce::dsp::Convolution::Stereo::yes,
                               juce::dsp::Convolution::Trim::no, juce::dsp::Convolution::Normalise::no);
    }
    void process (float* l, float* r, int n)
    {
        float* ch[2] = { l, r };
        juce::dsp::AudioBlock<float> b (ch, 2, (size_t) n);
        c.process (juce::dsp::ProcessContextReplacing<float> (b));
    }
};

//==============================================================================
// impulse responses built in code and energy-normalised (same recipes as the page)
namespace ir
{
    struct Rnd { std::mt19937 g; std::uniform_real_distribution<float> d { -1.0f, 1.0f }; explicit Rnd (unsigned s) : g (s) {} float operator()() { return d (g); } };
    inline double lpA (double fc, double sr) { return 1.0 - std::exp (-2.0 * kPi * fc / sr); }

    template <typename Fill>
    juce::AudioBuffer<float> make (double sec, double sr, Fill fill)
    {
        const int n = std::max (1, (int) std::floor (sec * sr));
        juce::AudioBuffer<float> b (2, n);
        double e = 0;
        for (int c = 0; c < 2; ++c)
        {
            float* d = b.getWritePointer (c);
            fill (d, n, sr, c);
            for (int i = 0; i < n; ++i) e += (double) d[i] * d[i];
        }
        const double s = 1.0 / std::sqrt (e / 2 > 0 ? e / 2 : 1);
        b.applyGain ((float) s);
        return b;
    }

    inline juce::AudioBuffer<float> cloud (double rt, double sr)
    {
        Rnd rnd (1234);
        return make (rt * 1.05 + 0.4, sr, [&] (float* d, int n, double s, int)
        {
            const double tau = rt / 6.91, hp = std::exp (-2 * kPi * 160 / s);
            double lp = 0, hy = 0, hx = 0, a = 0;
            for (int i = 0; i < n; ++i)
            {
                const double t = i / s;
                if ((i & 63) == 0) a = lpA (1300 + 6200 * std::exp (-t / (rt * 0.3)), s);
                lp += a * (rnd() - lp); hy = hp * (hy + lp - hx); hx = lp;
                d[i] = (float) (hy * (1 - std::exp (-t / 0.16)) * std::exp (-t / tau));
            }
        });
    }

    inline juce::AudioBuffer<float> spring (double dw, double sr)
    {
        Rnd rnd (777);
        return make (2.4, sr, [&] (float* d, int n, double s, int c)
        {
            const double tau = 0.3 + dw * 0.07, Td = c ? 0.0371 : 0.0313, la = lpA (4300, s), hp = std::exp (-2 * kPi * 300 / s);
            double lp = 0, hy = 0, hx = 0;
            for (int i = 0; i < n; ++i)
            {
                const double t = i / s;
                double x = rnd() * 0.35 * std::exp (-t / tau) * (1 - std::exp (-t / 0.006));
                const int m = (int) std::floor (t / Td); const double tm = t - m * Td, D = 0.011 + m * 0.0012;
                if (tm < D && m < 40)
                    x += 0.9 * std::pow (0.83, m) * std::sin (2 * kPi * (3600 * tm - 1500 * tm * tm / D)) * std::sin (kPi * tm / D);
                lp += la * (x - lp); hy = hp * (hy + lp - hx); hx = lp; d[i] = (float) hy;
            }
        });
    }

    inline juce::AudioBuffer<float> room (double sr)
    {
        Rnd rnd (99);
        return make (1.1, sr, [&] (float* d, int n, double s, int c)
        {
            const double tapsL[] = { .005, .011, .017, .026, .034, .047 }, tapsR[] = { .007, .013, .021, .029, .038, .051 };
            const double* taps = c ? tapsR : tapsL; const double la = lpA (5200, s);
            double lp = 0;
            for (int i = 0; i < n; ++i) { const double t = i / s; lp += la * (rnd() - lp); d[i] = t > 0.004 ? (float) (lp * 0.55 * std::exp (-t / 0.16)) : 0.0f; }
            for (int k = 0; k < 6; ++k) { const int idx = (int) std::floor (taps[k] * s); if (idx < n) d[idx] += (float) ((k % 2 ? -0.9 : 0.9) * std::pow (0.8, k)); }
        });
    }

    inline juce::AudioBuffer<float> plate (double sr)
    {
        Rnd rnd (4242);
        return make (2.6, sr, [&] (float* d, int n, double s, int)
        {
            const double hp = std::exp (-2 * kPi * 350 / s);
            double lp = 0, hy = 0, hx = 0, a = 0;
            for (int i = 0; i < n; ++i)
            {
                const double t = i / s;
                if ((i & 63) == 0) a = lpA (2800 + 7000 * std::exp (-t / 0.5), s);
                lp += a * (rnd() - lp); hy = hp * (hy + lp - hx); hx = lp;
                d[i] = (float) (hy * (1 - std::exp (-t / 0.004)) * std::exp (-t / 0.38));
            }
        });
    }
} // namespace ir
} // namespace rig
