#pragma once

#include "core/MathTypes.h"

#include <cstdint>
#include <optional>
#include <string>

namespace il2vr {

enum class VrHand {
    Left = 0,
    Right = 1
};

struct ControllerState {
    bool valid = false;
    Mat34 pose{};
    Mat34 rawPose{};
    bool grip = false;
    bool triggerClick = false;
    float triggerValue = 0.0f;
    float joyX = 0.0f;
    float joyY = 0.0f;
    bool primaryBtn = false;
    bool secondaryBtn = false;
    bool menuBtn = false;
    uint64_t buttonPressedMask = 0;
    uint64_t buttonTouchedMask = 0;
};

struct VrFrameState {
    bool runtimeRunning = false;
    bool hmdValid = false;
    Mat34 hmdRawPose{};
    Mat34 hmdSeatedPose{};
    ControllerState left{};
    ControllerState right{};
};

class IVrBackend {
public:
    virtual ~IVrBackend() = default;

    virtual bool init(bool allowAutoLaunch = false) = 0;
    virtual void shutdown() = 0;
    virtual bool isInitialized() const = 0;
    virtual std::string lastError() const = 0;

    virtual VrFrameState pollState() = 0;
    virtual std::optional<Mat34> getSeatedZeroPose() = 0;
    virtual bool setSeatedZeroPose(const Mat34& pose, bool commitLive = true) = 0;
    virtual void triggerHapticPulse(VrHand hand, uint16_t durationMicroSec) = 0;

    virtual void setOverlayVisible(bool /*visible*/) {}
    virtual bool isOverlayVisible() const { return false; }
    virtual void submitOverlayTextureD3D11(void* /*d3d11Texture2D*/) {}
};

} // namespace il2vr
