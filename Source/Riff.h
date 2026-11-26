#pragma once
// Karplus-Strong demo riff, same notes as the page's makeRiff()
#include "Dsp.h"

namespace rig
{
inline juce::AudioBuffer<float> makeRiff (double sr)
{
    ir::Rnd rnd (2024);
    const double q = 0.6, e = q / 2;
    const int len = (int) std::round (q * 16 * sr);
    std::vector<float> out ((size_t) len, 0.0f);
    struct O { double b, r; };
    auto ks = [&] (int start, double f, double dur, double amp, O o)
    {
        const int n = std::max (2, (int) std::round (sr / f));
        std::vector<double> b ((size_t) n);
        double lp = 0, m = 0;
        for (int i = 0; i < n; ++i) { lp += o.b * (rnd() - lp); b[(size_t) i] = lp; m += lp; }
        m /= n; for (auto& x : b) x -= m;
        const int tot = (int) std::floor (dur * sr), fade = (int) std::floor (0.02 * sr);
        int k = 0;
        for (int j = 0; j < tot; ++j)
        {
            const int i = start + j; if (i >= len) break;
            const double y = b[(size_t) k]; const int k1 = (k + 1) % n;
            b[(size_t) k] = o.r * 0.5 * (y + b[(size_t) k1]); k = k1;
            out[(size_t) i] += (float) (amp * y * (j > tot - fade ? (double) (tot - j) / fade : 1.0));
        }
    };
    auto hz = [] (double m) { return 440.0 * std::pow (2.0, (m - 69) / 12); };
    auto note = [&] (double t, double m, double d, double a, O o) { ks ((int) std::round (t * sr), hz (m), d, a, o); };

    { const int ms[] = { 52,59,62,66,67,66,62,59 }; for (int i = 0; i < 8; ++i) note (i * e, ms[i], std::min (2.2, (8 - i) * e + 0.05), 0.5, { 0.45, 0.9985 }); }
    { const int ms[] = { 48,55,59,64,67,64,59,55 }; for (int i = 0; i < 8; ++i) note (4 * q + i * e, ms[i], std::min (2.2, (8 - i) * e + 0.05), 0.5, { 0.45, 0.9985 }); }
    auto pc = [&] (double t, int root, double d, bool mute)
    { const int iv[] = { 0, 7, 12 }; for (int k = 0; k < 3; ++k) note (t + k * 0.007, root + iv[k], d, 0.36, { mute ? 0.22 : 0.6, mute ? 0.985 : 0.9975 }); };
    { const double P[][4] = { {0,40,.2,1},{.5,40,.16,1},{.75,40,.16,1},{1,43,.55,0},{1.5,45,.55,0},{2,40,.16,1},{2.25,40,.16,1},{2.5,47,.55,0},{3,45,.58,0} };
      for (auto& p : P) pc (8 * q + p[0] * q, (int) p[1], p[2], p[3] > 0.5); }
    { const double P[][3] = { {0,71,.3},{.5,74,.3},{1,76,.45},{1.75,74,.16},{2,71,.3},{2.5,69,.16},{2.75,67,.16},{3,64,.58} };
      for (auto& p : P) note (12 * q + p[0] * q, p[1], p[2], 0.55, { 0.55, 0.999 }); }

    float pk = 0; for (auto x : out) pk = std::max (pk, std::abs (x));
    const float s = 0.5f / (pk > 0 ? pk : 1.0f);
    juce::AudioBuffer<float> b (1, len);
    for (int i = 0; i < len; ++i) b.setSample (0, i, out[(size_t) i] * s);
    return b;
}
} // namespace rig
