#pragma once

#include "PluginProcessor.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>

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
static const juce::Colour env      { 0xffa99bf0 }; // modulation only: env knobs, arcs, assign boxes
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

/** The small violet checkbox that assigns the envelope follower to a knob's
 * target (Window, Offset, Cutoff, Drive) — 11x11, hand-painted rather than
 * routed through the LookAndFeel since nothing else on the panel looks like
 * it. Remember setClickingTogglesState(true): a ButtonAttachment reads
 * getToggleState() on click, and without it the box never actually toggles. */
class AssignBox : public juce::Button
{
public:
    AssignBox() : juce::Button ("assign") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool isHighlighted, bool isDown) override;
};

/** One rotary control: name above, live value below, coloured progress arc.
 * Optionally carries an AssignBox bound to a bool parameter (envtocutoff and
 * friends) at its top-right corner; when that box is ticked and the
 * envelope follower is contributing, the knob also draws a ghost pointer at
 * its own position and a violet arc out to where the modulation has pushed
 * it, via setModulation(). */
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramID,
          const juce::String& displayName, juce::Colour accent,
          const juce::String& assignParamID = {});
    void resized() override;

    /** Called from the editor's UI timer with the envelope's current
     * env * sensitivity. No-op on a knob with no assign box. Only touches
     * the label text/colour or repaints when something actually changed. */
    void setModulation (float envMod);

private:
    juce::RangedAudioParameter* param_;
    juce::Slider slider_;
    juce::Label nameLabel_, valueLabel_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment_;
    std::unique_ptr<AssignBox> assignBox_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> assignAttachment_;

    void refreshValue();
};

/** A row of mutually-exclusive pill buttons bound to a choice or bool parameter.
 * envAccent renders the selected pill violet (fill) with a well-dark label,
 * the same way the amber flag renders the limiter pill amber — used only for
 * the envelope follower's Main/Side source pills. */
class Pills : public juce::Component
{
public:
    Pills (juce::AudioProcessorValueTreeState&, const juce::String& paramID,
           const juce::StringArray& labels, juce::Colour accent, bool envAccent = false);
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
 * whatever rows of controls the caller lays out inside contentArea(). The
 * title sits directly above its row, not as a separate detached band. An
 * optional right-aligned tag (only the envelope follower uses it, for its
 * "-> N targets" readout) lives in the same header strip. */
class Module : public juce::Component
{
public:
    Module (juce::String title, juce::Colour accent);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Rectangle<int> contentArea() const;

    void setTag (const juce::String&);

private:
    juce::String title_;
    juce::Colour accent_;
    juce::Label tag_;
};

/** A five-lane scrolling oscilloscope: main, reference, comparator, slew and
 * output, drained at ~30 fps from the processor's lock-free scope FIFO. */
class Scope : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void pull (FasalasProcessor&);

private:
    static constexpr int historyLength = 1024;
    std::array<float, historyLength> mainBuf_ {}, refBuf_ {}, pcBuf_ {}, slewBuf_ {}, outBuf_ {};
    int writeIndex_ = 0;
};

/** The envelope follower's own small scope: ~2 s of history, the detected
 * input level as a filled grey area and the envelope as a violet line, drained
 * at ~30 fps from the processor's env-scope FIFO (already decimated to ~1 kHz
 * there, so this just keeps the last 2000 points). */
class EnvScope : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void pull (FasalasProcessor&);
    void setSourceLabel (const juce::String& s) { if (sourceLabel_ != s) { sourceLabel_ = s; repaint(); } }

private:
    static constexpr int historyLength = 2000;
    std::array<float, historyLength> inputBuf_ {}, envBuf_ {};
    int writeIndex_ = 0;
    juce::String sourceLabel_ { "SIDE" };
};

/** The whole plugin panel: seven modules across two rows, a top bar with the
 * wordmark and the lock LED, and the scope along the bottom — all sections
 * talking directly to the processor's APVTS. */
class Panel : public juce::Component
{
public:
    explicit Panel (FasalasProcessor&);
    ~Panel() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Called from the editor's UI timer: refreshes telemetry (lock, lamp,
     * readouts) and pulls the latest scope samples. */
    void refresh();

private:
    static void layoutRow (juce::Rectangle<int> area, std::initializer_list<juce::Component*> items, int gap);
    static void layoutCols (juce::Rectangle<int> area, std::initializer_list<std::pair<juce::Component*, float>> cols);

    FasalasProcessor& proc_;
    FasalasLookAndFeel lnf_;
    Scope scope_;

    // Top bar
    Lamp lockLed_ { hue::p4blue };
    juce::Label lockCaption_, wordmark_;

    // Input
    Module inMod_ { "Input", hue::p4blue };
    Knob gainA_, gainB_, hyst_, divA_, divB_;
    Pills stereo_;

    // Env follower
    Module envMod_ { juce::String::fromUTF8 ("Env follower"), hue::env };
    Pills envSource_;
    Knob envSens_, envRise_, envHold_, envFall_;
    EnvScope envScope_;

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
    Knob vcoOffset_, vcoSoften_;
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
