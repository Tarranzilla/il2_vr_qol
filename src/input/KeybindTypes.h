#pragma once

#include "vr/IVrBackend.h"

#include <cstdint>
#include <string>
#include <vector>

namespace il2vr {

enum class VrButtonId : int {
    None = 0,
    Trigger = 1,
    Grip = 2,
    Primary = 3,    // Left: X, Right: A
    Secondary = 4,  // Left: Y, Right: B
    ThumbClick = 5, // Left: L3, Right: R3
    StickUp = 6,
    StickDown = 7,
    StickLeft = 8,
    StickRight = 9
};

struct VrInputSlot {
    VrHand hand = VrHand::Right;
    VrButtonId button = VrButtonId::None;

    bool isValid() const noexcept { return button != VrButtonId::None; }
    bool operator==(const VrInputSlot& other) const noexcept {
        return button == other.button && (button == VrButtonId::None || hand == other.hand);
    }
    bool operator!=(const VrInputSlot& other) const noexcept { return !(*this == other); }
};

enum class VrComboRelation : int {
    Single = 0,
    Simultaneous = 1 // Both primary + secondary held/pressed together
};

enum class VrGesture : int {
    Tap1x = 0,
    Double2x = 1,
    Triple3x = 2,
    LongHold = 3,
    WhileHeld = 4
};

struct KeybindTrigger {
    VrInputSlot primary{};
    VrInputSlot secondary{};
    VrComboRelation relation = VrComboRelation::Single;
    VrGesture gesture = VrGesture::Tap1x;
    float holdSeconds = 0.60f;

    bool hasInput() const noexcept { return primary.isValid(); }
    bool isTwoButton() const noexcept {
        return relation == VrComboRelation::Simultaneous && primary.isValid() && secondary.isValid();
    }
    bool operator==(const KeybindTrigger& o) const noexcept {
        return primary == o.primary
            && secondary == o.secondary
            && relation == o.relation
            && gesture == o.gesture;
    }
};

enum class ActionCategory : int {
    MainFlight = 0,
    CompanionVr = 1,
    EngineSystems = 2,
    AllIl2 = 3
};

enum class ActionOutputType : int {
    CompanionAction = 0,
    Il2KeyboardCombo = 1,
    VJoyButton = 2
};

enum class CompanionActionId : int {
    None = 0,
    ToggleHud = 1,
    ToggleOriginMode = 2,
    RecenterSeat = 3,
    ToggleVrMouse = 4,
    FlightStickHold = 5,
    FlightThrottleHold = 6,
    ToggleCalloutLayer = 7
};

struct KeybindEntry {
    std::string id;
    std::string actionId;
    std::string labelEn;
    std::string labelPt;
    ActionCategory category = ActionCategory::MainFlight;
    int priorityRank = 100;

    ActionOutputType outputType = ActionOutputType::Il2KeyboardCombo;
    CompanionActionId companionAction = CompanionActionId::None;
    std::vector<std::string> keyCombo;
    uint8_t vjoyButton = 0; // 1-based when outputType == VJoyButton

    KeybindTrigger trigger{};
    bool enabled = true;
    bool showInCallout = true;
};

struct KeybindProfile {
    std::string id;
    std::string name;
    bool isImmutableDefault = false;
    std::vector<KeybindEntry> entries;
};

std::string formatVrSlotLabel(const VrInputSlot& slot, bool portuguese = false);
std::string formatTriggerSummary(const KeybindTrigger& trigger, bool portuguese = false);
std::string formatTriggerModeBadge(const KeybindTrigger& trigger, bool portuguese = false);
std::string formatKeyComboLabel(const std::vector<std::string>& combo);
const char* buttonIdToToken(VrButtonId id) noexcept;
VrButtonId tokenToButtonId(const std::string& token) noexcept;
const char* gestureToToken(VrGesture g) noexcept;
VrGesture tokenToGesture(const std::string& token) noexcept;
const char* companionActionToToken(CompanionActionId a) noexcept;
CompanionActionId tokenToCompanionAction(const std::string& token) noexcept;

} // namespace il2vr
