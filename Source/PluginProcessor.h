#pragma once

#include "Parameters.h"
#include "dsp/Engine.h"
#include "dsp/Limiter.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

namespace fsl
{

/** Read-only snapshot for the panel: updated once per block from the audio
 * thread, polled by a UI timer. */
struct Telemetry
{
    std::atomic<float> mainHz { 0.0f }, refHz { 0.0f }, vcoHz { 0.0f };
    std::atomic<float> vcoLoHz { 20.0f }, vcoHiHz { 500.0f };
    std::atomic<bool> locked { false }, harmonicLock { false };
    std::atomic<float> reduction { 1.0f }; // limiter gain reduction, 1 = none
};

/** One tick of every trace the panel's oscilloscope draws. */
struct ScopeSample
{
    float main = 0.0f, ref = 0.0f, pc = 0.0f, slew = 0.0f, out = 0.0f;
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

    /** Drains whatever scope samples have arrived since the last call. Safe
     * to call from the UI thread only; returns the number written to dest
     * (up to maxSamples). */
    int drainScope (ScopeSample* dest, int maxSamples);

private:
    EngineParams collectParams() const;
    void pushScope (const EngineTap&);

    Engine engineL_, engineR_;
    Limiter limiter_;

    std::unique_ptr<juce::dsp::Oversampling<float>> osMain_, osSide_;
    static constexpr int oversamplingStages = 2; // 2 stages of the half-band IIR = 4x

    static constexpr int scopeCapacity = 1 << 13;
    juce::AbstractFifo scopeFifo_ { scopeCapacity };
    std::array<ScopeSample, scopeCapacity> scopeBuffer_ {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FasalasProcessor)
};

} // namespace fsl
