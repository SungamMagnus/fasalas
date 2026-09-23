#pragma once

#include <algorithm>
#include <cmath>

namespace fsl
{

/**
 * Peak-detector envelope follower: rises to the peak, holds it, then falls —
 * the classic three-stage shape, not a one-pole smoother. Runs once per
 * base-rate sample on a single, stereo-linked (max of L/R) signal, so there
 * is one envelope regardless of how many engines are modulated from it.
 */
class EnvFollower
{
public:
    void prepare (double sampleRate) { sampleRate_ = sampleRate > 0.0 ? sampleRate : 48000.0; reset(); }
    void reset() { env_ = 0.0f; holdSamplesLeft_ = 0; }

    /** riseMs/fallMs are the coefficient times; holdMs is how long the peak
     * is held at its detected level before Fall is allowed to start. */
    void setTimes (float riseMs, float holdMs, float fallMs)
    {
        riseCoef_ = coef (riseMs);
        fallCoef_ = coef (fallMs);
        holdSamples_ = (int) std::round (std::max (0.0f, holdMs) * 0.001f * (float) sampleRate_);
    }

    /** l/r are the raw, un-gained selected input for this sample. */
    float process (float l, float r)
    {
        const float x = std::max (std::fabs (l), std::fabs (r));
        if (x > env_)
        {
            env_ += (x - env_) * riseCoef_;
            holdSamplesLeft_ = holdSamples_;
        }
        else if (holdSamplesLeft_ > 0)
        {
            --holdSamplesLeft_;
        }
        else
        {
            env_ += (x - env_) * fallCoef_;
        }
        return env_;
    }

    float current() const { return env_; }

private:
    float coef (float ms) const
    {
        const float t = std::max (0.05f, ms) * 0.001f * (float) sampleRate_;
        return 1.0f - std::exp (-1.0f / t);
    }

    double sampleRate_ = 48000.0;
    float env_ = 0.0f;
    float riseCoef_ = 0.5f, fallCoef_ = 0.01f;
    int holdSamples_ = 0, holdSamplesLeft_ = 0;
};

} // namespace fsl
