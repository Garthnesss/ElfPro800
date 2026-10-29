#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

extern "C" {
#include "p8_core.h"
#include "storage.h" /* struct preset_s, cp*/ /* sp indices */
}

/* ElfPro800 processor: owns the real 8-voice P600-derived engine, ticks it
 * at 2 kHz from the audio callback, and binds the full panel parameter set
 * to the engine's preset struct (continuous + stepped parameters) exactly
 * the way the hardware front panel does. Sound is the V0 placeholder voice
 * renderer until the analogue voice model milestone. */
class ElfPro800Processor final : public juce::AudioProcessor
{
public:
    ElfPro800Processor();
    ~ElfPro800Processor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ElfPro800"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    /* UI-facing state */
    void requestPresetDelta (int delta);
    int getPresetIndex() const { return presetIndex.load(); }
    juce::String getPresetName() const;
    float getVoiceLevel (int v) const { return voiceLevel[(size_t) v].load(); }
    int getArpMode() const { return arpModeApplied; }

    juce::AudioProcessorValueTreeState apvts;

    /* parameter ids (shared with the editor and the headless harness) */

private:
    juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void applyParamsToEngine();
    void syncParamsFromEngine();
    void loadPreset (int index);
    void buildFactoryPresets();

    p8_core_t core {};
    p8_cv_frame_t frame {};
    double sr = 48000.0;
    int samplesPerTick = 24;

    struct Voice { int note = -1; double phase = 0.0, lp = 0.0; };
    std::array<Voice, 16> voices {};
    int numVoices = 0;

    /* parameter bookkeeping: last values pushed to the engine (denormalised
       per parameter kind), to avoid refreshFullState() churn */
    std::vector<float> lastApplied;
    std::atomic<bool> paramsDirty { true };

    /* presets */
    struct preset_s basePreset {};
    std::vector<struct preset_s> factory;
    std::vector<juce::String> factoryNames;
    std::atomic<int> presetIndex { 0 };
    std::atomic<int> presetRequest { -1 }; /* -1 none, else absolute index */
    bool presetsBuilt = false;

    int arpModeApplied = -1, arpHoldApplied = -1;

    std::array<std::atomic<float>, 8> voiceLevel {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ElfPro800Processor)
};
