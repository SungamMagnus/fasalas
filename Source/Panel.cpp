#include "Panel.h"

#include <cmath>

namespace fsl
{

juce::Colour accentOf (const juce::Component& c)
{
    auto v = c.getProperties()["accent"];
    return v.isVoid() ? hue::fg : juce::Colour ((juce::uint32) (juce::int64) v);
}

void setAccent (juce::Component& c, juce::Colour colour)
{
    c.getProperties().set ("accent", (juce::int64) colour.getARGB());
}

// ───────────────────────────── LookAndFeel ─────────────────────────────

FasalasLookAndFeel::FasalasLookAndFeel()
{
    setColour (juce::Label::textColourId, hue::fg);
    setColour (juce::Slider::textBoxTextColourId, hue::fg);
}

void FasalasLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                            float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                            juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (2.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const juce::Colour accent = accentOf (slider);
    const float trackR = radius * 0.8f;
    const float strokeW = juce::jmax (2.0f, radius * 0.16f);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, trackR, trackR, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (hue::line2);
    g.strokePath (track, juce::PathStrokeType (strokeW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (sliderPos > 0.004f)
    {
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, trackR, trackR, 0.0f, rotaryStartAngle, angle, true);
        g.setColour (accent);
        g.strokePath (arc, juce::PathStrokeType (strokeW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float capR = radius * 0.52f;
    g.setColour (hue::cap);
    g.fillEllipse (juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (centre));
    g.setColour (hue::line2);
    g.drawEllipse (juce::Rectangle<float> (capR * 2.0f, capR * 2.0f).withCentre (centre), 1.0f);

    const juce::Point<float> p1 (centre.x + capR * 0.3f * std::sin (angle), centre.y - capR * 0.3f * std::cos (angle));
    const juce::Point<float> p2 (centre.x + capR * 0.88f * std::sin (angle), centre.y - capR * 0.88f * std::cos (angle));
    g.setColour (hue::fg);
    g.drawLine ({ p1, p2 }, 2.0f);
}

void FasalasLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                                bool isHighlighted, bool)
{
    const auto bounds = b.getLocalBounds().toFloat();
    const bool amber = (bool) b.getProperties().getWithDefault ("amber", false);
    const bool on = b.getToggleState();

    juce::Colour fill = juce::Colours::transparentBlack;
    juce::Colour border = amber ? hue::amber.withAlpha (0.55f) : hue::line2;

    if (on)       { fill = amber ? hue::amber : hue::fg; border = fill; }
    else if (isHighlighted) { border = amber ? hue::amber : hue::fg; }

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 2.0f);
    g.setColour (border);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 2.0f, 1.0f);
}

void FasalasLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool isHighlighted, bool)
{
    const bool amber = (bool) b.getProperties().getWithDefault ("amber", false);
    const bool on = b.getToggleState();

    juce::Colour c;
    if (on)                 c = amber ? hue::well : hue::bg;
    else if (isHighlighted) c = amber ? hue::amber : hue::fg;
    else                    c = amber ? juce::Colour (0xffdcae45) : hue::muted;

    g.setColour (c);
    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.drawFittedText (b.getButtonText().toUpperCase(), b.getLocalBounds(), juce::Justification::centred, 1);
}

juce::Font FasalasLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (juce::jmin (11.0f, buttonHeight * 0.52f)).withExtraKerningFactor (0.05f);
}

// ─────────────────────────────── Knob ──────────────────────────────────

Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramID,
            const juce::String& displayName, juce::Colour accent)
    : param_ (state.getParameter (paramID))
{
    jassert (param_ != nullptr);
    setAccent (slider_, accent);

    slider_.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider_.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f, juce::MathConstants<float>::pi * 2.8f, true);
    slider_.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    slider_.setMouseDragSensitivity (110);
    slider_.setDoubleClickReturnValue (true, param_->convertFrom0to1 (param_->getDefaultValue()));
    slider_.onValueChange = [this] { refreshValue(); };
    addAndMakeVisible (slider_);

    nameLabel_.setText (displayName, juce::dontSendNotification);
    nameLabel_.setJustificationType (juce::Justification::centred);
    nameLabel_.setFont (juce::Font (9.5f).withExtraKerningFactor (0.03f));
    nameLabel_.setColour (juce::Label::textColourId, hue::muted);
    nameLabel_.setMinimumHorizontalScale (0.55f);
    addAndMakeVisible (nameLabel_);

    valueLabel_.setJustificationType (juce::Justification::centred);
    valueLabel_.setFont (juce::Font (10.5f));
    valueLabel_.setColour (juce::Label::textColourId, hue::fg);
    valueLabel_.setMinimumHorizontalScale (0.55f);
    addAndMakeVisible (valueLabel_);

    attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramID, slider_);
    refreshValue();

    setSize (58, 76);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    nameLabel_.setBounds (r.removeFromTop (14));
    valueLabel_.setBounds (r.removeFromBottom (14));
    slider_.setBounds (r.reduced (3));
}

void Knob::refreshValue()
{
    valueLabel_.setText (param_->getCurrentValueAsText(), juce::dontSendNotification);
}

// ─────────────────────────────── Pills ─────────────────────────────────

Pills::Pills (juce::AudioProcessorValueTreeState& state, const juce::String& paramID,
              const juce::StringArray& labels, juce::Colour accent)
{
    auto* param = state.getParameter (paramID);
    jassert (param != nullptr);
    const int current = juce::roundToInt (param->convertFrom0to1 (param->getValue()));

    static int nextGroupId = 1000;
    const int groupId = nextGroupId++;

    const juce::Font f (10.5f);
    const int h = 22, gap = 4;
    int x = 0;

    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons_.add (new juce::TextButton (labels[i]));
        addAndMakeVisible (b);
        setAccent (*b, accent);
        b->setClickingTogglesState (true);
        b->setRadioGroupId (groupId, juce::dontSendNotification);
        b->setToggleState (i == current, juce::dontSendNotification);

        const int w = juce::jmax (30, juce::GlyphArrangement::getStringWidthInt (f, labels[i]) + 16);
        b->setBounds (x, 0, w, h);
        x += w + gap;

        b->onClick = [param, i]
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost (param->convertTo0to1 ((float) i));
            param->endChangeGesture();
        };
    }

    setSize (juce::jmax (0, x - gap), h);
}

void Pills::resized() {}

// ─────────────────────────────── Latch ─────────────────────────────────

Latch::Latch (juce::AudioProcessorValueTreeState& state, const juce::String& paramID,
              const juce::String& text, juce::Colour accent)
{
    button_.setButtonText (text);
    setAccent (button_, accent);
    addAndMakeVisible (button_);
    attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, paramID, button_);

    const int w = juce::jmax (40, juce::GlyphArrangement::getStringWidthInt (juce::Font (10.5f), text) + 20);
    setSize (w, 22);
}

void Latch::resized() { button_.setBounds (getLocalBounds()); }

void Latch::setAmber (bool b)
{
    button_.getProperties().set ("amber", b);
    button_.repaint();
}

// ─────────────────────────────── Lamp ──────────────────────────────────

void Lamp::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (on_ ? onColour_ : hue::cap);
    g.fillEllipse (r);
    g.setColour (on_ ? onColour_ : hue::line2);
    g.drawEllipse (r, 1.0f);
    if (on_)
    {
        g.setColour (onColour_.withAlpha (0.35f));
        g.fillEllipse (r.expanded (2.5f));
    }
}

// ─────────────────────────────── Module ────────────────────────────────

Module::Module (juce::String title, juce::Colour accent) : title_ (title), accent_ (accent) {}

void Module::paint (juce::Graphics& g)
{
    g.fillAll (hue::panel);
    auto head = getLocalBounds().removeFromTop (22);

    g.setColour (accent_);
    g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ (float) head.getX() + 14.0f, (float) head.getCentreY() }));

    g.setColour (hue::muted);
    g.setFont (juce::Font (10.5f).withExtraKerningFactor (0.07f));
    g.drawFittedText (title_.toUpperCase(), head.withTrimmedLeft (24).withTrimmedRight (8),
                       juce::Justification::centredLeft, 1);
}

juce::Rectangle<int> Module::contentArea() const { return getLocalBounds().withTrimmedTop (26).reduced (6); }

// ─────────────────────────────── Panel ─────────────────────────────────

Panel::Panel (juce::AudioProcessorValueTreeState& state)
    : state_ (state),
      gainA_ (state, pid::gainA, "Main", hue::p4blue),
      gainB_ (state, pid::gainB, "Sidechain", hue::p2pink),
      hyst_ (state, pid::hysteresis, "Hysteresis", hue::p4blue),
      divA_ (state, pid::divA, juce::String::fromUTF8 ("\xc3\xb7 Main"), hue::p4blue),
      divB_ (state, pid::divB, juce::String::fromUTF8 ("\xc3\xb7 Side"), hue::p2pink),
      stereo_ (state, pid::stereo, { "Mono", "Stereo" }, hue::p4blue),
      mode_ (state, pid::mode, { "XOR", "RS", "PFD", "CMP", "WIN" }, hue::p3rose),
      window_ (state, pid::window, "Window", hue::p3rose),
      rise_ (state, pid::rise, "Rise", hue::p1lilac),
      fall_ (state, pid::fall, "Fall", hue::p1lilac),
      shape_ (state, pid::shape, { "LIN", "EXP" }, hue::p1lilac),
      link_ (state, pid::link, "Link", hue::p1lilac),
      loop_ (state, pid::loop, "Loop", hue::p1lilac),
      vcoOffset_ (state, pid::vcoOffset, "Offset", hue::p1lilac),
      vcoRange_ (state, pid::vcoRange, { "LO", "MID", "HI" }, hue::p1lilac),
      loopTrack_ (state, pid::loopTrack, { "MAIN", "SIDE" }, hue::p1lilac),
      filterType_ (state, pid::filterType, { "LP", "BP", "HP", "NT" }, hue::p5sky),
      filterSlope_ (state, pid::filterSlope, { "12", "24" }, hue::p5sky),
      cutoff_ (state, pid::cutoff, "Cutoff", hue::p5sky),
      resonance_ (state, pid::resonance, "Resonance", hue::p5sky),
      drive_ (state, pid::drive, "Drive", hue::p5sky),
      mix_ (state, pid::mix, "Mix", hue::fg),
      level_ (state, pid::level, "Level", hue::fg),
      lim_ (state, pid::limiter, "Lim", hue::amber)
{
    juce::LookAndFeel::setDefaultLookAndFeel (&lnf_);
    lim_.setAmber (true);

    wordmark_.setText (juce::String::fromUTF8 ("fasal\xc3\xa1s"), juce::dontSendNotification);
    wordmark_.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
    wordmark_.setColour (juce::Label::textColourId, hue::p4blue);
    addAndMakeVisible (wordmark_);

    lockCaption_.setText ("Lock", juce::dontSendNotification);
    lockCaption_.setFont (juce::Font (10.0f).withExtraKerningFactor (0.06f));
    lockCaption_.setColour (juce::Label::textColourId, hue::muted);
    lockCaption_.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (lockCaption_);
    addAndMakeVisible (lockLed_);
    lockLed_.setSize (11, 11);

    for (auto* m : { &inMod_, &cmpMod_, &slewMod_, &loopMod_, &filtMod_, &outMod_ })
        addAndMakeVisible (m);
    for (auto* c : { (juce::Component*) &gainA_, (juce::Component*) &gainB_, (juce::Component*) &hyst_,
                      (juce::Component*) &divA_, (juce::Component*) &divB_, (juce::Component*) &stereo_ })
        inMod_.addAndMakeVisible (c);
    for (auto* c : { (juce::Component*) &mode_, (juce::Component*) &window_ })
        cmpMod_.addAndMakeVisible (c);
    for (auto* c : { (juce::Component*) &rise_, (juce::Component*) &fall_,
                      (juce::Component*) &shape_, (juce::Component*) &link_ })
        slewMod_.addAndMakeVisible (c);
    for (auto* c : { (juce::Component*) &loop_, (juce::Component*) &vcoOffset_,
                      (juce::Component*) &vcoRange_, (juce::Component*) &loopTrack_ })
        loopMod_.addAndMakeVisible (c);
    for (auto* c : { (juce::Component*) &filterType_, (juce::Component*) &filterSlope_,
                      (juce::Component*) &cutoff_, (juce::Component*) &resonance_, (juce::Component*) &drive_ })
        filtMod_.addAndMakeVisible (c);
    for (auto* c : { (juce::Component*) &mix_, (juce::Component*) &level_, (juce::Component*) &lim_ })
        outMod_.addAndMakeVisible (c);
    outMod_.addAndMakeVisible (limLamp_);
    limLamp_.setSize (11, 11);

    freqReadout_.setFont (juce::Font (9.5f));
    freqReadout_.setColour (juce::Label::textColourId, hue::muted);
    freqReadout_.setJustificationType (juce::Justification::centred);
    freqReadout_.setMinimumHorizontalScale (0.6f);
    freqReadout_.setSize (150, 30);
    freqReadout_.setText (juce::String::fromUTF8 ("Main \xe2\x80\x94\nRef \xe2\x80\x94"), juce::dontSendNotification);
    cmpMod_.addAndMakeVisible (freqReadout_);

    vcoReadout_.setFont (juce::Font (9.5f));
    vcoReadout_.setColour (juce::Label::textColourId, hue::muted);
    vcoReadout_.setJustificationType (juce::Justification::centred);
    vcoReadout_.setMinimumHorizontalScale (0.6f);
    vcoReadout_.setSize (90, 20);
    vcoReadout_.setText ("Off", juce::dontSendNotification);
    loopMod_.addAndMakeVisible (vcoReadout_);

    setSize (1040, 560);
}

Panel::~Panel() { juce::LookAndFeel::setDefaultLookAndFeel (nullptr); }

void Panel::paint (juce::Graphics& g)
{
    g.fillAll (hue::bg);
    g.setColour (hue::sep);
    g.drawRect (getLocalBounds().reduced (0), 1);
}

void Panel::layoutRow (juce::Rectangle<int> area, std::initializer_list<juce::Component*> items, int gap)
{
    int totalW = -gap;
    for (auto* c : items) totalW += c->getWidth() + gap;
    int x = area.getCentreX() - totalW / 2;
    for (auto* c : items)
    {
        c->setBounds (x, area.getCentreY() - c->getHeight() / 2, c->getWidth(), c->getHeight());
        x += c->getWidth() + gap;
    }
}

void Panel::resized()
{
    auto r = getLocalBounds();

    auto top = r.removeFromTop (40).reduced (14, 8);
    wordmark_.setBounds (top.removeFromLeft (200));
    lockLed_.setBounds (top.removeFromRight (16).withSizeKeepingCentre (11, 11));
    lockCaption_.setBounds (top.removeFromRight (56));

    r.reduce (1, 1);
    const int colW = r.getWidth() / 3;
    const int rowH = r.getHeight() / 2;
    juce::Rectangle<int> cells[6];
    for (int i = 0; i < 6; ++i)
        cells[i] = { r.getX() + (i % 3) * colW, r.getY() + (i / 3) * rowH, colW, rowH };
    cells[2].setWidth (r.getRight() - cells[2].getX());
    cells[5].setWidth (r.getRight() - cells[5].getX());

    inMod_.setBounds (cells[0]);
    cmpMod_.setBounds (cells[1]);
    slewMod_.setBounds (cells[2]);
    loopMod_.setBounds (cells[3]);
    filtMod_.setBounds (cells[4]);
    outMod_.setBounds (cells[5]);

    {
        auto a = inMod_.contentArea();
        auto row2 = a.removeFromBottom (26);
        layoutRow (a, { &gainA_, &gainB_, &hyst_, &divA_, &divB_ }, 6);
        layoutRow (row2, { &stereo_ }, 0);
    }
    {
        auto a = cmpMod_.contentArea();
        auto row1 = a.removeFromTop (26);
        layoutRow (row1, { &mode_ }, 0);
        layoutRow (a, { &window_, &freqReadout_ }, 10);
    }
    {
        auto a = slewMod_.contentArea();
        auto row2 = a.removeFromBottom (26);
        layoutRow (a, { &rise_, &fall_ }, 10);
        layoutRow (row2, { &shape_, &link_ }, 8);
    }
    {
        auto a = loopMod_.contentArea();
        auto row1 = a.removeFromTop (26);
        auto row3 = a.removeFromBottom (26);
        layoutRow (row1, { &loop_, &vcoReadout_ }, 10);
        layoutRow (a, { &vcoOffset_ }, 0);
        layoutRow (row3, { &vcoRange_, &loopTrack_ }, 8);
    }
    {
        auto a = filtMod_.contentArea();
        auto row1 = a.removeFromTop (26);
        layoutRow (row1, { &filterType_, &filterSlope_ }, 8);
        layoutRow (a, { &cutoff_, &resonance_, &drive_ }, 8);
    }
    {
        auto a = outMod_.contentArea();
        auto row2 = a.removeFromBottom (26);
        layoutRow (a, { &mix_, &level_ }, 10);
        layoutRow (row2, { &lim_, &limLamp_ }, 8);
    }
}

void Panel::updateTelemetry (const Telemetry& t)
{
    const bool locked = t.locked.load();
    const bool harmonic = t.harmonicLock.load();
    lockLed_.setOn (locked);
    lockCaption_.setText (locked && harmonic ? juce::String::fromUTF8 ("Lock \xc2\xb7 harm") : juce::String ("Lock"),
                           juce::dontSendNotification);

    auto fmtHz = [] (float hz) -> juce::String
    {
        if (hz <= 0.5f) return juce::String::fromUTF8 ("\xe2\x80\x94");
        return hz >= 1000.0f ? juce::String (hz / 1000.0f, 2) + " kHz" : juce::String (hz, 1) + " Hz";
    };

    freqReadout_.setText ("Main " + fmtHz (t.mainHz.load()) + "\nRef " + fmtHz (t.refHz.load()),
                           juce::dontSendNotification);

    vcoReadout_.setText (loop_.isOn() ? ("VCO " + fmtHz (t.vcoHz.load())) : juce::String ("Off"),
                          juce::dontSendNotification);

    limLamp_.setOn (lim_.isOn() && t.reduction.load() < 0.995f);
}

} // namespace fsl
