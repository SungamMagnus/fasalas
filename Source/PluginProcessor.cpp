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
    cutoffParam_ = apvts.getParameter (pid::cutoff);
    driveParam_ = apvts.getParameter (pid::drive);
    windowParam_ = apvts.getParameter (pid::window);
    vcoOffsetParam_ = apvts.getParameter (pid::vcoOffset);
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

    envFollower_.prepare (sampleRate);
    envBuffer_.assign ((size_t) juce::jmax (1, samplesPerBlock), 0.0f);
    envScopeInterval_ = juce::jmax (1, (int) std::round (sampleRate / 1000.0));
    envScopeCounter_ = 0;
    envScopePeak_ = 0.0f;

    setLatencySamples ((int) osMain_->getLatencyInSamples());
}

void FasalasProcessor::releaseResources()
{
    engineL_.reset();
    engineR_.reset();
    limiter_.reset();
    envFollower_.reset();
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
    p.soften = raw (pid::vcoSoften);

    p.filterType = (FilterType) (int) std::round (raw (pid::filterType));
    p.filterSlope = (FilterSlope) (int) std::round (raw (pid::filterSlope));
    p.cutoffHz = raw (pid::cutoff);
    p.resonance = raw (pid::resonance);
    p.driveDb = raw (pid::drive);

    p.mix = raw (pid::mix);
    p.levelDb = raw (pid::level);

    return p;
}

void FasalasProcessor::pushScope (const EngineTap& tap)
{
    int start1, size1, start2, size2;
    scopeFifo_.prepareToWrite (1, start1, size1, start2, size2);
    if (size1 > 0)
        scopeBuffer_[(size_t) start1] = { tap.main, tap.side, tap.pc, tap.slew, tap.out };
    else if (size2 > 0)
        scopeBuffer_[(size_t) start2] = { tap.main, tap.side, tap.pc, tap.slew, tap.out };
    scopeFifo_.finishedWrite (size1 + size2);
}

int FasalasProcessor::drainScope (ScopeSample* dest, int maxSamples)
{
    int start1, size1, start2, size2;
    scopeFifo_.prepareToRead (maxSamples, start1, size1, start2, size2);
    for (int i = 0; i < size1; ++i) dest[i] = scopeBuffer_[(size_t) (start1 + i)];
    for (int i = 0; i < size2; ++i) dest[size1 + i] = scopeBuffer_[(size_t) (start2 + i)];
    scopeFifo_.finishedRead (size1 + size2);
    return size1 + size2;
}

void FasalasProcessor::pushEnvScope (const EnvScopeSample& s)
{
    int start1, size1, start2, size2;
    envScopeFifo_.prepareToWrite (1, start1, size1, start2, size2);
    if (size1 > 0)
        envScopeBuffer_[(size_t) start1] = s;
    else if (size2 > 0)
        envScopeBuffer_[(size_t) start2] = s;
    envScopeFifo_.finishedWrite (size1 + size2);
}

int FasalasProcessor::drainEnvScope (EnvScopeSample* dest, int maxSamples)
{
    int start1, size1, start2, size2;
    envScopeFifo_.prepareToRead (maxSamples, start1, size1, start2, size2);
    for (int i = 0; i < size1; ++i) dest[i] = envScopeBuffer_[(size_t) (start1 + i)];
    for (int i = 0; i < size2; ++i) dest[size1 + i] = envScopeBuffer_[(size_t) (start2 + i)];
    envScopeFifo_.finishedRead (size1 + size2);
    return size1 + size2;
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

    // ── Envelope follower ── base rate, on the raw/un-gained selected input,
    // before either bus is touched by anything below. Stereo-linked: one
    // envelope (max of L/R) regardless of Mono/Stereo or how many engines
    // read it.
    if ((int) envBuffer_.size() < numSamples)
        envBuffer_.resize ((size_t) numSamples);
    const bool envUseSide = apvts.getRawParameterValue (pid::envSource)->load() > 0.5f;
    envFollower_.setTimes (apvts.getRawParameterValue (pid::envRise)->load(),
                            apvts.getRawParameterValue (pid::envHold)->load(),
                            apvts.getRawParameterValue (pid::envFall)->load());
    {
        const auto& src = envUseSide ? sideSource : mainBus;
        const float* eL = src.getReadPointer (0);
        const float* eR = src.getReadPointer (1);
        for (int i = 0; i < numSamples; ++i)
        {
            const float lvl = std::fmax (std::fabs (eL[i]), std::fabs (eR[i]));
            envBuffer_[(size_t) i] = envFollower_.process (eL[i], eR[i]);
            envScopePeak_ = std::fmax (envScopePeak_, lvl);
            if (++envScopeCounter_ >= envScopeInterval_)
            {
                pushEnvScope ({ envScopePeak_, envBuffer_[(size_t) i] });
                envScopePeak_ = 0.0f;
                envScopeCounter_ = 0;
            }
        }
        if (numSamples > 0)
            telemetry.envMod.store (envBuffer_[(size_t) (numSamples - 1)]
                                     * apvts.getRawParameterValue (pid::envSens)->load());
    }

    const auto p = collectParams();
    const bool stereo = apvts.getRawParameterValue (pid::stereo)->load() > 0.5f;
    const bool limOn = apvts.getRawParameterValue (pid::limiter)->load() > 0.5f;
    engineL_.setParams (p);
    engineR_.setParams (p);

    // Envelope -> target modulation: upward only, clamped, applied at a
    // cheap control rate inside the oversampled loop below via setModulated.
    const float envSens = apvts.getRawParameterValue (pid::envSens)->load();
    const bool modCutoff = apvts.getRawParameterValue (pid::envToCutoff)->load() > 0.5f;
    const bool modDrive  = apvts.getRawParameterValue (pid::envToDrive)->load() > 0.5f;
    const bool modWindow = apvts.getRawParameterValue (pid::envToWindow)->load() > 0.5f;
    const bool modOffset = apvts.getRawParameterValue (pid::envToOffset)->load() > 0.5f;
    const bool anyMod = modCutoff || modDrive || modWindow || modOffset;

    juce::dsp::AudioBlock<float> mainBlock (mainBus);
    juce::dsp::AudioBlock<float> sideBlock (sideSource);

    auto mainUp = osMain_->processSamplesUp (mainBlock);
    auto sideUp = osSide_->processSamplesUp (sideBlock);
    const int osSamples = (int) mainUp.getNumSamples();
    const int factor = (int) osMain_->getOversamplingFactor();
    const int controlStride = juce::jmax (1, 16 * factor);

    float* mL = mainUp.getChannelPointer (0);
    float* mR = mainUp.getChannelPointer (1);
    const float* sL = sideUp.getChannelPointer (0);
    const float* sR = sideUp.getChannelPointer (1);

    // Feed the scope at base rate, not the oversampled rate — plenty for a
    // UI display and a lot less traffic through the lock-free FIFO.
    const int scopeStride = numSamples > 0 ? juce::jmax (1, osSamples / numSamples) : 1;

    auto applyModulation = [&] (int osIndex)
    {
        const int baseIdx = juce::jlimit (0, numSamples - 1, osIndex / factor);
        const float add = envBuffer_[(size_t) baseIdx] * envSens;

        float cutoffHz = p.cutoffHz, driveDb = p.driveDb, window = p.window, vcoOffset = p.vcoOffset;
        if (modCutoff)
            cutoffHz = cutoffParam_->convertFrom0to1 (
                juce::jlimit (0.0f, 1.0f, cutoffParam_->convertTo0to1 (p.cutoffHz) + add));
        if (modDrive)
            driveDb = driveParam_->convertFrom0to1 (
                juce::jlimit (0.0f, 1.0f, driveParam_->convertTo0to1 (p.driveDb) + add));
        if (modWindow)
            window = windowParam_->convertFrom0to1 (
                juce::jlimit (0.0f, 1.0f, windowParam_->convertTo0to1 (p.window) + add));
        if (modOffset)
            vcoOffset = vcoOffsetParam_->convertFrom0to1 (
                juce::jlimit (0.0f, 1.0f, vcoOffsetParam_->convertTo0to1 (p.vcoOffset) + add));

        engineL_.setModulated (cutoffHz, driveDb, window, vcoOffset);
        engineR_.setModulated (cutoffHz, driveDb, window, vcoOffset);
    };

    if (stereo)
    {
        EngineTap tap;
        for (int i = 0; i < osSamples; ++i)
        {
            if (anyMod && (i % controlStride == 0)) applyModulation (i);
            mL[i] = engineL_.process (mL[i], sL[i], mL[i], tap);
            if (i % scopeStride == 0) pushScope (tap);
        }
        for (int i = 0; i < osSamples; ++i)
        {
            if (anyMod && (i % controlStride == 0)) applyModulation (i);
            mR[i] = engineR_.process (mR[i], sR[i], mR[i], tap);
        }
    }
    else
    {
        EngineTap tap;
        for (int i = 0; i < osSamples; ++i)
        {
            if (anyMod && (i % controlStride == 0)) applyModulation (i);
            const float a = 0.5f * (mL[i] + mR[i]);
            const float b = 0.5f * (sL[i] + sR[i]);
            const float out = engineL_.process (a, b, a, tap);
            mL[i] = out;
            mR[i] = out;
            if (i % scopeStride == 0) pushScope (tap);
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
