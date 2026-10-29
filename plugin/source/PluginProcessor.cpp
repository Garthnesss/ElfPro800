#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <cstring>

extern "C" {
#include "engine_bridge.h" /* refreshFullState */
#include "arp.h"           /* arp_setMode, arpMode_t */
#include "synth.h"         /* mt* LFO target bits */
}

/* ------------------------------------------------------------------ */
/* Parameter tables: the panel, bound to the engine's preset struct.   */
/* ------------------------------------------------------------------ */
namespace {
struct ContDef { const char* id; const char* name; int cp; };
const ContDef CONT[] = {
    { "freqA", "Osc A Freq", cpFreqA },       { "volA", "Osc A Level", cpVolA },
    { "pwA", "Osc A Pulse Width", cpAPW },    { "freqB", "Osc B Freq", cpFreqB },
    { "volB", "Osc B Level", cpVolB },        { "pwB", "Osc B Pulse Width", cpBPW },
    { "fineB", "Osc B Fine", cpFreqBFine },   { "cutoff", "Cutoff", cpCutoff },
    { "resonance", "Resonance", cpResonance },{ "filEnvAmt", "Filter Env Amt", cpFilEnvAmt },
    { "filAtt", "Filter Attack", cpFilAtt },  { "filDec", "Filter Decay", cpFilDec },
    { "filSus", "Filter Sustain", cpFilSus }, { "filRel", "Filter Release", cpFilRel },
    { "ampAtt", "Amp Attack", cpAmpAtt },     { "ampDec", "Amp Decay", cpAmpDec },
    { "ampSus", "Amp Sustain", cpAmpSus },    { "ampRel", "Amp Release", cpAmpRel },
    { "pmodFilEnv", "PolyMod Fil Env", cpPModFilEnv },
    { "pmodOscB", "PolyMod Osc B", cpPModOscB },
    { "lfoFreq", "LFO Freq", cpLFOFreq },     { "lfoAmt", "LFO Amount", cpLFOAmt },
    { "glide", "Glide", cpGlide },            { "unisonDetune", "Unison Detune", cpUnisonDetune },
    { "spread", "Spread", cpSpread },         { "drive", "Drive", cpDrive },
    { "ampVel", "Amp Velocity", cpAmpVelocity }, { "filVel", "Filter Velocity", cpFilVelocity },
};
struct BoolDef { const char* id; const char* name; int sp; };
const BoolDef BOOLS[] = {
    { "aSaw", "Osc A Saw", spASaw },   { "aTri", "Osc A Triangle", spATri },
    { "aSqr", "Osc A Square", spASqr },{ "bSaw", "Osc B Saw", spBSaw },
    { "bTri", "Osc B Triangle", spBTri }, { "bSqr", "Osc B Square", spBSqr },
    { "sync", "Osc B Sync", spSync },  { "pmodFA", "PolyMod to Freq A", spPModFA },
    { "pmodFil", "PolyMod to Filter", spPModFil }, { "unison", "Unison", spUnison },
};
struct TgtDef { const char* id; const char* name; int bit; };
const TgtDef TGTS[] = {
    { "lfoTgtPitch", "LFO to Pitch", mtVCO }, { "lfoTgtFilter", "LFO to Filter", mtVCF },
    { "lfoTgtAmp", "LFO to Amp", mtVCA },     { "lfoTgtPW", "LFO to Pulse Width", mtPW },
};
struct ChoiceDef { const char* id; const char* name; int sp; juce::StringArray options; };
const ChoiceDef CHOICES[] = {
    { "lfoShape", "LFO Shape", spLFOShape, { "Square", "Triangle", "Random" } },
    { "tracking", "Filter Tracking", spTrackingShift, { "Off", "Half", "Full" } },
};

const char* const FACTORY_NAMES[] = {
    "Init", "Analog Brass", "Sync Lead", "Deep Bass", "Glass Pad", "Pluck"
};
constexpr int FACTORY_COUNT = 6;
} // namespace

ElfPro800Processor::ElfPro800Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", makeLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout ElfPro800Processor::makeLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    for (const auto& d : CONT)
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { d.id, 1 }, d.name,
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    for (const auto& d : BOOLS)
        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { d.id, 1 }, d.name, false));
    for (const auto& d : TGTS)
        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { d.id, 1 }, d.name, false));
    for (const auto& d : CHOICES)
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { d.id, 1 }, d.name, d.options, 0));
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { "arpMode", 1 }, "Arpeggiator Mode",
        juce::StringArray { "Off", "Up/Down", "Random", "Assign" }, 0));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "arpHold", 1 }, "Arpeggiator Hold", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "volume", 1 }, "Volume",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.8f));
    return { params.begin(), params.end() };
}

void ElfPro800Processor::applyParamsToEngine()
{
    bool changed = false;
    auto getF = [&] (const char* id_) { return apvts.getRawParameterValue (id_)->load(); };

    for (const auto& d : CONT) {
        const float v = getF (d.id);
        const uint16_t nv = (uint16_t) std::lround (v * 65535.0f);
        if (currentPreset.continuousParameters[d.cp] != nv) {
            currentPreset.continuousParameters[d.cp] = nv; changed = true;
        }
    }
    for (const auto& d : BOOLS) {
        const uint8_t nv = getF (d.id) > 0.5f ? 1 : 0;
        if (currentPreset.steppedParameters[d.sp] != nv) {
            currentPreset.steppedParameters[d.sp] = nv; changed = true;
        }
    }
    {
        uint8_t mask = 0;
        for (const auto& d : TGTS) if (getF (d.id) > 0.5f) mask |= (uint8_t) d.bit;
        /* preserve the OnlyA/OnlyB bits (not panel-exposed) */
        const uint8_t nv = (uint8_t) ((currentPreset.steppedParameters[spLFOTargets] & (mtOnlyA | mtOnlyB)) | mask);
        if (currentPreset.steppedParameters[spLFOTargets] != nv) {
            currentPreset.steppedParameters[spLFOTargets] = nv; changed = true;
        }
    }
    for (const auto& d : CHOICES) {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (d.id))) {
            const uint8_t nv = (uint8_t) p->getIndex();
            if (currentPreset.steppedParameters[d.sp] != nv) {
                currentPreset.steppedParameters[d.sp] = nv; changed = true;
            }
        }
    }
    if (changed) refreshFullState();

    /* arpeggiator: not a preset parameter, applied directly */
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("arpMode"))) {
        const int mode = p->getIndex();
        const int hold = getF ("arpHold") > 0.5f ? 1 : 0;
        if (mode != arpModeApplied || hold != arpHoldApplied) {
            arpModeApplied = mode; arpHoldApplied = hold;
            arp_setMode ((arpMode_t) mode, (int8_t) hold);
        }
    }
}

void ElfPro800Processor::syncParamsFromEngine()
{
    auto setF = [&] (const char* id_, float v01) {
        if (auto* p = apvts.getParameter (id_))
            p->setValueNotifyingHost (p->convertTo0to1 (juce::jlimit (0.0f, 1.0f, v01)));
    };
    for (const auto& d : CONT) setF (d.id, currentPreset.continuousParameters[d.cp] / 65535.0f);
    for (const auto& d : BOOLS) setF (d.id, currentPreset.steppedParameters[d.sp] ? 1.0f : 0.0f);
    for (const auto& d : TGTS)
        setF (d.id, (currentPreset.steppedParameters[spLFOTargets] & d.bit) ? 1.0f : 0.0f);
    for (const auto& d : CHOICES) setF (d.id, (float) currentPreset.steppedParameters[d.sp]);
}

void ElfPro800Processor::buildFactoryPresets()
{
    factory.clear();
    auto mk = [&] (std::initializer_list<std::pair<int, float>> cps,
                   std::initializer_list<std::pair<int, int>> sps) {
        struct preset_s p = basePreset;
        for (auto& kv : cps) p.continuousParameters[kv.first] = (uint16_t) std::lround (kv.second * 65535.0f);
        for (auto& kv : sps) p.steppedParameters[kv.first] = (uint8_t) kv.second;
        factory.push_back (p);
    };
    mk ({}, {}); /* Init = engine default */
    mk ({ { cpCutoff, .55f }, { cpResonance, .18f }, { cpFilEnvAmt, .50f }, { cpFilAtt, .16f },
          { cpFilDec, .38f }, { cpFilSus, .55f }, { cpFilRel, .30f }, { cpAmpAtt, .10f },
          { cpAmpDec, .30f }, { cpAmpSus, .85f }, { cpAmpRel, .32f }, { cpVolB, .55f } },
        { { spBSaw, 1 } }); /* Analog Brass */
    mk ({ { cpFreqB, .585f }, { cpCutoff, .72f }, { cpResonance, .30f }, { cpGlide, .22f },
          { cpAmpDec, .40f }, { cpAmpSus, .90f }, { cpDrive, .25f } },
        { { spSync, 1 }, { spASaw, 1 }, { spBSaw, 1 } }); /* Sync Lead */
    mk ({ { cpCutoff, .30f }, { cpResonance, .38f }, { cpFilEnvAmt, .68f }, { cpFilDec, .35f },
          { cpFilSus, .20f }, { cpAmpAtt, .02f }, { cpAmpDec, .45f }, { cpAmpSus, .95f },
          { cpDrive, .55f }, { cpVolB, .35f }, { cpFreqB, .25f } },
        { { spASqr, 1 } }); /* Deep Bass */
    mk ({ { cpCutoff, .50f }, { cpResonance, .12f }, { cpFilAtt, .45f }, { cpAmpAtt, .50f },
          { cpAmpRel, .60f }, { cpFilRel, .55f }, { cpSpread, .65f }, { cpLFOAmt, .18f },
          { cpLFOFreq, .22f }, { cpVolB, .70f } },
        { { spBTri, 1 }, { spLFOTargets, mtVCF } }); /* Glass Pad */
    mk ({ { cpAmpAtt, .01f }, { cpAmpDec, .22f }, { cpAmpSus, .05f }, { cpAmpRel, .18f },
          { cpFilDec, .25f }, { cpFilSus, .10f }, { cpCutoff, .62f }, { cpResonance, .42f },
          { cpFilEnvAmt, .55f }, { cpDrive, .20f } },
        { { spASaw, 1 } }); /* Pluck */
    presetsBuilt = true;
}

void ElfPro800Processor::loadPreset (int index)
{
    if (! presetsBuilt) return;
    index = juce::jlimit (0, (int) factory.size() - 1, index);
    currentPreset = factory[(size_t) index];
    refreshFullState();
    presetIndex.store (index);
    syncParamsFromEngine();
}

void ElfPro800Processor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    samplesPerTick = juce::jmax (1, (int) std::lround (sr / 2000.0));
    numVoices = 0;
    for (auto& v : voices) v = Voice {};
    p8_core_init (&core);
    preset_loadDefault (1);
    refreshFullState();
    basePreset = currentPreset;
    buildFactoryPresets();
    loadPreset (presetIndex.load());
    arpModeApplied = -1; /* force arp application on first block */
    /* Boot guard (V0 finding): notes gated during the engine's post-init
       preset load are dropped; run 200 silent ticks before going live. */
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

int ElfPro800Processor::getNumPrograms() { return FACTORY_COUNT; }
int ElfPro800Processor::getCurrentProgram() { return presetIndex.load(); }
void ElfPro800Processor::setCurrentProgram (int index) { presetRequest.store (index); }
const juce::String ElfPro800Processor::getProgramName (int index)
{
    return FACTORY_NAMES[juce::jlimit (0, FACTORY_COUNT - 1, index)];
}
void ElfPro800Processor::requestPresetDelta (int delta)
{
    presetRequest.store (juce::jlimit (0, FACTORY_COUNT - 1, presetIndex.load() + delta));
}
juce::String ElfPro800Processor::getPresetName() const { return getProgramName (presetIndex.load()); }

void ElfPro800Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int req = presetRequest.exchange (-1);
    if (req >= 0) loadPreset (req);
    applyParamsToEngine();

    const int numSamples = buffer.getNumSamples();
    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    const float volume = apvts.getRawParameterValue ("volume")->load();

    struct Ev { int tick; uint8_t bytes[3]; int len; };
    std::array<Ev, 256> evs; int numEvs = 0;
    for (const auto meta : midi) {
        if (numEvs >= (int) evs.size()) break;
        const auto msg = meta.getMessage();
        const int len = msg.getRawDataSize();
        if (len <= 0 || len > 3) continue;
        Ev& e = evs[(size_t) numEvs++];
        e.tick = meta.samplePosition / samplesPerTick;
        e.len = len;
        std::memcpy (e.bytes, msg.getRawData(), (size_t) len);
    }

    int evIdx = 0;
    const int ticksInBlock = (numSamples + samplesPerTick - 1) / samplesPerTick;
    for (int tick = 0; tick < ticksInBlock; ++tick) {
        p8_inputs_t in {};
        uint8_t chunk[3 * 64]; uint16_t n = 0;
        while (evIdx < numEvs && evs[(size_t) evIdx].tick <= tick) {
            const Ev& e = evs[(size_t) evIdx++];
            const uint8_t st = e.bytes[0] & 0xf0, note = e.len > 1 ? e.bytes[1] : 0, vel = e.len > 2 ? e.bytes[2] : 0;
            if (st == 0x90 && vel) {
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
    for (int v = 0; v < 8; ++v)
        voiceLevel[(size_t) v].store (frame.amp[v] / 65535.0f);
}

juce::AudioProcessorEditor* ElfPro800Processor::createEditor() { return new ElfPro800Editor (*this); }

void ElfPro800Processor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetIndex", presetIndex.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}
void ElfPro800Processor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType())) {
            auto tree = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (tree);
            presetRequest.store ((int) tree.getProperty ("presetIndex", 0));
            paramsDirty.store (true);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ElfPro800Processor(); }
