// Renders the editor for each preset to PNG (visual check without a display)
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File dir (argc > 1 ? argv[1] : "snaps"); dir.createDirectory();
    BackupRigProcessor proc;
    for (int i = 0; i < 6; ++i)
    {
        auto* pp = proc.vts.getParameter (rig::gid::preset);
        pp->setValueNotifyingHost (pp->convertTo0to1 ((float) std::min (i, 3)));
        ui::MainView v (proc);
        if (i == 4) { for (auto* c : v.getChildren()) if (dynamic_cast<ui::Settings*> (c)) c->setVisible (true); }
        if (i == 5) { for (auto* c : v.getChildren()) if (auto* t = dynamic_cast<ui::Tuner*> (c)) { t->setVisible (true); t->update (110.4f, 6.0f, false); } }
        auto img = v.createComponentSnapshot (v.getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png; auto f = dir.getChildFile ("ui" + juce::String (i) + ".png"); f.deleteFile();
        juce::FileOutputStream os (f); png.writeImageToStream (img, os);
    }
    return 0;
}
