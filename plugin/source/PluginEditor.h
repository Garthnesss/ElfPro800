#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>

class ElfPro800Processor;

/* Full panel editor, following the approved mock: every control is bound
 * to a real engine parameter (no decorative controls). */
class ElfPro800Editor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ElfPro800Editor (ElfPro800Processor&);
    ~ElfPro800Editor() override = default;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob {
        juce::Slider slider; juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    struct Toggle {
        juce::TextButton btn;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
        bool paintedState = false;
    };
    struct Seg {
        juce::String paramId; std::vector<std::unique_ptr<juce::TextButton>> btns;
        int paintedIndex = -1;
    };

    Knob& addKnob (const char* id, const char* label);
    Toggle& addToggle (const char* id, const char* label);
    Seg& addSeg (const char* id, juce::StringArray options);
    void styleToggle (juce::TextButton&, bool on);
    void refreshSeg (Seg&);
    void layoutKnobRow (juce::Rectangle<int> area, std::vector<Knob*> knobs, int knobSize);
    void layoutToggleRow (juce::Rectangle<int> area, std::vector<Toggle*> toggles);
    void layoutSegRow (juce::Rectangle<int> area, Seg& seg);

    ElfPro800Processor& proc;

    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Toggle>> toggles;
    std::vector<std::unique_ptr<Seg>> segs;
    Knob *kFreqA{}, *kMix{}, *kPwA{}, *kFreqB{}, *kPwB{}, *kFineB{},
         *kCutoff{}, *kReso{}, *kFilEnvAmt{},
         *kFilAtt{}, *kFilDec{}, *kFilSus{}, *kFilRel{},
         *kAmpAtt{}, *kAmpDec{}, *kAmpSus{}, *kAmpRel{},
         *kPmodFilEnv{}, *kPmodOscB{}, *kLfoFreq{}, *kLfoAmt{},
         *kGlide{}, *kAmpVel{}, *kFilVel{}, *kDetune{}, *kSpread{}, *kDrive{}, *kVolume{};
    Toggle *tASaw{}, *tATri{}, *tASqr{}, *tBSaw{}, *tBTri{}, *tBSqr{}, *tSync{},
           *tPmodFA{}, *tPmodFil{}, *tUnison{},
           *tLfoPitch{}, *tLfoFilter{}, *tLfoAmp{}, *tLfoPW{}, *tArpHold{};
    Seg *segLfoShape{}, *segTracking{}, *segArpMode{};
    Knob* kArpRate {};

    juce::TextButton prevBtn { "<" }, nextBtn { ">" };
    juce::Label lcdNum, lcdName;
    int paintedPreset = -1;
    std::array<float, 8> ledLevel {};

    struct Section { juce::Rectangle<int> bounds; juce::String title, tag; };
    std::vector<Section> sections;
    juce::Rectangle<int> lcdBounds, ledBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ElfPro800Editor)
};
