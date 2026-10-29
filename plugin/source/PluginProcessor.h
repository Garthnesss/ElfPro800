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

    /* UI-facing state. Programs: 0..5 factory, 6..105 = user slots
       0..99 in the engine's own storage (pages 0..99 of the EEPROM
       image; sequencer pages start at 200 and are never touched). */
    static constexpr int FACTORY_PROGRAMS = 6;
    static constexpr int USER_SLOTS = 100;
    static constexpr int NUM_PROGRAMS = FACTORY_PROGRAMS + USER_SLOTS;
    void requestPresetDelta (int delta);
    void requestProgram (int program);
    int getPresetIndex() const { return presetIndex.load(); }
    juce::String getPresetName() const;
    bool programIsEmpty (int program) const;
    bool canSave() const { return presetIndex.load() >= FACTORY_PROGRAMS; }
    void requestSaveToCurrent();
    int getSaveFlash() const { return saveFlash.load(); }
    float getVoiceLevel (int v) const { return voiceLevel[(size_t) v].load(); }
    int getArpMode() const { return arpModeApplied; }

    juce::AudioProcessorValueTreeState apvts;

    /* parameter ids (shared with the editor and the headless harness) */

private:
    juce::AudioProcessorValueTreeState::ParameterLayout makeLayout();
    void applyParamsToEngine();
    void syncParamsFromEngine(); /* pushes engine -> APVTS, then captureShadows() */
    void loadPreset (int index); /* kept: factory-only path */
    void buildFactoryPresets();

    p8_core_t core {};
    p8_cv_frame_t frame {};
    double sr = 48000.0;
    int samplesPerTick = 24;

    /* CV-driven voice DSP state (one per engine voice). Pitch, pulse
       width, levels and filter cutoff all come from the engine's CV
       frame each tick - the DSP keeps only oscillator/filter state. */
    struct DVoice { double phA = 0.0, phB = 0.0, ic1 = 0.0, ic2 = 0.0; };
    std::array<DVoice, 8> dvoices {};

    /* Parameter authority bookkeeping. Two shadows per registered
       parameter: hostShadow = the value the host last presented (or
       that we last pushed and captured), engineBaseline = the engine's
       value at the last reconciliation. A host value only overrides a
       loaded preset when it CHANGES away from hostShadow; a preset
       load moves the baseline and the shadow together, so a stale host
       echo can never stomp a fresh preset (Garth's Live test). */
    std::vector<float> hostShadow;
    std::vector<float> engineBaseline;
    void captureShadows();

    /* presets */
    struct preset_s basePreset {};
    std::vector<struct preset_s> factory;
    std::vector<juce::String> factoryNames;
    std::atomic<int> presetIndex { 0 };
    std::atomic<int> presetRequest { -1 }; /* -1 none, else absolute program */
    bool presetsBuilt = false;
    std::array<bool, USER_SLOTS> userEmpty {};
    std::atomic<int> saveRequest { -1 };
    std::atomic<int> saveFlash { 0 };
    void scanUserSlots();
    void loadProgram (int program);

    int arpModeApplied = -1, arpHoldApplied = -1;
    uint16_t arpRateApplied = 0xffff;

    std::array<std::atomic<float>, 8> voiceLevel {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ElfPro800Processor)
};
