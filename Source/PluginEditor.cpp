#include "PluginEditor.h"
#include "BinaryData.h"

using namespace rig;

namespace ui
{
static juce::String U (const char* s) { return juce::String::fromUTF8 (s); }
static juce::Colour C (juce::uint32 argb) { return juce::Colour (argb); }

Fonts& Fonts::get()
{
    static Fonts f = []
    {
        Fonts x;
        auto T = [] (const void* d, int n) { return juce::Typeface::createSystemTypefaceFor (d, (size_t) n); };
        x.heebo      = T (BinaryData::Heebo_ttf, BinaryData::Heebo_ttfSize);
        x.barlowSemi = T (BinaryData::BarlowCondensedSemiBold_ttf, BinaryData::BarlowCondensedSemiBold_ttfSize);
        x.barlowBold = T (BinaryData::BarlowCondensedBold_ttf, BinaryData::BarlowCondensedBold_ttfSize);
        x.yellowtail = T (BinaryData::YellowtailRegular_ttf, BinaryData::YellowtailRegular_ttfSize);
        x.josefin    = T (BinaryData::JosefinSans_ttf, BinaryData::JosefinSans_ttfSize);
        x.archivo    = T (BinaryData::ArchivoBlackRegular_ttf, BinaryData::ArchivoBlackRegular_ttfSize);
        x.michroma   = T (BinaryData::MichromaRegular_ttf, BinaryData::MichromaRegular_ttfSize);
        return x;
    }();
    return f;
}
static juce::Font heebo (float h) { return Fonts::make (Fonts::get().heebo, h); }
static juce::Font barlow (float h, bool bold = true, float kern = 0.05f) { return Fonts::make (bold ? Fonts::get().barlowBold : Fonts::get().barlowSemi, h, kern); }

static const KnobSkin fxKnob { C (0xff5a5a5a), C (0xff181818), C (0xff050505), 0.55f, C (0xff222222), C (0xffffffff), C (0xffcfcac0), C (0xffe9e7e1) };

const Skin& Skin::forPreset (int i)
{
    static const std::vector<Skin> S = []
    {
        auto& F = Fonts::get();
        std::vector<Skin> s (4);
        s[0] = { C (0xff131313), C (0xff8fd6c2), C (0xffbfc4c4), C (0xffecebe6), C (0xfff1f0ea),
                 { {0.f, C (0xff1d1d1d)}, {1.f, C (0xff090909)} },
                 { C (0xff555555), C (0xff151515), C (0xff050505), 0.55f, C (0xff1b1b1b), C (0xfff4f4f0), C (0xffecebe6), C (0xffecebe6) },
                 F.yellowtail, 57.f, 0.f, 0 };
        s[1] = { C (0xff071220), C (0xffbfe6ff), C (0xff2e5274), C (0xffd6eaff), C (0xffe8f4ff),
                 { {0.f, C (0xff1c3a58)}, {1.f, C (0xff0e2136)} },
                 { C (0xffffffff), C (0xffa9b6c2), C (0xff56626e), 0.45f, C (0xff0a1726), C (0xff0b1a2a), C (0xff9cc6ea), C (0xffd6eaff) },
                 F.josefin, 42.f, 0.3f, 1 };
        s[2] = { C (0xff16120e), C (0xffff7a2e), C (0xffe9e1cf), C (0xff1d160c), C (0xff1d160c),
                 { {0.f, C (0xffe7c873)}, {0.6f, C (0xffc59c45)}, {1.f, C (0xffa7802f)} },
                 { C (0xfffbe7a6), C (0xffc99d40), C (0xff7b5a1c), 0.5f, C (0xff111111), C (0xff111111), C (0xff1d160c), C (0xff1d160c) },
                 F.archivo, 42.f, 0.01f, 2 };
        s[3] = { C (0xff0a0a0a), C (0xffff2a44), C (0xff3a3a3a), C (0xff0d0e10), C (0xff0d0e10),
                 { {0.f, C (0xfffdfefe)}, {0.16f, C (0xffc9ced2)}, {0.46f, C (0xff8c949a)}, {0.53f, C (0xffeef0f2)}, {0.82f, C (0xffa8aeb3)}, {1.f, C (0xffdfe2e5)} },
                 { C (0xff4a4a4a), C (0xff0c0c0c), C (0xff000000), 0.55f, C (0xff000000), C (0xfff2f2f2), C (0xff0d0e10), C (0xff0d0e10) },
                 F.michroma, 34.f, 0.05f, 3 };
        return s;
    }();
    return S[(size_t) juce::jlimit (0, 3, i)];
}

//==============================================================================
Knob::Knob (const juce::String& l, bool sm, bool m) : label (l), small (sm), ms (m)
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    setMouseDragSensitivity (200);
    setVelocityBasedMode (false);
    setScrollWheelEnabled (true);
    setTitle (l);
}

void Knob::paint (juce::Graphics& g)
{
    const float D = (float) dial(), cx = getWidth() * 0.5f, cy = 8.0f + D * 0.5f;
    const float a0 = juce::MathConstants<float>::pi * 1.25f, span = juce::MathConstants<float>::pi * 1.5f;
    const float prop = (float) valueToProportionOfLength (getValue());

    // tick ring: 11 ticks over 270 degrees
    g.setColour (skin.tick.withAlpha (0.85f));
    const float rIn = D * 0.5f + 8 - (D + 16) * 0.5f * 0.14f, rOut = D * 0.5f + 8;
    for (int i = 0; i <= 10; ++i)
    {
        const float a = a0 + span * (float) i / 10.0f, w = juce::degreesToRadians (2.4f);
        juce::Path p; p.addCentredArc (cx, cy, rOut, rOut, 0, a - w * 0.5f, a + w * 0.5f, true);
        p.addCentredArc (cx, cy, rIn, rIn, 0, a + w * 0.5f, a - w * 0.5f, false); p.closeSubPath();
        g.fillPath (p);
    }
    // skirt + shadow
    auto r = juce::Rectangle<float> (D, D).withCentre ({ cx, cy });
    g.setColour (juce::Colours::black.withAlpha (0.65f)); g.fillEllipse (r.translated (0, 5).expanded (2));
    g.setColour (skin.skirt); g.fillEllipse (r.expanded (4));
    juce::ColourGradient cg (skin.c0, r.getX() + D * 0.36f, r.getY() + D * 0.30f, skin.c2, r.getX() + D * 0.36f + D * 0.75f, r.getY() + D * 0.30f, true);
    cg.addColour (skin.s1, skin.c1);
    g.setGradientFill (cg); g.fillEllipse (r);
    g.setColour (juce::Colours::white.withAlpha (0.25f)); g.drawEllipse (r.reduced (0.5f), 1.0f);
    // pointer
    const float ang = a0 + span * prop;
    juce::Path ptr; const float pw = 3.0f, ph = D * 0.38f, top = small ? 4.0f : 5.0f;
    ptr.addRoundedRectangle (-pw * 0.5f, -D * 0.5f + top, pw, ph, 1.5f);
    g.setColour (skin.ptr); g.fillPath (ptr, juce::AffineTransform::rotation (ang).translated (cx, cy));
    if (hasKeyboardFocus (false)) { g.setColour (skin.ink.withAlpha (0.6f)); g.drawEllipse (r.expanded (10), 1.5f); }

    // label + value
    const float ly = 8 + D + 8 + 6;
    g.setColour (skin.ink); g.setFont (barlow (14));
    g.drawText (label, juce::Rectangle<float> (0, ly, (float) getWidth(), 16), juce::Justification::centred, false);
    g.setColour (skin.ink.withAlpha (isMouseButtonDown() ? 1.0f : 0.6f)); g.setFont (barlow (13, false, 0));
    const auto v = getValue();
    g.drawText (ms ? juce::String (juce::roundToInt (v)) + " ms" : juce::String (v, 1),
                juce::Rectangle<float> (0, ly + 19, (float) getWidth(), 15), juce::Justification::centred, false);
}

void Toggle::paintButton (juce::Graphics& g, bool, bool)
{
    const bool on = getToggleState();
    auto tr = juce::Rectangle<float> (8, (getHeight() - 20) * 0.5f, 34, 20);
    g.setColour (on ? accent : juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (tr, 10);
    g.setColour (on ? juce::Colours::white : C (0xffbbbbbb));
    g.fillEllipse (tr.getX() + 2 + (on ? 14 : 0), tr.getY() + 2, 16, 16);
    g.setColour (ink); g.setFont (barlow (14));
    g.drawText (getButtonText(), juce::Rectangle<float> (50, 0, getWidth() - 50.0f, (float) getHeight()), juce::Justification::centredLeft, false);
}

void Stomp::paintButton (juce::Graphics& g, bool over, bool)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colours::white.withAlpha (over ? 0.11f : 0.07f)); g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    auto led = juce::Rectangle<float> (12, (r.getHeight() - 10) * 0.5f, 10, 10);
    if (getToggleState())
    {
        g.setColour (accent.withAlpha (0.35f)); g.fillEllipse (led.expanded (5));
        g.setColour (accent); g.fillEllipse (led);
    }
    else { g.setColour (C (0xff3a3a3a)); g.fillEllipse (led); }
    g.setColour (C (0xffe9e7e1)); g.setFont (barlow (18, true, 0.04f));
    g.drawText (getButtonText(), r.withTrimmedLeft (31).withTrimmedRight (12), juce::Justification::centredLeft, false);
}

void Pill::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat();
    juce::Colour bg = go || active ? accent : juce::Colours::white.withAlpha (down ? 0.22f : over ? 0.16f : 0.10f);
    if ((go || active) && (down || over)) bg = bg.brighter (0.1f);
    g.setColour (bg); g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (go || active ? C (0xff0a0a0a) : C (0xfff3f1ec));
    g.setFont (Fonts::make (Fonts::get().heebo, fontH).boldened());
    g.drawText (getButtonText(), r.reduced (10, 0), juce::Justification::centred, false);
}

FootSwitch::FootSwitch (int i) : juce::Button (presets()[(size_t) i].fs), idx (i) {}

void FootSwitch::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto& p = presets()[(size_t) idx];
    const auto acc = C (p.accent);
    auto r = getLocalBounds().toFloat();
    juce::ColourGradient bg (C (0xff1f1f1f), 0, 0, C (0xff0e0e0e), 0, r.getHeight(), false);
    g.setColour (juce::Colours::black); g.fillRoundedRectangle (r.translated (0, 2), 10);
    g.setGradientFill (bg); g.fillRoundedRectangle (r, 10);
    g.setColour (juce::Colours::white.withAlpha (over ? 0.12f : 0.07f)); g.drawRoundedRectangle (r.reduced (0.5f), 10, 1);
    const float cx = r.getCentreX();
    auto led = juce::Rectangle<float> (12, 12).withCentre ({ cx, 14 });
    if (on) { g.setColour (acc.withAlpha (0.3f)); g.fillEllipse (led.expanded (7)); g.setColour (acc); }
    else g.setColour (C (0xff3a3a3a));
    g.fillEllipse (led);
    g.setColour (on ? acc : C (0xfff3f1ec)); g.setFont (barlow (19));
    g.drawText (p.fs, juce::Rectangle<float> (0, 22, r.getWidth(), 20), juce::Justification::centred, false);
    g.setColour (C (0xffb4b0a7)); g.setFont (heebo (13));
    g.drawText (U (p.he), juce::Rectangle<float> (0, 41, r.getWidth(), 16), juce::Justification::centred, false);
    auto dome = juce::Rectangle<float> (38, 38).withCentre ({ cx, 80 + (down ? 2.0f : 0.0f) });
    g.setColour (C (0xff4a4a4a)); g.fillEllipse (dome.expanded (5));
    g.setColour (C (0xff0a0a0a)); g.fillEllipse (dome.expanded (4));
    juce::ColourGradient dg (juce::Colours::white, dome.getX() + 38 * 0.4f, dome.getY() + 38 * 0.34f, C (0xff2e2e2e), dome.getX() + 38 * 0.4f + 30, dome.getY() + 38 * 0.34f, true);
    dg.addColour (0.26, C (0xffc9c9c9)); dg.addColour (0.68, C (0xff6a6a6a));
    g.setGradientFill (dg); g.fillEllipse (dome);
}

//==============================================================================
FxUnit::FxUnit (BackupRigProcessor& p, int preset, int fi)
{
    const auto& P = presets()[(size_t) preset];
    const auto& f = P.fx[(size_t) fi];
    label = f.label; fixed = f.fixed;
    const auto acc = C (P.accent);
    if (! fixed)
    {
        sw = std::make_unique<Stomp> (f.label); sw->accent = acc; addAndMakeVisible (*sw);
        sa = std::make_unique<juce::ButtonParameterAttachment> (*p.vts.getParameter (swId (P.id, f.id)), *sw);
        sw->onStateChange = [this] { repaint(); for (auto& k : knobs) k->setAlpha (isOn() ? 1.0f : 0.4f); };
    }
    if (f.tap > 0)
    {
        tap = std::make_unique<Pill> ("TAP"); tap->accent = acc; addAndMakeVisible (*tap);
        auto* timeParam = p.vts.getParameter (knobId (P.id, "time"));
        const float factor = f.tap;
        tap->onClick = [this, timeParam, factor]
        {
            const double now = juce::Time::getMillisecondCounterHiRes();
            if (! taps.empty() && now - taps.back() > 2000) taps.clear();
            taps.push_back (now); if (taps.size() > 4) taps.erase (taps.begin());
            tap->active = true; tap->repaint();
            juce::Timer::callAfterDelay (90, [sp = juce::Component::SafePointer<Pill> (tap.get())] { if (sp) { sp->active = false; sp->repaint(); } });
            if (taps.size() > 1 && timeParam != nullptr)
            {
                const double iv = (taps.back() - taps.front()) / (double) (taps.size() - 1);
                const float ms = juce::jlimit (80.0f, 1200.0f, (float) std::round (iv * factor));
                timeParam->setValueNotifyingHost (timeParam->convertTo0to1 (ms));
            }
        };
    }
    for (auto& k : f.k)
    {
        auto kn = std::make_unique<Knob> (k.label, true, k.ms); kn->skin = fxKnob;
        auto* prm = p.vts.getParameter (knobId (P.id, k.key));
        kn->setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
        ka.push_back (std::make_unique<juce::SliderParameterAttachment> (*prm, *kn));
        addAndMakeVisible (*kn); knobs.push_back (std::move (kn));
    }
    for (auto& k : knobs) k->setAlpha (isOn() ? 1.0f : 0.4f);
}

void FxUnit::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillRoundedRectangle (r.translated (0, 8).reduced (4, 0), 12);
    juce::ColourGradient bg (C (0xf01e1e1e), 0, 0, C (0xf00c0c0c), 0, r.getHeight(), false);
    g.setGradientFill (bg); g.fillRoundedRectangle (r, 12);
    g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawHorizontalLine (1, 12, r.getWidth() - 12);
    if (fixed) { g.setColour (C (0xffe9e7e1)); g.setFont (barlow (18, true, 0.04f)); g.drawText (label, 16, 8, 200, 44, juce::Justification::centredLeft); }
}

void FxUnit::resized()
{
    if (sw) sw->setBounds (10, 8, juce::jmax (110, (int) juce::GlyphArrangement::getStringWidth (barlow (18), label) + 50), 44);
    if (tap) tap->setBounds (getWidth() - 10 - 70, 8, 70, 44);
    const int n = (int) knobs.size(), kw = 84, kh = Knob::heightFor (true);
    const int y = 58, avail = getWidth() - 20;
    const float gap = (float) (avail - n * kw) / (float) n; // space-around
    for (int i = 0; i < n; ++i) knobs[(size_t) i]->setBounds (10 + (int) (gap * 0.5f + i * (kw + gap)), y, kw, kh);
}

//==============================================================================
Tuner::Tuner() { addAndMakeVisible (close); close.go = true; close.fontH = 17; }

void Tuner::resized()
{
    box = juce::Rectangle<int> (460, 380).withCentre (getLocalBounds().getCentre());
    close.setBounds (box.getX() + 20, box.getBottom() - 72, box.getWidth() - 40, 52);
}

void Tuner::update (float hz, float c, bool o)
{
    static const char* N[] = { "C","C\xe2\x99\xaf","D","D\xe2\x99\xaf","E","F","F\xe2\x99\xaf","G","G\xe2\x99\xaf","A","A\xe2\x99\xaf","B" };
    const float m = 69 + 12 * std::log2 (hz / 440.0f); const int r = juce::roundToInt (m);
    note = U (N[((r % 12) + 12) % 12]); ok = o; needle = juce::jlimit (2.0f, 98.0f, 50 + c);
    info = juce::String (hz, 1) + " Hz   " + (c > 0 ? "+" : "") + juce::String (juce::roundToInt (c)) + " cent";
    repaint();
}

void Tuner::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.72f));
    auto b = box.toFloat();
    g.setColour (C (0xff0e0e0e)); g.fillRoundedRectangle (b, 20);
    g.setColour (C (0xff2a2a2a)); g.drawRoundedRectangle (b, 20, 1);
    const auto green = C (0xff3ddc84);
    g.setColour (ok ? green : C (0xffdddddd)); g.setFont (barlow (140, true, 0));
    g.drawText (note, b.withHeight (170).translated (0, 16), juce::Justification::centred, false);
    auto sc = juce::Rectangle<float> (b.getX() + 20, b.getY() + 196, b.getWidth() - 40, 56);
    g.setColour (C (0xff151515)); g.fillRoundedRectangle (sc, 8);
    g.setColour (C (0xff3a3a3a));
    for (int i = 0; i < 10; ++i) g.fillRect (sc.getX() + sc.getWidth() * i / 10.0f, sc.getY(), 2.0f, sc.getHeight());
    g.setColour (green); g.fillRect (sc.getCentreX() - 1, sc.getY(), 2.0f, sc.getHeight());
    g.setColour (ok ? green : C (0xffffb020));
    g.fillRoundedRectangle (sc.getX() + sc.getWidth() * needle / 100.0f - 3, sc.getY() - 4, 6, sc.getHeight() + 8, 3);
    g.setColour (C (0xffb4b0a7)); g.setFont (heebo (14));
    g.drawText (info, juce::Rectangle<float> (sc.getX(), sc.getBottom() + 10, 220, 20), juce::Justification::centredLeft, false);
    g.drawText (U (u8"הפלט מושתק בזמן כיוון"), juce::Rectangle<float> (sc.getRight() - 220, sc.getBottom() + 10, 220, 20), juce::Justification::centredRight, false);
}

//==============================================================================
Settings::Settings (BackupRigProcessor& p)
    : proc (p),
      qFast (U (u8"מהיר")), qHq (U (u8"איכות")), oSt (U (u8"סטריאו")), oMono (U (u8"מונו")),
      c12 ("1+2"), c1 ("1"), c2 ("2"), calib (U (u8"כיול כניסה")), resetP (U (u8"איפוס הפריסט")), closeB (U (u8"סגור"))
{
    for (auto* s : { &inS, &gateS, &outS })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::TextBoxRight, true, 70, 28);
        s->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s->setColour (juce::Slider::textBoxTextColourId, C (0xffb4b0a7));
        s->setColour (juce::Slider::trackColourId, C (0xff8fd6c2));
        s->setColour (juce::Slider::thumbColourId, juce::Colours::white);
        s->setColour (juce::Slider::backgroundColourId, C (0xff333333));
        addAndMakeVisible (*s);
    }
    ia = std::make_unique<juce::SliderParameterAttachment> (*p.vts.getParameter (gid::inDb), inS);
    ga = std::make_unique<juce::SliderParameterAttachment> (*p.vts.getParameter (gid::gate), gateS);
    oa = std::make_unique<juce::SliderParameterAttachment> (*p.vts.getParameter (gid::outDb), outS);

    auto setParam = [this] (const char* id, float v) { if (auto* q = proc.vts.getParameter (id)) q->setValueNotifyingHost (q->convertTo0to1 (v)); refresh(); };
    qFast.onClick = [=] { setParam (gid::hq, 0); };  qHq.onClick = [=] { setParam (gid::hq, 1); };
    oSt.onClick = [=] { setParam (gid::mono, 0); }; oMono.onClick = [=] { setParam (gid::mono, 1); };
    c12.onClick = [=] { setParam (gid::inCh, 0); }; c1.onClick = [=] { setParam (gid::inCh, 1); }; c2.onClick = [=] { setParam (gid::inCh, 2); };
    calib.go = true; calib.onClick = [this] { if (onCalib) onCalib(); };
    resetP.onClick = [this] { if (onReset) onReset(); };
    closeB.onClick = [this] { if (onClose) onClose(); };
    for (auto* b : { &qFast, &qHq, &oSt, &oMono, &c12, &c1, &c2, &calib, &resetP, &closeB }) { b->fontH = 15; addAndMakeVisible (*b); }
    refresh();
}

void Settings::refresh()
{
    const bool hq = proc.vts.getRawParameterValue (gid::hq)->load() > 0.5f;
    const bool mono = proc.vts.getRawParameterValue (gid::mono)->load() > 0.5f;
    const int ch = (int) proc.vts.getRawParameterValue (gid::inCh)->load();
    qFast.active = ! hq; qHq.active = hq; oSt.active = ! mono; oMono.active = mono;
    c12.active = ch == 0; c1.active = ch == 1; c2.active = ch == 2;
    const auto acc = Skin::forPreset ((int) proc.vts.getRawParameterValue (gid::preset)->load()).accent;
    for (auto* b : { &qFast, &qHq, &oSt, &oMono, &c12, &c1, &c2, &calib }) { b->accent = acc; b->repaint(); }
    for (auto* s : { &inS, &gateS, &outS }) s->setColour (juce::Slider::trackColourId, acc);
    repaint();
}

void Settings::mouseDown (const juce::MouseEvent& e)
{
    if (! box.contains (e.getPosition()) && onClose) onClose();
}

static const int kRowY0 = 70, kRowH = 52;

void Settings::resized()
{
    box = juce::Rectangle<int> (600, 620).withCentre (getLocalBounds().getCentre());
    const int x0 = box.getX() + 20, wL = 130, ctlX = x0, ctlW = box.getWidth() - 40 - wL;
    auto row = [&] (int i) { return juce::Rectangle<int> (ctlX, box.getY() + kRowY0 + i * kRowH + 4, ctlW, kRowH - 8); };
    inS.setBounds (row (0)); gateS.setBounds (row (1)); outS.setBounds (row (2));
    auto seg = [&] (int i, std::initializer_list<Pill*> bs)
    {
        auto r = row (i); const int n = (int) bs.size(), w = (r.getWidth() - (n - 1) * 6) / n; int k = 0;
        for (auto* b : bs) b->setBounds (r.getRight() - w - k++ * (w + 6), r.getY(), w, r.getHeight());
    };
    seg (3, { &qFast, &qHq }); seg (4, { &oSt, &oMono }); seg (5, { &c12, &c1, &c2 });
    const int by = box.getY() + kRowY0 + 6 * kRowH + 14;
    closeB.setBounds (x0, by, 110, 50); resetP.setBounds (x0 + 120, by, 170, 50); calib.setBounds (box.getRight() - 20 - 170, by, 170, 50);
}

void Settings::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.72f));
    auto b = box.toFloat();
    g.setColour (C (0xff181818)); g.fillRoundedRectangle (b, 18);
    g.setColour (C (0xfff3f1ec)); g.setFont (heebo (22).boldened());
    g.drawText (U (u8"הגדרות הריג"), b.withHeight (56).reduced (20, 0).translated (0, 10), juce::Justification::centredRight, false);
    const char* labels[] = { u8"כניסה", u8"נויז גייט", u8"יציאה", u8"מצב עיבוד", u8"יציאה", u8"ערוץ הגיטרה" };
    for (int i = 0; i < 6; ++i)
    {
        auto r = juce::Rectangle<float> (b.getRight() - 20 - 125, b.getY() + kRowY0 + i * kRowH, 125, (float) kRowH);
        g.setColour (C (0xfff3f1ec)); g.setFont (heebo (16).boldened());
        g.drawText (U (labels[i]), r, juce::Justification::centredRight, false);
        g.setColour (C (0xff2a2a2a)); g.drawHorizontalLine ((int) r.getBottom(), b.getX() + 20, b.getRight() - 20);
    }
    // notes
    juce::String notes = calibMsg.isNotEmpty() ? calibMsg + "\n\n" : juce::String();
    notes << U (u8"• בפלאגין בתוך DAW: שמים את הריג על ערוץ גיטרה (מונו או סטריאו). ב'ערוץ הגיטרה' בוחרים מאיזה ערוץ כניסה לקחת את הגיטרה.\n"
                u8"• באפליקציה העצמאית: Options > Audio Settings, בוחרים כרטיס קול, ASIO ב-Windows וגודל באפר 64 או 128.\n"
                u8"• 'כיול כניסה': מנגנים חזק שלוש שניות והריג מכוון את עוצמת הכניסה.\n"
                u8"• 'מהיר' חוסך עיבוד, 'איכות' נקי יותר בתווים גבוהים בליד.\n"
                u8"• יוצאים בכבל אחד למגבר או ל-DI? בוחרים מונו.\n"
                u8"• מקלדת: 1 עד 4 בוחרים פריסט, חצים מדלגים, T פותח טיונר.");
    juce::AttributedString as; as.setReadingDirection (juce::AttributedString::rightToLeft);
    as.setJustification (juce::Justification::topRight);
    as.append (notes, heebo (14.5f), C (0xffb4b0a7));
    as.setLineSpacing (3);
    juce::TextLayout tl; tl.createLayout (as, b.getWidth() - 40);
    tl.draw (g, juce::Rectangle<float> (b.getX() + 20, b.getY() + kRowY0 + 6 * kRowH + 80, b.getWidth() - 40, 200));
}

//==============================================================================
MainView::MainView (BackupRigProcessor& p)
    : proc (p), tunerB (U (u8"טיונר")), setB (U (u8"הגדרות")), demoB (U (u8"ריף דמו")), settings (p)
{
    setWantsKeyboardFocus (true);
    for (auto* b : { &tunerB, &setB, &demoB }) addAndMakeVisible (*b);
    tunerB.onClick = [this] { openTuner (true); };
    setB.onClick = [this] { settings.refresh(); settings.setVisible (true); settings.toFront (false); };
    demoB.onClick = [this] { proc.demoOn.store (! proc.demoOn.load()); demoB.active = proc.demoOn.load(); demoB.repaint(); };
    demoB.active = proc.demoOn.load();
    for (int i = 0; i < 4; ++i)
    {
        fs[(size_t) i] = std::make_unique<FootSwitch> (i);
        fs[(size_t) i]->onClick = [this, i] { selectPreset (i); };
        addAndMakeVisible (*fs[(size_t) i]);
    }
    addChildComponent (tuner); addChildComponent (settings);
    tuner.close.onClick = [this] { openTuner (false); };
    settings.onClose = [this] { settings.setVisible (false); grabKeyboardFocus(); };
    settings.onReset = [this] { proc.resetPreset (cur); };
    settings.onCalib = [this]
    {
        calPk = 0; calibUntil = juce::Time::getMillisecondCounterHiRes() + 3000;
        settings.calibMsg = U (u8"נגן חזק…"); settings.repaint();
    };
    setSize (W, H);
    rebuild();
    startTimerHz (30);
}

MainView::~MainView() { proc.setTunerOpen (false); }

void MainView::selectPreset (int i)
{
    if (auto* q = proc.vts.getParameter (gid::preset)) q->setValueNotifyingHost (q->convertTo0to1 ((float) i));
    rebuild();
}

void MainView::openTuner (bool on)
{
    tunerOn = on; tuner.setVisible (on); if (on) tuner.toFront (false);
    proc.setTunerOpen (on);
    if (! on) grabKeyboardFocus();
}

bool MainView::keyPressed (const juce::KeyPress& k)
{
    const auto c = k.getTextCharacter();
    if (c >= '1' && c <= '4') { selectPreset (c - '1'); return true; }
    if (c == 't' || c == 'T') { openTuner (! tunerOn); return true; }
    if (k == juce::KeyPress::escapeKey) { openTuner (false); settings.setVisible (false); return true; }
    if (k == juce::KeyPress::rightKey || k == juce::KeyPress::downKey || k == juce::KeyPress::pageDownKey) { selectPreset ((cur + 1) % 4); return true; }
    if (k == juce::KeyPress::leftKey || k == juce::KeyPress::upKey || k == juce::KeyPress::pageUpKey) { selectPreset ((cur + 3) % 4); return true; }
    return false;
}

void MainView::rebuild()
{
    const int id = juce::jlimit (0, 3, (int) proc.vts.getRawParameterValue (gid::preset)->load());
    if (id == cur && ! ampKnobs.empty()) return;
    cur = id;
    const auto& P = presets()[(size_t) id];
    const auto& sk = Skin::forPreset (id);

    ampKa.clear(); ampKnobs.clear(); ampSa.clear(); ampSw.clear(); units.clear();
    for (auto& k : P.amp)
    {
        auto kn = std::make_unique<Knob> (k.label, false, k.ms); kn->skin = sk.knob;
        auto* prm = proc.vts.getParameter (knobId (P.id, k.key));
        kn->setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
        ampKa.push_back (std::make_unique<juce::SliderParameterAttachment> (*prm, *kn));
        addAndMakeVisible (*kn); ampKnobs.push_back (std::move (kn));
    }
    for (auto& w : P.sw)
    {
        auto t = std::make_unique<Toggle> (w.label); t->ink = sk.plateInk; t->accent = sk.accent;
        ampSa.push_back (std::make_unique<juce::ButtonParameterAttachment> (*proc.vts.getParameter (swId (P.id, w.key)), *t));
        addAndMakeVisible (*t); ampSw.push_back (std::move (t));
    }
    for (int i = 0; i < (int) P.fx.size(); ++i)
    {
        auto u = std::make_unique<FxUnit> (proc, id, i);
        addAndMakeVisible (*u); units.push_back (std::move (u));
    }
    for (int i = 0; i < 4; ++i) { fs[(size_t) i]->on = i == id; fs[(size_t) i]->repaint(); }
    for (auto* b : { &tunerB, &setB, &demoB }) b->accent = sk.accent;
    tuner.close.accent = sk.accent;
    settings.refresh();
    resized();
    tuner.toFront (false); settings.toFront (false);
    repaint();
}

void MainView::resized()
{
    if (cur < 0 || ampKnobs.empty()) return;
    const auto& P = presets()[(size_t) cur];
    // top bar (RTL: brand on the right)
    const int topH = 56;
    demoB.setBounds (16, 6, 110, 44); setB.setBounds (134, 6, 100, 44); tunerB.setBounds (242, 6, 90, 44);
    pillR = { W - 16 - 110 - 130, 6, 120, 44 };
    meterR = { 346, 23, pillR.getX() - 14 - 346, 10 };

    // amp head
    const int kh = Knob::heightFor (false);
    const bool hasSw = ! P.sw.empty();
    const int plateH = 14 + 64 + 16 + kh + (hasSw ? 56 : 0) + 16;
    headR = { 16, topH + 12, W - 32, plateH + 24 };
    plateR = headR.reduced (12);
    const int n = (int) ampKnobs.size(), kw = 96, avail = plateR.getWidth() - 24;
    const int colW = avail / n;
    for (int i = 0; i < n; ++i)
        ampKnobs[(size_t) i]->setBounds (plateR.getX() + 12 + i * colW + (colW - kw) / 2, plateR.getY() + 14 + 64 + 16, kw, kh);
    for (int i = 0; i < (int) ampSw.size(); ++i)
        ampSw[(size_t) i]->setBounds (plateR.getX() + 12 + i * 130, plateR.getY() + 14 + 64 + 16 + kh + 12, 120, 44);

    // board
    boardR = { 0, H - 122, W, 122 };
    const int fw = (W - 20 - 3 * 8) / 4;
    for (int i = 0; i < 4; ++i) fs[(size_t) i]->setBounds (10 + i * (fw + 8), boardR.getY() + 10, fw, 106);

    // grille: blurb + units
    grilleR = { 0, headR.getBottom() + 18, W, boardR.getY() - headR.getBottom() - 18 };
    {
        juce::AttributedString as; as.setReadingDirection (juce::AttributedString::rightToLeft);
        as.append (U (P.blurb), heebo (15), C (0xffeeece6)); as.setLineSpacing (2);
        juce::TextLayout tl; const float bw = juce::jmin (700.0f, W - 32.0f);
        tl.createLayout (as, bw - 28);
        const int bh = (int) std::ceil (tl.getHeight()) + 24;
        blurbR = { W - 16 - (int) bw, grilleR.getY() + 18, (int) bw, bh };
    }
    const int un = (int) units.size(), gap = 12, uw = (W - 32 - (un - 1) * gap) / juce::jmax (1, un);
    const int uh = 58 + Knob::heightFor (true) + 8;
    for (int i = 0; i < un; ++i) units[(size_t) i]->setBounds (16 + i * (uw + gap), blurbR.getBottom() + 14, uw, uh);

    tuner.setBounds (getLocalBounds()); settings.setBounds (getLocalBounds());
    renderGrille();
}

void MainView::renderGrille()
{
    if (grilleR.isEmpty()) return;
    const int w = grilleR.getWidth(), h = grilleR.getHeight();
    grilleImg = juce::Image (juce::Image::ARGB, w, h, true);
    juce::Graphics g (grilleImg);
    auto diag = [&] (juce::Colour c, float th, float step, bool up)
    {
        g.setColour (c);
        for (float s = -(float) h; s < (float) w + h; s += step)
            up ? g.drawLine (s, (float) h, s + h, 0, th) : g.drawLine (s, 0, s + h, (float) h, th);
    };
    auto dots = [&] (juce::Colour c, float r, float ox, float oy, float sx, float sy)
    {
        g.setColour (c);
        for (float y = oy; y < h; y += sy) for (float x = ox; x < w; x += sx) g.fillEllipse (x - r, y - r, 2 * r, 2 * r);
    };
    switch (Skin::forPreset (cur).grille)
    {
        case 0:
            g.setGradientFill (juce::ColourGradient (C (0xffb3bcbe), 0, 0, C (0xff8f989b), 0, (float) h, false)); g.fillAll();
            diag (juce::Colour (40, 48, 50).withAlpha (0.55f), 1.0f, 4 * 1.414f, false);
            diag (juce::Colours::white.withAlpha (0.22f), 1.0f, 4 * 1.414f, true);
            dots (juce::Colours::white.withAlpha (0.55f), 1.0f, 3, 5, 23, 19);
            dots (juce::Colour (150, 235, 220).withAlpha (0.5f), 1.0f, 11, 2, 31, 29);
            break;
        case 1:
        {
            juce::ColourGradient rg (C (0xff2b6a93), w * 0.5f, h * 1.18f, C (0xff050c16), w * 0.5f - w * 0.7f, h * 1.18f, true);
            rg.addColour (0.52, C (0xff0f2a42));
            g.setGradientFill (rg); g.fillAll();
            dots (juce::Colours::white, 1.0f, 0, 0, 97, 89);
            dots (juce::Colours::white.withAlpha (0.75f), 0.7f, 40, 20, 61, 53);
            dots (juce::Colour (191, 230, 255).withAlpha (0.85f), 1.1f, 10, 50, 151, 137);
            dots (juce::Colours::white.withAlpha (0.5f), 0.6f, 25, 70, 37, 41);
            break;
        }
        case 2:
            for (int y = 0; y < h; y += 6) for (int x = 0; x < w; x += 6)
            { g.setColour (((x / 6 + y / 6) & 1) ? C (0xff383128) : C (0xff5f5547)); g.fillRect (x, y, 6, 6); }
            g.setColour (juce::Colours::black.withAlpha (0.28f)); for (int y = 0; y < h; y += 6) g.fillRect (0, y, w, 2);
            g.setColour (juce::Colour (255, 240, 210).withAlpha (0.09f)); for (int x = 0; x < w; x += 6) g.fillRect (x, 0, 2, h);
            break;
        default:
            g.fillAll (C (0xff1d1d1d));
            diag (C (0xff141414), 2.0f, 4 * 1.414f, false);
            diag (juce::Colours::white.withAlpha (0.06f), 1.0f, 5 * 1.414f, true);
            break;
    }
    // inner top shadow
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.5f), 0, 0, juce::Colours::transparentBlack, 0, 28, false));
    g.fillRect (0, 0, w, 28);
}

void MainView::paint (juce::Graphics& g)
{
    if (cur < 0) return;
    const auto& P = presets()[(size_t) cur];
    const auto& sk = Skin::forPreset (cur);
    g.fillAll (sk.tolex);

    // top bar
    g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillRect (0, 0, W, 56);
    g.setColour (C (0xfff3f1ec)); g.setFont (heebo (17).boldened());
    g.drawText (U (u8"ריג גיבוי"), juce::Rectangle<int> (W - 16 - 110, 6, 110, 44), juce::Justification::centredRight, false);
    {
        const bool running = runTicks > 0;
        const bool demo = proc.demoOn.load();
        auto dot = juce::Rectangle<float> (9, 9).withCentre ({ (float) pillR.getRight() - 6, (float) pillR.getCentreY() });
        if (running) { g.setColour (C (0xff3ddc84).withAlpha (0.35f)); g.fillEllipse (dot.expanded (4)); g.setColour (C (0xff3ddc84)); }
        else g.setColour (C (0xff555555));
        g.fillEllipse (dot);
        g.setColour (C (0xffb4b0a7)); g.setFont (heebo (14));
        g.drawText (! running ? U (u8"כבוי") : demo ? U (u8"דמו") : U (u8"בלייב"), pillR.withTrimmedRight (18), juce::Justification::centredRight, false);
    }
    {
        auto m = meterR.toFloat();
        g.setColour (juce::Colours::black); g.fillRoundedRectangle (m, 5);
        const float db = 20 * std::log10 (level + 1e-9f), f = juce::jlimit (0.0f, 1.0f, (db + 60) / 60);
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (juce::Rectangle<float> (m.getX(), m.getY(), m.getWidth() * f, m.getHeight()).toNearestInt());
        juce::ColourGradient mg (C (0xff3ddc84), m.getX(), 0, C (0xffff3b30), m.getRight(), 0, false);
        mg.addColour (0.679, C (0xff3ddc84)); mg.addColour (0.68, C (0xffffd23f)); mg.addColour (0.879, C (0xffffd23f)); mg.addColour (0.88, C (0xffff3b30));
        g.setGradientFill (mg); g.fillRoundedRectangle (m, 5);
    }
    {
        auto m = meterR.toFloat(); juce::ignoreUnused (m);
        g.setColour (juce::Colours::white.withAlpha (0.1f)); g.drawRoundedRectangle (meterR.toFloat(), 5, 1);
    }

    // head (tolex) + faceplate
    auto hr = headR.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (hr.translated (0, 14).reduced (6, 0), 14);
    g.setColour (sk.tolex.brighter (0.15f)); g.fillRoundedRectangle (hr, 14);
    g.setColour (sk.tolex); g.fillRoundedRectangle (hr.reduced (1), 13);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    for (float y = hr.getY() + 2; y < hr.getBottom() - 2; y += 4) for (float x = hr.getX() + 2; x < hr.getRight() - 2; x += 4) g.fillRect (x, y, 1.0f, 1.0f);
    auto pr = plateR.toFloat();
    g.setColour (sk.pipe); g.fillRoundedRectangle (pr.expanded (2), 8);
    {
        juce::ColourGradient pg (sk.plate.front().second, 0, pr.getY(), sk.plate.back().second, 0, pr.getBottom(), false);
        for (size_t i = 1; i + 1 < sk.plate.size(); ++i) pg.addColour (sk.plate[i].first, sk.plate[i].second);
        g.setGradientFill (pg); g.fillRoundedRectangle (pr, 6);
        if (sk.grille == 2)
        {
            g.setColour (juce::Colours::white.withAlpha (0.08f));
            for (float x = pr.getX(); x < pr.getRight(); x += 3) g.fillRect (x, pr.getY(), 1.0f, pr.getHeight());
        }
        g.setColour (juce::Colours::white.withAlpha (0.4f)); g.drawHorizontalLine ((int) pr.getY() + 1, pr.getX() + 6, pr.getRight() - 6);
        g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, 0, pr.getBottom() - 8, juce::Colours::black.withAlpha (0.35f), 0, pr.getBottom(), false));
        g.fillRoundedRectangle (pr.withTop (pr.getBottom() - 8), 6);
    }
    // nameplate
    {
        auto np = juce::Rectangle<float> (pr.getX() + 12, pr.getY() + 14, pr.getWidth() - 24, 64);
        auto jewel = juce::Rectangle<float> (24, 24).withCentre ({ np.getRight() - 12, np.getCentreY() });
        const bool running = runTicks > 0;
        if (running) { g.setColour (sk.accent.withAlpha (0.35f)); g.fillEllipse (jewel.expanded (10)); }
        g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillEllipse (jewel.expanded (3));
        g.setColour (juce::Colours::white.withAlpha (0.15f)); g.drawEllipse (jewel.expanded (3.5f), 1);
        juce::ColourGradient jg (juce::Colours::white, jewel.getX() + 8.4f, jewel.getY() + 7.7f, juce::Colours::black, jewel.getX() + 8.4f + 26, jewel.getY() + 7.7f, true);
        jg.addColour (0.08, juce::Colours::white); jg.addColour (0.34, sk.accent);
        g.setOpacity (1.0f);
        g.setGradientFill (jg);
        { juce::Graphics::ScopedSaveState ss (g); if (! running) g.setOpacity (0.45f); g.fillEllipse (jewel); }

        float fsz = sk.nameSize;
        auto fnt = Fonts::make (sk.nameFace, fsz, sk.nameKern);
        const float maxW = np.getWidth() - 48;
        while (juce::GlyphArrangement::getStringWidth (fnt, P.name) > maxW && fsz > 14) { fsz -= 1; fnt = Fonts::make (sk.nameFace, fsz, sk.nameKern); }
        g.setColour (sk.nameCol); g.setFont (fnt);
        g.drawText (P.name, np.withHeight (fsz * 1.15f).withWidth (maxW), juce::Justification::centredLeft, false);
        g.setColour (sk.plateInk.withAlpha (0.7f)); g.setFont (barlow (14, false));
        g.drawText (U (P.cab), juce::Rectangle<float> (np.getX(), np.getY() + fsz * 1.15f + 2, maxW, 18), juce::Justification::centredLeft, false);
    }

    // grille
    if (grilleImg.isValid()) g.drawImageAt (grilleImg, grilleR.getX(), grilleR.getY());
    {
        auto br = blurbR.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.66f)); g.fillRoundedRectangle (br, 10);
        juce::AttributedString as; as.setReadingDirection (juce::AttributedString::rightToLeft);
        as.setJustification (juce::Justification::topRight);
        as.append (U (P.blurb), heebo (15), C (0xffeeece6)); as.setLineSpacing (2);
        juce::TextLayout tl; tl.createLayout (as, br.getWidth() - 28);
        tl.draw (g, br.reduced (14, 12));
    }

    // board
    auto bd = boardR.toFloat();
    g.setGradientFill (juce::ColourGradient (C (0xff2a2a2a), 0, bd.getY(), C (0xff121212), 0, bd.getBottom(), false));
    g.fillRoundedRectangle (bd.withHeight (bd.getHeight() + 20), 16);
    g.setColour (C (0xff3a3a3a)); g.drawHorizontalLine ((int) bd.getY(), 16, W - 16.0f);
}

float MainView::pitch (const float* buf, int n, double sr) const
{
    double rms = 0; for (int i = 0; i < n; ++i) rms += buf[i] * buf[i];
    if (std::sqrt (rms / n) < 0.004) return 0;
    const int minL = (int) std::floor (sr / 1100), maxL = std::min ((int) std::floor (sr / 60), n >> 1);
    std::vector<float> ns ((size_t) maxL + 2, 0.0f); float mx = 0;
    for (int L = minL; L <= maxL + 1; ++L)
    {
        double ac = 0, en = 0;
        for (int i = 0; i < n - L; ++i) { const double a = buf[i], b = buf[i + L]; ac += a * b; en += a * a + b * b; }
        ns[(size_t) L] = en > 0 ? (float) (2 * ac / en) : 0.0f;
        if (L <= maxL && ns[(size_t) L] > mx) mx = ns[(size_t) L];
    }
    if (mx < 0.6f) return 0;
    const float th = mx * 0.9f;
    for (int L = minL + 1; L <= maxL; ++L)
        if (ns[(size_t) L] >= th && ns[(size_t) L] >= ns[(size_t) L - 1] && ns[(size_t) L] >= ns[(size_t) L + 1])
        {
            const float a = ns[(size_t) L - 1], b = ns[(size_t) L], c = ns[(size_t) L + 1], d = a - 2 * b + c;
            return (float) (sr / (L + (d != 0 ? 0.5f * (a - c) / d : 0.0f)));
        }
    return 0;
}

void MainView::timerCallback()
{
    // follow host automation / state restore
    const int id = juce::jlimit (0, 3, (int) proc.vts.getRawParameterValue (gid::preset)->load());
    if (id != cur) rebuild();

    const float pk = proc.peak.exchange (0.0f);
    level = std::max (pk, level * 0.9f);
    if (proc.audioRunning.exchange (false)) runTicks = 15; else if (runTicks > 0) --runTicks;
    if (demoB.active != proc.demoOn.load()) { demoB.active = proc.demoOn.load(); demoB.repaint(); }
    repaint (meterR.expanded (2)); repaint (pillR); repaint (plateR.getRight() - 60, plateR.getY(), 60, 90);

    if (calibUntil > 0)
    {
        const float inDb = proc.vts.getRawParameterValue (gid::inDb)->load();
        calPk = std::max (calPk, pk / dbToG (inDb));
        if (juce::Time::getMillisecondCounterHiRes() > calibUntil)
        {
            calibUntil = 0;
            if (calPk > 1e-4f)
            {
                const float v = juce::jlimit (-12.0f, 30.0f, (float) juce::roundToInt (20 * std::log10 (0.5f / calPk)));
                if (auto* q = proc.vts.getParameter (gid::inDb)) q->setValueNotifyingHost (q->convertTo0to1 (v));
                settings.calibMsg = U (u8"הכניסה כוילה.");
            }
            else settings.calibMsg = U (u8"לא נקלט סיגנל. בדוק שהגיטרה מחוברת ונסה שוב.");
            settings.repaint();
        }
    }

    if (tunerOn && (++frame % 3 == 0))
    {
        const double sr = proc.currentSampleRate();
        const int n = juce::jlimit (1024, 8192, (int) std::round (2048 * sr / 48000.0));
        tbuf.resize ((size_t) n);
        proc.copyMonitor (tbuf.data(), n);
        const float f = pitch (tbuf.data(), n, sr);
        if (f > 0)
        {
            const float m = 69 + 12 * std::log2 (f / 440.0f); const float c = (m - std::round (m)) * 100;
            cents = cents * 0.6f + c * 0.4f;
            tuner.update (f, cents, std::abs (cents) < 4);
        }
    }
}
} // namespace ui

//==============================================================================
BackupRigEditor::BackupRigEditor (BackupRigProcessor& p) : AudioProcessorEditor (p), view (p)
{
    addAndMakeVisible (view);
    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) ui::MainView::W / ui::MainView::H);
    setResizeLimits (600, 468, 1600, 1248);
    setSize (900, 702);
}

void BackupRigEditor::resized()
{
    const float s = (float) getWidth() / (float) ui::MainView::W;
    view.setTransform (juce::AffineTransform::scale (s));
    view.setBounds (0, 0, ui::MainView::W, ui::MainView::H);
}

#ifndef RIG_HEADLESS
juce::AudioProcessorEditor* BackupRigProcessor::createEditor() { return new BackupRigEditor (*this); }
#endif
