#include "PluginEditor.h"
#include "PluginProcessor.h"

ElfPro800Editor::ElfPro800Editor (ElfPro800Processor& p) : AudioProcessorEditor (p), proc (p)
{
    setupSlider (cutoff,    "cutoff",    "CUTOFF");
    setupSlider (resonance, "resonance", "RESONANCE");
    setupSlider (filEnvAmt, "filEnvAmt", "FILTER ENV AMT");
    setupSlider (volume,    "volume",    "VOLUME");
    setSize (760, 380);
}

void ElfPro800Editor::setupSlider (ParamSlider& s, const char* paramID, const char* name)
{
    s.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    s.slider.setColour (juce::Slider::rotarySliderFillColourId, juce::Colour (0xffff7a1a));
    s.slider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff3a3d44));
    s.slider.setColour (juce::Slider::thumbColourId, juce::Colour (0xfff2efe6));
    addAndMakeVisible (s.slider);
    s.label.setText (name, juce::dontSendNotification);
    s.label.setJustificationType (juce::Justification::centred);
    s.label.setColour (juce::Label::textColourId, juce::Colour (0xffa9a69c));
    s.label.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
    addAndMakeVisible (s.label);
    s.attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        proc.apvts, paramID, s.slider);
}

void ElfPro800Editor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff14161a));
    g.setColour (juce::Colour (0xff2c2f36));
    g.drawRoundedRectangle (8.0f, 8.0f, (float) getWidth() - 16.0f, (float) getHeight() - 16.0f, 10.0f, 1.5f);

    g.setFont (juce::Font (juce::FontOptions (34.0f, juce::Font::bold)));
    g.setColour (juce::Colour (0xfff2efe6));
    g.drawText ("ELF", 30, 22, 62, 40, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xffff7a1a));
    g.drawText ("PRO800", 92, 22, 190, 40, juce::Justification::centredLeft);
    g.setColour (juce::Colour (0xff8f8c82));
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText ("EIGHT-VOICE POLYPHONIC SYNTHESIZER  ·  V1 SCAFFOLD UI", 32, 58, 480, 18, juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff7dffb0));
    g.setFont (juce::Font (juce::FontOptions ("Courier New", 15.0f, juce::Font::plain)));
    g.drawText ("042  Analog Brass", getWidth() - 250, 26, 210, 24, juce::Justification::centredRight);
}

void ElfPro800Editor::resized()
{
    auto area = getLocalBounds().reduced (30);
    area.removeFromTop (90);
    auto row = area.removeFromTop (150);
    const int w = row.getWidth() / 4;
    ParamSlider* sliders[] = { &cutoff, &resonance, &filEnvAmt, &volume };
    for (auto* s : sliders) {
        auto cell = row.removeFromLeft (w).reduced (14, 0);
        s->label.setBounds (cell.removeFromBottom (20));
        s->slider.setBounds (cell);
    }
}
