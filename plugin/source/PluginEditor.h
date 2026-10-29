#pragma once
#include <JuceHeader.h>

class ElfPro800Processor;

/* V1 scaffold editor: brand + the four wired parameters as rotary sliders.
   The full panel (mock: elfchords-p8-panel-mock.html) is the next UI
   milestone; this proves the parameter path end to end. */
class ElfPro800Editor final : public juce::AudioProcessorEditor
{
public:
    explicit ElfPro800Editor (ElfPro800Processor&);
    ~ElfPro800Editor() override = default;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct ParamSlider {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };
    void setupSlider (ParamSlider&, const char* paramID, const char* name);
    ElfPro800Processor& proc;
    ParamSlider cutoff, resonance, filEnvAmt, volume;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ElfPro800Editor)
};
