// Minimal VST3 host check: scan, instantiate, list params, process audio at several block sizes/rates, state round-trip.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_events/juce_events.h>
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::VST3PluginFormat fmt; juce::OwnedArray<juce::PluginDescription> found;
    fmt.findAllTypesForFile (found, argv[1]);
    if (found.isEmpty()) { std::cout << "FAIL: no plugin found" << std::endl; return 1; }
    auto& d = *found[0];
    std::cout << "found: " << d.name << " by " << d.manufacturerName << " cat=" << d.category << " in=" << d.numInputChannels << " out=" << d.numOutputChannels << std::endl;
    int fails = 0;
    for (double sr : { 44100.0, 48000.0, 96000.0 })
        for (int bs : { 32, 128, 480, 1024 })
        {
            juce::String err;
            auto p = fmt.createInstanceFromDescription (d, sr, bs, err);
            if (! p) { std::cout << "FAIL create " << err << std::endl; return 1; }
            p->setPlayConfigDetails (2, 2, sr, bs);
            p->prepareToPlay (sr, bs);
            juce::Thread::sleep (300);
            juce::AudioBuffer<float> b (2, bs); juce::MidiBuffer m; double e = 0; bool bad = false;
            auto& params = p->getParameters();
            for (int blk = 0; blk < (int) (sr * 3 / bs); ++blk)
            {
                for (int i = 0; i < bs; ++i) { const float x = 0.3f * std::sin (2 * juce::MathConstants<float>::pi * 110.0f * (float) (blk * bs + i) / (float) sr) * ((blk * bs + i) % (int) sr < sr / 2 ? 1.f : 0.f); b.setSample (0, i, x); b.setSample (1, i, x); }
                if (blk % 50 == 0) params[0]->setValueNotifyingHost ((float) ((blk / 50) % 4) / 3.0f); // cycle presets
                if (blk % 7 == 0) { auto* q = params[(blk / 7) % params.size()]; q->setValueNotifyingHost (juce::Random::getSystemRandom().nextFloat()); }
                p->processBlock (b, m);
                for (int c = 0; c < 2; ++c) for (int i = 0; i < bs; ++i) { const float y = b.getSample (c, i); if (! std::isfinite (y) || std::abs (y) > 1.01f) bad = true; e += y * y; }
            }
            juce::MemoryBlock st; p->getStateInformation (st); p->setStateInformation (st.getData(), (int) st.getSize());
            std::cout << "sr " << sr << " bs " << bs << ": params " << params.size() << " rms " << juce::String (10 * std::log10 (e / (sr * 3 * 2) + 1e-20), 1) << " dB state " << (int) st.getSize() << " bytes" << (bad ? "  FAIL (NaN or >0dBFS)" : "  ok") << std::endl;
            fails += bad; p->releaseResources();
        }
    std::cout << (fails ? "SOME FAILURES" : "ALL OK") << std::endl;
    return fails;
}
