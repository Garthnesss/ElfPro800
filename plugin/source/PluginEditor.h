#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

class ElfPro800Processor;

/* Hardware-panel editor (Figma design v2): skeuomorphic knobs drawn by
 * PanelLAF, printed musical scales (semitones / cents / Hz), a
 * last-touched parameter readout in the LED display, mixer LED meters,
 * and the 16-step gate sequencer. Every control binds to a real engine
 * parameter; the step gate binds to the processor's sequencer state. */
class ElfPro800Editor final : public juce::AudioProcessorEditor,
                              private juce::Timer,
                              private juce::AudioProcessorParameter::Listener
{
public:
    explicit ElfPro800Editor (ElfPro800Processor&);
    ~ElfPro800Editor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /* Value formatting for the display readout, shared with the harness
       so what the panel prints is exactly what is tested. v01 is the
       parameter's normalised value; slowFil/slowAmp are the envelope
       slow toggles (they multiply envelope times by ~4). */
    static juce::String formatValue (const juce::String& id, float v01, bool slowFil, bool slowAmp);
    static juce::String displayName (const juce::String& id);

private:
    void timerCallback() override;
    void parameterValueChanged (int parameterIndex, float newValue) override;
    void parameterGestureChanged (int, bool) override {}

    /* A pill/button with an LED dot, painted in the hardware style. */
    struct LedButton final : public juce::TextButton
    {
        juce::Colour ledColour { 0xffff7a1a };
        bool accent = false; /* strong orange fill when on (SYNC) */
        bool filled = false; /* always filled orange (SAVE) */
        void paintButton (juce::Graphics&, bool, bool) override;
    };
    struct Toggle {
        LedButton btn;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> att;
    };
    struct Knob {
        juce::Slider slider; juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
        juce::Point<float> centre; /* absolute, set in resized() */
    };
    struct Seg {
        juce::String paramId; std::vector<std::unique_ptr<LedButton>> btns;
        int paintedIndex = -1;
    };
    struct StepPad final : public juce::Button
    {
        StepPad() : juce::Button ({}) {}
        bool stepOn = false, isCurrent = false, transportOn = false;
        int index = 0;
        void paintButton (juce::Graphics&, bool, bool) override;
    };
    struct PlayButton final : public juce::Button
    {
        PlayButton() : juce::Button ({}) {}
        bool playing = false;
        void paintButton (juce::Graphics&, bool, bool) override;
    };

    Knob& addKnob (const char* id, const char* label);
    Toggle& addToggle (const char* id, const char* label, bool accent = false);
    Seg& addSeg (const char* id, juce::StringArray options);
    void refreshSeg (Seg&);
    void placeKnob (Knob&, float cx, float cy, int size);
    void paintScales (juce::Graphics&);
    void paintMeters (juce::Graphics&);

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
           *tLfoPitch{}, *tLfoFilter{}, *tLfoAmp{}, *tLfoPW{}, *tArpHold{},
           *tFilSlow{}, *tAmpSlow{};
    Seg *segLfoShape{}, *segTracking{}, *segArpMode{};
    Knob* kArpRate {};

    juce::TextButton prevBtn { "<" }, nextBtn { ">" };
    LedButton factoryBtn, userBtn, saveBtn;
    PlayButton playBtn;
    LedButton recBtn;
    std::array<std::unique_ptr<StepPad>, 16> pads;
    int paintedSaveFlash = 0, lcdFlashTicks = 0;
    juce::Label lcdNum, lcdName, lcdReadout;
    int paintedPreset = -1;
    std::array<float, 8> ledLevel {};
    float meterA = 0.0f, meterB = 0.0f;
    int paintedStep = -1; uint32_t paintedPattern = 0xffffffffu;
    bool paintedPlaying = false, paintedRec = false;

    std::atomic<int> lastParamIndex { -1 };
    std::atomic<juce::int64> lastEditMs { 0 };
    juce::String readoutText;

    struct Section { juce::Rectangle<int> bounds; juce::String title; };
    std::vector<Section> sections;
    juce::Rectangle<int> lcdBounds { 928, 20, 362, 68 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ElfPro800Editor)
};
