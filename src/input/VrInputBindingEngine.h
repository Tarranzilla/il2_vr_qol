#pragma once

#include "input/KeybindTypes.h"
#include "vr/IVrBackend.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace il2vr {

struct VrBindingEvalResult {
    bool toggleHud = false;
    bool toggleOriginMode = false;
    bool recenterSeat = false;
    bool toggleVrMouse = false;
    bool toggleCalloutLayer = false;

    bool stickGripHeld = false;
    bool throttleGripHeld = false;

    uint16_t vjoyButtonsMask = 0; // Bit (button-1) for vJoy buttons 1..16
    std::vector<std::vector<std::string>> keyCombosTapped;
    std::vector<std::vector<std::string>> keyCombosHeld;

    bool pulseLeft = false;
    bool pulseRight = false;
    uint16_t pulseLeftUs = 0;
    uint16_t pulseRightUs = 0;
    std::string lastTriggeredLabel;
    std::vector<std::string> activeEntryIds;
};

struct VrCaptureResult {
    bool completed = false;
    KeybindTrigger trigger{};
};

class VrInputBindingEngine {
public:
    static bool isSlotPressed(const VrFrameState& state, const VrInputSlot& slot) noexcept;

    void reset() noexcept;

    VrBindingEvalResult update(
        const VrFrameState& vrState,
        const KeybindProfile& profile,
        float dtSeconds,
        bool suppressWeapons,
        bool suppressLeftPrimary);

    // Captures a single button/stick direction OR two-button simultaneous combo live on VR controllers.
    void startCapture(VrGesture desiredGesture, float desiredHoldSec) noexcept;
    void cancelCapture() noexcept;
    bool isCapturing() const noexcept { return captureActive_; }
    VrCaptureResult pollCapture(const VrFrameState& vrState, float dtSeconds) noexcept;

private:
    static constexpr size_t kMaxEntries = 96;
    struct EntryRuntime {
        bool prevTriggerDown = false;
        bool prevSinglePrimaryDown = false;
        float holdElapsed = 0.0f;
        bool longHoldFired = false;

        int clickCount = 0;
        float windowRemaining = 0.0f;
        bool tapDeferredPending = false;
        float tapDeferRemaining = 0.0f;
    };

    static bool isTriggerHeldNow(const VrFrameState& state, const KeybindTrigger& trig) noexcept;
    void dispatchEntryActivation(
        const KeybindEntry& entry,
        bool suppressWeapons,
        VrBindingEvalResult& out);

    std::array<EntryRuntime, kMaxEntries> runtimes_{};
    bool captureActive_ = false;
    VrGesture captureGesture_ = VrGesture::Tap1x;
    float captureHoldSec_ = 0.60f;
    float captureTimeout_ = 0.0f;
    float captureWindowTimer_ = 0.0f;
    VrInputSlot captureFirst_{};
    VrInputSlot captureSecond_{};
    uint32_t captureBaselineMaskLeft_ = 0;
    uint32_t captureBaselineMaskRight_ = 0;
};

} // namespace il2vr
