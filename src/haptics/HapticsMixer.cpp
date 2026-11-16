#include "haptics/HapticsMixer.h"

#include <algorithm>
#include <cmath>

namespace il2vr {

void HapticsMixer::notifyInteractionPulse(bool isLeftHand, float holdSeconds) noexcept {
    if (isLeftHand) {
        leftInteractionHold_ = std::max(leftInteractionHold_, std::max(holdSeconds, 0.0f));
    } else {
        rightInteractionHold_ = std::max(rightInteractionHold_, std::max(holdSeconds, 0.0f));
    }
}

void HapticsMixer::reset() noexcept {
    accInitialized_ = false;
    smoothedAcc_[0] = 0.0f;
    smoothedAcc_[1] = 0.0f;
    smoothedAcc_[2] = 0.0f;
    buffetSmoothed_ = 0.0f;
    gunDecayTimer_ = 0.0f;
    gunIntensity_ = 0.0f;
    enginePhase_ = 0.0f;
    enginePulseHold_ = 0.0f;
    impactHold_ = 0.0f;
    impactPeak_ = 0.0f;
    leftInteractionHold_ = 0.0f;
    rightInteractionHold_ = 0.0f;
}

uint16_t HapticsMixer::normToMicroSec(float level0To1) noexcept {
    if (level0To1 <= 0.02f) {
        return 0;
    }
    const float v = std::clamp(level0To1 * 3800.0f, 250.0f, 3999.0f);
    return static_cast<uint16_t>(std::lround(v));
}

HapticsFrameOutput HapticsMixer::update(
    const Il2TelemetrySnapshot& snapshot,
    float throttleNorm,
    float dtSeconds) noexcept {
    HapticsFrameOutput out{};
    const float dt = std::clamp(dtSeconds, 0.0f, 0.25f);

    if (leftInteractionHold_ > 0.0f) {
        leftInteractionHold_ = std::max(0.0f, leftInteractionHold_ - dt);
    }
    if (rightInteractionHold_ > 0.0f) {
        rightInteractionHold_ = std::max(0.0f, rightInteractionHold_ - dt);
    }

    if (snapshot.paused || !cfg_.masterEnabled) {
        return out;
    }

    // 1. Aircraft Shake / Stall Buffet + Acceleration Jerk.
    const bool hasAcc = snapshot.hasMotion || snapshot.hasState;
    float jerkLevel = 0.0f;
    if (hasAcc) {
        const float* acc = snapshot.hasMotion ? snapshot.motionAcc : snapshot.stateAcc;
        if (!accInitialized_) {
            smoothedAcc_[0] = acc[0];
            smoothedAcc_[1] = acc[1];
            smoothedAcc_[2] = acc[2];
            accInitialized_ = true;
        } else {
            const float alpha = std::clamp(dt * 8.0f, 0.0f, 1.0f);
            smoothedAcc_[0] += (acc[0] - smoothedAcc_[0]) * alpha;
            smoothedAcc_[1] += (acc[1] - smoothedAcc_[1]) * alpha;
            smoothedAcc_[2] += (acc[2] - smoothedAcc_[2]) * alpha;
            const float dx = acc[0] - smoothedAcc_[0];
            const float dy = acc[1] - smoothedAcc_[1];
            const float dz = acc[2] - smoothedAcc_[2];
            const float jerkMag = std::sqrt(dx * dx + dy * dy + dz * dz);
            jerkLevel = std::clamp((jerkMag - 1.2f) / 10.0f, 0.0f, 1.0f);
        }
    }
    const float stallLevel = std::clamp(snapshot.stallBuffetAmp, 0.0f, 1.0f);
    const float targetBuffet = std::max(stallLevel, jerkLevel);
    buffetSmoothed_ += (targetBuffet - buffetSmoothed_) * std::clamp(dt * 18.0f, 0.0f, 1.0f);
    if (cfg_.buffetEnabled) {
        out.buffetLevel = std::clamp(buffetSmoothed_ * cfg_.buffetGain, 0.0f, 1.0f);
    }

    // 2. Gunfire / Cannon / Ordnance Release.
    if (snapshot.pendingGunShots > 0 || snapshot.pendingOrdnanceReleases > 0) {
        gunDecayTimer_ = 0.055f;
        const float rawShot = 0.72f + 0.12f * static_cast<float>(std::min<uint32_t>(snapshot.pendingGunShots, 3u));
        gunIntensity_ = std::max(gunIntensity_, std::clamp(rawShot, 0.72f, 1.0f));
    }
    if (gunDecayTimer_ > 0.0f) {
        if (cfg_.gunsEnabled) {
            out.gunsLevel = std::clamp(gunIntensity_ * cfg_.gunsGain, 0.0f, 1.0f);
        }
        gunDecayTimer_ = std::max(0.0f, gunDecayTimer_ - dt);
        if (gunDecayTimer_ <= 0.0f) {
            gunIntensity_ = 0.0f;
        }
    }

    // 3. Rhythmic Engine Pulse (frequency driven by RPM, amplitude driven by engine power / throttle).
    const float rpm = snapshot.maxRpm();
    if (cfg_.engineEnabled && rpm > 150.0f) {
        const float rpmNorm = std::clamp((rpm - 400.0f) / 2400.0f, 0.0f, 1.0f);
        const float mp = snapshot.maxManifoldPressurePa();
        const float powerNorm = (mp > 1000.0f)
            ? std::clamp(0.40f * rpmNorm + 0.60f * std::clamp((mp - 40000.0f) / 90000.0f, 0.0f, 1.0f), 0.0f, 1.0f)
            : std::clamp(0.45f * rpmNorm + 0.55f * std::clamp(throttleNorm, 0.0f, 1.0f), 0.0f, 1.0f);

        const float pulseHz = std::clamp(rpm / 145.0f, 3.5f, 24.0f);
        enginePhase_ += dt * pulseHz;
        if (enginePhase_ >= 1.0f) {
            enginePhase_ -= std::floor(enginePhase_);
            enginePulseHold_ = 0.022f;
        }
        if (enginePulseHold_ > 0.0f) {
            out.engineLevel = std::clamp((0.24f + 0.52f * powerNorm) * cfg_.engineGain, 0.0f, 1.0f);
            enginePulseHold_ = std::max(0.0f, enginePulseHold_ - dt);
        }
    } else {
        enginePhase_ = 0.0f;
        enginePulseHold_ = 0.0f;
    }

    // 4. Impacts / Hits / Damage.
    if (snapshot.pendingHits > 0 || snapshot.pendingDamages > 0) {
        impactHold_ = 0.10f;
        impactPeak_ = std::max(impactPeak_, std::clamp(snapshot.pendingPeakImpact, 0.55f, 1.0f));
    }
    if (impactHold_ > 0.0f) {
        if (cfg_.impactEnabled) {
            out.impactLevel = std::clamp(impactPeak_ * cfg_.impactGain, 0.0f, 1.0f);
        }
        impactHold_ = std::max(0.0f, impactHold_ - dt);
        if (impactHold_ <= 0.0f) {
            impactPeak_ = 0.0f;
        }
    }

    // Route channels per hand cleanly.
    const bool gunsOnLeft = (cfg_.gunsHand == 1 || cfg_.gunsHand == 2);
    const bool gunsOnRight = (cfg_.gunsHand == 0 || cfg_.gunsHand == 2);
    const bool engineOnLeft = (cfg_.engineHand == 1 || cfg_.engineHand == 2);
    const bool engineOnRight = (cfg_.engineHand == 0 || cfg_.engineHand == 2);

    const float leftSum =
        out.buffetLevel +
        out.impactLevel +
        (gunsOnLeft ? out.gunsLevel : 0.0f) +
        (engineOnLeft ? out.engineLevel * 0.55f : 0.0f);
    const float rightSum =
        out.buffetLevel +
        out.impactLevel +
        (gunsOnRight ? out.gunsLevel : 0.0f) +
        (engineOnRight ? out.engineLevel * 0.55f : 0.0f);

    const float master = std::clamp(cfg_.masterGain, 0.0f, 2.0f);
    out.leftDurationUs = (leftInteractionHold_ > 0.0f)
        ? 0
        : normToMicroSec(std::clamp(leftSum * master, 0.0f, 1.0f));
    out.rightDurationUs = (rightInteractionHold_ > 0.0f)
        ? 0
        : normToMicroSec(std::clamp(rightSum * master, 0.0f, 1.0f));

    return out;
}

} // namespace il2vr
