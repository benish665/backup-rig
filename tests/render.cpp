// Offline test: renders the demo riff through each rig, prints level/band stats, checks NaN, measures CPU.
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Riff.h"
#include <chrono>
#include <thread>

juce::AudioProcessorEditor* BackupRigProcessor::createEditor() { return nullptr; }

static double bandRms (const float* x, int n, double sr, double f)
{
    const double w0 = 2 * rig::kPi * f / sr, al = std::sin (w0) / (2 * 1.41), cw = std::cos (w0);
    const double b0 = al / (1 + al), b2 = -al / (1 + al), a1 = -2 * cw / (1 + al), a2 = (1 - al) / (1 + al);
    double z1 = 0, z2 = 0, e = 0;
    for (int i = 0; i < n; ++i) { const double y = b0 * x[i] + z1; z1 = -a1 * y + z2; z2 = b2 * x[i] - a2 * y; e += y * y; }
    return 10 * std::log10 (e / n + 1e-20);
}

static juce::String stats (const juce::AudioBuffer<float>& b, double sr)
{
    juce::String s;
    for (int c = 0; c < 2; ++c)
    {
        const float* x = b.getReadPointer (c); const int n = b.getNumSamples();
        double e = 0; float pk = 0; bool bad = false;
        for (int i = 0; i < n; ++i) { if (! std::isfinite (x[i])) bad = true; e += x[i] * x[i]; pk = std::max (pk, std::abs (x[i])); }
        s << (c ? " R " : "L ") << juce::String (10 * std::log10 (e / n + 1e-20), 1) << "dB pk " << juce::String (pk, 3) << (bad ? " NaN!" : "");
        if (c == 0) { s << " bands"; for (double f : { 125., 250., 500., 1000., 2000., 4000., 8000. }) s << " " << juce::String (bandRms (x, n, sr, f), 1); }
    }
    return s;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 48000; const juce::String outDir = argc > 1 ? argv[1] : "out";
    juce::File (outDir).createDirectory();
    BackupRigProcessor proc;
    rig::Engine eng (proc.vts);
    eng.prepare (sr);
    std::this_thread::sleep_for (std::chrono::milliseconds (800));
    auto riff = rig::makeRiff (sr);
    const int len = riff.getNumSamples();
    const char* ids[] = { "clean", "sky", "plexi", "lead" };
    juce::WavAudioFormat wav;

    for (int c = 0; c < 4; ++c)
    {
        auto& ch = *eng.ch[(size_t) c];
        ch.reset(); ch.apply();
        // warm-up block so the convolution engine swaps in its IR
        { float z[64] {}, l[64], r[64]; for (int k = 0; k < 20; ++k) ch.process (z, l, r, 64); std::this_thread::sleep_for (std::chrono::milliseconds (100)); ch.process (z, l, r, 64); }
        ch.reset(); ch.apply();
        std::this_thread::sleep_for (std::chrono::milliseconds (100));
        juce::AudioBuffer<float> out (2, len);
        const auto t0 = std::chrono::steady_clock::now();
        for (int off = 0; off < len; off += 64)
        {
            const int m = std::min (64, len - off);
            ch.process (riff.getReadPointer (0) + off, out.getWritePointer (0) + off, out.getWritePointer (1) + off, m);
        }
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::cout << ids[c] << "  " << stats (out, sr) << "  cpu " << juce::String (secs / (len / sr) * 100, 2) << "% of realtime" << std::endl;
        auto f = juce::File (outDir).getChildFile (juce::String (ids[c]) + "_cpp.wav"); f.deleteFile();
        if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0)))
            w->writeFromAudioSampleBuffer (out, 0, len);
    }

    // full processor path in demo mode, with preset switching mid-way, plus HQ mode
    for (int hq = 0; hq < 2; ++hq)
    {
        proc.vts.getParameter (rig::gid::hq)->setValueNotifyingHost ((float) hq);
        proc.prepareToPlay (sr, 256);
        std::this_thread::sleep_for (std::chrono::milliseconds (800));
        proc.demoOn = true;
        juce::AudioBuffer<float> buf (2, 256), all (2, (int) (sr * 16)); juce::MidiBuffer mb;
        const auto t0 = std::chrono::steady_clock::now();
        for (int off = 0; off + 256 <= all.getNumSamples(); off += 256)
        {
            const int sec = off / (int) sr;
            auto* pp = proc.vts.getParameter (rig::gid::preset);
            pp->setValueNotifyingHost (pp->convertTo0to1 ((float) ((sec / 4) % 4)));
            buf.clear(); proc.processBlock (buf, mb);
            for (int c = 0; c < 2; ++c) all.copyFrom (c, off, buf, c, 0, 256);
        }
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::cout << "full hq=" << hq << "  " << stats (all, sr) << "  cpu " << juce::String (secs / 16.0 * 100, 2) << "%" << std::endl;
        auto f = juce::File (outDir).getChildFile ("full_hq" + juce::String (hq) + ".wav"); f.deleteFile();
        if (auto w = std::unique_ptr<juce::AudioFormatWriter> (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0)))
            w->writeFromAudioSampleBuffer (all, 0, all.getNumSamples());
    }
    return 0;
}
