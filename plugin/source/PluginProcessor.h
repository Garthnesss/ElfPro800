#pragma once
#include <JuceHeader.h>
#include <array>
#include <cstdint>

extern "C" {
#include "p8_core.h"
}

/* ElfPro800 V1 scaffold processor.
 *
 * Owns one p8_core (the real P600-derived engine, 8 voices) and drives it
 * at its native 2 kHz tick rate from the audio callback. Sound comes from
 * the same placeholder voice renderer the V0 render tool used (saw +
 * one-pole lowpass per voice, pitch tracked from note on/off, level and
 * filter from the engine's per-voice CV frame). The analogue voice model
 * replaces it in a later V1 milestone; the engine path is already real.
 */
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

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Init"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void applyParamsToEngine();

    p8_core_t core {};
    p8_cv_frame_t frame {};
    double sr = 48000.0;
    int samplesPerTick = 24;
    int tickPos = 0;

    struct Voice { int note = -1; double phase = 0.0, lp = 0.0; };
    std::array<Voice, 16> voices {};   /* placeholder-DSP tracking, as in tools/render.c */
    int numVoices = 0;

    std::atomic<float>* pCutoff = nullptr;
    std::atomic<float>* pResonance = nullptr;
    std::atomic<float>* pFilEnvAmt = nullptr;
    std::atomic<float>* pVolume = nullptr;
    float lastCutoff = -1.0f, lastResonance = -1.0f, lastFilEnvAmt = -1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ElfPro800Processor)
};
