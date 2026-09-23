#pragma once

#include "Comparator.h"
#include "MultimodeFilter.h"
#include "Slew.h"
#include "Vco.h"

#include <algorithm>
#include <cmath>

namespace fsl
{

/** Everything the panel needs to draw one sample: the scope traces and the
 * frequency/lock telemetry, exactly the signals the browser prototype shows. */
struct EngineTap
{
    float main = 0.0f, side = 0.0f;   // conditioned (post-gain, pre-squaring) inputs
    float pc = 0.0f;                  // comparator output
    float slew = 0.0f;                // slew output / VCO control voltage
    float out = 0.0f;                 // engine output (pre mix/level was applied inline)
    bool qa = false, qb = false;      // squared/divided main and side, for the scope's gate fill
};

struct EngineParams
{
    float gainADb = 0.0f, gainBDb = 0.0f;
    float hysteresis = 0.02f;
    int divA = 1, divB = 1;

    CompareMode mode = CompareMode::xor_;
    float window = 0.2f;

    float riseMs = 0.2f, fallMs = 0.2f;
    SlewShape slewShape = SlewShape::exponential;

    bool loopOn = false;
    bool loopLockToMain = true;   // false = locks to the sidechain instead
    VcoRange vcoRange = VcoRange::mid;
    float vcoOffset = 5.0f;
    float soften = 0.35f;

    FilterType filterType = FilterType::lowpass;
    FilterSlope filterSlope = FilterSlope::twentyFour;
    float cutoffHz = 3200.0f;
    float resonance = 0.35f;
    float driveDb = 3.0f;

    float mix = 1.0f;
    float levelDb = -6.0f;
};

/**
 * One channel of the signal chain: condition both inputs, compare, slew,
 * optionally close the loop through a VCO, drive into the filter, blend with
 * dry. The limiter is stereo-linked and lives one level up, in the processor.
 */
class Engine
{
public:
    void prepare (double sampleRate)
    {
        sampleRate_ = sampleRate > 0.0 ? sampleRate : 48000.0;
        slew_.prepare (sampleRate_);
        vco_.prepare (sampleRate_);
        filter_.prepare (sampleRate_);
        reset();
    }

    void reset()
    {
        condA_.reset(); condB_.reset(); comparator_.reset();
        slew_.reset(); vco_.reset(); filter_.reset();
        dcX_ = dcY_ = 0.0f;
        periodA_ = periodB_ = 0.0f; countA_ = countB_ = 0;
    }

    void setParams (const EngineParams& p)
    {
        gainA_ = dbToLin (p.gainADb);
        gainB_ = dbToLin (p.gainBDb);
        hysteresis_ = std::max (0.0005f, p.hysteresis);
        divA_ = p.divA; divB_ = p.divB;
        mode_ = p.mode; window_ = p.window;

        slew_.setShape (p.slewShape);
        slew_.setTimes (p.riseMs, p.fallMs);

        loop_ = p.loopOn; lockToMain_ = p.loopLockToMain;
        vcoRange_ = p.vcoRange;
        const auto top = vcoTopFrequency (p.vcoRange, p.vcoOffset);
        const auto lo  = vcoRangeTable (p.vcoRange).lo;
        vcoLo_ = lo;
        vcoHi_ = std::min (top, (float) (0.45 * sampleRate_));
        soften_ = p.soften;

        filter_.setType (p.filterType);
        filter_.setSlope (p.filterSlope);
        resonance_ = p.resonance;
        filter_.setParams (p.cutoffHz, resonance_);

        driveLin_ = dbToLin (p.driveDb);
        mix_ = p.mix;
        levelLin_ = dbToLin (p.levelDb);
    }

    /** Cheap per-control-tick update for just the four envelope-modulatable
     * targets — cutoff, drive, window and the VCO's top frequency — without
     * touching gains, dividers, comparator mode or slew times the way the
     * full setParams() does. Called far more often than setParams(), so it
     * only recomputes what these four actually feed. */
    void setModulated (float cutoffHz, float driveDb, float window, float vcoOffset)
    {
        filter_.setParams (cutoffHz, resonance_);
        driveLin_ = dbToLin (driveDb);
        window_ = window;
        const auto top = vcoTopFrequency (vcoRange_, vcoOffset);
        vcoHi_ = std::min (top, (float) (0.45 * sampleRate_));
    }

    /** dry is the raw main input (before any gain), used for the dry/wet blend. */
    float process (float mainRaw, float sideRaw, float dry, EngineTap& tap)
    {
        float a = mainRaw * gainA_;
        float b = sideRaw * gainB_;
        float vcoOut = 0.0f;

        if (loop_)
        {
            // Loop mode: the slew output is a linear CV for the VCO, which
            // replaces the main input at comparator input 1 (as on the module).
            const float cv = std::clamp (slew_.current(), -1.0f, 1.0f);
            vcoOut = vco_.process (cv, vcoLo_, vcoHi_, soften_);
            b = lockToMain_ ? a : b;
            a = vcoOut;
        }

        bool qa, qb;
        const bool risingA = condA_.process (a, hysteresis_, divA_, qa);
        const bool risingB = condB_.process (b, hysteresis_, divB_, qb);

        trackPeriod (risingA, periodA_, countA_);
        trackPeriod (risingB, periodB_, countB_);

        bool hold = false;
        const float pc = comparator_.process (mode_, loop_, qa, qb, risingA, risingB,
                                               a, b, hysteresis_, window_, hold);

        const float slewTarget = hold ? slew_.current() : pc;
        const float sl = slew_.process (slewTarget);

        const float driveIn = loop_ ? vcoOut * 0.8f : sl;
        const float x = std::tanh (driveIn * driveLin_);
        const float filtered = filter_.process (x);

        // 5 Hz one-pole DC blocker: PFD holds and asymmetric slews leave DC behind.
        const float dc = filtered - dcX_ + 0.9993f * dcY_;
        dcX_ = filtered; dcY_ = dc;

        const float wet = dc;
        const float out = (dry * (1.0f - mix_) + wet * mix_) * levelLin_;

        tap.main = a; tap.side = b; tap.pc = pc; tap.slew = sl; tap.out = out;
        tap.qa = qa; tap.qb = qb;
        return out;
    }

    /** Detected frequencies in Hz, smoothed over a few edges; 0 when nothing has been seen recently. */
    float mainFrequencyHz() const { return periodA_ > 0.0f ? (float) sampleRate_ / periodA_ : 0.0f; }
    float sideFrequencyHz() const { return periodB_ > 0.0f ? (float) sampleRate_ / periodB_ : 0.0f; }
    float vcoFrequencyHz()  const { return vco_.lastFrequency(); }
    float vcoRangeLowHz()   const { return vcoLo_; }
    float vcoRangeHighHz()  const { return vcoHi_; }

private:
    static float dbToLin (float db) { return std::pow (10.0f, db / 20.0f); }

    void trackPeriod (bool rising, float& period, int& count)
    {
        ++count;
        if (rising)
        {
            period = period > 0.0f ? period * 0.7f + (float) count * 0.3f : (float) count;
            count = 0;
        }
        if ((double) count > sampleRate_ * 0.25) period = 0.0f;
    }

    double sampleRate_ = 48000.0;

    Conditioner condA_, condB_;
    Comparator comparator_;
    Slew slew_;
    Vco vco_;
    MultimodeFilter filter_;

    float gainA_ = 1.0f, gainB_ = 1.0f, hysteresis_ = 0.02f;
    int divA_ = 1, divB_ = 1;
    CompareMode mode_ = CompareMode::xor_;
    float window_ = 0.2f;

    bool loop_ = false, lockToMain_ = true;
    float vcoLo_ = 20.0f, vcoHi_ = 500.0f;
    VcoRange vcoRange_ = VcoRange::mid;
    float soften_ = 0.35f;
    float resonance_ = 0.35f;

    float driveLin_ = 1.0f, mix_ = 1.0f, levelLin_ = 1.0f;
    float dcX_ = 0.0f, dcY_ = 0.0f;

    float periodA_ = 0.0f, periodB_ = 0.0f;
    int countA_ = 0, countB_ = 0;
};

} // namespace fsl
