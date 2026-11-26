#include "PluginProcessor.h"
#ifndef RIG_HEADLESS
#include "PluginEditor.h"
#endif
#include "Riff.h"

using namespace rig;

juce::AudioProcessorValueTreeState::ParameterLayout BackupRigProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout L;
    using PID = juce::ParameterID;
    auto pctDb = [] (float v, int) { return (v > 0 ? "+" : "") + juce::String (juce::roundToInt (v)) + " dB"; };

    L.add (std::make_unique<juce::AudioParameterChoice> (PID { gid::preset, 1 }, "Preset", juce::StringArray { "CLEAN", "SKY", "PLEXI", "LEAD" }, 0));
    L.add (std::make_unique<juce::AudioParameterFloat> (PID { gid::inDb, 1 }, "Input", juce::NormalisableRange<float> (-12, 30, 1), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctDb)));
    L.add (std::make_unique<juce::AudioParameterFloat> (PID { gid::gate, 1 }, "Noise Gate", juce::NormalisableRange<float> (-90, -30, 1), -56.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return v <= -90 ? juce::String ("Off") : juce::String (juce::roundToInt (v)) + " dB"; })));
    L.add (std::make_unique<juce::AudioParameterFloat> (PID { gid::outDb, 1 }, "Output", juce::NormalisableRange<float> (-30, 6, 1), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (pctDb)));
    L.add (std::make_unique<juce::AudioParameterBool> (PID { gid::mono, 1 }, "Mono Out", false));
    L.add (std::make_unique<juce::AudioParameterBool> (PID { gid::hq, 1 }, "High Quality", false));
    L.add (std::make_unique<juce::AudioParameterChoice> (PID { gid::inCh, 1 }, "Input Channel", juce::StringArray { "1+2", "1", "2" }, 1));

    for (auto& p : presets())
    {
        auto grp = std::make_unique<juce::AudioProcessorParameterGroup> (p.id, p.fs, " | ");
        auto addKnob = [&] (const KnobDef& k)
        {
            const juce::String name = juce::String (p.fs) + " " + k.label;
            if (k.ms)
                grp->addChild (std::make_unique<juce::AudioParameterFloat> (PID { knobId (p.id, k.key), 1 }, name,
                    juce::NormalisableRange<float> (k.min, k.max, 1), k.def,
                    juce::AudioParameterFloatAttributes().withLabel ("ms").withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " ms"; })));
            else
                grp->addChild (std::make_unique<juce::AudioParameterFloat> (PID { knobId (p.id, k.key), 1 }, name,
                    juce::NormalisableRange<float> (k.min, k.max, 0.1f), k.def,
                    juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1); })));
        };
        for (auto& k : p.amp) addKnob (k);
        for (auto& w : p.sw) grp->addChild (std::make_unique<juce::AudioParameterBool> (PID { swId (p.id, w.key), 1 }, juce::String (p.fs) + " " + w.label, w.def));
        for (auto& f : p.fx)
        {
            if (! f.fixed) grp->addChild (std::make_unique<juce::AudioParameterBool> (PID { swId (p.id, f.id), 1 }, juce::String (p.fs) + " " + f.label + " On", f.on));
            for (auto& k : f.k) addKnob (k);
        }
        L.add (std::move (grp));
    }
    return L;
}

BackupRigProcessor::BackupRigProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      vts (*this, nullptr, "BackupRig", createLayout()),
      engine (vts)
{
    startTimer (50);
}

BackupRigProcessor::~BackupRigProcessor() { stopTimer(); }

bool BackupRigProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo() || in.isDisabled();
}

void BackupRigProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    monoIn.assign ((size_t) std::max (samplesPerBlock, 64), 0.0f);
    monBuf.assign (monoIn.size(), 0.0f);
    engine.prepare (sr);
    riff = makeRiff (sr); riffPos = 0;
    setLatencySamples (0);
}

void BackupRigProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if ((int) monoIn.size() < n) { monoIn.resize ((size_t) n); monBuf.resize ((size_t) n); } // safety, should not happen

    const int inCh = getTotalNumInputChannels();
    const bool demo = demoOn.load();
    if (demo && riff.getNumSamples() > 0)
    {
        const float* r = riff.getReadPointer (0);
        for (int i = 0; i < n; ++i) { monoIn[(size_t) i] = r[riffPos]; if (++riffPos >= riff.getNumSamples()) riffPos = 0; }
    }
    else if (inCh == 0)
        std::fill (monoIn.begin(), monoIn.begin() + n, 0.0f);
    else
    {
        const int sel = (int) vts.getRawParameterValue (gid::inCh)->load(); // 0 = 1+2, 1 = ch1, 2 = ch2
        const float* a = buffer.getReadPointer (0);
        const float* b = inCh > 1 ? buffer.getReadPointer (1) : nullptr;
        for (int i = 0; i < n; ++i)
        {
            float x = 0;
            if (sel != 2 || b == nullptr) x += a[i];
            if (sel != 1 && b != nullptr) x += b[i];
            monoIn[(size_t) i] = x;
        }
    }

    const int outCh = getTotalNumOutputChannels();
    float* L = buffer.getWritePointer (0);
    float* R = outCh > 1 ? buffer.getWritePointer (1) : nullptr;
    if (R == nullptr)
    {
        float rs[rig::kBlock];
        for (int off = 0; off < n; off += rig::kBlock)
        {
            const int m = std::min (rig::kBlock, n - off);
            engine.process (monoIn.data() + off, L + off, rs, m, ! demo, monBuf.data() + off);
            for (int i = 0; i < m; ++i) L[off + i] = 0.5f * (L[off + i] + rs[i]);
        }
    }
    else
        engine.process (monoIn.data(), L, R, n, ! demo, monBuf.data());

    for (int c = 2; c < outCh; ++c) buffer.clear (c, 0, n);

    // meter + tuner feed
    float pk = 0; int w = monWrite.load (std::memory_order_relaxed);
    for (int i = 0; i < n; ++i) { const float x = monBuf[(size_t) i]; pk = std::max (pk, std::abs (x)); monitor[w] = x; w = (w + 1) & (kMon - 1); }
    monWrite.store (w, std::memory_order_release);
    peak.store (std::max (peak.load (std::memory_order_relaxed), pk), std::memory_order_relaxed);
    audioRunning.store (true);
}

void BackupRigProcessor::copyMonitor (float* dest, int n) const
{
    const int w = monWrite.load (std::memory_order_acquire);
    for (int i = 0; i < n; ++i) dest[i] = monitor[(w - n + i) & (kMon - 1)];
}

int BackupRigProcessor::getCurrentProgram() { return (int) vts.getRawParameterValue (gid::preset)->load(); }
void BackupRigProcessor::setCurrentProgram (int i)
{
    if (auto* p = vts.getParameter (gid::preset)) p->setValueNotifyingHost (p->convertTo0to1 ((float) juce::jlimit (0, 3, i)));
}
const juce::String BackupRigProcessor::getProgramName (int i)
{
    const auto& P = presets();
    return i >= 0 && i < (int) P.size() ? juce::String (P[(size_t) i].fs) : juce::String();
}

void BackupRigProcessor::resetPreset (int idx)
{
    const auto& p = presets()[(size_t) idx];
    auto reset = [this] (const juce::String& id) { if (auto* q = vts.getParameter (id)) q->setValueNotifyingHost (q->getDefaultValue()); };
    for (auto& k : p.amp) reset (knobId (p.id, k.key));
    for (auto& w : p.sw) reset (swId (p.id, w.key));
    for (auto& f : p.fx) { if (! f.fixed) reset (swId (p.id, f.id)); for (auto& k : f.k) reset (knobId (p.id, k.key)); }
}

void BackupRigProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = vts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void BackupRigProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (vts.state.getType())) vts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new BackupRigProcessor(); }
