#include "PluginEditor.h"

namespace fsl
{

FasalasEditor::FasalasEditor (FasalasProcessor& p)
    : AudioProcessorEditor (&p), processor_ (p), panel_ (p)
{
    addAndMakeVisible (panel_);
    setResizable (true, true);
    setResizeLimits (920, 480, 1500, 900);
    setSize (940, 640);
    startTimerHz (30);
}

FasalasEditor::~FasalasEditor() { stopTimer(); }

void FasalasEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff161c1f)); }

void FasalasEditor::resized() { panel_.setBounds (getLocalBounds()); }

void FasalasEditor::timerCallback()
{
    panel_.refresh();
}

} // namespace fsl
