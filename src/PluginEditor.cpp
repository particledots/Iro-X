#include "PluginEditor.h"

IroxEditor::IroxEditor (IroxProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setSize (560, 260);
}

void IroxEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff15171c));
    auto area = getLocalBounds().reduced (24);

    g.setColour (juce::Colour (0xffe8eaf0));
    g.setFont (juce::FontOptions (34.0f, juce::Font::bold));
    g.drawText ("Iro-x", area.removeFromTop (46), juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff9aa3b5));
    g.setFont (juce::FontOptions (15.0f));
    g.drawText ("Version " + IroxProcessor::versionString() + "  -  Milestone 0 (empty effect)", area.removeFromTop (24), juce::Justification::centredLeft);

    area.removeFromTop (14);
    g.setColour (juce::Colour (0xffe8eaf0));
    g.drawFittedText ("The audio of this track passes through unchanged. The effects arrive in the next milestone.",
                      area.removeFromTop (44), juce::Justification::topLeft, 2);

    g.setColour (juce::Colour (0xff6f7889));
    g.setFont (juce::FontOptions (13.0f));
    g.drawFittedText ("Unofficial plugin, not made, approved or supported by Befaco. Based on the open-source Befaco Iroi "
                      "firmware (GPL-3.0). Do not contact Befaco about this plugin.",
                      area.removeFromBottom (44), juce::Justification::bottomLeft, 3);
}
