#pragma once

#include <algorithm>
#include <cmath>

namespace fsl
{

enum class VcoRange { low = 0, mid, high };

/**
 * The A-196's own range/offset table (section 4, control 1 • 2): Range picks
 * the bottom of the band, Offset (0..10) picks the top within it. The module's
 * HIGH band tops out at 100 kHz on real hardware; capped here at 20 kHz
 * (raise it once the plugin runs oversampled and can follow it further).
 */
struct VcoRangeTable
{
    float lo, hiAtOffset0, hiAtOffset10;
};

inline VcoRangeTable vcoRangeTable (VcoRange r)
{
    switch (r)
    {
        case VcoRange::low:  return { 2.0f, 50.0f, 1000.0f };
        case VcoRange::mid:  return { 20.0f, 500.0f, 10000.0f };
        default:             return { 200.0f, 5000.0f, 20000.0f };
    }
}

inline float vcoTopFrequency (VcoRange r, float offset0to10)
{
    const auto t = vcoRangeTable (r);
    const float o = std::clamp (offset0to10, 0.0f, 10.0f) / 10.0f;
    return t.hiAtOffset0 * std::pow (t.hiAtOffset10 / t.hiAtOffset0, o);
}

/**
 * Loop-mode oscillator: a band-limited (polyBLEP) square, linear in the CV
 * that drives it — the A-196's own VCO is a linear, not exponential, design.
 */
class Vco
{
public:
    void prepare (double sampleRate) { sampleRate_ = sampleRate > 0.0 ? sampleRate : 48000.0; }
    void reset() { phase_ = 0.0f; }

    /** cv in -1..+1 (the slew output); frequency = lo + (hi - lo) * (cv + 1) / 2.
     * soften morphs the output from the band-limited square (0) towards a
     * triangle (1) by clipping a triangle that shares the square's zero
     * crossings less and less hard — the corners round off progressively
     * instead of cross-fading between two waveforms, so loop locking (which
     * only cares about the edges) is unaffected. Below 0.02 this is exactly
     * the plain polyBLEP square. */
    float process (float cv, float lo, float hi, float soften = 0.0f)
    {
        freq_ = lo + (hi - lo) * (std::clamp (cv, -1.0f, 1.0f) + 1.0f) * 0.5f;
        freq_ = std::min (freq_, (float) (0.45 * sampleRate_));

        const float dt = freq_ / (float) sampleRate_;
        float v;

        if (soften < 0.02f)
        {
            float p2 = phase_ + 0.5f;
            if (p2 >= 1.0f) p2 -= 1.0f;
            v = (phase_ < 0.5f ? 1.0f : -1.0f) + blep (phase_, dt) - blep (p2, dt);
        }
        else
        {
            const float k = 1.0f / std::max (soften, 0.02f);
            v = std::clamp (tri (phase_) * k, -1.0f, 1.0f);
        }

        phase_ += dt;
        if (phase_ >= 1.0f) phase_ -= 1.0f;

        return v;
    }

    float lastFrequency() const { return freq_; }

private:
    static float blep (float t, float dt)
    {
        if (dt <= 0.0f) return 0.0f;
        if (t < dt) { t = t / dt; return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    // Triangle with the same zero crossings as the square above: 0 at p=0,
    // +1 at p=0.25, 0 at p=0.5, -1 at p=0.75.
    static float tri (float p)
    {
        if (p < 0.25f) return 4.0f * p;
        if (p < 0.75f) return 2.0f - 4.0f * p;
        return 4.0f * p - 4.0f;
    }

    double sampleRate_ = 48000.0;
    float phase_ = 0.0f;
    float freq_ = 110.0f;
};

} // namespace fsl
