#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace fsl
{

namespace pid
{
// Input
extern const juce::String gainA;
extern const juce::String gainB;
extern const juce::String hysteresis;
extern const juce::String divA;
extern const juce::String divB;
extern const juce::String stereo;

// Env follower
extern const juce::String envSource;
extern const juce::String envSens;
extern const juce::String envRise;
extern const juce::String envHold;
extern const juce::String envFall;
extern const juce::String envToCutoff;
extern const juce::String envToDrive;
extern const juce::String envToWindow;
extern const juce::String envToOffset;

// Comparator
extern const juce::String mode;
extern const juce::String window;

// Slew
extern const juce::String rise;
extern const juce::String fall;
extern const juce::String link;
extern const juce::String shape;

// Loop / VCO
extern const juce::String loop;
extern const juce::String vcoRange;
extern const juce::String vcoOffset;
extern const juce::String vcoSoften;
extern const juce::String loopTrack;

// Filter
extern const juce::String filterType;
extern const juce::String filterSlope;
extern const juce::String cutoff;
extern const juce::String resonance;
extern const juce::String drive;

// Output
extern const juce::String mix;
extern const juce::String level;
extern const juce::String limiter;
} // namespace pid

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace fsl
