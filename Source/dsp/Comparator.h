#pragma once

#include <algorithm>
#include <cmath>

namespace fsl
{

enum class CompareMode { xor_ = 0, rs, pfd, cmp, win };

/** Gain, Schmitt trigger and edge divider — the A-196's own conditioning
 * ahead of each phase-comparator input, so quiet or noisy material still
 * gives clean edges. */
class Conditioner
{
public:
    void reset() { state_ = false; halfCount_ = 0; divided_ = false; }

    /** x already at working level (post input-gain). Returns the divided
     * square (false/true) and whether it just rose on this call. */
    bool process (float x, float hysteresis, int divisor, bool& square)
    {
        bool edge = false;
        if (! state_ && x > hysteresis)       { state_ = true;  edge = true; }
        else if (state_ && x < -hysteresis)   { state_ = false; edge = true; }

        const bool wasHigh = divided_;
        if (edge)
        {
            halfCount_ = (halfCount_ + 1) % (2 * std::max (1, divisor));
            divided_ = halfCount_ >= std::max (1, divisor);
        }
        square = divided_;
        return divided_ && ! wasHigh; // rising edge of the divided square
    }

private:
    bool state_ = false;
    int halfCount_ = 0;
    bool divided_ = false;
};

/**
 * The three A-196 phase comparators plus two new ones for audio-rate use.
 *
 * XOR  (PC1) — exclusive-or of the two squares; locks at harmonics.
 * RS   (PC2) — RS flip-flop, main sets / sidechain resets; drives the lock LED.
 * PFD  (PC3) — edge-triggered tri-state network (CD4046-style); ignores harmonics.
 * CMP  — new: plain hysteresis comparator on the raw (unsquared) waveforms.
 * WIN  — new: window comparator, high while the two signals sit within
 *        +/- Window of each other.
 *
 * `loop` flips the polarity of RS and PFD: in loop mode the second input is
 * the reference and the first is the VCO being corrected, and without the
 * flip the loop pushes the VCO away from lock instead of into it (the same
 * sense a 4046 is wired in).
 */
class Comparator
{
public:
    void reset() { rs_ = false; up_ = false; dn_ = false; cmpState_ = false; }

    /** Returns pc in {-1, +1}; hold is true only for a PFD tri-state hold. */
    float process (CompareMode mode, bool loop,
                    bool qa, bool qb, bool risingA, bool risingB,
                    float rawA, float rawB, float hysteresis, float window,
                    bool& hold)
    {
        hold = false;
        switch (mode)
        {
            case CompareMode::xor_:
                return (qa != qb) ? 1.0f : -1.0f;

            case CompareMode::rs:
            {
                if (risingA) rs_ = true;
                if (risingB) rs_ = false;
                const float v = rs_ ? 1.0f : -1.0f;
                return loop ? -v : v;
            }

            case CompareMode::pfd:
            {
                if (risingA) up_ = true;
                if (risingB) dn_ = true;
                if (up_ && dn_) { up_ = false; dn_ = false; }
                const float v = (up_ ? 1.0f : 0.0f) - (dn_ ? 1.0f : 0.0f);
                hold = (v == 0.0f);
                return loop ? -v : v;
            }

            case CompareMode::cmp:
            {
                const float d = rawA - rawB;
                if (! cmpState_ && d > hysteresis) cmpState_ = true;
                else if (cmpState_ && d < -hysteresis) cmpState_ = false;
                return cmpState_ ? 1.0f : -1.0f;
            }

            default: // win
                return (std::fabs (rawA - rawB) < window) ? 1.0f : -1.0f;
        }
    }

private:
    bool rs_ = false, up_ = false, dn_ = false, cmpState_ = false;
};

} // namespace fsl
