/* Dev tool: renders the editor offscreen to a PNG so panel layout can be
 * eyeballed without a host. Usage: ElfPro800Snapshot [out.png] */
#include <JuceHeader.h>
#include <cstdio>

#include "../source/PluginProcessor.h"
#include "../source/PluginEditor.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    ElfPro800Processor proc;
    proc.prepareToPlay (48000.0, 512);
    auto* ed = proc.createEditor();
    ed->setSize (1320, 764);
    const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), false, 1.0f);
    delete ed;
    juce::File out (argc > 1 ? argv[1] : "panel.png");
    out.deleteFile();
    juce::PNGImageFormat png;
    juce::FileOutputStream os (out);
    if (! os.openedOk() || ! png.writeImageToStream (img, os)) {
        std::printf ("snapshot FAILED\n");
        return 1;
    }
    os.flush();
    std::printf ("snapshot written: %s (%d bytes)\n", out.getFullPathName().toRawUTF8(), (int) out.getSize());
    return 0;
}
