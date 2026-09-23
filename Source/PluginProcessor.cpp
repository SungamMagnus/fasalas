#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cmath>

namespace fsl
{

FasalasProcessor::FasalasProcessor()
    : AudioProcessor (BusesProperties()
                           .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                           .withInput ("Sidechain", juce::AudioChannelSet::stereo(), true)
                           .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

bool FasalasProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto main = layouts.getMainOutputChannelSet();
    if (main != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != main)
        return false;

    const auto sc = layouts.getChannelSet (true, 1);
    if (! sc.isDisabled() && sc != juce::AudioChannelSet::stereo())
        return false;

    return true;
}

void FasalasProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    osMain_ = std::make_unique<juce::dsp::Oversampling<float>> (
        2, oversamplingStages, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    osSide_ = std::make_unique<juce::dsp::Oversampling<float>> (
        2, oversamplingStages, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    osMain_->initProcessing ((size_t) samplesPerBlock);
    osSide_->initProcessing ((size_t) samplesPerBlock);

    const double osRate = sampleRate * (double) osMain_->getOversamplingFactor();
    engineL_.prepare (osRate);
    engineR_.prepare (osRate);
    limiter_.prepare (sampleRate);

    setLatencySamples ((int) osMain_->getLatencyInSamples());
}

void FasalasProcessor::releaseResources()
{
    engineL_.reset();
    engineR_.reset();
    limiter_.reset();
    if (osMain_) osMain_->reset();
    if (osSide_) osSide_->reset();
}

EngineParams FasalasProcessor::collectParams() const
{
    EngineParams p;
    auto raw = [this] (const juce::String& id) { return apvts.getRawParameterValue (id)->load(); };

    p.gainADb = raw (pid::gainA);
    p.gainBDb = raw (pid::gainB);
    p.hysteresis = raw (pid::hysteresis);
    p.divA = (int) std::round (raw (pid::divA));
    p.divB = (int) std::round (raw (pid::divB));

    p.mode = (CompareMode) (int) std::round (raw (pid::mode));
    p.window = raw (pid::window);

    p.riseMs = raw (pid::rise);
    p.fallMs = raw (pid::link) > 0.5f ? p.riseMs : raw (pid::fall);
    p.slewShape = (SlewShape) (int) std::round (raw (pid::shape));

    p.loopOn = raw (pid::loop) > 0.5f;
    p.loopLockToMain = std::round (raw (pid::loopTrack)) < 0.5f;
    p.vcoRange = (VcoRange) (int) std::round (raw (pid::vcoRange));
    p.vcoOffset = raw (pid::vcoOffset);

    p.filterType = (FilterType) (int) std::round (raw (pid::filterType));
    p.filterSlope = (FilterSlope) (int) std::round (raw (pid::filterSlope));
    p.cutoffHz = raw (pid::cutoff);
    p.resonance = raw (pid::resonance);
    p.driveDb = raw (pid::drive);

    p.mix = raw (pid::mix);
    p.levelDb = raw (pid::level);

    return p;
}

void FasalasProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // Main bus 0's input and output channel ranges alias the same memory for a
    // matched-layout effect, so this view is both the raw input and, once
    // processed in place, the final output.
    auto mainBus = getBusBuffer (buffer, true, 0);
    auto sideBus = getBusBuffer (buffer, true, 1);
    const int numSamples = buffer.getNumSamples();
    const bool hasSidechain = sideBus.getNumChannels() >= 2;

    juce::AudioBuffer<float> sideScratch;
    if (! hasSidechain)
    {
        sideScratch.setSize (2, numSamples);
        sideScratch.clear();
    }
    auto& sideSource = hasSidechain ? sideBus : sideScratch;

    const auto p = collectParams();
    const bool stereo = apvts.getRawParameterValue (pid::stereo)->load() > 0.5f;
    const bool limOn = apvts.getRawParameterValue (pid::limiter)->load() > 0.5f;
    engineL_.setParams (p);
    engineR_.setParams (p);

    juce::dsp::AudioBlock<float> mainBlock (mainBus);
    juce::dsp::AudioBlock<float> sideBlock (sideSource);

    auto mainUp = osMain_->processSamplesUp (mainBlock);
    auto sideUp = osSide_->processSamplesUp (sideBlock);
    const int osSamples = (int) mainUp.getNumSamples();

    float* mL = mainUp.getChannelPointer (0);
    float* mR = mainUp.getChannelPointer (1);
    const float* sL = sideUp.getChannelPointer (0);
    const float* sR = sideUp.getChannelPointer (1);

    if (stereo)
    {
        EngineTap tap;
        for (int i = 0; i < osSamples; ++i)
            mL[i] = engineL_.process (mL[i], sL[i], mL[i], tap);
        for (int i = 0; i < osSamples; ++i)
            mR[i] = engineR_.process (mR[i], sR[i], mR[i], tap);
    }
    else
    {
        EngineTap tap;
        for (int i = 0; i < osSamples; ++i)
        {
            const float a = 0.5f * (mL[i] + mR[i]);
            const float b = 0.5f * (sL[i] + sR[i]);
            const float out = engineL_.process (a, b, a, tap);
            mL[i] = out;
            mR[i] = out;
        }
    }

    osMain_->processSamplesDown (mainBlock);

    if (limOn)
    {
        limiter_.process (mainBus.getWritePointer (0), mainBus.getWritePointer (1), numSamples);
        telemetry.reduction.store (limiter_.readReduction());
    }
    else
    {
        telemetry.reduction.store (1.0f);
    }

    // Telemetry for the panel: frequencies, lock, and whether the loop's own
    // range table currently spans. Left channel stands in for the pair.
    const float fa = engineL_.mainFrequencyHz();
    const float fb = engineL_.sideFrequencyHz();
    telemetry.mainHz.store (fa);
    telemetry.refHz.store (fb);
    telemetry.vcoHz.store (engineL_.vcoFrequencyHz());
    telemetry.vcoLoHz.store (engineL_.vcoRangeLowHz());
    telemetry.vcoHiHz.store (engineL_.vcoRangeHighHz());

    bool locked = false, harmonic = false;
    if (fa > 0.0f && fb > 0.0f)
    {
        const float ratio = fb / fa;
        if (p.mode == CompareMode::xor_)
        {
            const float r = std::max (ratio, 1.0f / ratio);
            locked = std::fabs (r - std::round (r)) < 0.015f * r;
            harmonic = locked && std::fabs (ratio - 1.0f) > 0.01f;
        }
        else
        {
            locked = std::fabs (ratio - 1.0f) < 0.01f;
        }
    }
    telemetry.locked.store (locked);
    telemetry.harmonicLock.store (harmonic);
}

juce::AudioProcessorEditor* FasalasProcessor::createEditor()
{
    return new FasalasEditor (*this);
}

void FasalasProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); true)
    {
        std::unique_ptr<juce::XmlElement> xml (state.createXml());
        copyXmlToBinary (*xml, destData);
    }
}

void FasalasProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

} // namespace fsl

// This creates the audio engine that's plugged into the appropriate wrapper for VST3/AU/Standalone.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new fsl::FasalasProcessor();
}
