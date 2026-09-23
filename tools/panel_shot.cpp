// Offscreen render of the real editor. Dev tool: verifies the panel without a
// host, and produces docs/panel.png for the README. Writes panel.png into the
// directory given as argv[1].
#include <juce_gui_basics/juce_gui_basics.h>

#include "Panel.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"

using namespace fsl;

namespace
{
void setNorm (FasalasProcessor& p, const juce::String& id, float norm)
{
    if (auto* param = p.apvts.getParameter (id))
        param->setValueNotifyingHost (norm);
}

void setValue (FasalasProcessor& p, const juce::String& id, float value)
{
    if (auto* param = p.apvts.getParameter (id))
        param->setValueNotifyingHost (param->convertTo0to1 (value));
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::String ("."));

    FasalasProcessor proc;
    const double sr = 48000.0;
    const int blockSize = 256;
    proc.setRateAndBufferSizeDetails (sr, blockSize);
    proc.prepareToPlay (sr, blockSize);

    // A working patch: every section doing something, so the panel shows
    // itself actually in use rather than sitting at its defaults.
    setValue (proc, pid::gainA, 3.0f);
    setValue (proc, pid::gainB, -2.0f);
    setNorm  (proc, pid::hysteresis, 0.1f);
    setValue (proc, pid::divA, 1.0f);
    setValue (proc, pid::divB, 2.0f);
    setNorm  (proc, pid::stereo, 1.0f); // Stereo

    setNorm  (proc, pid::mode, 2.0f / 4.0f); // PFD
    setNorm  (proc, pid::window, 0.3f);

    setValue (proc, pid::rise, 3.5f);
    setValue (proc, pid::fall, 40.0f);
    setNorm  (proc, pid::link, 0.0f);
    setNorm  (proc, pid::shape, 1.0f); // Exponential

    setNorm  (proc, pid::loop, 1.0f); // on
    setNorm  (proc, pid::vcoRange, 1.0f / 2.0f); // Mid
    setValue (proc, pid::vcoOffset, 6.5f);
    setNorm  (proc, pid::loopTrack, 0.0f); // Main

    setNorm  (proc, pid::filterType, 0.0f); // Low Pass
    setNorm  (proc, pid::filterSlope, 1.0f); // 24 dB
    setValue (proc, pid::cutoff, 2600.0f);
    setNorm  (proc, pid::resonance, 0.6f);
    setValue (proc, pid::drive, 9.0f);

    setNorm  (proc, pid::mix, 1.0f);
    setValue (proc, pid::level, -3.0f);
    setNorm  (proc, pid::limiter, 1.0f); // on

    // Push real signal through it so the lock LED, the limiter lamp and the
    // frequency readouts all show a genuine state rather than their rest values.
    juce::AudioBuffer<float> buffer (4, blockSize); // [mainL mainR sideL sideR]
    juce::MidiBuffer midi;
    double phaseMain = 0.0, phaseSide = 0.0;
    const double freqMain = 110.0, freqSide = 220.0;

    for (int block = 0; block < 60; ++block)
    {
        buffer.clear();
        auto* mL = buffer.getWritePointer (0);
        auto* mR = buffer.getWritePointer (1);
        auto* sL = buffer.getWritePointer (2);
        auto* sR = buffer.getWritePointer (3);

        for (int i = 0; i < blockSize; ++i)
        {
            const float m = 0.7f * (float) std::sin (phaseMain);
            const float s = 0.7f * (float) std::sin (phaseSide);
            mL[i] = m; mR[i] = m;
            sL[i] = s; sR[i] = s;
            phaseMain += juce::MathConstants<double>::twoPi * freqMain / sr;
            phaseSide += juce::MathConstants<double>::twoPi * freqSide / sr;
        }

        proc.processBlock (buffer, midi);
    }

    auto* fasalasEditor = new FasalasEditor (proc);
    std::unique_ptr<juce::AudioProcessorEditor> editor (fasalasEditor);
    editor->setSize (1040, 560);
    fasalasEditor->refreshTelemetryNow();

    const float scale = 2.0f;
    juce::Image img (juce::Image::ARGB, (int) (editor->getWidth() * scale), (int) (editor->getHeight() * scale), true);
    juce::Graphics g (img);
    g.addTransform (juce::AffineTransform::scale (scale));
    editor->paintEntireComponent (g, false);

    const auto out = outDir.getChildFile ("panel.png");
    juce::FileOutputStream stream (out);
    stream.setPosition (0);
    stream.truncate();
    juce::PNGImageFormat().writeImageToStream (img, stream);
    std::printf ("%s\n", out.getFullPathName().toRawUTF8());

    return 0;
}
