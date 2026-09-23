#pragma once

#include <cmath>

namespace fsl
{

enum class FilterType { lowpass = 0, bandpass, highpass, notch };
enum class FilterSlope { twelve = 0, twentyFour };

/**
 * Resonant multimode filter, 12 or 24 dB/oct.
 *
 * One Zavalishin topology-preserving-transform state-variable stage covers
 * LP/BP/HP/Notch and self-oscillates cleanly near full resonance. 24 dB/oct
 * cascades two stages with a Butterworth Q split (0.54 / 1.31 in filter-Q
 * terms) so the pair stays flat through the passband instead of doubling one
 * resonant peak; only the second stage carries the resonance control.
 */
class MultimodeFilter
{
public:
    void prepare (double sampleRate) { sampleRate_ = sampleRate > 0.0 ? sampleRate : 48000.0; }

    void reset()
    {
        s1_[0] = s1_[1] = 0.0f;
        s2_[0] = s2_[1] = 0.0f;
    }

    void setType (FilterType t)   { type_ = t; }
    void setSlope (FilterSlope s) { slope24_ = (s == FilterSlope::twentyFour); }

    /** cutoffHz in Hz, resonance in 0..1. Call once per block (or per sample if modulated). */
    void setParams (float cutoffHz, float resonance)
    {
        const float fc = juce_clamp (cutoffHz, 20.0f, (float) (0.45 * sampleRate_));
        g_ = std::tan (juce_pi * fc / (float) sampleRate_);

        if (slope24_)
        {
            k1_ = 1.848f;
            k2_ = 0.765f + (0.03f - 0.765f) * resonance;
        }
        else
        {
            k1_ = 1.414f + (0.03f - 1.414f) * resonance;
        }
    }

    float process (float x)
    {
        float y = stage (s1_, x, k1_);
        if (slope24_)
            y = stage (s2_, y, k2_);

        if (! std::isfinite (y))
        {
            reset();
            y = 0.0f;
        }
        return y;
    }

private:
    static constexpr float juce_pi = 3.14159265358979323846f;
    static float juce_clamp (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

    /** One TPT SVF stage. state[0] = ic1eq (bandpass integrator), state[1] = ic2eq (lowpass integrator). */
    float stage (float (&st)[2], float v0, float k)
    {
        const float g = g_;
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;

        const float v3 = v0 - st[1];
        const float v1 = a1 * st[0] + a2 * v3;
        const float v2 = st[1] + a2 * st[0] + a3 * v3;
        st[0] = 2.0f * v1 - st[0];
        st[1] = 2.0f * v2 - st[1];

        switch (type_)
        {
            case FilterType::lowpass:  return v2;
            case FilterType::bandpass: return v1;
            case FilterType::highpass: return v0 - k * v1 - v2;
            default:                   return v0 - k * v1; // notch
        }
    }

    double sampleRate_ = 48000.0;
    FilterType type_ = FilterType::lowpass;
    bool slope24_ = true;
    float g_ = 0.1f, k1_ = 1.4f, k2_ = 0.4f;
    float s1_[2] { 0.0f, 0.0f };
    float s2_[2] { 0.0f, 0.0f };
};

} // namespace fsl
