#pragma once
// The four rigs, ported from the page's BUILD object, plus the global graph (input, gate, preset crossfade, master).
#include "Dsp.h"
#include "Presets.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>

namespace rig
{
constexpr int kBlock = 64; // internal processing chunk (also the shimmer feedback loop latency)

using PV = std::atomic<float>*;

struct Chain
{
    juce::AudioProcessorValueTreeState& vts;
    const char* pid;
    double sr = 48000;
    bool snapNext = true;
    const std::atomic<bool>* monoOut = nullptr;

    Chain (juce::AudioProcessorValueTreeState& v, const char* id) : vts (v), pid (id) {}
    virtual ~Chain() = default;

    PV k (const char* key) { auto* p = vts.getRawParameterValue (knobId (pid, key)); jassert (p != nullptr); return p; }
    PV s (const char* key) { auto* p = vts.getRawParameterValue (swId (pid, key)); jassert (p != nullptr); return p; }
    static float v (PV p) { return p->load(); }
    static bool on (PV p) { return p->load() > 0.5f; }

    // smoothed set or snap (first apply after prepare/reset snaps, like the page building the graph)
    template <typename T> void S (T& x, float val) { if (snapNext) x.snap (val); else x.set (val); }
    void SG (Biquad& b, float g) { if (snapNext) b.snapGain (g); else b.setGain (g); }
    void SF (Biquad& b, float f) { if (snapNext) b.snapFreq (f); else b.setFreq (f); }

    virtual void prepare (double sampleRate, bool hq) = 0;
    virtual void reset() = 0;
    virtual void applyParams() = 0;
    void apply() { applyParams(); snapNext = false; }
    virtual void process (const float* in, float* L, float* R, int n) = 0;
    virtual void setQuality (bool hq) = 0;
    virtual void messageThreadTick() {}
};

static constexpr float kMK[4] = { 0.68f, 0.71f, 0.13f, 0.1035f };
inline float lvlG (float level, int idx) { return dbToG ((level - 5) * 2.4f) * kMK[idx]; }

struct ToneStack
{
    Biquad b, m, t; float bs, ms, mo, ts;
    void init (float bf, float bs_, float mf, float mq, float ms_, float mo_, float tf, float ts_)
    { b.init (FT::LS, bf, 1, 0); m.init (FT::Peak, mf, mq, 0); t.init (FT::HS, tf, 1, 0); bs = bs_; ms = ms_; mo = mo_; ts = ts_; }
    void prepare (double sr) { b.prepare (sr); m.prepare (sr); t.prepare (sr); }
    void reset() { b.reset(); m.reset(); t.reset(); }
    void set (Chain& c, float bass, float mid, float treb)
    { c.SG (b, (bass - 5) * bs); c.SG (m, (mid - 5) * ms + mo); c.SG (t, (treb - 5) * ts); }
    void process (float* x, int n) { b.process (x, n); m.process (x, n); t.process (x, n); }
};

inline void makeCab (FilterChain& fc, const char* name)
{
    struct E { FT t; float f, q, g; };
    std::vector<E> e;
    const juce::String nm (name);
    if (nm == "open212") e = { {FT::HP,75,-3,0},{FT::Peak,115,1.1f,2.5f},{FT::Peak,520,1,-2.5f},{FT::Peak,2400,1.4f,2.5f},{FT::Peak,3600,2,1.5f},{FT::LP,6000,-1,0},{FT::LP,7800,-3,0} };
    if (nm == "gbA")     e = { {FT::HP,85,-2,0},{FT::Peak,105,1.4f,4},{FT::Peak,420,1.1f,-3},{FT::Peak,1700,1.1f,2.5f},{FT::Peak,3100,2,4},{FT::LP,5000,-1,0},{FT::LP,6500,-3,0} };
    if (nm == "gbB")     e = { {FT::HP,90,-2,0},{FT::Peak,140,1.2f,3},{FT::Peak,520,1,-2},{FT::Peak,1300,1,3},{FT::Peak,2500,2,2},{FT::LP,4400,-1,0},{FT::LP,6000,-3,0} };
    if (nm == "v30")     e = { {FT::HP,85,-2,0},{FT::Peak,110,1.3f,3},{FT::Peak,600,1,-1.5f},{FT::Peak,1250,1,2},{FT::Peak,2700,1.6f,4.5f},{FT::Peak,4300,3,-5},{FT::LP,5200,-1,0},{FT::LP,6400,-3,0} };
    for (auto& x : e) fc.add (x.t, x.f, x.q, x.g);
}

// IR that is rebuilt (debounced, on the message thread) when a knob changes
struct DebouncedIR
{
    float loaded = -1, pending = -1; juce::uint32 since = 0; int delayMs = 250;
    template <typename Make>
    void tick (float want, Make make)
    {
        if (want == loaded) { pending = want; return; }
        const auto now = juce::Time::getMillisecondCounter();
        if (want != pending) { pending = want; since = now; return; }
        if (now - since >= (juce::uint32) delayMs) { loaded = want; make (want); }
    }
};

//==============================================================================
struct Clean : Chain
{
    PV vol = k ("vol"), treb = k ("treb"), mid = k ("mid"), bass = k ("bass"), level = k ("level"), rev = k ("rev"), dwell = k ("dwell"),
       speed = k ("speed"), inten = k ("inten"), crate = k ("crate"), cdepth = k ("cdepth"),
       bright = s ("bright"), spring = s ("spring"), vib = s ("vib"), chorusOn = s ("chorus");

    Biquad hp, br, c1; Shaper s1, pw; ToneStack ts; FilterChain cb;
    Conv spr; Gain sprG; Smooth trD, trF; double trPh = 0; Chorus ch; Gain chW, lvl; DebouncedIR dw;
    float x[kBlock], sl[kBlock], sr_[kBlock], cl[kBlock], cr[kBlock];

    Clean (juce::AudioProcessorValueTreeState& v) : Chain (v, "clean")
    {
        hp.init (FT::HP, 70, -3, 0); br.init (FT::HS, 2400, 1, 0); s1.init (Curve::Tube, 3, 1, 0); c1.init (FT::HP, 25, -3, 0);
        ts.init (120, 2.4f, 550, 0.55f, 1.8f, -2.5f, 2600, 2); pw.init (Curve::Soft, 2.5f, 1, 0); makeCab (cb, "open212");
        ch.init (0.011f);
    }
    void prepare (double rate, bool hq) override
    {
        sr = rate;
        for (auto* b : { &hp, &br, &c1 }) b->prepare (sr);
        s1.prepare (sr, kBlock, hq); pw.prepare (sr, kBlock, hq); ts.prepare (sr); cb.prepare (sr);
        spr.prepare (sr, kBlock); spr.load (ir::spring (v (dwell), sr), sr); dw.loaded = dw.pending = v (dwell);
        for (auto* g : { &sprG, &chW, &lvl }) g->prepare (sr);
        trD.prepare (sr, 0.015); trF.prepare (sr, 0.015); ch.prepare (sr);
        snapNext = true;
    }
    void reset() override
    {
        hp.reset(); br.reset(); c1.reset(); s1.reset(); pw.reset(); ts.reset(); cb.reset(); spr.reset(); ch.reset(); trPh = 0; snapNext = true;
    }
    void setQuality (bool hq) override { s1.setQuality (hq); pw.setQuality (hq); }
    void messageThreadTick() override
    {
        dw.tick (v (dwell), [this] (float d) { spr.load (ir::spring (d, sr), sr); });
    }
    void applyParams() override
    {
        SG (br, on (bright) ? 2.5f : 0.0f);
        S (s1.d, 0.25f + std::pow (v (vol) / 10.0f, 2.0f) * 5.5f);
        ts.set (*this, v (bass), v (mid), v (treb));
        S (pw.d, 1.0f);
        S (sprG.s, on (spring) ? v (rev) / 10.0f * 1.2f : 0.0f);
        const float d = on (vib) ? v (inten) / 10.0f * 0.5f : 0.0f;
        S (trD, d); S (trF, 1.5f + v (speed) / 10.0f * 8.5f);
        if (snapNext) { ch.rate.snap (0.15f + v (crate) / 10 * 2.2f); ch.depth.snap (0.0004f + v (cdepth) / 10 * 0.0035f); }
        else ch.set (0.15f + v (crate) / 10 * 2.2f, 0.0004f + v (cdepth) / 10 * 0.0035f);
        S (chW.s, on (chorusOn) ? 0.7f : 0.0f);
        S (lvl.s, lvlG (v (level), 0));
    }
    void process (const float* in, float* L, float* R, int n) override
    {
        std::copy (in, in + n, x);
        hp.process (x, n); br.process (x, n); s1.process (x, n); c1.process (x, n); ts.process (x, n); pw.process (x, n); cb.process (x, n);
        std::copy (x, x + n, L); std::copy (x, x + n, R);
        std::copy (x, x + n, sl); std::copy (x, x + n, sr_);
        spr.process (sl, sr_, n);
        sprG.mixInto (sl, sr_, L, R, n);
        for (int i = 0; i < n; ++i)
        {
            const float d = trD.next(), f = trF.next();
            const float g = 1.0f - d + d * (float) std::sin (2.0 * kPi * trPh);
            trPh += f / sr; if (trPh >= 1.0) trPh -= 1.0;
            L[i] *= g; R[i] *= g;
        }
        ch.process (L, R, cl, cr, n);
        chW.mixInto (cl, cr, L, R, n);
        lvl.process (L, R, n);
    }
};

//==============================================================================
struct Sky : Chain
{
    PV decay = k ("decay"), mix = k ("mix"), tone = k ("tone"), shim = k ("shim"), mod = k ("mod"), level = k ("level"),
       time = k ("time"), fb = k ("fb"), dmix = k ("dmix"), gain = k ("gain"), bass = k ("bass"), treb = k ("treb"), dly = s ("dly");

    Biquad hp, c1, bs, tr; Shaper s1; FilterChain cb;
    Gain dry, dW, dRev, shG, fbG, wet, lvl;
    PingPong dl; Delay pre[2]; Conv rv; Octave oc; Chorus md; Biquad toneF; Delay fbLine[2]; DebouncedIR dc;
    float amp[kBlock], L2[kBlock], R2[kBlock], rL[kBlock], rR[kBlock], oL[kBlock], oR[kBlock], vL[kBlock], vR[kBlock], mL[kBlock], mR[kBlock];

    static float rt (float v) { return 2 + v / 10 * 10; }

    Sky (juce::AudioProcessorValueTreeState& v) : Chain (v, "sky")
    {
        hp.init (FT::HP, 80, -3, 0); s1.init (Curve::Tube, 4, 1, 0); c1.init (FT::HP, 30, -3, 0);
        bs.init (FT::LS, 150, 1, 0); tr.init (FT::HS, 2500, 1, 0); makeCab (cb, "open212");
        dl.init (3200); md.init (0.02f); toneF.init (FT::LP, 5000, -3, 0); dc.delayMs = 300;
    }
    void prepare (double rate, bool hq) override
    {
        sr = rate;
        for (auto* b : { &hp, &c1, &bs, &tr, &toneF }) b->prepare (sr);
        s1.prepare (sr, kBlock, hq); cb.prepare (sr);
        for (auto* g : { &dry, &dW, &dRev, &shG, &fbG, &wet, &lvl }) g->prepare (sr);
        dl.prepare (sr); pre[0].prepare ((int) (0.2 * sr)); pre[1].prepare ((int) (0.2 * sr));
        fbLine[0].prepare (kBlock * 2); fbLine[1].prepare (kBlock * 2);
        oc.prepare (sr); md.prepare (sr);
        rv.prepare (sr, kBlock); rv.load (ir::cloud (rt (v (decay)), sr), sr); dc.loaded = dc.pending = v (decay);
        snapNext = true;
    }
    void reset() override
    {
        for (auto* b : { &hp, &c1, &bs, &tr, &toneF }) b->reset();
        s1.reset(); cb.reset(); dl.reset(); pre[0].reset(); pre[1].reset(); fbLine[0].reset(); fbLine[1].reset(); oc.reset(); md.reset(); rv.reset();
        snapNext = true;
    }
    void setQuality (bool hq) override { s1.setQuality (hq); }
    void messageThreadTick() override
    {
        dc.tick (v (decay), [this] (float d) { rv.load (ir::cloud (rt (d), sr), sr); });
    }
    void applyParams() override
    {
        S (s1.d, 0.5f + std::pow (v (gain) / 10, 2.0f) * 5);
        SG (bs, (v (bass) - 5) * 2); SG (tr, (v (treb) - 5) * 2);
        const float m = v (mix) / 10;
        S (dry.s, 1 - std::max (0.0f, m - 0.5f) * 1.4f); S (wet.s, m * 1.8f);
        SF (toneF, 1500 * std::pow (8.0f, v (tone) / 10));
        S (shG.s, v (shim) / 10 * 0.9f); S (fbG.s, v (shim) / 10 * 0.28f);
        const float mr = 0.2f + v (mod) / 10 * 0.6f, mdp = 0.0005f + v (mod) / 10 * 0.004f;
        if (snapNext) { md.rate.snap (mr); md.depth.snap (mdp); dl.time.snap (v (time) / 1000); dl.fb.snap (v (fb) / 10 * 0.75f); }
        else { md.set (mr, mdp); dl.set (v (time) / 1000, v (fb) / 10 * 0.75f); }
        S (dW.s, on (dly) ? v (dmix) / 10 * 0.9f : 0.0f); S (dRev.s, on (dly) ? 0.5f : 0.0f);
        S (lvl.s, lvlG (v (level), 1));
    }
    void process (const float* in, float* L, float* R, int n) override
    {
        std::copy (in, in + n, amp);
        hp.process (amp, n); s1.process (amp, n); c1.process (amp, n); bs.process (amp, n); tr.process (amp, n); cb.process (amp, n);

        // dry
        for (int i = 0; i < n; ++i) { const float g = dry.s.next(); L[i] = R[i] = amp[i] * g; }
        // ping-pong delay
        dl.process (amp, L2, R2, n);
        dW.mixInto (L2, R2, L, R, n);
        // reverb input bus
        std::copy (amp, amp + n, rL); std::copy (amp, amp + n, rR);
        dRev.mixInto (L2, R2, rL, rR, n);
        // octave shifter fed by rin + feedback of the toned wet signal (one block back)
        for (int i = 0; i < n; ++i)
        {
            const float g = fbG.s.next();
            oL[i] = rL[i] + g * fbLine[0].read ((float) (kBlock - 1 - i));
            oR[i] = rR[i] + g * fbLine[1].read ((float) (kBlock - 1 - i));
        }
        oc.process (oL, oR, oL, oR, n);
        const float preD = 0.035f * (float) sr;
        for (int i = 0; i < n; ++i) { vL[i] = pre[0].tick (rL[i], preD); vR[i] = pre[1].tick (rR[i], preD); }
        shG.mixInto (oL, oR, vL, vR, n);
        rv.process (vL, vR, n);
        md.process (vL, vR, mL, mR, n);
        toneF.process (mL, mR, n);
        for (int i = 0; i < n; ++i) { fbLine[0].write (mL[i]); fbLine[1].write (mR[i]); }
        wet.mixInto (mL, mR, L, R, n);
        lvl.process (L, R, n);
    }
};

//==============================================================================
struct Plexi : Chain
{
    PV pres = k ("pres"), bass = k ("bass"), mid = k ("mid"), treb = k ("treb"), vol = k ("vol"), level = k ("level"),
       width = k ("width"), room = k ("room"), wall = s ("wall"), roomOn = s ("room");

    struct Side
    {
        Biquad hp, br, c1, l1, c2, pr; Shaper s1, s2, pw; ToneStack ts; FilterChain cb; float tw;
        Side (const char* cabName, float tw_) : tw (tw_)
        {
            hp.init (FT::HP, 90, -3, 0); br.init (FT::HS, 2800, 1, 3.5f); s1.init (Curve::Tube, 6, 2, 0); c1.init (FT::HP, 35, -3, 0);
            l1.init (FT::LP, 7500, -3, 0); s2.init (Curve::Tube, 4, 2, 0); c2.init (FT::HP, 45, -3, 0);
            ts.init (100, 2.2f, 650, 0.55f, 2.4f, 1, 2200, 2.4f); pw.init (Curve::Soft, 3, 2, 0); pr.init (FT::HS, 3800, 1, 0); makeCab (cb, cabName);
        }
        void prepare (double sr, bool hq)
        {
            for (auto* b : { &hp, &br, &c1, &l1, &c2, &pr }) b->prepare (sr);
            s1.prepare (sr, kBlock, hq); s2.prepare (sr, kBlock, hq); pw.prepare (sr, kBlock, hq); ts.prepare (sr); cb.prepare (sr);
        }
        void reset() { for (auto* b : { &hp, &br, &c1, &l1, &c2, &pr }) b->reset(); s1.reset(); s2.reset(); pw.reset(); ts.reset(); cb.reset(); }
        void setQuality (bool hq) { s1.setQuality (hq); s2.setQuality (hq); pw.setQuality (hq); }
        void apply (Plexi& c)
        {
            const float vv = Chain::v (c.vol) / 10;
            c.S (s1.d, (1 + vv * vv * 12) * tw); c.S (s2.d, 1.2f);
            ts.set (c, Chain::v (c.bass), Chain::v (c.mid), Chain::v (c.treb));
            c.S (pw.d, 1 + vv * 0.8f); c.SG (pr, (Chain::v (c.pres) - 5) * 1.8f); c.SG (br, 3.5f - vv * 2);
        }
        void process (float* x, int n)
        {
            hp.process (x, n); br.process (x, n); s1.process (x, n); c1.process (x, n); l1.process (x, n); s2.process (x, n);
            c2.process (x, n); ts.process (x, n); pw.process (x, n); pr.process (x, n); cb.process (x, n);
        }
    };

    Side A { "gbA", 1.0f }, B { "gbB", 1.15f };
    Delay hz; double lph = 0; Gain aR, bR, rG, lvl; Conv rm;
    float a[kBlock], b[kBlock], rl[kBlock], rr[kBlock];

    Plexi (juce::AudioProcessorValueTreeState& v) : Chain (v, "plexi") {}
    void prepare (double rate, bool hq) override
    {
        sr = rate; A.prepare (sr, hq); B.prepare (sr, hq); hz.prepare ((int) (0.05 * sr));
        for (auto* g : { &aR, &bR, &rG, &lvl }) g->prepare (sr);
        rm.prepare (sr, kBlock); rm.load (ir::room (sr), sr);
        snapNext = true;
    }
    void reset() override { A.reset(); B.reset(); hz.reset(); rm.reset(); snapNext = true; }
    void setQuality (bool hq) override { A.setQuality (hq); B.setQuality (hq); }
    void applyParams() override
    {
        A.apply (*this); B.apply (*this);
        const bool mono = monoOut != nullptr && monoOut->load();
        const float w = on (wall) && ! mono ? v (width) / 10 : 0.0f;
        S (aR.s, 1 - w); S (bR.s, w);
        S (rG.s, on (roomOn) ? v (room) / 10 * 0.8f : 0.0f);
        S (lvl.s, lvlG (v (level), 2));
    }
    void process (const float* in, float* L, float* R, int n) override
    {
        for (int i = 0; i < n; ++i)
        {
            a[i] = in[i];
            const float d = (0.016f + 0.0005f * (float) std::sin (2.0 * kPi * lph)) * (float) sr;
            lph += 0.23 / sr; if (lph >= 1.0) lph -= 1.0;
            b[i] = hz.tick (in[i], d);
        }
        A.process (a, n); B.process (b, n);
        for (int i = 0; i < n; ++i) { L[i] = a[i]; R[i] = a[i] * aR.s.next() + b[i] * bR.s.next(); }
        std::copy (L, L + n, rl); std::copy (R, R + n, rr);
        rm.process (rl, rr, n);
        rG.mixInto (rl, rr, L, R, n);
        lvl.process (L, R, n);
    }
};

//==============================================================================
struct Lead : Chain
{
    PV pre_ = k ("pre"), bass = k ("bass"), mid = k ("mid"), treb = k ("treb"), pres = k ("pres"), level = k ("level"),
       bdrive = k ("bdrive"), time = k ("time"), fb = k ("fb"), dmix = k ("dmix"), pmix = k ("pmix"),
       boost = s ("boost"), dly = s ("dly"), plateOn = s ("plate");

    Biquad bHp, bPk, hp, c1, l1, c2, l2, pr, sm; Gain bG; Shaper s1, s2, s3, pw; ToneStack ts; FilterChain cb;
    PingPong dl; Gain dW, pW, lvl, dry; Delay pd; Conv pl;
    float x[kBlock], dL[kBlock], dR[kBlock], pL[kBlock], pR[kBlock];

    Lead (juce::AudioProcessorValueTreeState& v) : Chain (v, "lead")
    {
        bHp.init (FT::HP, 20, -3, 0); bPk.init (FT::Peak, 800, 0.8f, 0); hp.init (FT::HP, 90, -3, 0);
        s1.init (Curve::Tube, 8, 2, 1); c1.init (FT::HP, 60, -3, 0); l1.init (FT::LP, 9000, -3, 0);
        s2.init (Curve::Tube, 6, 2, 0); c2.init (FT::HP, 120, -3, 0); l2.init (FT::LP, 6500, -3, 0); s3.init (Curve::Soft, 4, 2, 0);
        ts.init (110, 2.2f, 750, 0.7f, 2.4f, 1.5f, 2400, 2.2f); pw.init (Curve::Soft, 3, 2, 0); pr.init (FT::HS, 4000, 1, 0);
        makeCab (cb, "v30"); sm.init (FT::LP, 6500, -3, 0); dl.init (2600);
    }
    void prepare (double rate, bool hq) override
    {
        sr = rate;
        for (auto* b : { &bHp, &bPk, &hp, &c1, &l1, &c2, &l2, &pr, &sm }) b->prepare (sr);
        for (auto* s : { &s1, &s2, &s3, &pw }) s->prepare (sr, kBlock, hq);
        ts.prepare (sr); cb.prepare (sr); dl.prepare (sr); pd.prepare ((int) (0.1 * sr));
        for (auto* g : { &bG, &dW, &pW, &lvl, &dry }) g->prepare (sr);
        dry.snap (1);
        pl.prepare (sr, kBlock); pl.load (ir::plate (sr), sr);
        snapNext = true;
    }
    void reset() override
    {
        for (auto* b : { &bHp, &bPk, &hp, &c1, &l1, &c2, &l2, &pr, &sm }) b->reset();
        for (auto* s : { &s1, &s2, &s3, &pw }) s->reset();
        ts.reset(); cb.reset(); dl.reset(); pd.reset(); pl.reset(); snapNext = true;
    }
    void setQuality (bool hq) override { for (auto* s : { &s1, &s2, &s3, &pw }) s->setQuality (hq); }
    void applyParams() override
    {
        const bool b = on (boost);
        SF (bHp, b ? 200.0f : 20.0f); SG (bPk, b ? 4 + v (bdrive) / 10 * 4 : 0.0f); S (bG.s, b ? 1.2f + v (bdrive) / 10 * 1.6f : 1.0f);
        const float vv = v (pre_) / 10;
        S (s1.d, 1.5f + vv * vv * 30); S (s2.d, 2.0f); S (s3.d, 1.5f);
        ts.set (*this, v (bass), v (mid), v (treb)); S (pw.d, 1.3f); SG (pr, (v (pres) - 5) * 1.8f);
        if (snapNext) { dl.time.snap (v (time) / 1000); dl.fb.snap (v (fb) / 10 * 0.7f); }
        else dl.set (v (time) / 1000, v (fb) / 10 * 0.7f);
        S (dW.s, on (dly) ? v (dmix) / 10 * 0.9f : 0.0f);
        S (pW.s, on (plateOn) ? v (pmix) / 10 * 1.1f : 0.0f);
        S (lvl.s, lvlG (v (level), 3));
    }
    void process (const float* in, float* L, float* R, int n) override
    {
        std::copy (in, in + n, x);
        bHp.process (x, n); bPk.process (x, n); bG.process (x, n);
        hp.process (x, n); s1.process (x, n); c1.process (x, n); l1.process (x, n); s2.process (x, n);
        c2.process (x, n); l2.process (x, n); s3.process (x, n); ts.process (x, n); pw.process (x, n); pr.process (x, n);
        cb.process (x, n); sm.process (x, n);
        for (int i = 0; i < n; ++i) L[i] = R[i] = x[i];
        dl.process (x, dL, dR, n);
        dW.mixInto (dL, dR, L, R, n);
        const float preD = 0.025f * (float) sr;
        for (int i = 0; i < n; ++i) pL[i] = pR[i] = pd.tick (x[i], preD);
        pl.process (pL, pR, n);
        pW.mixInto (pL, pR, L, R, n);
        lvl.process (L, R, n);
    }
};

//==============================================================================
// Global graph: input select + gain, DC block, noise gate, the 4 rigs with crossfade + tails, mono, master, soft clip.
struct Engine
{
    juce::AudioProcessorValueTreeState& vts;
    std::array<std::unique_ptr<Chain>, 4> ch;
    std::array<Gain, 4> send;
    std::array<bool, 4> live {};
    std::array<juce::int64, 4> retireAt {};
    int cur = 0;
    double sr = 48000;
    std::atomic<bool> mono { false };
    std::atomic<bool> tunerMute { false };
    bool hq = false;

    Gain inG, master; Biquad dc, e1, e2;
    float ek = 1; bool gateOff = false;
    juce::int64 clock = 0;

    PV pPreset, pIn, pGate, pOut, pMono, pHq, pInCh;

    float inb[kBlock], gx[kBlock], cL[kBlock], cR[kBlock];

    explicit Engine (juce::AudioProcessorValueTreeState& v) : vts (v)
    {
        ch[0] = std::make_unique<Clean> (v); ch[1] = std::make_unique<Sky> (v);
        ch[2] = std::make_unique<Plexi> (v); ch[3] = std::make_unique<Lead> (v);
        for (auto& c : ch) c->monoOut = &mono;
        pPreset = v.getRawParameterValue (gid::preset); pIn = v.getRawParameterValue (gid::inDb);
        pGate = v.getRawParameterValue (gid::gate); pOut = v.getRawParameterValue (gid::outDb);
        pMono = v.getRawParameterValue (gid::mono); pHq = v.getRawParameterValue (gid::hq); pInCh = v.getRawParameterValue (gid::inCh);
        dc.init (FT::HP, 30, -3, 0); e1.init (FT::LP, 30, -3, 0); e2.init (FT::LP, 30, -3, 0);
    }

    void prepare (double rate)
    {
        sr = rate; hq = pHq->load() > 0.5f;
        for (auto& c : ch) { c->prepare (sr, hq); }
        for (auto& g : send) { g.prepare (sr, 0.006); g.snap (0); }
        inG.prepare (sr, 0.03); master.prepare (sr, 0.02);
        dc.prepare (sr); e1.prepare (sr); e2.prepare (sr);
        cur = juce::jlimit (0, 3, (int) pPreset->load());
        live.fill (false); live[(size_t) cur] = true; send[(size_t) cur].snap (1);
        clock = 0;
        updateGlobals (true);
    }

    void messageThreadTick() { for (auto& c : ch) c->messageThreadTick(); }

    void updateGlobals (bool snap)
    {
        const bool m = pMono->load() > 0.5f; mono.store (m);
        const bool q = pHq->load() > 0.5f;
        if (q != hq) { hq = q; for (auto& c : ch) c->setQuality (hq); }
        const float outG = tunerMute.load() ? 0.0f : dbToG (pOut->load());
        if (snap) master.snap (outG); else master.set (outG);
        const float thr = pGate->load();
        static constexpr float mult[4] = { 0.1f, 0.1f, 0.5f, 1.0f };
        gateOff = thr <= -90.0f;
        ek = 1.0f / (dbToG (thr) * mult[cur]);
    }

    void select (int id)
    {
        if (id == cur) return;
        const int old = cur; cur = id;
        send[(size_t) old].set (0);
        retireAt[(size_t) old] = clock + (juce::int64) ((old == 1 ? 26.0 : 12.0) * sr);
        if (! live[(size_t) id]) { ch[(size_t) id]->reset(); live[(size_t) id] = true; }
        retireAt[(size_t) id] = 0;
        send[(size_t) id].set (1);
    }

    // in: mono input samples (already chosen channel), inputGainDb applied unless demo
    // monitorOut receives the post-input-gain signal (for meter/tuner)
    void process (const float* in, float* outL, float* outR, int n, bool applyInGain, float* monitorOut)
    {
        const int want = juce::jlimit (0, 3, (int) pPreset->load());
        if (want != cur) select (want);
        updateGlobals (false);
        inG.set (applyInGain ? dbToG (pIn->load()) : 1.0f);
        for (size_t c = 0; c < 4; ++c) if (live[c]) ch[c]->apply();

        for (int off = 0; off < n; off += kBlock)
        {
            const int m = std::min (kBlock, n - off);
            float* L = outL + off; float* R = outR + off;
            std::copy (in + off, in + off + m, inb);
            inG.process (inb, m);
            if (monitorOut) std::copy (inb, inb + m, monitorOut + off);
            dc.process (inb, m);
            // noise gate
            for (int i = 0; i < m; ++i)
            {
                float g = 1.0f;
                float e = e2.tick (e1.tick (std::abs (inb[i]) * ek));
                e1.tickUpdate(); e2.tickUpdate();
                if (! gateOff)
                {
                    const float xx = juce::jlimit (-1.0f, 1.0f, e);
                    if (xx <= 0.5f) g = 0; else if (xx >= 1) g = 1; else { const float t = (xx - 0.5f) / 0.5f; g = t * t * (3 - 2 * t); }
                }
                gx[i] = inb[i] * g;
            }
            std::fill (L, L + m, 0.0f); std::fill (R, R + m, 0.0f);
            for (size_t c = 0; c < 4; ++c)
            {
                if (! live[c]) continue;
                float sb[kBlock];
                for (int i = 0; i < m; ++i) sb[i] = gx[i] * send[c].s.next();
                ch[c]->process (sb, cL, cR, m);
                for (int i = 0; i < m; ++i) { L[i] += cL[i]; R[i] += cR[i]; }
                if ((int) c != cur && retireAt[c] > 0 && clock >= retireAt[c]) { live[c] = false; retireAt[c] = 0; }
            }
            const bool mo = mono.load();
            for (int i = 0; i < m; ++i)
            {
                float l = L[i], r = R[i];
                if (mo) l = r = 0.5f * (l + r);
                const float g = master.s.next() * 0.5f;
                L[i] = clip (l * g * 2.0f); R[i] = clip (r * g * 2.0f);
            }
            clock += m;
        }
    }

    static inline float clip (float u)
    {
        u = juce::jlimit (-2.0f, 2.0f, u);
        const float a = std::abs (u);
        return a < 0.8f ? u : std::copysign (0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f), u);
    }
};
} // namespace rig
