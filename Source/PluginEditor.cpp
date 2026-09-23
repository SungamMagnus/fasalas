#include "PluginEditor.h"

namespace fsl
{

FasalasEditor::FasalasEditor (FasalasProcessor& p)
    : AudioProcessorEditor (&p), processor_ (p), panel_ (p.apvts)
{
    addAndMakeVisible (panel_);
    setResizable (true, true);
    setResizeLimits (860, 460, 1600, 860);
    setSize (1040, 560);
    startTimerHz (20);
}

FasalasEditor::~FasalasEditor() { stopTimer(); }

void FasalasEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colour (0xff161c1f)); }

void FasalasEditor::resized() { panel_.setBounds (getLocalBounds()); }

void FasalasEditor::timerCallback()
{
    panel_.updateTelemetry (processor_.telemetry);
}

} // namespace fsl
