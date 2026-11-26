#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Rigs.h"

class BackupRigProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    BackupRigProcessor();
    ~BackupRigProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override { return 4; }
    int getCurrentProgram() override;
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState vts;

    // --- UI feeds (lock-free) ---
    static constexpr int kMon = 8192;
    float monitor[kMon] {};
    std::atomic<int> monWrite { 0 };
    std::atomic<float> peak { 0 };
    std::atomic<bool> demoOn { false };
    std::atomic<bool> audioRunning { false };
    double currentSampleRate() const { return sr; }
    void copyMonitor (float* dest, int n) const;
    void setTunerOpen (bool open) { engine.tunerMute.store (open); }
    void resetPreset (int idx);

private:
    void timerCallback() override { engine.messageThreadTick(); }

    rig::Engine engine;
    double sr = 48000;
    juce::AudioBuffer<float> riff; int riffPos = 0;
    std::vector<float> monoIn, monBuf;
    juce::uint32 lastBlockMs = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BackupRigProcessor)
};
