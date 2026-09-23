#pragma once

#include "Parameters.h"
#include "dsp/Engine.h"
#include "dsp/EnvFollower.h"
#include "dsp/Limiter.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>

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
    std::atomic<float> envMod { 0.0f };    // latest env * sensitivity, for the mod arcs
};

/** One tick of every trace the panel's oscilloscope draws. */
struct ScopeSample
{
    float main = 0.0f, ref = 0.0f, pc = 0.0f, slew = 0.0f, out = 0.0f;
};

/** One tick of the envelope follower's own mini scope: the detected input
 * level and the envelope it produced, decimated to ~1 kHz. */
struct EnvScopeSample
{
    float input = 0.0f, env = 0.0f;
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

    /** Same idea for the envelope follower's own mini scope. */
    int drainEnvScope (EnvScopeSample* dest, int maxSamples);

private:
    EngineParams collectParams() const;
    void pushScope (const EngineTap&);
    void pushEnvScope (const EnvScopeSample&);

    Engine engineL_, engineR_;
    Limiter limiter_;

    std::unique_ptr<juce::dsp::Oversampling<float>> osMain_, osSide_;
    static constexpr int oversamplingStages = 2; // 2 stages of the half-band IIR = 4x

    static constexpr int scopeCapacity = 1 << 13;
    juce::AbstractFifo scopeFifo_ { scopeCapacity };
    std::array<ScopeSample, scopeCapacity> scopeBuffer_ {};

    // Envelope follower: runs once per base-rate sample, ahead of the
    // oversampling, on the raw (un-gained) selected input.
    EnvFollower envFollower_;
    std::vector<float> envBuffer_;              // this block's envelope, base rate
    int envScopeInterval_ = 48;                 // base samples between ~1 kHz pushes
    int envScopeCounter_ = 0;
    float envScopePeak_ = 0.0f;

    static constexpr int envScopeCapacity = 1 << 12;
    juce::AbstractFifo envScopeFifo_ { envScopeCapacity };
    std::array<EnvScopeSample, envScopeCapacity> envScopeBuffer_ {};

    // Cached so the per-control-tick modulation step never has to look these
    // up by ID; RangedAudioParameter::convertTo0to1/convertFrom0to1 give the
    // same curve the knob itself uses (log ranges included).
    juce::RangedAudioParameter* cutoffParam_ = nullptr;
    juce::RangedAudioParameter* driveParam_ = nullptr;
    juce::RangedAudioParameter* windowParam_ = nullptr;
    juce::RangedAudioParameter* vcoOffsetParam_ = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FasalasProcessor)
};

} // namespace fsl
