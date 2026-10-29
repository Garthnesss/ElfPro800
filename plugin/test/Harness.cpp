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
#include "synth.h" /* mixer_volumeFromMixAndDrive */
#include <vector>
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
static void setChoice (const char* id, int index)
{
    auto* p = P->apvts.getParameter (id);
    p->setValueNotifyingHost (p->convertTo0to1 ((float) index));
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
        { "freqA", cpFreqA }, { "pwA", cpAPW }, { "freqB", cpFreqB },
        { "pwB", cpBPW }, { "fineB", cpFreqBFine }, { "cutoff", cpCutoff },
        { "resonance", cpResonance }, { "filEnvAmt", cpFilEnvAmt }, { "filAtt", cpFilAtt },
        { "filDec", cpFilDec }, { "filSus", cpFilSus }, { "filRel", cpFilRel },
        { "ampAtt", cpAmpAtt }, { "ampDec", cpAmpDec }, { "ampSus", cpAmpSus }, { "ampRel", cpAmpRel },
        { "pmodFilEnv", cpPModFilEnv }, { "pmodOscB", cpPModOscB }, { "lfoFreq", cpLFOFreq },
        { "lfoAmt", cpLFOAmt }, { "unisonDetune", cpUnisonDetune },
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
    setChoice ("lfoShape", 2); runBlock (midi);
    CHECK (currentPreset.steppedParameters[spLFOShape] == 2, "lfoShape choice = Random");
    setChoice ("tracking", 1); runBlock (midi);
    CHECK (currentPreset.steppedParameters[spTrackingShift] == 1, "tracking choice = Half");
    /* 9. arp control reaches the arp module */
    setChoice ("arpMode", 3); runBlock (midi);
    CHECK (arp_getMode() == amAssign, "arp mode applied");
    setChoice ("arpMode", 0); runBlock (midi);
    CHECK (arp_getMode() == amOff, "arp off applied");
    noteOff (midi, 57); runBlock (midi);

    /* 5b. mixer law (SCI layout): Mix + Drive derive cpVolA/cpVolB, and
       the Glide knob writes the shared cpGlideVolB slot -> cpGlide. */
    setParam ("drive", 0.75f); setParam ("mix", 0.123f);
    runBlock (midi);
    {
        const uint16_t mix = (uint16_t) std::lround (0.123f * 65535.0f);
        const uint16_t drv = (uint16_t) std::lround (0.75f * 65535.0f);
        CHECK (std::abs ((int) currentPreset.continuousParameters[cpVolA]
                         - (int) mixer_volumeFromMixAndDrive (mix, drv)) <= 4, "mix -> volA law");
        CHECK (std::abs ((int) currentPreset.continuousParameters[cpVolB]
                         - (int) mixer_volumeFromMixAndDrive ((uint16_t) (65535 - mix), drv)) <= 4, "mix -> volB law");
        CHECK (currentPreset.continuousParameters[cpMixVolA] == mix, "mix stored");
        CHECK (currentPreset.continuousParameters[cpDrive] == drv, "drive stored");
    }
    setParam ("glide", 0.123f); runBlock (midi);
    CHECK (currentPreset.continuousParameters[cpGlide]
           == (uint16_t) std::lround (0.123f * 65535.0f), "glide via cpGlideVolB");
    setParam ("glide", 0.0f); runBlock (midi);

    /* 10. preset switching: Sync Lead (index 2) sets its cutoff */
    proc.setCurrentProgram (2);
    runBlock (midi);
    CHECK (proc.getCurrentProgram() == 2, "program index");
    CHECK (std::abs ((int) currentPreset.continuousParameters[cpCutoff] - (int) std::lround (0.72 * 65535.0)) <= 2,
           "preset 2 cutoff in engine");
    CHECK (proc.getPresetName() == "Sync Lead", "preset name");
    proc.setCurrentProgram (0);
    runBlock (midi);


    /* ================= section 11: audio-path proof =================
       The v1 harness proved parameters reach the engine; these render
       audio and prove the major controls are AUDIBLE in the output of
       the CV-driven renderer (Garth's Live-test feedback, 2026-10-28). */
    auto renderHeld = [&] (int captureBlocks) {
        std::vector<float> out;
        /* settle on the SHARED midi buffer: the first blockRMS call is
           what delivers the note-on queued by the test */
        for (int i = 0; i < 25; ++i) blockRMS (midi);
        for (int i = 0; i < captureBlocks; ++i) {
            juce::AudioBuffer<float> buf (2, BLOCK);
            buf.clear();
            juce::MidiBuffer m2;
            P->processBlock (buf, m2);
            out.insert (out.end(), buf.getReadPointer (0), buf.getReadPointer (0) + BLOCK);
        }
        return out;
    };
    struct M { double rms, bright, zcr; };
    auto measure = [&] (const std::vector<float>& x) {
        M m { 0, 0, 0 };
        double s = 0, d = 0; long cross = 0;
        for (size_t i = 0; i < x.size(); ++i) {
            s += (double) x[i] * x[i];
            if (i) { const double df = x[i] - x[i - 1]; d += df * df; }
            if (i && ((x[i - 1] < 0) != (x[i] < 0))) ++cross;
        }
        m.rms = std::sqrt (s / (double) x.size());
        m.bright = m.rms > 1e-6 ? std::sqrt (d / (double) x.size()) / m.rms : 0.0;
        m.zcr = cross * SR / (double) x.size();
        return m;
    };
    auto resetPanel = [&] {
        proc.setCurrentProgram (0);
        juce::MidiBuffer m; runBlock (m);
        setParam ("drive", 0.0f); setParam ("mix", 0.0f); /* engine default mix law inputs */
        runBlock (m);
        /* default preset: mix=1.0 (all A) drive=0 in cp terms; restore audible A level via drive */
        setParam ("mix", 0.0f); setParam ("drive", 0.75f); runBlock (m);
    };

    resetPanel();
    noteOn (midi, 60);
    auto base = measure (renderHeld (60));
    std::printf ("base: rms %.4f bright %.3f zcr %.1f\n", base.rms, base.bright, base.zcr);
    CHECK (base.rms > 0.02, "renderer base sound");

    /* pitch: freqA +0.25 of knob = +12 semitones (probe calibration) */
    setParam ("freqA", 0.25f); runBlock (midi);
    auto up = measure (renderHeld (60));
    std::printf ("freqA+12st: zcr %.1f (ratio %.2f)\n", up.zcr, up.zcr / base.zcr);
    CHECK (up.zcr / base.zcr > 1.65 && up.zcr / base.zcr < 2.4, "freqA knob transposes +12st");
    setParam ("freqA", 0.0f); runBlock (midi);

    /* waveform: triangle is much darker than saw */
    setParam ("aSaw", 0.0f); setParam ("aTri", 1.0f); runBlock (midi);
    auto tri = measure (renderHeld (60));
    std::printf ("tri: bright %.3f vs saw %.3f\n", tri.bright, base.bright);
    CHECK (tri.bright < base.bright * 0.65, "triangle darker than saw");
    setParam ("aTri", 0.0f); setParam ("aSqr", 1.0f); runBlock (midi);

    /* pulse width: a 50% pulse has NO even harmonics; a narrow pulse is
       full of them. Measure 2nd-harmonic / fundamental (Goertzel). */
    auto goertzel = [&] (const std::vector<float>& x, double freq) {
        const double w = 2.0 * juce::MathConstants<double>::pi * freq / SR;
        const double cw = std::cos (w), coeff = 2.0 * cw;
        double s0 = 0, s1 = 0, s2 = 0;
        for (float v : x) { s0 = v + coeff * s1 - s2; s2 = s1; s1 = s0; }
        return std::sqrt (s1 * s1 + s2 * s2 - coeff * s1 * s2) / (double) x.size();
    };
    auto harmRatio = [&] (const std::vector<float>& x) {
        return goertzel (x, 523.25) / juce::jmax (1e-9, goertzel (x, 261.63));
    };
    setParam ("pwA", 0.5f); runBlock (midi);
    auto pw50 = renderHeld (60);
    setParam ("pwA", 0.13f); runBlock (midi);
    auto pw13 = renderHeld (60);
    const double r50 = harmRatio (pw50), r13 = harmRatio (pw13);
    std::printf ("pw50 2nd/fund %.3f pw13 %.3f\n", r50, r13);
    CHECK (r13 > r50 * 4.0 && r13 > 0.25, "pulse width reshapes harmonic content");
    setParam ("aSqr", 0.0f); setParam ("aSaw", 1.0f); setParam ("pwA", 0.5f); runBlock (midi);

    /* osc B alone, +12 st via freqB */
    setParam ("mix", 1.0f); setParam ("bSaw", 1.0f); setParam ("freqB", 0.25f); runBlock (midi);
    auto bOnly = measure (renderHeld (60));
    std::printf ("oscB+12st: zcr %.1f (ratio %.2f)\n", bOnly.zcr, bOnly.zcr / base.zcr);
    CHECK (bOnly.zcr / base.zcr > 1.6 && bOnly.zcr / base.zcr < 2.45, "osc B path + transpose");
    resetPanel(); runBlock (midi);

    /* resonance: audible peak at fixed cutoff */
    setParam ("cutoff", 0.45f); setParam ("resonance", 0.05f); runBlock (midi);
    auto resLo = measure (renderHeld (60));
    setParam ("resonance", 0.95f); runBlock (midi);
    auto resHi = measure (renderHeld (60));
    std::printf ("res: rms %.4f -> %.4f, bright %.3f -> %.3f\n", resLo.rms, resHi.rms, resLo.bright, resHi.bright);
    CHECK (resHi.rms > resLo.rms * 1.08 && resHi.bright > resLo.bright * 1.1, "resonance peaks audibly");
    resetPanel(); runBlock (midi);

    /* LFO -> pitch: windowed pitch wobbles */
    auto wobble = [&] (const std::vector<float>& x) {
        const int W = 8; const size_t wlen = x.size() / W;
        double mean = 0, z[8];
        for (int w = 0; w < W; ++w) {
            long c = 0;
            for (size_t i = (size_t) w * wlen + 1; i < (size_t) (w + 1) * wlen; ++i)
                if ((x[i - 1] < 0) != (x[i] < 0)) ++c;
            z[w] = c * SR / (double) wlen; mean += z[w] / W;
        }
        double var = 0; for (int w = 0; w < W; ++w) var += (z[w] - mean) * (z[w] - mean) / W;
        return mean > 0 ? std::sqrt (var) / mean : 0.0;
    };
    setParam ("lfoAmt", 0.85f); setParam ("lfoFreq", 0.30f);
    setParam ("lfoTgtPitch", 1.0f); setChoice ("lfoShape", 1); runBlock (midi);
    auto wob = wobble (renderHeld (150));
    setParam ("lfoAmt", 0.0f); runBlock (midi);
    auto steady = wobble (renderHeld (150));
    std::printf ("lfo wobble %.3f vs steady %.3f\n", wob, steady);
    CHECK (wob > steady * 2.5 && wob > 0.05, "LFO modulates pitch audibly");
    noteOff (midi, 60); runBlock (midi);

    /* arp rate lands in settings */
    setParam ("arpRate", 0.3f); runBlock (midi);
    CHECK (settings.seqArpClock == (uint16_t) std::lround (0.3f * 65535.0f), "arp rate -> settings.seqArpClock");

    std::printf ("HARNESS PASS\\n");
    return 0;
}
