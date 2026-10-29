#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <cmath>
#include <map>

namespace {
const juce::Colour PANEL (0xff14161a), SECT (0xff16181d), EDGE (0xff2e3138),
                   ORANGE (0xffff7a1a), ORANGEHOT (0xffffa040), CREAM (0xfff2efe6),
                   GREY (0xffa9a69c), DIM (0xff7a776e), NUMGREY (0xffa8a599),
                   LEDGREEN (0xff85f594), LEDDIM (0xff4f9e6d), DISPBG (0xff140b06);

/* Knob geometry, shared by the LookAndFeel, the printed scales and the
   arc drawing: angle(v) = 135 deg + 270 deg * v, measured from the +x
   axis, clockwise, y down (min at bottom-left, max at bottom-right). */
float knobAngleRad (float v) { return (135.0f + 270.0f * v) * juce::MathConstants<float>::pi / 180.0f; }
juce::Point<float> onCircle (juce::Point<float> c, float r, float v)
{
    const float a = knobAngleRad (v);
    return { c.x + r * std::cos (a), c.y + r * std::sin (a) };
}
void strokeArc (juce::Graphics& g, juce::Point<float> c, float r, float v0, float v1, float thickness, juce::Colour col)
{
    if (v1 <= v0) return;
    juce::Path p;
    const int steps = juce::jmax (2, (int) ((v1 - v0) * 40.0f));
    for (int i = 0; i <= steps; ++i) {
        const auto pt = onCircle (c, r, v0 + (v1 - v0) * (float) i / (float) steps);
        if (i == 0) p.startNewSubPath (pt); else p.lineTo (pt);
    }
    g.setColour (col);
    g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

class PanelLAF final : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                           float, float, juce::Slider&) override
    {
        const float size = (float) juce::jmin (width, height);
        const auto c = juce::Point<float> (x + width * 0.5f, y + height * 0.5f);
        const float tickOuter = size * 0.5f - 1.0f;
        const float bodyR = tickOuter - 7.0f;

        for (int i = 0; i <= 10; ++i) { /* tick ring */
            const float v = (float) i / 10.0f;
            g.setColour (i == 5 ? juce::Colour (0xff6a6d72) : juce::Colour (0xff4a4d52));
            g.drawLine ({ onCircle (c, tickOuter - 4.0f, v), onCircle (c, tickOuter, v) }, 1.4f);
        }
        strokeArc (g, c, bodyR + 3.5f, 0.0f, 1.0f, 2.6f, juce::Colour (0xff2c2f36));       /* track */
        strokeArc (g, c, bodyR + 3.5f, 0.0f, sliderPos, 2.6f, ORANGE);                     /* value */
        {   /* body: dark radial shading + top-left sheen */
            juce::ColourGradient grad (juce::Colour (0xff33363d), c.x - bodyR * 0.4f, c.y - bodyR * 0.5f,
                                       juce::Colour (0xff17181c), c.x + bodyR * 0.3f, c.y + bodyR * 0.8f, true);
            g.setGradientFill (grad);
            g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
            g.setColour (juce::Colour (0xffe8e8e8).withAlpha (0.10f));
            g.fillEllipse (c.x - bodyR * 0.62f, c.y - bodyR * 0.78f, bodyR * 0.9f, bodyR * 0.5f);
            g.setColour (juce::Colour (0xff0c0d10));
            g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.2f);
            g.setColour (juce::Colour (0xff24262b));
            g.fillEllipse (c.x - bodyR * 0.66f, c.y - bodyR * 0.66f, bodyR * 1.32f, bodyR * 1.32f);
        }
        const auto dot = onCircle (c, bodyR * 0.66f, sliderPos); /* pointer dot */
        g.setColour (CREAM);
        g.fillEllipse (dot.x - 2.6f, dot.y - 2.6f, 5.2f, 5.2f);
    }
};
PanelLAF panelLAF;

double interpLog (const double* ys, double v)
{
    static const double xs[5] = { 0.0, 0.25, 0.5, 0.75, 1.0 };
    v = juce::jlimit (0.0, 1.0, v);
    const int i = juce::jmin (3, (int) (v / 0.25));
    const double f = (v - xs[i]) / 0.25;
    return std::exp (std::log (ys[i]) + (std::log (ys[i + 1]) - std::log (ys[i])) * f);
}
juce::String fmtMs (double ms)
{
    if (ms < 1000.0) return juce::String ((int) std::lround (ms)) + " MS";
    if (ms < 10000.0) return juce::String (ms / 1000.0, 1) + " S";
    if (ms < 120000.0) return juce::String ((int) std::lround (ms / 1000.0)) + " S";
    return juce::String (ms / 60000.0, 1) + " MIN";
}
juce::String fmtHz (double hz)
{
    if (hz < 0.995) return juce::String (hz, 2) + " HZ";
    if (hz < 9.95) return juce::String (hz, 1) + " HZ";
    if (hz < 995.0) return juce::String ((int) std::lround (hz)) + " HZ";
    return juce::String (hz / 1000.0, 2) + " KHZ";
}
double lfoHz (double v)
{   /* lfo.c: exponentialCourse speed law at the 500 Hz timer */
    const double cv = v * 65535.0;
    const double spd = std::exp (-(65535.0 - cv) / 8000.0) * 65535.0;
    const double cycleTicks = 2.0 * 16777216.0 / (spd * 16.0);
    return 500.0 / cycleTicks;
}
} // namespace

/* ================= value formatting (readout) ================= */

juce::String ElfPro800Editor::displayName (const juce::String& id)
{
    static const std::map<juce::String, juce::String> names = {
        { "freqA", "OSC A FREQ" }, { "freqB", "OSC B FREQ" }, { "fineB", "OSC B FINE" },
        { "pwA", "OSC A PULSE W" }, { "pwB", "OSC B PULSE W" }, { "mix", "OSC MIX" },
        { "drive", "DRIVE" }, { "cutoff", "CUTOFF" }, { "resonance", "RESONANCE" },
        { "filEnvAmt", "FILTER ENV AMT" }, { "filAtt", "FILTER ATTACK" }, { "filDec", "FILTER DECAY" },
        { "filSus", "FILTER SUSTAIN" }, { "filRel", "FILTER RELEASE" },
        { "ampAtt", "AMP ATTACK" }, { "ampDec", "AMP DECAY" }, { "ampSus", "AMP SUSTAIN" },
        { "ampRel", "AMP RELEASE" }, { "pmodFilEnv", "POLYMOD FIL ENV" }, { "pmodOscB", "POLYMOD OSC B" },
        { "lfoFreq", "LFO RATE" }, { "lfoAmt", "LFO AMOUNT" }, { "glide", "GLIDE" },
        { "ampVel", "AMP VELOCITY" }, { "filVel", "FILTER VELOCITY" },
        { "unisonDetune", "UNISON DETUNE" }, { "spread", "VOICE SPREAD" },
        { "arpRate", "ARP / GATE RATE" }, { "volume", "VOLUME" },
        { "aSaw", "OSC A SAW" }, { "aTri", "OSC A TRIANGLE" }, { "aSqr", "OSC A PULSE" },
        { "bSaw", "OSC B SAW" }, { "bTri", "OSC B TRIANGLE" }, { "bSqr", "OSC B PULSE" },
        { "sync", "HARD SYNC" }, { "pmodFA", "POLYMOD > FREQ A" }, { "pmodFil", "POLYMOD > FILTER" },
        { "unison", "UNISON" }, { "lfoTgtPitch", "LFO > PITCH" }, { "lfoTgtFilter", "LFO > FILTER" },
        { "lfoTgtAmp", "LFO > AMP" }, { "lfoTgtPW", "LFO > PW" }, { "arpHold", "ARP HOLD" },
        { "filEnvSlow", "FILTER ENV SLOW" }, { "ampEnvSlow", "AMP ENV SLOW" },
        { "lfoShape", "LFO SHAPE" }, { "tracking", "KEY TRACKING" }, { "arpMode", "ARP MODE" },
    };
    const auto it = names.find (id);
    return it != names.end() ? it->second : id.toUpperCase();
}

juce::String ElfPro800Editor::formatValue (const juce::String& id, float v01, bool slowFil, bool slowAmp)
{
    const double v = juce::jlimit (0.0f, 1.0f, v01);
    /* Envelope times measured on the real engine (probe, Oct 29 2026):
       attack to 90% of plateau, decay/release to 5%, log-interpolated.
       The SLOW toggles shift the ADSR speed by 2 bits = ~4x. */
    static const double ATT[5] = { 11, 66, 752, 9768, 134624 };
    static const double DEC[5] = { 4, 4, 186, 5946, 85726 };
    static const double REL[5] = { 6, 42, 484, 6242, 86022 };
    const auto pct = [] (double x) { return juce::String ((int) std::lround (x * 100.0)) + " %"; };

    if (id == "freqA" || id == "freqB") {
        const double st = (v - 0.5) * 48.0;
        return (st > 0.04 ? "+" : "") + juce::String (st, 1) + " ST";
    }
    if (id == "fineB") return (v >= 0.5 ? "+" : "") + juce::String ((int) std::lround ((v - 0.5) * 100.0)) + " CT";
    if (id == "pwA" || id == "pwB") return juce::String ((int) std::lround (1.0 + 98.0 * v)) + " %";
    if (id == "cutoff") return fmtHz (juce::jmin (17500.0, 40.0 + 20000.0 * std::pow (v, 2.5)));
    if (id == "lfoFreq") return fmtHz (lfoHz (v));
    if (id == "glide") { /* constant-rate slew, measured: ~0.097 s/oct at v=0, exponential */
        const double sPerOct = 0.0972 * std::exp (5.958 * v);
        return sPerOct < 0.995 ? juce::String ((int) std::lround (sPerOct * 1000.0)) + " MS/OCT" : fmtMs (sPerOct * 1000.0) + "/OCT";
    }
    if (id == "unisonDetune") return juce::String::fromUTF8 ("±") + juce::String ((int) std::lround (146.5 * v)) + " CT";
    if (id == "arpRate") {
        const double sps = ElfPro800Processor::seqStepsPerSec (v);
        return sps <= 0.0 ? juce::String ("STOPPED") : juce::String (sps, 1) + " ST/S";
    }
    if (id == "filAtt") return fmtMs (interpLog (ATT, v) * (slowFil ? 4.0 : 1.0));
    if (id == "filDec") return fmtMs (interpLog (DEC, v) * (slowFil ? 4.0 : 1.0));
    if (id == "filRel") return fmtMs (interpLog (REL, v) * (slowFil ? 4.0 : 1.0));
    if (id == "ampAtt") return fmtMs (interpLog (ATT, v) * (slowAmp ? 4.0 : 1.0));
    if (id == "ampDec") return fmtMs (interpLog (DEC, v) * (slowAmp ? 4.0 : 1.0));
    if (id == "ampRel") return fmtMs (interpLog (REL, v) * (slowAmp ? 4.0 : 1.0));
    return pct (v); /* levels, amounts, sustains, velocities, spread, volume */
}

/* ================= custom buttons ================= */

void ElfPro800Editor::LedButton::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    auto b = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = getToggleState();
    const bool filledNow = filled || (accent && on);
    g.setColour (filledNow ? ORANGE : (on ? juce::Colour (0xff3a2413) : juce::Colour (0xff22242a)));
    if (! isEnabled()) g.setColour (juce::Colour (0xff1b1d21));
    g.fillRoundedRectangle (b, 5.0f);
    g.setColour (filledNow || on ? juce::Colour (0xff8a4b1e) : EDGE);
    g.drawRoundedRectangle (b, 5.0f, 1.0f);
    if (highlighted && isEnabled()) { g.setColour (juce::Colours::white.withAlpha (0.05f)); g.fillRoundedRectangle (b, 5.0f); }

    const float ledCx = b.getX() + 11.0f, ledCy = b.getCentreY();
    if (on || filledNow) {
        g.setColour (filledNow ? juce::Colour (0xff2a1503) : ledColour);
        if (! filledNow) { g.setColour (ledColour.withAlpha (0.25f)); g.fillEllipse (ledCx - 6, ledCy - 6, 12, 12); g.setColour (ledColour); }
        g.fillEllipse (ledCx - 3, ledCy - 3, 6, 6);
    } else {
        g.setColour (juce::Colour (0xff3a3d44));
        g.fillEllipse (ledCx - 3, ledCy - 3, 6, 6);
    }
    g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
    g.setColour (! isEnabled() ? DIM : (filledNow ? juce::Colour (0xff2a1503) : (on ? juce::Colour (0xffffb27a) : juce::Colour (0xff8f8c82))));
    g.drawText (getButtonText(), b.withTrimmedLeft (12.0f), juce::Justification::centred, false);
}

void ElfPro800Editor::StepPad::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    auto full = getLocalBounds().toFloat();
    auto pad = full.withHeight (full.getHeight() - 14.0f).reduced (0.5f);
    g.setColour (stepOn ? juce::Colour (0xff4a2c14) : juce::Colour (0xff22242a));
    g.fillRoundedRectangle (pad, 5.0f);
    g.setColour (isCurrent && transportOn ? juce::Colour (0xff63e06b) : (stepOn ? juce::Colour (0xff8a4b1e) : EDGE));
    g.drawRoundedRectangle (pad, 5.0f, (isCurrent && transportOn) ? 1.6f : 1.0f);
    if (highlighted) { g.setColour (juce::Colours::white.withAlpha (0.05f)); g.fillRoundedRectangle (pad, 5.0f); }
    const float ledCy = pad.getY() + 10.0f;
    if (stepOn) {
        g.setColour (juce::Colour (0xffffa040).withAlpha (0.25f));
        g.fillEllipse (pad.getCentreX() - 6, ledCy - 6, 12, 12);
        g.setColour (juce::Colour (0xffffa040));
    } else g.setColour (juce::Colour (0xff3a3d44));
    g.fillEllipse (pad.getCentreX() - 3, ledCy - 3, 6, 6);
    if (index % 4 == 0) { g.setColour (DIM); g.fillRect (pad.getCentreX() - 0.5f, pad.getBottom() - 8.0f, 1.0f, 4.0f); }
    g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
    g.setColour (stepOn ? CREAM : DIM);
    g.drawText (juce::String (index + 1), full.withTop (full.getBottom() - 13.0f), juce::Justification::centred, false);
}

void ElfPro800Editor::PlayButton::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    const auto c = b.getCentre();
    const float r = b.getWidth() * 0.5f - 1.0f;
    g.setColour (playing ? juce::Colour (0xff1d3a20) : juce::Colour (0xff22242a));
    g.fillEllipse (b);
    g.setColour (juce::Colour (0xff63e06b));
    g.drawEllipse (b, playing ? 1.8f : 1.2f);
    if (highlighted) { g.setColour (juce::Colours::white.withAlpha (0.06f)); g.fillEllipse (b); }
    g.setColour (juce::Colour (0xff63e06b));
    if (playing) {
        g.fillRoundedRectangle (c.x - 6.0f, c.y - 6.0f, 12.0f, 12.0f, 1.5f);
    } else {
        juce::Path tri;
        tri.addTriangle (c.x - 5.0f, c.y - 8.0f, c.x - 5.0f, c.y + 8.0f, c.x + 8.0f, c.y);
        g.fillPath (tri);
    }
}

/* ================= construction ================= */

ElfPro800Editor::ElfPro800Editor (ElfPro800Processor& p) : AudioProcessorEditor (p), proc (p)
{
    setLookAndFeel (&panelLAF);

    kLfoFreq = &addKnob ("lfoFreq", "RATE");   kLfoAmt = &addKnob ("lfoAmt", "AMOUNT");
    segLfoShape = &addSeg ("lfoShape", { "SQR", "TRI", "RND" });
    tLfoPitch = &addToggle ("lfoTgtPitch", "PITCH"); tLfoFilter = &addToggle ("lfoTgtFilter", "FILTER");
    tLfoAmp = &addToggle ("lfoTgtAmp", "AMP");       tLfoPW = &addToggle ("lfoTgtPW", "PW");

    kFreqA = &addKnob ("freqA", "FREQ"); kPwA = &addKnob ("pwA", "PULSE W");
    tASaw = &addToggle ("aSaw", "SAW"); tATri = &addToggle ("aTri", "TRI"); tASqr = &addToggle ("aSqr", "PULSE");

    kFreqB = &addKnob ("freqB", "FREQ"); kFineB = &addKnob ("fineB", "FINE"); kPwB = &addKnob ("pwB", "PULSE W");
    tBSaw = &addToggle ("bSaw", "SAW"); tBTri = &addToggle ("bTri", "TRI"); tBSqr = &addToggle ("bSqr", "PLS");
    tSync = &addToggle ("sync", "SYNC", true);

    kMix = &addKnob ("mix", "OSC MIX"); kDrive = &addKnob ("drive", "DRIVE");

    kPmodFilEnv = &addKnob ("pmodFilEnv", "FIL ENV"); kPmodOscB = &addKnob ("pmodOscB", "OSC B");
    tPmodFA = &addToggle ("pmodFA", "FREQ A"); tPmodFil = &addToggle ("pmodFil", "FILTER");

    kCutoff = &addKnob ("cutoff", "CUTOFF"); kReso = &addKnob ("resonance", "RESONANCE");
    kFilEnvAmt = &addKnob ("filEnvAmt", "ENV AMT");
    segTracking = &addSeg ("tracking", { "OFF", "1/2", "FULL" });

    kFilAtt = &addKnob ("filAtt", "ATTACK"); kFilDec = &addKnob ("filDec", "DECAY");
    kFilSus = &addKnob ("filSus", "SUSTAIN"); kFilRel = &addKnob ("filRel", "RELEASE");
    tFilSlow = &addToggle ("filEnvSlow", "SLOW");
    kAmpAtt = &addKnob ("ampAtt", "ATTACK"); kAmpDec = &addKnob ("ampDec", "DECAY");
    kAmpSus = &addKnob ("ampSus", "SUSTAIN"); kAmpRel = &addKnob ("ampRel", "RELEASE");
    tAmpSlow = &addToggle ("ampEnvSlow", "SLOW");
    kFilVel = &addKnob ("filVel", "VEL"); kAmpVel = &addKnob ("ampVel", "VEL");

    kGlide = &addKnob ("glide", "GLIDE"); kDetune = &addKnob ("unisonDetune", "DETUNE"); kSpread = &addKnob ("spread", "SPREAD");
    tUnison = &addToggle ("unison", "UNISON");
    segArpMode = &addSeg ("arpMode", { "OFF", "UPDN", "RND", "ASN" });
    kArpRate = &addKnob ("arpRate", "RATE");
    tArpHold = &addToggle ("arpHold", "HOLD");
    kVolume = &addKnob ("volume", "VOLUME");
    kVolume->label.setVisible (false);

    sections = {
        { { 28, 106, 200, 216 }, "LFO" }, { { 240, 106, 262, 216 }, "OSCILLATOR A" },
        { { 514, 106, 262, 216 }, "OSCILLATOR B" }, { { 788, 106, 196, 216 }, "MIXER" },
        { { 996, 106, 296, 216 }, "POLY-MOD" }, { { 28, 336, 330, 216 }, "FILTER" },
        { { 370, 336, 296, 216 }, "FILTER ENVELOPE" }, { { 678, 336, 296, 216 }, "AMPLIFIER ENVELOPE" },
        { { 986, 336, 306, 216 }, "VOICE · ARPEGGIATOR" }, { { 28, 566, 820, 140 }, "SEQUENCER" },
        { { 860, 566, 432, 140 }, "PRESET BROWSER" },
    };

    prevBtn.onClick = [this] { proc.requestPresetDelta (-1); };
    nextBtn.onClick = [this] { proc.requestPresetDelta (+1); };
    for (auto* b : { &prevBtn, &nextBtn }) {
        b->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff23262c));
        b->setColour (juce::TextButton::textColourOffId, CREAM);
        addAndMakeVisible (*b);
    }
    factoryBtn.setButtonText ("FACTORY");
    factoryBtn.onClick = [this] { proc.requestProgram (0); };
    userBtn.setButtonText ("USER");
    userBtn.onClick = [this] { proc.requestProgram (ElfPro800Processor::FACTORY_PROGRAMS); };
    saveBtn.setButtonText ("SAVE");
    saveBtn.filled = true;
    saveBtn.onClick = [this] { proc.requestSaveToCurrent(); };
    for (auto* b : { &factoryBtn, &userBtn, &saveBtn }) addAndMakeVisible (*b);

    playBtn.onClick = [this] { proc.seqSetPlaying (! proc.seqIsPlaying()); };
    addAndMakeVisible (playBtn);
    recBtn.setButtonText ("REC");
    recBtn.ledColour = juce::Colour (0xffff4444);
    recBtn.setClickingTogglesState (true);
    recBtn.onClick = [this] { proc.seqSetRec (recBtn.getToggleState()); };
    addAndMakeVisible (recBtn);
    for (int i = 0; i < 16; ++i) {
        auto pad = std::make_unique<StepPad>();
        pad->index = i;
        pad->onClick = [this, i] { proc.seqToggleStep (i); };
        addAndMakeVisible (*pad);
        pads[(size_t) i] = std::move (pad);
    }

    lcdNum.setFont (juce::Font (juce::FontOptions ("Courier New", 26.0f, juce::Font::bold)));
    lcdNum.setColour (juce::Label::textColourId, juce::Colour (0xff7dffb0));
    addAndMakeVisible (lcdNum);
    lcdName.setFont (juce::Font (juce::FontOptions ("Courier New", 17.0f, juce::Font::plain)));
    lcdName.setColour (juce::Label::textColourId, juce::Colour (0xffb8ffd2));
    addAndMakeVisible (lcdName);
    lcdReadout.setFont (juce::Font (juce::FontOptions (11.5f, juce::Font::bold)));
    lcdReadout.setColour (juce::Label::textColourId, LEDGREEN);
    addAndMakeVisible (lcdReadout);

    for (auto* param : proc.getParameters()) param->addListener (this);

    setSize (1320, 764);
    startTimerHz (30);
}

ElfPro800Editor::~ElfPro800Editor()
{
    stopTimer();
    for (auto* param : proc.getParameters()) param->removeListener (this);
    setLookAndFeel (nullptr);
}

void ElfPro800Editor::parameterValueChanged (int parameterIndex, float)
{
    lastParamIndex.store (parameterIndex);
    lastEditMs.store (juce::Time::currentTimeMillis());
}

ElfPro800Editor::Knob& ElfPro800Editor::addKnob (const char* id, const char* label)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (k->slider);
    k->label.setText (label, juce::dontSendNotification);
    k->label.setJustificationType (juce::Justification::centred);
    k->label.setColour (juce::Label::textColourId, GREY);
    k->label.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
    addAndMakeVisible (k->label);
    k->att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->slider);
    knobs.push_back (std::move (k));
    return *knobs.back();
}

ElfPro800Editor::Toggle& ElfPro800Editor::addToggle (const char* id, const char* label, bool accent)
{
    auto t = std::make_unique<Toggle>();
    t->btn.setButtonText (label);
    t->btn.setClickingTogglesState (true);
    t->btn.accent = accent;
    addAndMakeVisible (t->btn);
    t->att = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, id, t->btn);
    toggles.push_back (std::move (t));
    return *toggles.back();
}

ElfPro800Editor::Seg& ElfPro800Editor::addSeg (const char* id, juce::StringArray options)
{
    auto s = std::make_unique<Seg>();
    s->paramId = id;
    for (int i = 0; i < options.size(); ++i) {
        auto b = std::make_unique<LedButton>();
        b->setButtonText (options[i]);
        const int idx = i;
        b->onClick = [this, id, idx] {
            if (auto* p = proc.apvts.getParameter (id))
                p->setValueNotifyingHost (p->convertTo0to1 ((float) idx));
        };
        addAndMakeVisible (*b);
        s->btns.push_back (std::move (b));
    }
    segs.push_back (std::move (s));
    return *segs.back();
}

void ElfPro800Editor::refreshSeg (Seg& s)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (s.paramId))) {
        const int idx = p->getIndex();
        if (idx != s.paintedIndex) {
            s.paintedIndex = idx;
            for (int i = 0; i < (int) s.btns.size(); ++i)
                s.btns[(size_t) i]->setToggleState (i == idx, juce::dontSendNotification);
        }
    }
}

void ElfPro800Editor::placeKnob (Knob& k, float cx, float cy, int size)
{
    k.slider.setBounds ((int) cx - size / 2, (int) cy - size / 2, size, size);
    k.label.setBounds ((int) cx - 42, (int) cy + size / 2 + 3, 84, 12);
    k.centre = { cx, cy };
}

/* ================= layout (Figma design coordinates) ================= */

void ElfPro800Editor::resized()
{
    /* LFO */
    placeKnob (*kLfoFreq, 80, 170, 60);  placeKnob (*kLfoAmt, 176, 170, 60);
    segLfoShape->btns[0]->setBounds (44, 230, 52, 22); segLfoShape->btns[1]->setBounds (102, 230, 52, 22); segLfoShape->btns[2]->setBounds (160, 230, 52, 22);
    tLfoPitch->btn.setBounds (44, 268, 80, 22);  tLfoAmp->btn.setBounds (132, 268, 80, 22);
    tLfoFilter->btn.setBounds (44, 294, 80, 22); tLfoPW->btn.setBounds (132, 294, 80, 22);
    /* OSC A */
    placeKnob (*kFreqA, 302, 182, 76); placeKnob (*kPwA, 398, 182, 64);
    tASaw->btn.setBounds (264, 256, 64, 28); tATri->btn.setBounds (336, 256, 64, 28); tASqr->btn.setBounds (408, 256, 72, 28);
    /* OSC B */
    placeKnob (*kFreqB, 572, 182, 76); placeKnob (*kFineB, 648, 182, 54); placeKnob (*kPwB, 716, 182, 64);
    tBSaw->btn.setBounds (532, 256, 52, 28); tBTri->btn.setBounds (590, 256, 52, 28); tBSqr->btn.setBounds (648, 256, 52, 28); tSync->btn.setBounds (706, 256, 56, 28);
    /* MIXER */
    placeKnob (*kMix, 840, 182, 64); placeKnob (*kDrive, 932, 182, 64);
    /* POLY-MOD */
    placeKnob (*kPmodFilEnv, 1062, 182, 64); placeKnob (*kPmodOscB, 1166, 182, 64);
    tPmodFA->btn.setBounds (1020, 266, 124, 26); tPmodFil->btn.setBounds (1156, 266, 124, 26);
    /* FILTER */
    placeKnob (*kCutoff, 102, 432, 100); placeKnob (*kReso, 204, 420, 64); placeKnob (*kFilEnvAmt, 292, 420, 64);
    placeKnob (*kFilVel, 204, 504, 52);
    segTracking->btns[0]->setBounds (244, 494, 30, 24); segTracking->btns[1]->setBounds (279, 494, 30, 24); segTracking->btns[2]->setBounds (314, 494, 34, 24);
    /* FILTER ENVELOPE */
    placeKnob (*kFilAtt, 412, 414, 60); placeKnob (*kFilDec, 482, 414, 60); placeKnob (*kFilSus, 552, 414, 60); placeKnob (*kFilRel, 622, 414, 60);
    tFilSlow->btn.setBounds (394, 486, 76, 26);
    /* AMPLIFIER ENVELOPE */
    placeKnob (*kAmpAtt, 720, 414, 60); placeKnob (*kAmpDec, 790, 414, 60); placeKnob (*kAmpSus, 860, 414, 60); placeKnob (*kAmpRel, 930, 414, 60);
    tAmpSlow->btn.setBounds (702, 486, 76, 26);
    placeKnob (*kAmpVel, 930, 510, 48);
    /* VOICE · ARPEGGIATOR */
    placeKnob (*kGlide, 1034, 414, 60); placeKnob (*kDetune, 1114, 414, 60); placeKnob (*kSpread, 1194, 414, 60);
    tUnison->btn.setBounds (1236, 348, 52, 24);
    placeKnob (*kArpRate, 1034, 510, 52);
    segArpMode->btns[0]->setBounds (1076, 478, 44, 26); segArpMode->btns[1]->setBounds (1124, 478, 52, 26);
    segArpMode->btns[2]->setBounds (1180, 478, 44, 26); segArpMode->btns[3]->setBounds (1228, 478, 52, 26);
    tArpHold->btn.setBounds (1076, 514, 84, 24);
    /* header */
    placeKnob (*kVolume, 884, 56, 64);
    lcdNum.setBounds (944, 32, 84, 36);
    lcdName.setBounds (1032, 30, 246, 22);
    lcdReadout.setBounds (1032, 57, 250, 14);
    /* SEQUENCER */
    playBtn.setBounds (52, 592, 46, 46);
    recBtn.setBounds (48, 656, 54, 22);
    for (int i = 0; i < 16; ++i) pads[(size_t) i]->setBounds (132 + i * 44, 592, 38, 58);
    /* PRESET BROWSER */
    prevBtn.setBounds (884, 610, 40, 44);
    nextBtn.setBounds (1248, 610, 40, 44);
    factoryBtn.setBounds (932, 668, 92, 26);
    userBtn.setBounds (1032, 668, 92, 26);
    saveBtn.setBounds (1132, 668, 108, 26);
}

/* ================= painting ================= */

void ElfPro800Editor::paintScales (juce::Graphics& g)
{
    g.setColour (NUMGREY);
    g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
    auto num = [&] (juce::Point<float> c, float r, float v, const juce::String& s) {
        const auto pt = onCircle (c, r, v);
        g.drawText (s, (int) pt.x - 17, (int) pt.y - 6, 34, 12, juce::Justification::centred, false);
    };
    for (auto* k : { kFreqA, kFreqB }) { /* semitones, +-24 over the throw */
        num (k->centre, 54.0f, 0.00f, "-24"); num (k->centre, 54.0f, 0.25f, "-12");
        num (k->centre, 54.0f, 0.50f, "0");   num (k->centre, 54.0f, 0.75f, "+12");
        num (k->centre, 54.0f, 1.00f, "+24");
    }
    num (kFineB->centre, 40.0f, 0.0f, "-50"); num (kFineB->centre, 40.0f, 1.0f, "+50");
    for (const double hz : { 100.0, 1000.0, 10000.0 }) { /* cutoff Hz, renderer's own law */
        const float v = (float) std::pow ((hz - 40.0) / 20000.0, 0.4);
        num (kCutoff->centre, 76.0f, v, hz >= 1000.0 ? juce::String ((int) (hz / 1000.0)) + "k" : juce::String ((int) hz));
    }
    for (const double hz : { 0.1, 1.0, 10.0 }) { /* LFO Hz, firmware's exponential law */
        const double halfTicks = 250.0 / hz;
        const double spd = (16777216.0 / halfTicks) / 16.0;
        const float v = (float) ((65535.0 + 8000.0 * std::log (spd / 65535.0)) / 65535.0);
        num (kLfoFreq->centre, 47.0f, v, hz < 1.0 ? juce::String ("0.1") : juce::String ((int) hz));
    }
}

void ElfPro800Editor::paintMeters (juce::Graphics& g)
{
    const float vals[2] = { meterA, meterB };
    const float cxs[2] = { 840.0f, 932.0f };
    for (int col = 0; col < 2; ++col)
        for (int i = 0; i < 8; ++i) {
            const float y = 288.0f - (float) i * 8.0f;
            const bool lit = vals[col] * 8.0f > (float) i + 0.15f;
            g.setColour (! lit ? juce::Colour (0xff1d2b20)
                               : (i < 6 ? juce::Colour (0xff63e06b) : (i == 6 ? ORANGE : juce::Colour (0xffff5540))));
            g.fillEllipse (cxs[col] - 3.0f, y - 3.0f, 6.0f, 6.0f);
        }
}

void ElfPro800Editor::paint (juce::Graphics& g)
{
    juce::ColourGradient bg (juce::Colour (0xff191c21), 0.0f, 0.0f, juce::Colour (0xff0f1114), 0.0f, 764.0f, false);
    g.setGradientFill (bg);
    g.fillAll();
    for (auto pt : { juce::Point<float> (16, 16), juce::Point<float> (1304, 16), juce::Point<float> (16, 748), juce::Point<float> (1304, 748) }) { /* corner screws */
        g.setColour (juce::Colour (0xff23262c)); g.fillEllipse (pt.x - 6, pt.y - 6, 12, 12);
        g.setColour (EDGE); g.drawEllipse (pt.x - 6, pt.y - 6, 12, 12, 1.0f);
        g.drawLine (pt.x - 3.5f, pt.y, pt.x + 3.5f, pt.y, 1.2f);
    }
    g.setColour (CREAM);
    g.setFont (juce::Font (juce::FontOptions (30.0f, juce::Font::bold)));
    g.drawText ("ELFPRO800", 28, 16, 320, 34, juce::Justification::topLeft, false);
    g.setColour (DIM);
    g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
    g.drawText ("E I G H T - V O I C E   P O L Y P H O N I C   S Y N T H E S I Z E R", 30, 54, 420, 12, juce::Justification::topLeft, false);
    g.drawText ("VOICES", 574, 24, 188, 11, juce::Justification::centred, false);
    for (int i = 0; i < 8; ++i) { /* voice LEDs */
        const float cx = 580.0f + (float) i * 24.0f, cy = 56.0f;
        if (ledLevel[(size_t) i] > 0.03f) {
            g.setColour (juce::Colour (0xff63e06b).withAlpha (0.22f)); g.fillEllipse (cx - 9, cy - 9, 18, 18);
            g.setColour (juce::Colour (0xff63e06b)); g.fillEllipse (cx - 5, cy - 5, 10, 10);
        } else { g.setColour (juce::Colour (0xff1d2b20)); g.fillEllipse (cx - 5, cy - 5, 10, 10); }
    }
    g.setColour (DIM);
    g.drawText ("VOLUME", 852, 8, 64, 11, juce::Justification::centred, false);

    g.setColour (DISPBG); /* LED display */
    g.fillRoundedRectangle (lcdBounds.toFloat(), 8.0f);
    g.setColour (juce::Colour (0xff592914));
    g.drawRoundedRectangle (lcdBounds.toFloat(), 8.0f, 1.2f);

    for (const auto& s : sections) { /* section boxes */
        g.setColour (SECT);
        g.fillRoundedRectangle (s.bounds.toFloat(), 10.0f);
        g.setColour (EDGE);
        g.drawRoundedRectangle (s.bounds.toFloat(), 10.0f, 1.0f);
        g.setColour (ORANGE);
        g.fillRect (s.bounds.getX() + 12, s.bounds.getY() + 10, 5, 12);
        g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
        g.drawText (s.title, s.bounds.getX() + 24, s.bounds.getY() + 8, s.bounds.getWidth() - 30, 14, juce::Justification::topLeft, false);
    }
    g.setColour (DIM); /* captions */
    g.setFont (juce::Font (juce::FontOptions (9.0f, juce::Font::bold)));
    g.drawText ("SHAPE", 44, 216, 80, 11, juce::Justification::topLeft, false);
    g.drawText ("TARGETS", 44, 254, 80, 11, juce::Justification::topLeft, false);
    g.drawText ("WAVEFORM SELECT", 264, 240, 200, 11, juce::Justification::topLeft, false);
    g.drawText ("WAVEFORM · HARD SYNC", 532, 240, 220, 11, juce::Justification::topLeft, false);
    g.drawText ("LEVEL", 856, 300, 60, 11, juce::Justification::centred, false);
    g.drawText ("SOURCES", 1020, 238, 120, 11, juce::Justification::topLeft, false);
    g.drawText ("DESTINATIONS", 1020, 254, 160, 11, juce::Justification::topLeft, false);
    g.drawText ("KEY TRACK", 244, 478, 100, 11, juce::Justification::topLeft, false);
    g.drawText ("ARPEGGIATOR", 1076, 462, 140, 11, juce::Justification::topLeft, false);
    g.drawText ("PLAY", 52, 642, 46, 10, juce::Justification::centred, false);
    g.drawText ("STEP GATE · CLICK A STEP TO TOGGLE · REC CAPTURES HELD NOTES", 300, 574, 532, 12, juce::Justification::topRight, false);
    g.drawText ("6 FACTORY + 100 USER SLOTS · SAVED WITH YOUR SONG", 932, 656, 308, 11, juce::Justification::centred, false);

    paintScales (g);
    paintMeters (g);

    {   /* sequencer progress bar */
        g.setColour (juce::Colour (0xff22242a));
        g.fillRect (132.0f, 662.0f, 698.0f, 4.0f);
        if (paintedPlaying) {
            g.setColour (ORANGE);
            g.fillRect (132.0f, 662.0f, 698.0f * ((float) (paintedStep + 1) / 16.0f), 4.0f);
        }
    }
    {   /* preset browser display */
        auto d = juce::Rectangle<float> (932.0f, 610.0f, 308.0f, 44.0f);
        g.setColour (DISPBG); g.fillRoundedRectangle (d, 6.0f);
        g.setColour (juce::Colour (0xff592914)); g.drawRoundedRectangle (d, 6.0f, 1.0f);
        g.setColour (ORANGEHOT);
        g.setFont (juce::Font (juce::FontOptions (15.0f, juce::Font::bold)));
        const int idx = proc.getPresetIndex();
        g.drawText (juce::String::formatted ("%03d · ", idx + 1) + proc.getPresetName(), d, juce::Justification::centred, false);
    }
    g.setColour (DIM);
    g.setFont (juce::Font (juce::FontOptions (9.5f, juce::Font::bold)));
    g.drawText ("DESIGNED BY GARTH ELFSTROM", 30, 728, 400, 12, juce::Justification::topLeft, false);
    g.drawText ("MADE WITH THE P600 ENGINE · GPL-3.0", 790, 728, 500, 12, juce::Justification::topRight, false);
}

/* ================= refresh ================= */

void ElfPro800Editor::timerCallback()
{
    for (int v = 0; v < 8; ++v)
        ledLevel[(size_t) v] = juce::jmax (ledLevel[(size_t) v] * 0.72f, proc.getVoiceLevel (v));
    meterA = juce::jmax (meterA * 0.80f, proc.getMixLevelA());
    meterB = juce::jmax (meterB * 0.80f, proc.getMixLevelB());

    const int idx = proc.getPresetIndex();
    const int flash = proc.getSaveFlash();
    const bool flashFired = (flash != paintedSaveFlash);
    if (flashFired) { paintedSaveFlash = flash; lcdFlashTicks = 12; }
    if (lcdFlashTicks > 0) --lcdFlashTicks;
    if (idx != paintedPreset || flashFired) {
        paintedPreset = idx;
        lcdNum.setText (juce::String::formatted ("%03d", idx + 1), juce::dontSendNotification);
        lcdName.setText (proc.getPresetName(), juce::dontSendNotification);
    }
    lcdName.setColour (juce::Label::textColourId, lcdFlashTicks > 0 ? ORANGEHOT : juce::Colour (0xffb8ffd2));
    saveBtn.setEnabled (proc.canSave());

    /* last-touched parameter readout */
    juce::String newReadout;
    const int pi = lastParamIndex.load();
    if (pi >= 0 && juce::Time::currentTimeMillis() - lastEditMs.load() < 2600) {
        auto* param = proc.getParameters()[(size_t) pi];
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param)) {
            const juce::String id = rp->paramID;
            const bool sF = proc.apvts.getRawParameterValue ("filEnvSlow")->load() > 0.5f;
            const bool sA = proc.apvts.getRawParameterValue ("ampEnvSlow")->load() > 0.5f;
            const bool custom = id != "lfoShape" && id != "tracking" && id != "arpMode"
                                && ! id.endsWith ("Saw") && ! id.endsWith ("Tri") && ! id.endsWith ("Sqr");
            const juce::String val = (custom && dynamic_cast<juce::AudioParameterFloat*> (param) != nullptr)
                ? formatValue (id, param->getValue(), sF, sA)
                : (param->getValue() > 0.5f || dynamic_cast<juce::AudioParameterChoice*> (param) != nullptr
                       ? param->getCurrentValueAsText() : juce::String ("OFF"));
            if (dynamic_cast<juce::AudioParameterBool*> (param) != nullptr)
                newReadout = displayName (id) + "   " + (param->getValue() > 0.5f ? "ON" : "OFF");
            else
                newReadout = displayName (id) + "   " + val.toUpperCase();
        }
    }
    if (newReadout.isEmpty())
        newReadout = idx < ElfPro800Processor::FACTORY_PROGRAMS ? "FACTORY BANK" : "USER BANK";
    if (newReadout != readoutText) { readoutText = newReadout; lcdReadout.setText (readoutText, juce::dontSendNotification); }

    for (auto& s : segs) refreshSeg (*s);

    const uint32_t pat = proc.seqGetPattern();
    const int step = proc.seqGetStep();
    const bool playing = proc.seqIsPlaying(), rec = proc.seqIsRec();
    if (pat != paintedPattern || step != paintedStep || playing != paintedPlaying || rec != paintedRec) {
        paintedPattern = pat; paintedStep = step; paintedPlaying = playing; paintedRec = rec;
        playBtn.playing = playing;
        if (recBtn.getToggleState() != rec) recBtn.setToggleState (rec, juce::dontSendNotification);
        for (int i = 0; i < 16; ++i) {
            auto& pad = *pads[(size_t) i];
            pad.stepOn = (pat >> (unsigned) i) & 1u;
            pad.isCurrent = (i == step);
            pad.transportOn = playing;
        }
    }
    repaint();
}
