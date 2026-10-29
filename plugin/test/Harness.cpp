/* Headless harness: drives the real processor (no GUI, no audio device)
 * and verifies the whole panel path: MIDI -> engine -> audible output,
 * every APVTS parameter -> the engine's preset struct / arp state, and
 * preset switching. Exits non-zero on the first failure. */
#include <JuceHeader.h>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "../source/PluginProcessor.h"
extern "C" {
#include "storage.h"
#include "arp.h"
}

static ElfPro800Processor* P = nullptr;
static const double SR = 48000.0;
static const int BLOCK = 512;

#define CHECK(cond, msg) do { if (! (cond)) { std::printf ("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); return 1; } } while (0)

static void runBlock (juce::MidiBuffer& midi)
{
    juce::AudioBuffer<float> buf (2, BLOCK);
    buf.clear();
    P->processBlock (buf, midi);
    midi.clear();
}
static double blockRMS (juce::MidiBuffer& midi)
{
    juce::AudioBuffer<float> buf (2, BLOCK);
    buf.clear();
    P->processBlock (buf, midi);
    midi.clear();
    double s = 0; for (int i = 0; i < BLOCK; ++i) { const double x = buf.getSample (0, i); s += x * x; }
    return std::sqrt (s / BLOCK);
}
static void setParam (const char* id, float v01)
{
    auto* p = P->apvts.getParameter (id);
    p->setValueNotifyingHost (p->convertTo0to1 (v01));
}
static void noteOn (juce::MidiBuffer& m, int note)  { m.addEvent (juce::MidiMessage::noteOn (1, note, 0.8f), 0); }
static void noteOff (juce::MidiBuffer& m, int note) { m.addEvent (juce::MidiMessage::noteOff (1, note), 0); }

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    ElfPro800Processor proc;
    P = &proc;
    proc.prepareToPlay (SR, BLOCK);
    juce::MidiBuffer midi;

    /* 1. silence with no notes */
    double rms = 0; for (int i = 0; i < 20; ++i) rms = juce::jmax (rms, blockRMS (midi));
    CHECK (rms < 1e-4, "silence without notes");

    /* 2. a held chord sounds */
    noteOn (midi, 60); noteOn (midi, 64); noteOn (midi, 67);
    double sustain = 0; for (int i = 0; i < 90; ++i) sustain = blockRMS (midi);
    std::printf ("sustain RMS %.4f\n", sustain);
    CHECK (sustain > 0.01, "chord sustains audibly");

    /* 3. cutoff reaches the sound: samples differ between extremes */
    auto renderWith = [&] (float cutoff) {
        setParam ("cutoff", cutoff);
        juce::AudioBuffer<float> buf (2, BLOCK); buf.clear();
        juce::MidiBuffer m2; P->processBlock (buf, m2);
        return std::vector<float> (buf.getReadPointer (0), buf.getReadPointer (0) + BLOCK);
    };
    auto dark = renderWith (0.03f), bright = renderWith (1.0f);
    CHECK (std::memcmp (dark.data(), bright.data(), BLOCK * sizeof (float)) != 0,
           "cutoff changes the rendered audio");
    setParam ("cutoff", 0.68f);

    /* 4. release: notes off -> tail dies away */
    noteOff (midi, 60); noteOff (midi, 64); noteOff (midi, 67);
    double tail = 0; for (int i = 0; i < 200; ++i) tail = blockRMS (midi);
    std::printf ("tail RMS %.5f\n", tail);
    CHECK (tail < sustain * 0.25, "release tail decays");

    /* 5. every continuous parameter lands in the engine preset struct */
    struct CP { const char* id; int cp; };
    const CP cps[] = {
        { "freqA", cpFreqA }, { "volA", cpVolA }, { "pwA", cpAPW }, { "freqB", cpFreqB },
        { "volB", cpVolB }, { "pwB", cpBPW }, { "fineB", cpFreqBFine }, { "cutoff", cpCutoff },
        { "resonance", cpResonance }, { "filEnvAmt", cpFilEnvAmt }, { "filAtt", cpFilAtt },
        { "filDec", cpFilDec }, { "filSus", cpFilSus }, { "filRel", cpFilRel },
        { "ampAtt", cpAmpAtt }, { "ampDec", cpAmpDec }, { "ampSus", cpAmpSus }, { "ampRel", cpAmpRel },
        { "pmodFilEnv", cpPModFilEnv }, { "pmodOscB", cpPModOscB }, { "lfoFreq", cpLFOFreq },
        { "lfoAmt", cpLFOAmt }, { "glide", cpGlide }, { "unisonDetune", cpUnisonDetune },
        { "spread", cpSpread }, { "drive", cpDrive }, { "ampVel", cpAmpVelocity }, { "filVel", cpFilVelocity },
    };
    noteOn (midi, 57);
    for (const auto& e : cps) {
        setParam (e.id, 0.123f);
        runBlock (midi);
        const int got = currentPreset.continuousParameters[e.cp];
        CHECK (std::abs (got - (int) std::lround (0.123f * 65535.0f)) <= 2, e.id);
        setParam (e.id, 0.5f);
        runBlock (midi);
    }
    /* 6. every boolean lands in steppedParameters */
    struct BP { const char* id; int sp; };
    const BP bps[] = {
        { "aSaw", spASaw }, { "aTri", spATri }, { "aSqr", spASqr }, { "bSaw", spBSaw },
        { "bTri", spBTri }, { "bSqr", spBSqr }, { "sync", spSync }, { "pmodFA", spPModFA },
        { "pmodFil", spPModFil }, { "unison", spUnison },
    };
    for (const auto& e : bps) {
        setParam (e.id, 1.0f); runBlock (midi);
        CHECK (currentPreset.steppedParameters[e.sp] == 1, e.id);
        setParam (e.id, 0.0f); runBlock (midi);
        CHECK (currentPreset.steppedParameters[e.sp] == 0, e.id);
    }
    /* 7. LFO target bitmask */
    setParam ("lfoTgtFilter", 1.0f); setParam ("lfoTgtPW", 1.0f);
    runBlock (midi);
    CHECK ((currentPreset.steppedParameters[spLFOTargets] & (mtVCF | mtPW)) == (mtVCF | mtPW), "lfo targets mask");
    setParam ("lfoTgtFilter", 0.0f); setParam ("lfoTgtPW", 0.0f);
    runBlock (midi);
    /* 8. choices land too */
    setParam ("lfoShape", 1.0f); runBlock (midi);
    CHECK (currentPreset.steppedParameters[spLFOShape] == 2, "lfoShape choice = Random");
    setParam ("tracking", 0.5f); runBlock (midi);
    CHECK (currentPreset.steppedParameters[spTrackingShift] == 1, "tracking choice = Half");
    /* 9. arp control reaches the arp module */
    setParam ("arpMode", 1.0f); runBlock (midi);
    CHECK (arp_getMode() == amAssign, "arp mode applied");
    setParam ("arpMode", 0.0f); runBlock (midi);
    CHECK (arp_getMode() == amOff, "arp off applied");
    noteOff (midi, 57); runBlock (midi);

    /* 10. preset switching: Sync Lead (index 2) sets its cutoff */
    proc.setCurrentProgram (2);
    runBlock (midi);
    CHECK (proc.getCurrentProgram() == 2, "program index");
    CHECK (std::abs ((int) currentPreset.continuousParameters[cpCutoff] - (int) std::lround (0.72 * 65535.0)) <= 2,
           "preset 2 cutoff in engine");
    CHECK (proc.getPresetName() == "Sync Lead", "preset name");
    proc.setCurrentProgram (0);
    runBlock (midi);

    std::printf ("HARNESS PASS\n");
    return 0;
}
