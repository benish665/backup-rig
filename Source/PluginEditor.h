#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

namespace ui
{
struct Fonts
{
    juce::Typeface::Ptr heebo, barlowSemi, barlowBold, yellowtail, josefin, archivo, michroma;
    static Fonts& get();
    static juce::Font make (const juce::Typeface::Ptr& t, float h, float kern = 0.0f)
    { return juce::Font (juce::FontOptions (t).withHeight (h).withKerningFactor (kern)); }
};

struct KnobSkin
{
    juce::Colour c0, c1, c2; float s1 = 0.55f; juce::Colour skirt, ptr, tick, ink;
};

struct Skin
{
    juce::Colour tolex, accent, pipe, plateInk, nameCol;
    std::vector<std::pair<float, juce::Colour>> plate;
    KnobSkin knob;
    juce::Typeface::Ptr nameFace; float nameSize, nameKern;
    int grille; // 0 clean, 1 sky, 2 plexi, 3 lead
    static const Skin& forPreset (int i);
};

//==============================================================================
class Knob : public juce::Slider
{
public:
    Knob (const juce::String& label, bool small, bool ms);
    void paint (juce::Graphics&) override;
    KnobSkin skin; juce::String label; bool small, ms;
    int dial() const { return small ? 46 : 72; }
    static int heightFor (bool small) { return (small ? 46 : 72) + 16 + 40; }
};

class Toggle : public juce::ToggleButton // BRIGHT-style slide toggle on the faceplate
{
public:
    explicit Toggle (const juce::String& t) : juce::ToggleButton (t) {}
    void paintButton (juce::Graphics&, bool, bool) override;
    juce::Colour ink, accent;
};

class Stomp : public juce::ToggleButton // pedal on/off with LED
{
public:
    explicit Stomp (const juce::String& t) : juce::ToggleButton (t) {}
    void paintButton (juce::Graphics&, bool, bool) override;
    juce::Colour accent;
};

class Pill : public juce::Button
{
public:
    explicit Pill (const juce::String& t) : juce::Button (t) {}
    void paintButton (juce::Graphics&, bool, bool) override;
    bool go = false, active = false; juce::Colour accent { 0xff8fd6c2 }; float fontH = 15;
};

class FootSwitch : public juce::Button
{
public:
    FootSwitch (int idx);
    void paintButton (juce::Graphics&, bool, bool) override;
    int idx; bool on = false;
};

//==============================================================================
class FxUnit : public juce::Component
{
public:
    FxUnit (BackupRigProcessor&, int preset, int fxIndex);
    void paint (juce::Graphics&) override;
    void resized() override;
    std::unique_ptr<Stomp> sw; std::unique_ptr<Pill> tap;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> ka;
    std::unique_ptr<juce::ButtonParameterAttachment> sa;
    juce::String label; bool fixed;
    std::vector<double> taps;
    bool isOn() const { return fixed || (sw && sw->getToggleState()); }
};

//==============================================================================
class Tuner : public juce::Component
{
public:
    Tuner();
    void paint (juce::Graphics&) override;
    void resized() override;
    void update (float hz, float cents, bool ok);
    void mouseDown (const juce::MouseEvent&) override {}
    Pill close { juce::String (juce::CharPointer_UTF8 ("\xd7\xa1\xd7\x92\xd7\x95\xd7\xa8 \xd7\x98\xd7\x99\xd7\x95\xd7\xa0\xd7\xa8")) };
    juce::String note { "-" }, info { "- Hz" }; float needle = 50; bool ok = false;
    juce::Rectangle<int> box;
};

class Settings : public juce::Component
{
public:
    Settings (BackupRigProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    void mouseDown (const juce::MouseEvent&) override;
    BackupRigProcessor& proc;
    juce::Slider inS, gateS, outS;
    std::unique_ptr<juce::SliderParameterAttachment> ia, ga, oa;
    Pill qFast, qHq, oSt, oMono, c12, c1, c2, calib, resetP, closeB;
    juce::Rectangle<int> box;
    juce::String calibMsg;
    std::function<void()> onCalib, onReset, onClose;
};

//==============================================================================
class MainView : public juce::Component, private juce::Timer
{
public:
    explicit MainView (BackupRigProcessor&);
    ~MainView() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    static constexpr int W = 1000, H = 780;

private:
    void timerCallback() override;
    void rebuild();
    void selectPreset (int i);
    void openTuner (bool);
    void renderGrille();
    float pitch (const float* buf, int n, double sr) const;

    BackupRigProcessor& proc;
    int cur = -1;
    Pill tunerB, setB, demoB;
    std::vector<std::unique_ptr<Knob>> ampKnobs;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> ampKa;
    std::vector<std::unique_ptr<Toggle>> ampSw;
    std::vector<std::unique_ptr<juce::ButtonParameterAttachment>> ampSa;
    std::vector<std::unique_ptr<FxUnit>> units;
    std::array<std::unique_ptr<FootSwitch>, 4> fs;
    Tuner tuner; Settings settings;
    juce::Image grilleImg;
    juce::Rectangle<int> headR, plateR, grilleR, blurbR, boardR, meterR, pillR;
    float level = 0; int runTicks = 0; bool tunerOn = false; int frame = 0; float cents = 0;
    double calibUntil = 0; float calPk = 0;
    std::vector<float> tbuf;
};
} // namespace ui

class BackupRigEditor : public juce::AudioProcessorEditor
{
public:
    explicit BackupRigEditor (BackupRigProcessor&);
    void resized() override;
private:
    ui::MainView view;
};
