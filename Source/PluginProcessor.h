#pragma once

#include "Parameters.h"
#include "dsp/Engine.h"
#include "dsp/Limiter.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>

namespace fsl
{

/** Read-only snapshot for the panel: updated once per block from the audio
 * thread, polled by a UI timer. No scope — see the DSP notes in the README
 * for why the oscilloscope from the browser prototype isn't in this release. */
struct Telemetry
{
    std::atomic<float> mainHz { 0.0f }, refHz { 0.0f }, vcoHz { 0.0f };
    std::atomic<float> vcoLoHz { 20.0f }, vcoHiHz { 500.0f };
    std::atomic<bool> locked { false }, harmonicLock { false };
    std::atomic<float> reduction { 1.0f }; // limiter gain reduction, 1 = none
};

class FasalasProcessor : public juce::AudioProcessor
{
public:
    FasalasProcessor();
    ~FasalasProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Fasal\xc3\xa1s"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    Telemetry telemetry;

private:
    EngineParams collectParams() const;

    Engine engineL_, engineR_;
    Limiter limiter_;

    std::unique_ptr<juce::dsp::Oversampling<float>> osMain_, osSide_;
    static constexpr int oversamplingStages = 2; // 2 stages of the half-band IIR = 4x

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FasalasProcessor)
};

} // namespace fsl
