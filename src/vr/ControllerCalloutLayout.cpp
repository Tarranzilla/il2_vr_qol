#include "vr/ControllerCalloutLayout.h"

#include "input/KeybindProfileStore.h"
#include "input/VrInputBindingEngine.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace il2vr {

namespace {

Vec3 normalizeSafe(const Vec3& v, const Vec3& fallback) noexcept {
    const float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
    if (lenSq < 1e-8f) {
        return fallback;
    }
    const float inv = 1.0f / std::sqrt(lenSq);
    return {v.x * inv, v.y * inv, v.z * inv};
}

Vec3 crossVec3(const Vec3& a, const Vec3& b) noexcept {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float dotVec3(const Vec3& a, const Vec3& b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 transformPointByMat(const Mat34& m, const Vec3& p) noexcept {
    return {
        m.m[0][0] * p.x + m.m[0][1] * p.y + m.m[0][2] * p.z + m.m[0][3],
        m.m[1][0] * p.x + m.m[1][1] * p.y + m.m[1][2] * p.z + m.m[1][3],
        m.m[2][0] * p.x + m.m[2][1] * p.y + m.m[2][2] * p.z + m.m[2][3]
    };
}

Mat34 buildBillboardPose(const Vec3& cardPos, const Vec3& viewRight, const Vec3& viewUp, const Vec3& viewForwardToEye) noexcept {
    Mat34 p = Mat34::identity();
    p.m[0][0] = viewRight.x;          p.m[0][1] = viewUp.x; p.m[0][2] = viewForwardToEye.x; p.m[0][3] = cardPos.x;
    p.m[1][0] = viewRight.y;          p.m[1][1] = viewUp.y; p.m[1][2] = viewForwardToEye.y; p.m[1][3] = cardPos.y;
    p.m[2][0] = viewRight.z;          p.m[2][1] = viewUp.z; p.m[2][2] = viewForwardToEye.z; p.m[2][3] = cardPos.z;
    return p;
}

std::string buildOutputBadge(const KeybindEntry& e) {
    switch (e.outputType) {
    case ActionOutputType::CompanionAction:
        return "VR";
    case ActionOutputType::VJoyButton:
        return "vJoy " + std::to_string(e.vjoyButton);
    case ActionOutputType::Il2KeyboardCombo:
        return formatKeyComboLabel(e.keyCombo);
    default:
        return "";
    }
}

uint32_t colorForCard(const CalloutCardSpec& c) noexcept {
    if (c.isUnboundNotice) {
        return 0xFFEF4444u; // Red-coral warning for unbound multi-button press
    }
    if (c.activeHighlight) {
        return 0xFFFACC15u; // Bright gold highlight when pressed or triggered
    }
    if (c.isTwoButton) {
        return 0xFF38BDF8u;
    }
    if (c.isCompanion) {
        return 0xFF38BDF8u;
    }
    return 0xFF4ADE80u;
}

Vec3 pickCardBorderAnchor(
    const CalloutCardSpec& card,
    const Vec3& viewRight,
    const Vec3& viewUp,
    const Vec3& targetPinPos) noexcept {
    const Vec3 diff{
        targetPinPos.x - card.centerWorld.x,
        targetPinPos.y - card.centerWorld.y,
        targetPinPos.z - card.centerWorld.z
    };
    const float projU = dotVec3(diff, viewRight);
    const float projV = dotVec3(diff, viewUp);
    const float hw = card.widthM * 0.5f;
    const float hh = card.heightM * 0.5f;

    // Clamp onto the perimeter rectangle edge facing the button pin cleanly.
    const float u = std::clamp(projU, -hw, hw);
    const float v = std::clamp(projV, -hh, hh);
    return {
        card.centerWorld.x + viewRight.x * u + viewUp.x * v,
        card.centerWorld.y + viewRight.y * u + viewUp.y * v,
        card.centerWorld.z + viewRight.z * u + viewUp.z * v
    };
}

} // namespace

Vec3 ControllerCalloutLayout::computeButtonWorldPos(
    const Mat34& controllerPose,
    const VrInputSlot& slot) noexcept {
    const float mirror = (slot.hand == VrHand::Left) ? 1.0f : -1.0f;
    Vec3 local{0.0f, 0.012f, -0.015f};
    switch (slot.button) {
    case VrButtonId::Trigger:
        local = {-0.008f * mirror, -0.022f, -0.042f};
        break;
    case VrButtonId::Grip:
        local = {0.020f * mirror, -0.024f, 0.022f};
        break;
    case VrButtonId::Primary: // L: X, R: A
        local = {0.005f * mirror, 0.014f, -0.008f};
        break;
    case VrButtonId::Secondary: // L: Y, R: B
        local = {0.013f * mirror, 0.018f, -0.022f};
        break;
    case VrButtonId::ThumbClick: // L3 / R3
        local = {-0.015f * mirror, 0.016f, -0.016f};
        break;
    case VrButtonId::StickUp:
        local = {-0.015f * mirror, 0.016f, -0.029f};
        break;
    case VrButtonId::StickDown:
        local = {-0.015f * mirror, 0.016f, -0.003f};
        break;
    case VrButtonId::StickLeft:
        local = {-0.027f * mirror, 0.016f, -0.016f};
        break;
    case VrButtonId::StickRight:
        local = {-0.003f * mirror, 0.016f, -0.016f};
        break;
    case VrButtonId::None:
    default:
        break;
    }
    return transformPointByMat(controllerPose, local);
}

std::string ControllerCalloutLayout::shortButtonPinLabel(const VrInputSlot& slot, bool portuguese) {
    if (!slot.isValid()) {
        return "";
    }
    switch (slot.button) {
    case VrButtonId::Trigger:    return portuguese ? "Gatilho" : "Trig";
    case VrButtonId::Grip:       return "Grip";
    case VrButtonId::Primary:    return (slot.hand == VrHand::Left) ? "X" : "A";
    case VrButtonId::Secondary:  return (slot.hand == VrHand::Left) ? "Y" : "B";
    case VrButtonId::ThumbClick: return (slot.hand == VrHand::Left) ? "L3" : "R3";
    case VrButtonId::StickUp:    return portuguese ? "Cima" : "Up";
    case VrButtonId::StickDown:  return portuguese ? "Baixo" : "Down";
    case VrButtonId::StickLeft:  return portuguese ? "Esq" : "Left";
    case VrButtonId::StickRight: return portuguese ? "Dir" : "Right";
    case VrButtonId::None:
    default:                     return "";
    }
}

CalloutScene ControllerCalloutLayout::buildScene(
    const KeybindProfile& profile,
    const VrFrameState& vrState,
    bool portuguese,
    bool showCompanionCards,
    const std::vector<std::string>& activeEntryIds) {
    CalloutScene scene{};
    if (!vrState.hmdValid) {
        return scene;
    }

    // Build HMD horizontal viewer basis so callout cards stay level and face the pilot clearly.
    const Vec3 hmdForwardRaw{
        -vrState.hmdSeatedPose.m[0][2],
        0.0f,
        -vrState.hmdSeatedPose.m[2][2]
    };
    const Vec3 hmdForward = normalizeSafe(hmdForwardRaw, {0.0f, 0.0f, -1.0f});
    const Vec3 worldUp{0.0f, 1.0f, 0.0f};
    const Vec3 viewRight = normalizeSafe(crossVec3(hmdForward, worldUp), {1.0f, 0.0f, 0.0f});
    const Vec3 viewUp = worldUp;
    const Vec3 viewFaceEye{-hmdForward.x, -hmdForward.y, -hmdForward.z};

    struct PendingCard {
        CalloutCardSpec card;
        Vec3 pin1;
        Vec3 pin2;
        VrInputSlot slot1;
        VrInputSlot slot2;
        float sortKey = 0.0f;
    };

    auto isSlotHandValid = [&](const VrInputSlot& s) -> bool {
        if (!s.isValid()) {
            return false;
        }
        return (s.hand == VrHand::Left) ? vrState.left.valid : vrState.right.valid;
    };

    auto getPinPos = [&](const VrInputSlot& s) -> Vec3 {
        const Mat34& pose = (s.hand == VrHand::Left) ? vrState.left.pose : vrState.right.pose;
        return computeButtonWorldPos(pose, s);
    };

    auto ensurePin = [&](const VrInputSlot& slot, const Vec3& pos, uint32_t col) {
        const bool pressed = VrInputBindingEngine::isSlotPressed(vrState, slot);
        for (auto& existing : scene.pins) {
            if (existing.slot == slot) {
                if (!existing.activeHighlight && col != 0xFF94A3B8u && existing.rgbaColor == 0xFF94A3B8u) {
                    existing.rgbaColor = col;
                }
                if (pressed) {
                    existing.activeHighlight = true;
                    existing.rgbaColor = 0xFFFACC15u;
                }
                return;
            }
        }
        if (scene.pins.size() >= kMaxPins) {
            return;
        }
        scene.pins.push_back(CalloutPin{
            slot,
            pos,
            pressed,
            pressed ? 0xFFFACC15u : col,
            shortButtonPinLabel(slot, portuguese)
        });
    };

    // Always emit labeled base physical button pins for each valid controller, plus any pressed inputs.
    constexpr std::array<VrButtonId, 5> kBaseButtons = {
        VrButtonId::Primary,
        VrButtonId::Secondary,
        VrButtonId::ThumbClick,
        VrButtonId::Trigger,
        VrButtonId::Grip
    };
    constexpr std::array<VrButtonId, 9> kAllButtons = {
        VrButtonId::Primary,
        VrButtonId::Secondary,
        VrButtonId::ThumbClick,
        VrButtonId::Trigger,
        VrButtonId::Grip,
        VrButtonId::StickUp,
        VrButtonId::StickDown,
        VrButtonId::StickLeft,
        VrButtonId::StickRight
    };
    std::vector<VrInputSlot> pressedSlots;
    for (VrHand hand : {VrHand::Left, VrHand::Right}) {
        const bool handValid = (hand == VrHand::Left) ? vrState.left.valid : vrState.right.valid;
        if (!handValid) {
            continue;
        }
        for (VrButtonId b : kBaseButtons) {
            const VrInputSlot slot{hand, b};
            ensurePin(slot, getPinPos(slot), 0xFF94A3B8u);
        }
        for (VrButtonId b : kAllButtons) {
            const VrInputSlot slot{hand, b};
            if (VrInputBindingEngine::isSlotPressed(vrState, slot)) {
                pressedSlots.push_back(slot);
                ensurePin(slot, getPinPos(slot), 0xFF94A3B8u);
            }
        }
    }

    auto slotParticipatesInTwoButton = [&](const VrInputSlot& slot) -> bool {
        for (const auto& e : profile.entries) {
            if (!e.enabled || !e.trigger.isTwoButton()) {
                continue;
            }
            if ((e.trigger.primary == slot || e.trigger.secondary == slot) &&
                VrInputBindingEngine::isSlotPressed(vrState, e.trigger.primary) &&
                VrInputBindingEngine::isSlotPressed(vrState, e.trigger.secondary)) {
                return true;
            }
        }
        return false;
    };

    std::vector<PendingCard> leftCards;
    std::vector<PendingCard> rightCards;
    std::vector<PendingCard> centerCards;

    for (const auto& entry : profile.entries) {
        if (!entry.enabled || !entry.showInCallout || !entry.trigger.hasInput()) {
            continue;
        }
        if (!showCompanionCards && entry.outputType == ActionOutputType::CompanionAction) {
            continue;
        }
        if (!isSlotHandValid(entry.trigger.primary)) {
            continue;
        }
        if (entry.trigger.isTwoButton() && !isSlotHandValid(entry.trigger.secondary)) {
            continue;
        }

        PendingCard pc{};
        pc.card.entryId = entry.id;
        pc.card.title = (portuguese && !entry.labelPt.empty()) ? entry.labelPt : entry.labelEn;
        pc.card.inputLabel = formatVrSlotLabel(entry.trigger.primary, portuguese);
        if (entry.trigger.isTwoButton()) {
            pc.card.inputLabel += " + " + formatVrSlotLabel(entry.trigger.secondary, portuguese);
        }
        pc.card.modeBadge = formatTriggerModeBadge(entry.trigger, portuguese);
        pc.card.outputLabel = buildOutputBadge(entry);
        pc.card.isTwoButton = entry.trigger.isTwoButton();
        pc.card.isCompanion = (entry.outputType == ActionOutputType::CompanionAction);
        if (std::find(activeEntryIds.begin(), activeEntryIds.end(), entry.id) != activeEntryIds.end()) {
            pc.card.activeHighlight = true;
        } else if (entry.trigger.isTwoButton()) {
            if (VrInputBindingEngine::isSlotPressed(vrState, entry.trigger.primary) &&
                VrInputBindingEngine::isSlotPressed(vrState, entry.trigger.secondary)) {
                pc.card.activeHighlight = true;
            }
        } else if (VrInputBindingEngine::isSlotPressed(vrState, entry.trigger.primary) &&
                   !slotParticipatesInTwoButton(entry.trigger.primary)) {
            if (entry.trigger.gesture == VrGesture::WhileHeld ||
                entry.trigger.gesture == VrGesture::LongHold ||
                entry.trigger.gesture == VrGesture::Tap1x) {
                pc.card.activeHighlight = true;
            }
        }
        pc.card.widthM = pc.card.isTwoButton ? 0.185f : 0.165f;
        pc.card.heightM = 0.044f;

        pc.slot1 = entry.trigger.primary;
        pc.pin1 = getPinPos(pc.slot1);
        if (pc.card.isTwoButton) {
            pc.slot2 = entry.trigger.secondary;
            pc.pin2 = getPinPos(pc.slot2);
        }

        const uint32_t col = colorForCard(pc.card);
        ensurePin(pc.slot1, pc.pin1, col);
        if (pc.card.isTwoButton) {
            ensurePin(pc.slot2, pc.pin2, col);
        }

        if (pc.card.isTwoButton && pc.slot1.hand != pc.slot2.hand) {
            pc.card.side = CalloutSide::Center;
            pc.sortKey = dotVec3(pc.pin1, viewUp) + dotVec3(pc.pin2, viewUp);
            centerCards.push_back(std::move(pc));
        } else if (pc.slot1.hand == VrHand::Left) {
            pc.card.side = CalloutSide::Left;
            pc.sortKey = dotVec3(pc.pin1, viewUp);
            leftCards.push_back(std::move(pc));
        } else {
            pc.card.side = CalloutSide::Right;
            pc.sortKey = dotVec3(pc.pin1, viewUp);
            rightCards.push_back(std::move(pc));
        }
    }

    // If 2+ inputs are pressed simultaneously and no enabled 2-button combo matches any active pair,
    // display a prominent two-line "No Binding Assigned" notice connecting the lit buttons.
    if (pressedSlots.size() >= 2) {
        bool hasBoundTwoButtonMatch = false;
        for (const auto& entry : profile.entries) {
            if (!entry.enabled || !entry.trigger.isTwoButton()) {
                continue;
            }
            if (VrInputBindingEngine::isSlotPressed(vrState, entry.trigger.primary) &&
                VrInputBindingEngine::isSlotPressed(vrState, entry.trigger.secondary)) {
                hasBoundTwoButtonMatch = true;
                break;
            }
        }
        if (!hasBoundTwoButtonMatch) {
            const VrInputSlot& s1 = pressedSlots[0];
            const VrInputSlot& s2 = pressedSlots[1];
            PendingCard notice{};
            notice.card.entryId = "__unbound_combo__";
            notice.card.title = portuguese ? "Sem Bind Configurado" : "No Binding Assigned";
            notice.card.inputLabel = formatVrSlotLabel(s1, portuguese) + " + " + formatVrSlotLabel(s2, portuguese);
            notice.card.modeBadge = portuguese ? "SEM ACAO" : "UNBOUND";
            notice.card.outputLabel = "--";
            notice.card.isTwoButton = true;
            notice.card.isCompanion = false;
            notice.card.activeHighlight = true;
            notice.card.isUnboundNotice = true;
            notice.card.widthM = 0.195f;
            notice.card.heightM = 0.048f;
            notice.slot1 = s1;
            notice.slot2 = s2;
            notice.pin1 = getPinPos(s1);
            notice.pin2 = getPinPos(s2);
            notice.sortKey = 1e6f;
            ensurePin(s1, notice.pin1, 0xFFEF4444u);
            ensurePin(s2, notice.pin2, 0xFFEF4444u);
            if (s1.hand != s2.hand) {
                notice.card.side = CalloutSide::Center;
                centerCards.insert(centerCards.begin(), std::move(notice));
            } else if (s1.hand == VrHand::Left) {
                notice.card.side = CalloutSide::Left;
                leftCards.insert(leftCards.begin(), std::move(notice));
            } else {
                notice.card.side = CalloutSide::Right;
                rightCards.insert(rightCards.begin(), std::move(notice));
            }
        }
    }

    const Vec3 leftBase{vrState.left.pose.m[0][3], vrState.left.pose.m[1][3], vrState.left.pose.m[2][3]};
    const Vec3 rightBase{vrState.right.pose.m[0][3], vrState.right.pose.m[1][3], vrState.right.pose.m[2][3]};

    auto placeSideStack = [&](std::vector<PendingCard>& list, const Vec3& basePos, float lateralSign) {
        std::sort(list.begin(), list.end(), [](const PendingCard& a, const PendingCard& b) {
            return a.sortKey > b.sortKey;
        });
        const size_t n = list.size();
        const float rowSpacing = 0.072f;
        for (size_t i = 0; i < n; ++i) {
            // Split into 2 tidy vertical columns if there are more than 5 cards on one hand.
            const size_t colIdx = (n > 5) ? (i / 5) : 0;
            const size_t rowInCol = (n > 5) ? (i % 5) : i;
            const size_t rowsThisCol = (n > 5) ? std::min<size_t>(5, n - colIdx * 5) : n;
            const float centerRow = (static_cast<float>(rowsThisCol) - 1.0f) * 0.5f;
            const float vOff = (centerRow - static_cast<float>(rowInCol)) * rowSpacing + 0.09f;
            const float uOff = lateralSign * (0.24f + static_cast<float>(colIdx) * 0.225f);
            const float fOff = -0.025f;
            list[i].card.centerWorld = {
                basePos.x + viewRight.x * uOff + viewUp.x * vOff + hmdForward.x * fOff,
                basePos.y + viewRight.y * uOff + viewUp.y * vOff + hmdForward.y * fOff,
                basePos.z + viewRight.z * uOff + viewUp.z * vOff + hmdForward.z * fOff
            };
        }
    };

    placeSideStack(leftCards, leftBase, -1.0f);
    placeSideStack(rightCards, rightBase, +1.0f);

    if (!centerCards.empty()) {
        Vec3 bridgeBase{};
        if (vrState.left.valid && vrState.right.valid) {
            bridgeBase = {
                (leftBase.x + rightBase.x) * 0.5f,
                std::max(leftBase.y, rightBase.y),
                (leftBase.z + rightBase.z) * 0.5f
            };
        } else if (vrState.left.valid) {
            bridgeBase = leftBase;
        } else {
            bridgeBase = rightBase;
        }
        const float rowSpacing = 0.072f;
        for (size_t i = 0; i < centerCards.size(); ++i) {
            const float vOff = 0.22f + static_cast<float>(i) * rowSpacing;
            centerCards[i].card.centerWorld = {
                bridgeBase.x + viewUp.x * vOff - hmdForward.x * 0.020f,
                bridgeBase.y + viewUp.y * vOff - hmdForward.y * 0.020f,
                bridgeBase.z + viewUp.z * vOff - hmdForward.z * 0.020f
            };
        }
    }

    // Collect up to kMaxCards total cleanly prioritized (keep any live unbound notice at the front).
    std::vector<PendingCard> allCards;
    allCards.reserve(leftCards.size() + rightCards.size() + centerCards.size());
    auto pushByNotice = [&](std::vector<PendingCard>& src, bool wantNotice) {
        for (auto& c : src) {
            if (c.card.isUnboundNotice == wantNotice) {
                allCards.push_back(std::move(c));
            }
        }
    };
    pushByNotice(centerCards, true);
    pushByNotice(leftCards, true);
    pushByNotice(rightCards, true);
    pushByNotice(centerCards, false);
    pushByNotice(leftCards, false);
    pushByNotice(rightCards, false);
    if (allCards.size() > kMaxCards) {
        allCards.resize(kMaxCards);
    }

    // Strict iterative AABB repulsion in HMD billboard plane (u, v) so NO two cards ever overlap.
    const Vec3 origin{vrState.hmdSeatedPose.m[0][3], vrState.hmdSeatedPose.m[1][3], vrState.hmdSeatedPose.m[2][3]};
    const float marginM = 0.024f;
    for (int iter = 0; iter < 36; ++iter) {
        bool anyMoved = false;
        for (size_t i = 0; i < allCards.size(); ++i) {
            for (size_t j = i + 1; j < allCards.size(); ++j) {
                auto& ci = allCards[i].card;
                auto& cj = allCards[j].card;
                const Vec3 di{ci.centerWorld.x - origin.x, ci.centerWorld.y - origin.y, ci.centerWorld.z - origin.z};
                const Vec3 dj{cj.centerWorld.x - origin.x, cj.centerWorld.y - origin.y, cj.centerWorld.z - origin.z};
                const float du = dotVec3(dj, viewRight) - dotVec3(di, viewRight);
                const float dv = dotVec3(dj, viewUp) - dotVec3(di, viewUp);

                const float reqU = (ci.widthM + cj.widthM) * 0.5f + marginM;
                const float reqV = (ci.heightM + cj.heightM) * 0.5f + marginM;
                const float penU = reqU - std::fabs(du);
                const float penV = reqV - std::fabs(dv);
                if (penU > 0.0f && penV > 0.0f) {
                    anyMoved = true;
                    // Push along minimum penetration axis (tie-break vertically for tidy columns).
                    if (penV <= penU * 1.15f) {
                        const float dirV = (dv >= 0.0f) ? 1.0f : -1.0f;
                        const float step = penV * 0.52f;
                        ci.centerWorld.x -= viewUp.x * (dirV * step);
                        ci.centerWorld.y -= viewUp.y * (dirV * step);
                        ci.centerWorld.z -= viewUp.z * (dirV * step);
                        cj.centerWorld.x += viewUp.x * (dirV * step);
                        cj.centerWorld.y += viewUp.y * (dirV * step);
                        cj.centerWorld.z += viewUp.z * (dirV * step);
                    } else {
                        const float dirU = (du >= 0.0f) ? 1.0f : -1.0f;
                        const float step = penU * 0.52f;
                        ci.centerWorld.x -= viewRight.x * (dirU * step);
                        ci.centerWorld.y -= viewRight.y * (dirU * step);
                        ci.centerWorld.z -= viewRight.z * (dirU * step);
                        cj.centerWorld.x += viewRight.x * (dirU * step);
                        cj.centerWorld.y += viewRight.y * (dirU * step);
                        cj.centerWorld.z += viewRight.z * (dirU * step);
                    }
                }
            }
        }
        if (!anyMoved) {
            break;
        }
    }

    // Finalize card billboard poses and generate 1 or 2 leader lines per card.
    for (auto& pc : allCards) {
        pc.card.pose = buildBillboardPose(pc.card.centerWorld, viewRight, viewUp, viewFaceEye);
        const uint32_t col = colorForCard(pc.card);
        const uint32_t secondaryCol = pc.card.isUnboundNotice
            ? 0xFFF87171u
            : (pc.card.activeHighlight ? 0xFFFDE047u : 0xFFF59E0Bu);

        if (scene.lines.size() < kMaxLines) {
            Vec3 anchor1 = pickCardBorderAnchor(pc.card, viewRight, viewUp, pc.pin1);
            anchor1.x -= viewFaceEye.x * 0.006f;
            anchor1.y -= viewFaceEye.y * 0.006f;
            anchor1.z -= viewFaceEye.z * 0.006f;
            scene.lines.push_back(CalloutLine{pc.pin1, anchor1, col, false, pc.card.activeHighlight});
        }
        if (pc.card.isTwoButton && scene.lines.size() < kMaxLines) {
            Vec3 anchor2 = pickCardBorderAnchor(pc.card, viewRight, viewUp, pc.pin2);
            anchor2.x -= viewFaceEye.x * 0.006f;
            anchor2.y -= viewFaceEye.y * 0.006f;
            anchor2.z -= viewFaceEye.z * 0.006f;
            scene.lines.push_back(CalloutLine{pc.pin2, anchor2, secondaryCol, true, pc.card.activeHighlight});
        }
        scene.cards.push_back(std::move(pc.card));
    }

    return scene;
}

bool ControllerCalloutLayout::hasAnyCardOverlap(
    const std::vector<CalloutCardSpec>& cards,
    const Mat34& hmdPose,
    float marginM) noexcept {
    const Vec3 hmdForward = normalizeSafe({-hmdPose.m[0][2], 0.0f, -hmdPose.m[2][2]}, {0.0f, 0.0f, -1.0f});
    const Vec3 viewUp{0.0f, 1.0f, 0.0f};
    const Vec3 viewRight = normalizeSafe(crossVec3(hmdForward, viewUp), {1.0f, 0.0f, 0.0f});

    for (size_t i = 0; i < cards.size(); ++i) {
        for (size_t j = i + 1; j < cards.size(); ++j) {
            const float du = std::fabs(
                dotVec3(cards[j].centerWorld, viewRight) - dotVec3(cards[i].centerWorld, viewRight));
            const float dv = std::fabs(
                dotVec3(cards[j].centerWorld, viewUp) - dotVec3(cards[i].centerWorld, viewUp));
            const float reqU = (cards[i].widthM + cards[j].widthM) * 0.5f + marginM;
            const float reqV = (cards[i].heightM + cards[j].heightM) * 0.5f + marginM;
            if (du < reqU && dv < reqV) {
                return true;
            }
        }
    }
    return false;
}

} // namespace il2vr
