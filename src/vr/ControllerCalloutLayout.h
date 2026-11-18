#pragma once

#include "core/MathTypes.h"
#include "input/KeybindTypes.h"
#include "vr/IVrBackend.h"

#include <cstdint>
#include <string>
#include <vector>

namespace il2vr {

enum class CalloutSide : int {
    Left = 0,
    Right = 1,
    Center = 2
};

struct CalloutPin {
    VrInputSlot slot{};
    Vec3 worldPos{};
    bool activeHighlight = false;
    uint32_t rgbaColor = 0xFF38BDF8u; // ABGR or ARGB helper
};

struct CalloutLine {
    Vec3 startWorld{};
    Vec3 endWorld{};
    uint32_t rgbaColor = 0xFF38BDF8u;
    bool isSecondary = false;
};

struct CalloutCardSpec {
    std::string entryId;
    std::string title;        // Action name (e.g., "Landing Gear")
    std::string inputLabel;   // Button(s) (e.g., "L X + L Y")
    std::string modeBadge;    // Timing/combo rule (e.g., "JUNTOS 0.65s", "3x SEQUENCIA")
    std::string outputLabel;  // Output key/btn (e.g., "G", "vJoy 1", "HUD")
    bool isTwoButton = false;
    bool isCompanion = false;
    CalloutSide side = CalloutSide::Right;

    Vec3 centerWorld{};
    Mat34 pose = Mat34::identity();
    float widthM = 0.165f;
    float heightM = 0.046f;
    float uvU0 = 0.0f;
    float uvV0 = 0.0f;
    float uvU1 = 1.0f;
    float uvV1 = 1.0f;
};

struct CalloutScene {
    std::vector<CalloutPin> pins;
    std::vector<CalloutLine> lines;
    std::vector<CalloutCardSpec> cards;
};

class ControllerCalloutLayout {
public:
    static constexpr size_t kMaxCards = 16;
    static constexpr size_t kMaxLines = 24;
    static constexpr size_t kMaxPins = 16;

    static Vec3 computeButtonWorldPos(
        const Mat34& controllerPose,
        const VrInputSlot& slot) noexcept;

    static CalloutScene buildScene(
        const KeybindProfile& profile,
        const VrFrameState& vrState,
        bool portuguese,
        bool showCompanionCards = true);

    // Exposed for unit testing strict non-overlapping separation in view-plane (u, v).
    static bool hasAnyCardOverlap(
        const std::vector<CalloutCardSpec>& cards,
        const Mat34& hmdPose,
        float marginM = 0.004f) noexcept;
};

} // namespace il2vr
