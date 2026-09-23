#pragma once

#include <algorithm>
#include <cmath>

namespace fsl
{

enum class SlewShape { linear = 0, exponential };

/**
 * The A-196's low pass filter stage (section 4, control 5), generalised to
 * separate rise/fall times and two shapes: Exponential is the module's own
 * RC low-pass; Linear is a constant-rate limiter, like the A-171 the manual
 * suggests as an external replacement. This is where "smooth CV" (slow) and
 * "frequency jitter" (fast) live, on purpose.
 */
class Slew
{
public:
    void prepare (double sampleRate) { sampleRate_ = sampleRate > 0.0 ? sampleRate : 48000.0; }
    void reset() { value_ = 0.0f; }

    void setShape (SlewShape s) { shape_ = s; }

    /** riseMs/fallMs in milliseconds. Recompute coefficients when they change. */
    void setTimes (float riseMs, float fallMs)
    {
        const float riseSamples = std::max (0.5f, riseMs * 0.001f * (float) sampleRate_);
        const float fallSamples = std::max (0.5f, fallMs * 0.001f * (float) sampleRate_);
        upStep_ = 2.0f / riseSamples;
        dnStep_ = 2.0f / fallSamples;
        upCoef_ = 1.0f - std::exp (-1.0f / riseSamples);
        dnCoef_ = 1.0f - std::exp (-1.0f / fallSamples);
    }

    float process (float target)
    {
        if (shape_ == SlewShape::linear)
        {
            if (target > value_) value_ = std::min (target, value_ + upStep_);
            else                 value_ = std::max (target, value_ - dnStep_);
        }
        else
        {
            const float c = target > value_ ? upCoef_ : dnCoef_;
            value_ += (target - value_) * c;
        }
        return value_;
    }

    float current() const { return value_; }

private:
    double sampleRate_ = 48000.0;
    SlewShape shape_ = SlewShape::exponential;
    float value_ = 0.0f;
    float upStep_ = 0.1f, dnStep_ = 0.1f, upCoef_ = 0.1f, dnCoef_ = 0.1f;
};

} // namespace fsl
