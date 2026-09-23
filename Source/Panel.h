#pragma once

#include "PluginProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace fsl
{

/** House palette: sungam.art's dark tokens for structure, its logo pastels
 * for per-section accents, Afbökun's amber reserved for the limiter alone. */
namespace hue
{
static const juce::Colour bg       { 0xff161c1f };
static const juce::Colour panel    { 0xff1c2428 };
static const juce::Colour well     { 0xff0e1011 };
static const juce::Colour sep      { 0xff3e4b51 };
static const juce::Colour line2    { 0xff323d42 };
static const juce::Colour cap      { 0xff222b30 };
static const juce::Colour fg       { 0xfff4f4f4 };
static const juce::Colour muted    { 0xff9a9a9a };
static const juce::Colour dim      { 0xff647075 };
static const juce::Colour p1lilac  { 0xffcdb4db }; // Slew, Loop
static const juce::Colour p2pink   { 0xffffc8dd }; // sidechain accents
static const juce::Colour p3rose   { 0xffffafcc }; // Comparator
static const juce::Colour p4blue   { 0xffbde0fe }; // Input, lock LED
static const juce::Colour p5sky    { 0xffa2d2fe }; // Filter
static const juce::Colour amber    { 0xffc08d16 }; // the limiter, and nothing else
}

class FasalasLookAndFeel : public juce::LookAndFeel_V4
{
public:
    FasalasLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                            float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                            juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                                bool isHighlighted, bool isDown) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool isHighlighted, bool isDown) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
};

/** Reads an accent colour a component stashed in its Properties, so one
 * shared LookAndFeel can paint every knob and pill in its section's hue. */
juce::Colour accentOf (const juce::Component&);
void setAccent (juce::Component&, juce::Colour);

/** One rotary control: name above, live value below, coloured progress arc. */
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramID,
          const juce::String& displayName, juce::Colour accent);
    void resized() override;

private:
    juce::RangedAudioParameter* param_;
    juce::Slider slider_;
    juce::Label nameLabel_, valueLabel_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment_;

    void refreshValue();
};

/** A row of mutually-exclusive pill buttons bound to a choice or bool parameter. */
class Pills : public juce::Component
{
public:
    Pills (juce::AudioProcessorValueTreeState&, const juce::String& paramID,
           const juce::StringArray& labels, juce::Colour accent);
    void resized() override;

private:
    juce::OwnedArray<juce::TextButton> buttons_;
};

/** A single latching pill bound to a bool parameter (Link, Loop, Lim…). */
class Latch : public juce::Component
{
public:
    Latch (juce::AudioProcessorValueTreeState&, const juce::String& paramID,
           const juce::String& text, juce::Colour accent);
    void resized() override;
    bool isOn() const { return button_.getToggleState(); }
    void setAmber (bool);

private:
    juce::TextButton button_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment_;
};

/** A small round indicator: the lock LED and the limiter's reduction lamp. */
class Lamp : public juce::Component
{
public:
    explicit Lamp (juce::Colour onColour) : onColour_ (onColour) {}
    void setOn (bool b) { if (on_ != b) { on_ = b; repaint(); } }
    void paint (juce::Graphics&) override;

private:
    juce::Colour onColour_;
    bool on_ = false;
};

/** One titled section of the panel — a coloured heading dot, a title, and
 * whatever rows of controls the caller lays out inside contentArea(). */
class Module : public juce::Component
{
public:
    Module (juce::String title, juce::Colour accent);
    void paint (juce::Graphics&) override;
    juce::Rectangle<int> contentArea() const;

private:
    juce::String title_;
    juce::Colour accent_;
};

/** The whole plugin panel: six modules in a 3x2 grid, a top bar with the
 * wordmark and the lock LED, all six sections talking directly to the APVTS. */
class Panel : public juce::Component
{
public:
    explicit Panel (juce::AudioProcessorValueTreeState&);
    ~Panel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Called from the editor's UI timer with a fresh snapshot from the audio thread. */
    void updateTelemetry (const Telemetry&);

private:
    static void layoutRow (juce::Rectangle<int> area, std::initializer_list<juce::Component*> items, int gap);

    juce::AudioProcessorValueTreeState& state_;
    FasalasLookAndFeel lnf_;

    // Top bar
    Lamp lockLed_ { hue::p4blue };
    juce::Label lockCaption_, wordmark_;

    // Input
    Module inMod_ { "Input", hue::p4blue };
    Knob gainA_, gainB_, hyst_, divA_, divB_;
    Pills stereo_;

    // Comparator
    Module cmpMod_ { "Comparator", hue::p3rose };
    Pills mode_;
    Knob window_;
    juce::Label freqReadout_;

    // Slew
    Module slewMod_ { "Slew", hue::p1lilac };
    Knob rise_, fall_;
    Pills shape_;
    Latch link_;

    // Loop
    Module loopMod_ { juce::String::fromUTF8 ("Loop \xC2\xB7 VCO"), hue::p1lilac };
    Latch loop_;
    juce::Label vcoReadout_;
    Knob vcoOffset_;
    Pills vcoRange_, loopTrack_;

    // Filter
    Module filtMod_ { "Filter", hue::p5sky };
    Pills filterType_, filterSlope_;
    Knob cutoff_, resonance_, drive_;

    // Output
    Module outMod_ { "Output", hue::fg };
    Knob mix_, level_;
    Latch lim_;
    Lamp limLamp_ { hue::amber };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Panel)
};

} // namespace fsl
