#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>
#include <cstring>

extern "C" {
#include "engine_bridge.h" /* refreshFullState */
#include "arp.h"           /* arp_setMode, arpMode_t */
#include "clock.h"         /* clock_setSpeed */
#include "hardware.h"      /* storage_read / storage_write (preset pages) */
#include "synth.h"         /* mt* LFO target bits, mixer_updatePanelLayout */
}

/* ------------------------------------------------------------------ */
/* Parameter tables: the panel, bound to the engine's preset struct.   */
/* ------------------------------------------------------------------ */
namespace {
struct ContDef { const char* id; const char* name; int cp; };
const ContDef CONT[] = {
    { "freqA", "Osc A Freq", cpFreqA },       { "mix", "Osc Mix", cpMixVolA },
    { "pwA", "Osc A Pulse Width", cpAPW },    { "freqB", "Osc B Freq", cpFreqB },
    { "pwB", "Osc B Pulse Width", cpBPW },
    { "fineB", "Osc B Fine", cpFreqBFine },   { "cutoff", "Cutoff", cpCutoff },
    { "resonance", "Resonance", cpResonance },{ "filEnvAmt", "Filter Env Amt", cpFilEnvAmt },
    { "filAtt", "Filter Attack", cpFilAtt },  { "filDec", "Filter Decay", cpFilDec },
    { "filSus", "Filter Sustain", cpFilSus }, { "filRel", "Filter Release", cpFilRel },
    { "ampAtt", "Amp Attack", cpAmpAtt },     { "ampDec", "Amp Decay", cpAmpDec },
    { "ampSus", "Amp Sustain", cpAmpSus },    { "ampRel", "Amp Release", cpAmpRel },
    { "pmodFilEnv", "PolyMod Fil Env", cpPModFilEnv },
    { "pmodOscB", "PolyMod Osc B", cpPModOscB },
    { "lfoFreq", "LFO Freq", cpLFOFreq },     { "lfoAmt", "LFO Amount", cpLFOAmt },
    { "glide", "Glide", cpGlideVolB },            { "unisonDetune", "Unison Detune", cpUnisonDetune },
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
        juce::ParameterID { "arpRate", 1 }, "Arpeggiator Rate",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "volume", 1 }, "Volume",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.8f));
    return { params.begin(), params.end() };
}

void ElfPro800Processor::captureShadows()
{
    const size_t need = std::size (CONT) + std::size (BOOLS) + 1 + std::size (CHOICES);
    if (hostShadow.size() != need) {
        hostShadow.assign (need, -1.0f);
        engineBaseline.assign (need, -1.0f);
    }
    size_t i = 0;
    for (const auto& d : CONT) {
        engineBaseline[i] = currentPreset.continuousParameters[d.cp] / 65535.0f;
        hostShadow[i] = apvts.getRawParameterValue (d.id)->load();
        ++i;
    }
    for (const auto& d : BOOLS) {
        engineBaseline[i] = currentPreset.steppedParameters[d.sp] ? 1.0f : 0.0f;
        hostShadow[i] = apvts.getRawParameterValue (d.id)->load() > 0.5f ? 1.0f : 0.0f;
        ++i;
    }
    {
        uint8_t em = 0, hm = 0;
        for (const auto& d : TGTS) {
            if (currentPreset.steppedParameters[spLFOTargets] & d.bit) em |= (uint8_t) d.bit;
            if (apvts.getRawParameterValue (d.id)->load() > 0.5f) hm |= (uint8_t) d.bit;
        }
        engineBaseline[i] = (float) em;
        hostShadow[i] = (float) hm;
        ++i;
    }
    for (const auto& d : CHOICES) {
        engineBaseline[i] = (float) currentPreset.steppedParameters[d.sp];
        if (auto* cp = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (d.id)))
            hostShadow[i] = (float) cp->getIndex();
        ++i;
    }
}

void ElfPro800Processor::applyParamsToEngine()
{
    if (hostShadow.empty()) captureShadows();
    bool changed = false;
    auto raw = [&] (const char* id_) { return apvts.getRawParameterValue (id_)->load(); };
    constexpr float EPS = 0.0011f;
    size_t i = 0;

    for (const auto& d : CONT) {
        const float host = raw (d.id);
        const float eng = currentPreset.continuousParameters[d.cp] / 65535.0f;
        if (std::abs (host - hostShadow[i]) > EPS) {
            hostShadow[i] = host;
            if (std::abs (host - eng) > EPS) {
                currentPreset.continuousParameters[d.cp] = (uint16_t) std::lround (host * 65535.0f);
                changed = true;
            }
            engineBaseline[i] = host;
        } else if (std::abs (eng - engineBaseline[i]) > EPS) {
            engineBaseline[i] = eng; /* a preset load moved the engine; follow it */
        }
        ++i;
    }
    for (const auto& d : BOOLS) {
        const float host = raw (d.id) > 0.5f ? 1.0f : 0.0f;
        const float eng = currentPreset.steppedParameters[d.sp] ? 1.0f : 0.0f;
        if (host != hostShadow[i]) {
            hostShadow[i] = host;
            if (host != eng) {
                currentPreset.steppedParameters[d.sp] = host > 0.5f ? 1 : 0;
                changed = true;
            }
            engineBaseline[i] = host;
        } else if (eng != engineBaseline[i]) {
            engineBaseline[i] = eng;
        }
        ++i;
    }
    {
        uint8_t hostMask = 0;
        for (const auto& d : TGTS) if (raw (d.id) > 0.5f) hostMask |= (uint8_t) d.bit;
        const uint8_t engMask = (uint8_t) (currentPreset.steppedParameters[spLFOTargets] & (mtVCO | mtVCF | mtVCA | mtPW));
        if ((float) hostMask != hostShadow[i]) {
            hostShadow[i] = (float) hostMask;
            if (hostMask != engMask) {
                currentPreset.steppedParameters[spLFOTargets] =
                    (uint8_t) ((currentPreset.steppedParameters[spLFOTargets] & (mtOnlyA | mtOnlyB)) | hostMask);
                changed = true;
            }
            engineBaseline[i] = (float) hostMask;
        } else if ((float) engMask != engineBaseline[i]) {
            engineBaseline[i] = (float) engMask;
        }
        ++i;
    }
    for (const auto& d : CHOICES) {
        auto* cp = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (d.id));
        if (cp == nullptr) { ++i; continue; }
        const float host = (float) cp->getIndex();
        const float eng = (float) currentPreset.steppedParameters[d.sp];
        if (host != hostShadow[i]) {
            hostShadow[i] = host;
            if (host != eng) {
                currentPreset.steppedParameters[d.sp] = (uint8_t) host;
                changed = true;
            }
            engineBaseline[i] = host;
        } else if (eng != engineBaseline[i]) {
            engineBaseline[i] = eng;
        }
        ++i;
    }
    if (changed) refreshFullState();

    /* arp/seq rate lives in settings (the preset copy is a display
       mirror); the engine clock takes it via clock_setSpeed. */
    {
        const uint16_t rv = (uint16_t) std::lround (raw ("arpRate") * 65535.0f);
        if (rv != arpRateApplied) {
            arpRateApplied = rv;
            settings.seqArpClock = rv;
            currentPreset.continuousParameters[cpSeqArpClock] = rv;
            clock_setSpeed (rv);
        }
    }

    /* arpeggiator: not a preset parameter, applied directly */
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("arpMode"))) {
        const int mode = p->getIndex();
        const int hold = raw ("arpHold") > 0.5f ? 1 : 0;
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
    captureShadows();
}

void ElfPro800Processor::scanUserSlots()
{
    for (int i = 0; i < USER_SLOTS; ++i)
        userEmpty[(size_t) i] = preset_checkPage ((uint16_t) i) == 0;
}

void ElfPro800Processor::loadProgram (int program)
{
    program = juce::jlimit (0, NUM_PROGRAMS - 1, program);
    if (program < FACTORY_PROGRAMS) {
        loadPreset (program);
    } else {
        const int slot = program - FACTORY_PROGRAMS;
        /* On an empty/corrupt page the engine falls back to the default
           preset inside loadCurrent - exactly the Init behaviour. */
        preset_loadCurrent ((uint16_t) slot, 0);
        refreshFullState();
        presetIndex.store (program);
        syncParamsFromEngine();
    }
    arpRateApplied = 0xffff; /* a stored page may carry its own speed mirror */
}

void ElfPro800Processor::requestProgram (int program) { presetRequest.store (program); }

void ElfPro800Processor::requestSaveToCurrent()
{
    const int p = presetIndex.load();
    if (p >= FACTORY_PROGRAMS) saveRequest.store (p - FACTORY_PROGRAMS);
}

bool ElfPro800Processor::programIsEmpty (int program) const
{
    if (program < FACTORY_PROGRAMS || program >= NUM_PROGRAMS) return false;
    return userEmpty[(size_t) (program - FACTORY_PROGRAMS)];
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
          { cpFilDec, .38f }, { cpFilSus, .55f }, { cpFilRel, .30f }, { cpAmpAtt, .45f },
          { cpAmpDec, .30f }, { cpAmpSus, .85f }, { cpAmpRel, .32f }, { cpMixVolA, .45f }, { cpDrive, .55f } },
        { { spBSaw, 1 } }); /* Analog Brass */
    mk ({ { cpFreqB, .585f }, { cpCutoff, .72f }, { cpResonance, .30f }, { cpGlide, .22f },
          { cpAmpDec, .40f }, { cpAmpSus, .90f }, { cpDrive, .60f }, { cpMixVolA, .50f } },
        { { spSync, 1 }, { spASaw, 1 }, { spBSaw, 1 } }); /* Sync Lead */
    mk ({ { cpCutoff, .30f }, { cpResonance, .38f }, { cpFilEnvAmt, .68f }, { cpFilDec, .35f },
          { cpFilSus, .20f }, { cpAmpAtt, .02f }, { cpAmpDec, .45f }, { cpAmpSus, .95f },
          { cpDrive, .70f }, { cpMixVolA, .30f }, { cpFreqB, .25f } },
        { { spASqr, 1 } }); /* Deep Bass */
    mk ({ { cpCutoff, .50f }, { cpResonance, .12f }, { cpFilAtt, .45f }, { cpAmpAtt, .50f },
          { cpAmpRel, .60f }, { cpFilRel, .55f }, { cpSpread, .65f }, { cpLFOAmt, .18f },
          { cpLFOFreq, .22f }, { cpMixVolA, .55f }, { cpDrive, .50f } },
        { { spBTri, 1 }, { spLFOTargets, mtVCF } }); /* Glass Pad */
    mk ({ { cpAmpAtt, .01f }, { cpAmpDec, .22f }, { cpAmpSus, .05f }, { cpAmpRel, .18f },
          { cpFilDec, .25f }, { cpFilSus, .10f }, { cpCutoff, .62f }, { cpResonance, .42f },
          { cpFilEnvAmt, .55f }, { cpDrive, .55f }, { cpMixVolA, .50f } },
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
    for (auto& v : dvoices) v = DVoice {};
    p8_core_init (&core);
    preset_loadDefault (1);
    refreshFullState();
    /* Boot guard FIRST (V0 finding: the post-init preset load settles
       over ~200 ticks). The panel-layout conversion below must only run
       after the load completes: converting mid-load reads half-loaded
       oscillator levels and derives Mix=0.5 / Drive=0, which the mixer
       law turns into vol_a = vol_b = 0 - a silent synth. (Found by the
       harness the day the renderer started listening to the vol CVs;
       it had been latent since the SCI switch, inaudible under the old
       placeholder DSP.) */
    {
        p8_inputs_t in {}; p8_cv_frame_t f {};
        for (int i = 0; i < 200; ++i) p8_core_tick (&core, &in, &f);
        frame = f;
    }
    /* SCI panel layout: oscillator levels are derived from the Mix and
       Drive parameters (and Glide shares the cpGlideVolB pot slot) - the
       engine recomputes cpVolA/cpVolB/cpGlide from those on every refresh,
       so the panel binds to the source parameters, never the derived ones. */
    settings.panelLayout = 1;
    mixer_updatePanelLayout (1);
    refreshFullState();
    basePreset = currentPreset;
    buildFactoryPresets();
    loadPreset (presetIndex.load());
    scanUserSlots();
    arpModeApplied = -1; /* force arp application on first block */
    if (auto* p = apvts.getParameter ("arpRate"))
        p->setValueNotifyingHost (p->convertTo0to1 (settings.seqArpClock / 65535.0f));
    arpRateApplied = 0xffff;
}

void ElfPro800Processor::releaseResources() {}
bool ElfPro800Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

int ElfPro800Processor::getNumPrograms() { return NUM_PROGRAMS; }
int ElfPro800Processor::getCurrentProgram() { return presetIndex.load(); }
void ElfPro800Processor::setCurrentProgram (int index) { presetRequest.store (index); }
const juce::String ElfPro800Processor::getProgramName (int index)
{
    index = juce::jlimit (0, NUM_PROGRAMS - 1, index);
    if (index < FACTORY_PROGRAMS) return FACTORY_NAMES[index];
    return "User " + juce::String (index - FACTORY_PROGRAMS).paddedLeft ('0', 2);
}
void ElfPro800Processor::requestPresetDelta (int delta)
{
    presetRequest.store (juce::jlimit (0, NUM_PROGRAMS - 1, presetIndex.load() + delta));
}
juce::String ElfPro800Processor::getPresetName() const
{
    const int p = presetIndex.load();
    if (p < FACTORY_PROGRAMS) return FACTORY_NAMES[p];
    juce::String n = "User " + juce::String (p - FACTORY_PROGRAMS).paddedLeft ('0', 2);
    if (userEmpty[(size_t) (p - FACTORY_PROGRAMS)]) n += " (EMPTY)";
    return n;
}

/* ------------------------------------------------------------------ */
/* CV-driven voice renderer. The engine's CV frame is the single source
 * of truth, exactly as the DACs drive the hardware VCOs/VCF:
 *   pitch  <- osc_a/osc_b CVs (6148 units per octave, A440 = 21649,
 *             measured on the core 2026-10-28; knob offsets, glide,
 *             LFO, poly-mod, detune and tuner wiggles all ride the CV)
 *   waves  <- stepped params (saw/tri gates are frame bits; pulse uses
 *             the pw CVs, which the engine parks when square is off)
 *   levels <- vol_a/vol_b CVs (the Mix/Drive law is already applied)
 *   filter <- per-voice fil CV -> Hz (exponential fit), res CV -> Q
 *   amp    <- per-voice amp CV (the full envelope)
 * Oscillators are PolyBLEP (saw/pulse); the filter is a TPT SVF
 * (Zavalishin - unconditionally stable, see the Chamberlin lesson).   */
/* ------------------------------------------------------------------ */
namespace {
inline double freqFromCV (uint16_t cv)
{
    return juce::jlimit (8.0, 19000.0, 440.0 * std::pow (2.0, ((double) cv - 21649.0) / 6148.0));
}
inline double polyblep (double t, double dt)
{
    if (t < dt) { const double x = t / dt; return x + x - x * x - 1.0; }
    if (t > 1.0 - dt) { const double x = (t - 1.0) / dt; return x * x + x + x + 1.0; }
    return 0.0;
}
inline double waveSaw (double ph, double dt) { return 2.0 * ph - 1.0 - polyblep (ph, dt); }
inline double waveTri (double ph) { return ph < 0.5 ? 4.0 * ph - 1.0 : 3.0 - 4.0 * ph; }
inline double wavePulse (double ph, double dt, double duty)
{
    double y = ph < duty ? 1.0 : -1.0;
    y += polyblep (ph, dt);
    double t2 = ph - duty; if (t2 < 0.0) t2 += 1.0;
    y -= polyblep (t2, dt);
    return y;
}
} // namespace

void ElfPro800Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int req = presetRequest.exchange (-1);
    if (req >= 0) loadProgram (req);
    applyParamsToEngine();
    {
        const int sv = saveRequest.exchange (-1);
        if (sv >= 0 && sv < USER_SLOTS) {
            preset_saveCurrent ((uint16_t) sv);
            userEmpty[(size_t) sv] = false;
            saveFlash.fetch_add (1);
        }
    }

    const int numSamples = buffer.getNumSamples();
    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    const float volume = apvts.getRawParameterValue ("volume")->load();

    const bool sawA = currentPreset.steppedParameters[spASaw] != 0;
    const bool triA = currentPreset.steppedParameters[spATri] != 0;
    const bool sqrA = currentPreset.steppedParameters[spASqr] != 0;
    const bool sawB = currentPreset.steppedParameters[spBSaw] != 0;
    const bool triB = currentPreset.steppedParameters[spBTri] != 0;
    const bool sqrB = currentPreset.steppedParameters[spBSqr] != 0;
    const bool syncOn = currentPreset.steppedParameters[spSync] != 0;
    const int wavesA = (sawA ? 1 : 0) + (triA ? 1 : 0) + (sqrA ? 1 : 0);
    const int wavesB = (sawB ? 1 : 0) + (triB ? 1 : 0) + (sqrB ? 1 : 0);
    const double gainA = wavesA ? 0.62 / wavesA : 0.0;
    const double gainB = wavesB ? 0.62 / wavesB : 0.0;

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
            if (n + e.len <= sizeof (chunk)) { std::memcpy (chunk + n, e.bytes, (size_t) e.len); n = (uint16_t) (n + e.len); }
        }
        if (n) { in.midi_in = chunk; in.midi_in_len = n; }
        p8_core_tick (&core, &in, &frame);

        /* per-tick (CV-rate) DSP coefficients */
        const double resN = frame.res / 65535.0;
        const double kk = juce::jmax (0.05, 2.0 * (1.0 - resN * 0.99));
        const double dutyA = juce::jlimit (0.03, 0.97, frame.pw_a / 65535.0);
        const double dutyB = juce::jlimit (0.03, 0.97, frame.pw_b / 65535.0);
        const double lvlA = frame.vol_a / 65535.0, lvlB = frame.vol_b / 65535.0;
        double incA[8], incB[8], a1[8], a2[8], a3[8];
        for (int v = 0; v < 8; ++v) {
            incA[v] = freqFromCV (frame.osc_a[v]) / sr;
            incB[v] = freqFromCV (frame.osc_b[v]) / sr;
            const double x = frame.fil[v] / 65535.0;
            const double fc = juce::jlimit (40.0, 17500.0, 40.0 + 20000.0 * std::pow (x, 2.5));
            const double g = std::tan (juce::MathConstants<double>::pi * fc / sr);
            a1[v] = 1.0 / (1.0 + g * (g + kk));
            a2[v] = g * a1[v];
            a3[v] = g * a2[v];
        }

        const int start = tick * samplesPerTick;
        const int end = juce::jmin (numSamples, start + samplesPerTick);
        for (int s = start; s < end; ++s) {
            double mix = 0.0;
            for (int v = 0; v < 8; ++v) {
                DVoice& dv = dvoices[(size_t) v];
                const double amp = frame.amp[v] / 65535.0;
                double osc = 0.0;
                if (wavesA) {
                    double w = 0.0;
                    if (sawA) w += waveSaw (dv.phA, incA[v]);
                    if (triA) w += waveTri (dv.phA);
                    if (sqrA) w += wavePulse (dv.phA, incA[v], dutyA);
                    osc += w * gainA * lvlA;
                }
                if (wavesB) {
                    double w = 0.0;
                    if (sawB) w += waveSaw (dv.phB, incB[v]);
                    if (triB) w += waveTri (dv.phB);
                    if (sqrB) w += wavePulse (dv.phB, incB[v], dutyB);
                    osc += w * gainB * lvlB;
                }
                dv.phA += incA[v];
                if (dv.phA >= 1.0) { dv.phA -= 1.0; if (syncOn) dv.phB = 0.0; }
                dv.phB += incB[v];
                if (dv.phB >= 1.0) dv.phB -= 1.0;
                /* TPT SVF lowpass */
                const double v3 = osc - dv.ic2;
                const double v1 = a1[v] * dv.ic1 + a2[v] * v3;
                const double v2 = dv.ic2 + a2[v] * dv.ic1 + a3[v] * v3;
                dv.ic1 = 2.0 * v1 - dv.ic1;
                dv.ic2 = 2.0 * v2 - dv.ic2;
                mix += v2 * amp;
            }
            const double soft = mix * 0.9;
            const float out = (float) (volume * (soft / (1.0 + std::abs (soft))));
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
    /* Persist the 100 user preset pages (the first 25,600 bytes of the
       engine's EEPROM image) with the session. Settings and sequencer
       pages are deliberately NOT included. */
    juce::MemoryBlock pages ((size_t) USER_SLOTS * 256);
    for (int i = 0; i < USER_SLOTS; ++i)
        storage_read ((uint32_t) i, (uint8_t*) pages.getData() + (size_t) i * 256);
    state.setProperty ("userPages", juce::var (pages), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}
void ElfPro800Processor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType())) {
            auto tree = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (tree);
            if (auto* mb = tree.getProperty ("userPages").getBinaryData()) {
                if (mb->getSize() == (size_t) USER_SLOTS * 256) {
                    for (int i = 0; i < USER_SLOTS; ++i)
                        storage_write ((uint32_t) i, (uint8_t*) mb->getData() + (size_t) i * 256);
                    scanUserSlots();
                }
            }
            presetRequest.store ((int) tree.getProperty ("presetIndex", 0));
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ElfPro800Processor(); }
