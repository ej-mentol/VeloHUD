#pragma once
#include <cmath>

// Filter sensitivity constants (speed coefficients)
const float VELO_FILTER_SPEED_BASE = 16.25f;   // Base smoothness (equivalent to 0.15 at 100 FPS)
const float VELO_FILTER_SPEED_FAST = 69.31f;   // Fast reaction (jumping, equivalent to 0.50 at 100 FPS)
const float VELO_FILTER_SPEED_SNAP = 230.25f;  // Instant reaction (teleports, equivalent to 0.90 at 100 FPS)

const float VELO_THRESHOLD_FAST = 20.0f;      // Threshold for fast reaction
const float VELO_THRESHOLD_SNAP = 100.0f;     // Threshold for instant reaction


class CVelocityFilter {
public:
    CVelocityFilter() {
        Reset();
    }

    void Reset() {
        m_flCurrentVelocity = 0.0f;
        m_bFirstUpdate = true;
    }

    float Update(float flRawVelocity, float flFrameTime) {
        // The first sample (and a zero-length step) snaps to the target.
        if (m_bFirstUpdate || flFrameTime <= 0.0f) {
            if (std::isfinite(flRawVelocity)) {
                m_flCurrentVelocity = flRawVelocity;
                m_bFirstUpdate = false;
            }
            return m_flCurrentVelocity;
        }

        // A non-finite sample (NaN/Inf) must not poison the filter state:
        // std::fabs(NaN) is NaN, every threshold comparison turns false and
        // the stored velocity stays NaN forever.
        if (!std::isfinite(flRawVelocity) || !std::isfinite(flFrameTime))
            return m_flCurrentVelocity;

        float flDelta = std::fabs(flRawVelocity - m_flCurrentVelocity);
        float flSpeed = VELO_FILTER_SPEED_BASE;

        if (flDelta > VELO_THRESHOLD_SNAP) {
            flSpeed = VELO_FILTER_SPEED_SNAP;
        } else if (flDelta > VELO_THRESHOLD_FAST) {
            flSpeed = VELO_FILTER_SPEED_FAST;
        }

        // Time-independent lerp: current + (target - current) * (1 - exp(-speed * dt))
        float flFactor = 1.0f - std::exp(-flSpeed * flFrameTime);
        m_flCurrentVelocity = m_flCurrentVelocity + (flRawVelocity - m_flCurrentVelocity) * flFactor;
        
        return m_flCurrentVelocity;
    }

private:
    float m_flCurrentVelocity;
    bool m_bFirstUpdate;
};
