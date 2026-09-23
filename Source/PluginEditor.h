#pragma once

#include "Panel.h"
#include "PluginProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace fsl
{

class FasalasEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FasalasEditor (FasalasProcessor&);
    ~FasalasEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Pulls one telemetry snapshot into the panel immediately, without
     * waiting for the UI timer — used by the offscreen panel_shot tool so a
     * rendered screenshot shows a genuine lock/frequency state. */
    void refreshTelemetryNow() { timerCallback(); }

private:
    void timerCallback() override;

    FasalasProcessor& processor_;
    Panel panel_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FasalasEditor)
};

} // namespace fsl
