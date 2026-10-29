#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace {
const juce::Colour PANEL (0xff14161a), SECT (0xff1b1e24), EDGE (0xff2c2f36),
                   ORANGE (0xffff7a1a), CREAM (0xfff2efe6), GREY (0xffa9a69c),
                   DIM (0xff7a776e), LCDGREEN (0xff7dffb0), LCDDIM (0xff4f9e6d);
}

ElfPro800Editor::ElfPro800Editor (ElfPro800Processor& p) : AudioProcessorEditor (p), proc (p)
{
    kLfoFreq = &addKnob ("lfoFreq", "FREQ");   kLfoAmt = &addKnob ("lfoAmt", "AMOUNT");
    segLfoShape = &addSeg ("lfoShape", { "SQUARE", "TRI", "RANDOM" });
    tLfoPitch = &addToggle ("lfoTgtPitch", "PITCH"); tLfoFilter = &addToggle ("lfoTgtFilter", "FILTER");
    tLfoAmp = &addToggle ("lfoTgtAmp", "AMP");       tLfoPW = &addToggle ("lfoTgtPW", "PW");

    kFreqA = &addKnob ("freqA", "FREQ"); kPwA = &addKnob ("pwA", "PULSE WIDTH");
    tASaw = &addToggle ("aSaw", "SAW"); tATri = &addToggle ("aTri", "TRI"); tASqr = &addToggle ("aSqr", "PULSE");

    kFreqB = &addKnob ("freqB", "FREQ"); kFineB = &addKnob ("fineB", "FINE"); kPwB = &addKnob ("pwB", "PULSE WIDTH");
    tBSaw = &addToggle ("bSaw", "SAW"); tBTri = &addToggle ("bTri", "TRI"); tBSqr = &addToggle ("bSqr", "PULSE");
    tSync = &addToggle ("sync", "SYNC");

    kVolA = &addKnob ("volA", "OSC A"); kVolB = &addKnob ("volB", "OSC B"); kDrive = &addKnob ("drive", "DRIVE");

    kPmodFilEnv = &addKnob ("pmodFilEnv", "FIL ENV"); kPmodOscB = &addKnob ("pmodOscB", "OSC B");
    tPmodFA = &addToggle ("pmodFA", "FREQ A"); tPmodFil = &addToggle ("pmodFil", "FILTER");

    kCutoff = &addKnob ("cutoff", "CUTOFF"); kReso = &addKnob ("resonance", "RESONANCE");
    kFilEnvAmt = &addKnob ("filEnvAmt", "ENV AMT");
    segTracking = &addSeg ("tracking", { "OFF", "HALF", "FULL" });

    kFilAtt = &addKnob ("filAtt", "ATTACK"); kFilDec = &addKnob ("filDec", "DECAY");
    kFilSus = &addKnob ("filSus", "SUSTAIN"); kFilRel = &addKnob ("filRel", "RELEASE");
    kAmpAtt = &addKnob ("ampAtt", "ATTACK"); kAmpDec = &addKnob ("ampDec", "DECAY");
    kAmpSus = &addKnob ("ampSus", "SUSTAIN"); kAmpRel = &addKnob ("ampRel", "RELEASE");

    kGlide = &addKnob ("glide", "GLIDE"); kAmpVel = &addKnob ("ampVel", "AMP VEL"); kFilVel = &addKnob ("filVel", "FIL VEL");
    kDetune = &addKnob ("unisonDetune", "DETUNE"); kSpread = &addKnob ("spread", "SPREAD");
    tUnison = &addToggle ("unison", "UNISON");

    segArpMode = &addSeg ("arpMode", { "OFF", "UP/DOWN", "RANDOM", "ASSIGN" });
    tArpHold = &addToggle ("arpHold", "HOLD");
    kVolume = &addKnob ("volume", "VOLUME");

    prevBtn.onClick = [this] { proc.requestPresetDelta (-1); };
    nextBtn.onClick = [this] { proc.requestPresetDelta (+1); };
    for (auto* b : { &prevBtn, &nextBtn }) {
        b->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff23262c));
        b->setColour (juce::TextButton::textColourOffId, CREAM);
        addAndMakeVisible (*b);
    }
    lcdNum.setFont (juce::Font (juce::FontOptions ("Courier New", 26.0f, juce::Font::bold)));
    lcdNum.setColour (juce::Label::textColourId, LCDGREEN);
    addAndMakeVisible (lcdNum);
    lcdName.setFont (juce::Font (juce::FontOptions ("Courier New", 19.0f, juce::Font::plain)));
    lcdName.setColour (juce::Label::textColourId, juce::Colour (0xffb8ffd2));
    addAndMakeVisible (lcdName);

    setSize (1160, 648);
    startTimerHz (15);
}

ElfPro800Editor::Knob& ElfPro800Editor::addKnob (const char* id, const char* label)
{
    auto k = std::make_unique<Knob>();
    k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    k->slider.setColour (juce::Slider::rotarySliderFillColourId, ORANGE);
    k->slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff3a3d44));
    k->slider.setColour (juce::Slider::thumbColourId, CREAM);
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

void ElfPro800Editor::styleToggle (juce::TextButton& b, bool on)
{
    b.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff3a2413));
    b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff22242a));
    b.setColour (juce::TextButton::textColourOnId, juce::Colour (0xffffb27a));
    b.setColour (juce::TextButton::textColourOffId, DIM);
    b.setToggleState (on, juce::dontSendNotification);
}

ElfPro800Editor::Toggle& ElfPro800Editor::addToggle (const char* id, const char* label)
{
    auto t = std::make_unique<Toggle>();
    t->btn.setButtonText (label);
    t->btn.setClickingTogglesState (true);
    styleToggle (t->btn, false);
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
        auto b = std::make_unique<juce::TextButton> (options[i]);
        b->setClickingTogglesState (false);
        styleToggle (*b, false);
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
            for (int i = 0; i < (int) s.btns.size(); ++i) styleToggle (*s.btns[(size_t) i], i == idx);
        }
    }
}

void ElfPro800Editor::layoutKnobRow (juce::Rectangle<int> area, std::vector<Knob*> ks, int knobSize)
{
    const int n = (int) ks.size();
    const int cellW = n > 0 ? area.getWidth() / n : 0;
    for (auto* k : ks) {
        auto cell = area.removeFromLeft (cellW);
        k->label.setBounds (cell.removeFromBottom (15));
        auto kb = cell.withSizeKeepingCentre (knobSize, knobSize);
        k->slider.setBounds (kb);
    }
}
void ElfPro800Editor::layoutToggleRow (juce::Rectangle<int> area, std::vector<Toggle*> ts)
{
    const int n = (int) ts.size();
    const int cellW = n > 0 ? area.getWidth() / n : 0;
    for (auto* t : ts) {
        auto cell = area.removeFromLeft (cellW).reduced (2, 0);
        t->btn.setBounds (cell);
    }
}
void ElfPro800Editor::layoutSegRow (juce::Rectangle<int> area, Seg& s)
{
    const int n = (int) s.btns.size();
    const int cellW = n > 0 ? area.getWidth() / n : 0;
    for (auto& b : s.btns) b->setBounds (area.removeFromLeft (cellW).reduced (2, 0));
}

void ElfPro800Editor::resized()
{
    sections.clear();
    auto addSection = [&] (juce::Rectangle<int> b, const juce::String& t, const juce::String& tag = {}) {
        sections.push_back ({ b, t, tag });
        return b.reduced (10, 26).withTrimmedTop (2);
    };
    auto bounds = getLocalBounds().reduced (14);

    /* header */
    auto header = bounds.removeFromTop (78);
    ledBounds = { 26, 66, 200, 14 };
    auto volArea = header.removeFromRight (84);
    kVolume->label.setBounds (volArea.removeFromBottom (15));
    kVolume->slider.setBounds (volArea.withSizeKeepingCentre (58, 58));
    nextBtn.setBounds (header.removeFromRight (36).withSizeKeepingCentre (30, 30));
    prevBtn.setBounds (header.removeFromRight (36).withSizeKeepingCentre (30, 30));
    lcdBounds = header.reduced (190, 8).withTrimmedRight (8);
    lcdNum.setBounds (lcdBounds.removeFromLeft (64).withTrimmedLeft (12));
    lcdName.setBounds (lcdBounds);

    /* row 1 */
    auto row1 = bounds.removeFromTop (196);
    {
        auto inner = addSection (row1.removeFromLeft (216), "LFO", "MODULATION");
        layoutKnobRow (inner.removeFromTop (72), { kLfoFreq, kLfoAmt }, 52);
        layoutSegRow (inner.removeFromTop (24), *segLfoShape);
        inner.removeFromTop (6);
        layoutToggleRow (inner.removeFromTop (24), { tLfoPitch, tLfoFilter, tLfoAmp, tLfoPW });
        row1.removeFromLeft (8);
    }
    {
        auto inner = addSection (row1.removeFromLeft (216), "OSCILLATOR A");
        layoutKnobRow (inner.removeFromTop (72), { kFreqA, kPwA }, 52);
        layoutToggleRow (inner.removeFromTop (24), { tASaw, tATri, tASqr });
        row1.removeFromLeft (8);
    }
    {
        auto inner = addSection (row1.removeFromLeft (264), "OSCILLATOR B");
        layoutKnobRow (inner.removeFromTop (72), { kFreqB, kFineB, kPwB }, 52);
        layoutToggleRow (inner.removeFromTop (24), { tBSaw, tBTri, tBSqr, tSync });
        row1.removeFromLeft (8);
    }
    {
        auto inner = addSection (row1.removeFromLeft (196), "MIXER");
        layoutKnobRow (inner.removeFromTop (72), { kVolA, kVolB, kDrive }, 48);
        row1.removeFromLeft (8);
    }
    {
        auto inner = addSection (row1, "POLY-MOD");
        layoutKnobRow (inner.removeFromTop (72), { kPmodFilEnv, kPmodOscB }, 52);
        layoutToggleRow (inner.removeFromTop (24), { tPmodFA, tPmodFil });
    }

    bounds.removeFromTop (8);
    /* row 2 */
    auto row2 = bounds.removeFromTop (196);
    {
        auto inner = addSection (row2.removeFromLeft (300), "FILTER", "CUTOFF · RESO");
        layoutKnobRow (inner.removeFromTop (86), { kCutoff, kReso, kFilEnvAmt }, 62);
        inner.removeFromTop (4);
        layoutSegRow (inner.removeFromTop (24), *segTracking);
        row2.removeFromLeft (8);
    }
    {
        auto inner = addSection (row2.removeFromLeft (262), "ENVELOPE", "FILTER");
        layoutKnobRow (inner.removeFromTop (72), { kFilAtt, kFilDec, kFilSus, kFilRel }, 48);
        row2.removeFromLeft (8);
    }
    {
        auto inner = addSection (row2.removeFromLeft (262), "ENVELOPE", "AMPLIFIER");
        layoutKnobRow (inner.removeFromTop (72), { kAmpAtt, kAmpDec, kAmpSus, kAmpRel }, 48);
        row2.removeFromLeft (8);
    }
    {
        auto inner = addSection (row2.removeFromLeft (150), "PERFORMANCE");
        layoutKnobRow (inner.removeFromTop (64), { kGlide }, 46);
        layoutKnobRow (inner.removeFromTop (64), { kAmpVel }, 46);
        row2.removeFromLeft (8);
    }
    {
        auto inner = addSection (row2, "VOICE MODE", "8 VOICES");
        layoutKnobRow (inner.removeFromTop (64), { kDetune, kSpread }, 44);
        layoutKnobRow (inner.removeFromTop (64), { kFilVel }, 44);
        inner.removeFromTop (2);
        layoutToggleRow (inner.removeFromTop (24), { tUnison });
    }

    bounds.removeFromTop (8);
    /* arp strip */
    auto strip = bounds.removeFromTop (64);
    sections.push_back ({ strip, "ARPEGGIATOR", {} });
    auto sinner = strip.reduced (10, 24);
    layoutSegRow (sinner.removeFromLeft (430), *segArpMode);
    sinner.removeFromLeft (10);
    layoutToggleRow (sinner.removeFromLeft (90), { tArpHold });
}

void ElfPro800Editor::paint (juce::Graphics& g)
{
    g.fillAll (PANEL);
    /* header brand */
    g.setFont (juce::Font (juce::FontOptions (32.0f, juce::Font::bold)));
    g.setColour (CREAM);
    g.drawText ("ELF", 24, 14, 62, 38, juce::Justification::centredLeft);
    g.setColour (ORANGE);
    g.drawText ("PRO800", 84, 14, 180, 38, juce::Justification::centredLeft);
    g.setColour (DIM);
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText ("EIGHT-VOICE POLYPHONIC SYNTHESIZER", 26, 48, 300, 14, juce::Justification::centredLeft);
    /* voice LEDs */
    g.setFont (juce::Font (juce::FontOptions (8.5f)));
    g.setColour (DIM);
    g.drawText ("VOICES", ledBounds.getX(), ledBounds.getY(), 44, 12, juce::Justification::centredLeft);
    for (int v = 0; v < 8; ++v) {
        const float x = (float) ledBounds.getX() + 48.0f + (float) v * 17.0f;
        const float lvl = ledLevel[(size_t) v];
        g.setColour (lvl > 0.03f ? juce::Colour (0xffffb02e).withAlpha (juce::jlimit (0.25f, 1.0f, lvl + 0.25f))
                                 : juce::Colour (0xff2c2e33));
        g.fillEllipse (x, (float) ledBounds.getY() + 1.0f, 10.0f, 10.0f);
    }
    /* LCD */
    g.setColour (juce::Colour (0xff0b1410));
    g.fillRoundedRectangle (lcdBounds.toFloat(), 6.0f);
    g.setColour (juce::Colour (0xff233028));
    g.drawRoundedRectangle (lcdBounds.toFloat(), 6.0f, 1.0f);
    g.setColour (LCDDIM);
    g.setFont (juce::Font (juce::FontOptions ("Courier New", 10.5f, juce::Font::plain)));
    g.drawText ("P600 ENGINE · 8 VOICES", lcdBounds.withTrimmedRight (10).toFloat(), juce::Justification::centredRight);
    /* sections */
    for (const auto& s : sections) {
        g.setColour (SECT);
        g.fillRoundedRectangle (s.bounds.toFloat(), 8.0f);
        g.setColour (EDGE);
        g.drawRoundedRectangle (s.bounds.toFloat(), 8.0f, 1.0f);
        g.setColour (ORANGE);
        g.setFont (juce::Font (juce::FontOptions (10.5f, juce::Font::bold)));
        g.drawText (s.title, s.bounds.getX() + 10, s.bounds.getY() + 7, 160, 14, juce::Justification::centredLeft);
        if (s.tag.isNotEmpty()) {
            g.setColour (DIM);
            g.setFont (juce::Font (juce::FontOptions (9.0f)));
            g.drawText (s.tag, s.bounds.withTrimmedRight (10).toFloat().withHeight (22.0f), juce::Justification::centredRight);
        }
    }
    /* footer */
    g.setColour (juce::Colour (0xff63615a));
    g.setFont (juce::Font (juce::FontOptions (9.0f)));
    g.drawText ("ELFPRO800 · V1 PANEL", 16, getHeight() - 20, 300, 12, juce::Justification::centredLeft);
    g.drawText ("GPL-3.0 · P600 ENGINE", getLocalBounds().reduced (16).toFloat().withTrimmedTop (getHeight() - 36), juce::Justification::bottomRight);
}

void ElfPro800Editor::timerCallback()
{
    for (int v = 0; v < 8; ++v) {
        const float actual = proc.getVoiceLevel (v);
        ledLevel[(size_t) v] = juce::jmax (actual, ledLevel[(size_t) v] * 0.82f);
    }
    const int pi = proc.getPresetIndex();
    if (pi != paintedPreset) {
        paintedPreset = pi;
        lcdNum.setText (juce::String::formatted ("%03d", pi + 1), juce::dontSendNotification);
        lcdName.setText (proc.getPresetName(), juce::dontSendNotification);
    }
    for (auto& s : segs) refreshSeg (*s);
    for (auto& t : toggles) {
        const bool on = t->btn.getToggleState();
        if (on != t->paintedState) { t->paintedState = on; styleToggle (t->btn, on); }
    }
    repaint (ledBounds.expanded (4));
}
