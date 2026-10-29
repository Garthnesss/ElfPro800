#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <cstring>

extern "C" {
#include "storage.h"       /* currentPreset, cp* indices, preset_loadDefault */
#include "engine_bridge.h" /* refreshFullState */
}

ElfPro800Processor::ElfPro800Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", makeLayout())
{
    pCutoff    = apvts.getRawParameterValue ("cutoff");
    pResonance = apvts.getRawParameterValue ("resonance");
    pFilEnvAmt = apvts.getRawParameterValue ("filEnvAmt");
    pVolume    = apvts.getRawParameterValue ("volume");
}

juce::AudioProcessorValueTreeState::ParameterLayout ElfPro800Processor::makeLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    auto add = [&] (const char* id, const char* name, float def) {
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name,
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), def));
    };
    add ("cutoff",    "Cutoff",        0.68f);
    add ("resonance", "Resonance",     0.20f);
    add ("filEnvAmt", "Filter Env Amt", 0.40f);
    add ("volume",    "Volume",        0.80f);
    return { params.begin(), params.end() };
}

void ElfPro800Processor::applyParamsToEngine()
{
    const float c = pCutoff->load(), r = pResonance->load(), e = pFilEnvAmt->load();
    if (c == lastCutoff && r == lastResonance && e == lastFilEnvAmt)
        return;
    lastCutoff = c; lastResonance = r; lastFilEnvAmt = e;
    currentPreset.continuousParameters[cpCutoff]    = (uint16_t) (c * 65535.0f);
    currentPreset.continuousParameters[cpResonance] = (uint16_t) (r * 65535.0f);
    currentPreset.continuousParameters[cpFilEnvAmt] = (uint16_t) (e * 65535.0f);
    refreshFullState();
}

void ElfPro800Processor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    samplesPerTick = juce::jmax (1, (int) std::lround (sr / 2000.0)); /* engine tick = 2 kHz */
    tickPos = 0;
    numVoices = 0;
    for (auto& v : voices) v = Voice {};
    p8_core_init (&core);
    preset_loadDefault (1);
    refreshFullState();
    lastCutoff = -1.0f; /* force param application on first block */
    /* Boot guard (V0 finding): the engine drops notes gated during its
       post-init preset load, so run 200 silent ticks before going live. */
    p8_inputs_t in {}; p8_cv_frame_t f {};
    for (int i = 0; i < 200; ++i) p8_core_tick (&core, &in, &f);
    frame = f;
}

void ElfPro800Processor::releaseResources() {}

bool ElfPro800Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

void ElfPro800Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    applyParamsToEngine();

    const int numSamples = buffer.getNumSamples();
    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;

    /* Group this block's MIDI into per-tick chunks for the engine. */
    struct Ev { int tick; uint8_t bytes[3]; int len; };
    std::array<Ev, 256> evs; int numEvs = 0;
    for (const auto meta : midi) {
        if (numEvs >= (int) evs.size()) break;
        const auto msg = meta.getMessage();
        const int len = msg.getRawDataSize();
        if (len <= 0 || len > 3) continue; /* engine scaffold: short messages only */
        Ev& e = evs[(size_t) numEvs++];
        e.tick = meta.samplePosition / samplesPerTick;
        e.len = len;
        std::memcpy (e.bytes, msg.getRawData(), (size_t) len);
    }

    const float volume = pVolume->load();
    int evIdx = 0;
    const int ticksInBlock = (numSamples + samplesPerTick - 1) / samplesPerTick;

    for (int tick = 0; tick < ticksInBlock; ++tick) {
        /* assemble engine input for this tick */
        p8_inputs_t in {};
        uint8_t chunk[3 * 64]; uint16_t n = 0;
        while (evIdx < numEvs && evs[(size_t) evIdx].tick <= tick) {
            const Ev& e = evs[(size_t) evIdx++];
            const uint8_t st = e.bytes[0] & 0xf0, note = e.len > 1 ? e.bytes[1] : 0, vel = e.len > 2 ? e.bytes[2] : 0;
            if (st == 0x90 && vel) {           /* placeholder-DSP note tracking (as render.c) */
                if (numVoices < (int) voices.size()) {
                    voices[(size_t) numVoices].note = note;
                    voices[(size_t) numVoices].phase = 0.0;
                    voices[(size_t) numVoices].lp = 0.0;
                    ++numVoices;
                }
            } else if (st == 0x80 || (st == 0x90 && !vel)) {
                for (int k = 0; k < numVoices; ++k)
                    if (voices[(size_t) k].note == note) {
                        for (int m = k; m + 1 < numVoices; ++m) voices[(size_t) m] = voices[(size_t) (m + 1)];
                        --numVoices; break;
                    }
            }
            if (n + e.len <= sizeof (chunk)) { std::memcpy (chunk + n, e.bytes, (size_t) e.len); n = (uint16_t) (n + e.len); }
        }
        if (n) { in.midi_in = chunk; in.midi_in_len = n; }
        p8_core_tick (&core, &in, &frame);

        /* render this tick's samples with the placeholder voice DSP */
        const int start = tick * samplesPerTick;
        const int end = juce::jmin (numSamples, start + samplesPerTick);
        for (int s = start; s < end; ++s) {
            double mix = 0.0;
            for (int v = 0; v < numVoices; ++v) {
                Voice& vc = voices[(size_t) v];
                const double freq = 440.0 * std::pow (2.0, (vc.note - 69) / 12.0);
                vc.phase += freq / sr;
                if (vc.phase >= 1.0) vc.phase -= 1.0;
                const double saw = 2.0 * vc.phase - 1.0;
                const double amp = v < 8 ? frame.amp[v] / 65535.0 : 0.0;
                const double cut = v < 8 ? frame.fil[v] / 65535.0 : 0.5;
                const double a = 0.02 + 0.9 * cut * cut;
                vc.lp += a * (saw - vc.lp);
                mix += vc.lp * amp;
            }
            const float out = (float) juce::jlimit (-1.0, 1.0, mix * 0.11 * volume * 1.4);
            outL[s] = out;
            if (outR) outR[s] = out;
        }
    }
    /* clear any samples past the last full tick render */
    (void) tickPos;
}

juce::AudioProcessorEditor* ElfPro800Processor::createEditor()
{
    return new ElfPro800Editor (*this);
}

void ElfPro800Processor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ElfPro800Processor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ElfPro800Processor();
}
