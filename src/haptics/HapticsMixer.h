#pragma once

#include "telemetry/Il2TelemetryParser.h"

#include <cstdint>

namespace il2vr {

enum class HapticsHandMode : int {
    Right = 0,
    Left = 1,
    Both = 2
};

struct HapticsConfig {
    bool masterEnabled = true;
    float masterGain = 1.0f;

    bool buffetEnabled = true;
    float buffetGain = 1.0f;

    bool gunsEnabled = true;
    float gunsGain = 1.0f;
    int gunsHand = 0; // 0=Right (trigger hand default), 1=Left, 2=Both

    bool engineEnabled = true;
    float engineGain = 0.70f;
    int engineHand = 0; // 0=Right default, 1=Left, 2=Both

    bool impactEnabled = true;
    float impactGain = 1.0f;
};

struct HapticsFrameOutput {
    uint16_t leftDurationUs = 0;
    uint16_t rightDurationUs = 0;

    float buffetLevel = 0.0f;
    float gunsLevel = 0.0f;
    float engineLevel = 0.0f;
    float impactLevel = 0.0f;
};

class HapticsMixer {
public:
    HapticsConfig& config() noexcept { return cfg_; }
    const HapticsConfig& config() const noexcept { return cfg_; }
    void setConfig(const HapticsConfig& cfg) noexcept { cfg_ = cfg; }

    void notifyInteractionPulse(bool isLeftHand, float holdSeconds = 0.06f) noexcept;
    void triggerTestBuffet(float amplitude = 0.85f, float durationSeconds = 0.45f) noexcept;
    void stopTestEffects() noexcept;
    bool hasActiveTest() const noexcept { return testBuffetTimer_ > 0.0f || gunDecayTimer_ > 0.0f || impactHold_ > 0.0f; }
    void reset() noexcept;

    HapticsFrameOutput update(
        const Il2TelemetrySnapshot& snapshot,
        float throttleNorm,
        float dtSeconds) noexcept;

private:
    static uint16_t normToMicroSec(float level0To1) noexcept;

    HapticsConfig cfg_{};

    bool accInitialized_ = false;
    float smoothedAcc_[3]{};
    float buffetSmoothed_ = 0.0f;

    float gunDecayTimer_ = 0.0f;
    float gunIntensity_ = 0.0f;

    float enginePhase_ = 0.0f;
    float enginePulseHold_ = 0.0f;

    float impactHold_ = 0.0f;
    float impactPeak_ = 0.0f;

    float testBuffetTimer_ = 0.0f;
    float testBuffetAmp_ = 0.0f;

    float leftInteractionHold_ = 0.0f;
    float rightInteractionHold_ = 0.0f;
};

} // namespace il2vr
