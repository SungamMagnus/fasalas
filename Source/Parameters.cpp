#include "Parameters.h"

#include <cmath>

namespace fsl
{

namespace pid
{
const juce::String gainA      = "gaina";
const juce::String gainB      = "gainb";
const juce::String hysteresis = "hyst";
const juce::String divA       = "diva";
const juce::String divB       = "divb";
const juce::String stereo     = "stereo";

const juce::String envSource    = "envsource";
const juce::String envSens      = "envsens";
const juce::String envRise      = "envrise";
const juce::String envHold      = "envhold";
const juce::String envFall      = "envfall";
const juce::String envToCutoff  = "envtocutoff";
const juce::String envToDrive   = "envtodrive";
const juce::String envToWindow  = "envtowindow";
const juce::String envToOffset  = "envtooffset";

const juce::String mode       = "mode";
const juce::String window     = "window";

const juce::String rise       = "rise";
const juce::String fall       = "fall";
const juce::String link       = "link";
const juce::String shape      = "shape";

const juce::String loop       = "loop";
const juce::String vcoRange   = "vcorange";
const juce::String vcoOffset  = "vcooffset";
const juce::String vcoSoften  = "vcosoften";
const juce::String loopTrack  = "looptrack";

const juce::String filterType  = "filtertype";
const juce::String filterSlope = "filterslope";
const juce::String cutoff      = "cutoff";
const juce::String resonance   = "resonance";
const juce::String drive       = "drive";

const juce::String mix     = "mix";
const juce::String level   = "level";
const juce::String limiter = "limiter";
} // namespace pid

namespace
{
/** A frequency/time-style range skewed so its slider midpoint lands on the
 * geometric centre of min..max — JUCE's own logarithmic-feeling knob curve,
 * and (unlike a hand-rolled log mapping) one that a plain juce::Slider
 * reproduces exactly through SliderAttachment. */
juce::NormalisableRange<float> logRange (float min, float max)
{
    juce::NormalisableRange<float> r (min, max);
    const float centre = std::sqrt (min * max);
    r.skew = std::log (0.5f) / std::log ((centre - min) / (max - min));
    return r;
}

// Readable value text for the panel — getCurrentValueAsText() otherwise falls
// back to a fixed-precision numeric dump (e.g. "40.0000038").
juce::String fmtDb (float v, int)  { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; }
juce::String fmtPct (float v, int) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }
juce::String fmtPlain (float v, int) { return juce::String (v, 1); }

juce::String fmtMs (float v, int)
{
    if (v < 1.0f)    return juce::String (v, 2) + " ms";
    if (v < 1000.0f) return juce::String (v, 1) + " ms";
    return juce::String (v / 1000.0f, 2) + " s";
}

juce::String fmtHz (float v, int)
{
    return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz"
                         : juce::String (v, v < 100.0f ? 1 : 0) + " Hz";
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    auto dbAttrs  = AudioParameterFloatAttributes().withLabel ("dB").withStringFromValueFunction (fmtDb);
    auto msAttrs  = AudioParameterFloatAttributes().withLabel ("ms").withStringFromValueFunction (fmtMs);
    auto hzAttrs  = AudioParameterFloatAttributes().withLabel ("Hz").withStringFromValueFunction (fmtHz);
    auto pctAttrs = AudioParameterFloatAttributes().withLabel ("%").withStringFromValueFunction (fmtPct);
    auto plainAttrs = AudioParameterFloatAttributes().withStringFromValueFunction (fmtPlain);

    // ── Input ──────────────────────────────────────────────────────────
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::gainA, 1 }, "Main Gain", NormalisableRange<float> (-24.0f, 24.0f), 0.0f, dbAttrs));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::gainB, 1 }, "Sidechain Gain", NormalisableRange<float> (-24.0f, 24.0f), 0.0f, dbAttrs));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::hysteresis, 1 }, "Hysteresis", NormalisableRange<float> (0.0f, 0.5f), 0.02f, pctAttrs));
    params.push_back (std::make_unique<AudioParameterInt> (
        ParameterID { pid::divA, 1 }, "Div Main", 1, 16, 1));
    params.push_back (std::make_unique<AudioParameterInt> (
        ParameterID { pid::divB, 1 }, "Div Sidechain", 1, 16, 1));
    params.push_back (std::make_unique<AudioParameterBool> (
        ParameterID { pid::stereo, 1 }, "Stereo", true));

    // ── Env follower ───────────────────────────────────────────────────
    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { pid::envSource, 1 }, "Env Source", StringArray { "Main", "Side" }, 1));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::envSens, 1 }, "Sensitivity", NormalisableRange<float> (0.0f, 1.0f), 0.6f, pctAttrs));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::envRise, 1 }, "Env Rise", logRange (0.1f, 1000.0f), 2.0f, msAttrs));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::envHold, 1 }, "Env Hold", NormalisableRange<float> (0.0f, 1000.0f), 40.0f, msAttrs));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::envFall, 1 }, "Env Fall", logRange (1.0f, 5000.0f), 180.0f, msAttrs));
    params.push_back (std::make_unique<AudioParameterBool> (
        ParameterID { pid::envToCutoff, 1 }, "Env to Cutoff", true));
    params.push_back (std::make_unique<AudioParameterBool> (
        ParameterID { pid::envToDrive, 1 }, "Env to Drive", true));
    params.push_back (std::make_unique<AudioParameterBool> (
        ParameterID { pid::envToWindow, 1 }, "Env to Window", false));
    params.push_back (std::make_unique<AudioParameterBool> (
        ParameterID { pid::envToOffset, 1 }, "Env to Offset", false));

    // ── Comparator ─────────────────────────────────────────────────────
    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { pid::mode, 1 }, "Comparator Mode",
        StringArray { "XOR", "RS", "PFD", "CMP", "WIN" }, 0));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::window, 1 }, "Window", NormalisableRange<float> (0.01f, 1.0f), 0.2f, pctAttrs));

    // ── Slew ───────────────────────────────────────────────────────────
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::rise, 1 }, "Rise", logRange (0.01f, 2000.0f), 0.2f, msAttrs));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::fall, 1 }, "Fall", logRange (0.01f, 2000.0f), 0.2f, msAttrs));
    params.push_back (std::make_unique<AudioParameterBool> (
        ParameterID { pid::link, 1 }, "Link Rise/Fall", true));
    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { pid::shape, 1 }, "Slew Shape", StringArray { "Linear", "Exponential" }, 1));

    // ── Loop / VCO ─────────────────────────────────────────────────────
    params.push_back (std::make_unique<AudioParameterBool> (
        ParameterID { pid::loop, 1 }, "Loop", false));
    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { pid::vcoRange, 1 }, "VCO Range", StringArray { "Low", "Mid", "High" }, 1));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::vcoOffset, 1 }, "VCO Offset", NormalisableRange<float> (0.0f, 10.0f), 5.0f, plainAttrs));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::vcoSoften, 1 }, "Soften", NormalisableRange<float> (0.0f, 1.0f), 0.35f, pctAttrs));
    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { pid::loopTrack, 1 }, "Loop Locks To", StringArray { "Main", "Side" }, 0));

    // ── Filter ─────────────────────────────────────────────────────────
    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { pid::filterType, 1 }, "Filter Type",
        StringArray { "Low Pass", "Band Pass", "High Pass", "Notch" }, 0));
    params.push_back (std::make_unique<AudioParameterChoice> (
        ParameterID { pid::filterSlope, 1 }, "Filter Slope", StringArray { "12 dB", "24 dB" }, 1));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::cutoff, 1 }, "Cutoff", logRange (20.0f, 20000.0f), 3200.0f, hzAttrs));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::resonance, 1 }, "Resonance", NormalisableRange<float> (0.0f, 1.0f), 0.35f, pctAttrs));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::drive, 1 }, "Drive", NormalisableRange<float> (0.0f, 24.0f), 3.0f, dbAttrs));

    // ── Output ─────────────────────────────────────────────────────────
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::mix, 1 }, "Mix", NormalisableRange<float> (0.0f, 1.0f), 1.0f, pctAttrs));
    params.push_back (std::make_unique<AudioParameterFloat> (
        ParameterID { pid::level, 1 }, "Level", NormalisableRange<float> (-24.0f, 6.0f), -6.0f, dbAttrs));
    params.push_back (std::make_unique<AudioParameterBool> (
        ParameterID { pid::limiter, 1 }, "Limiter", false));

    return { params.begin(), params.end() };
}

} // namespace fsl
