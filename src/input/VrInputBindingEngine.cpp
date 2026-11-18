#include "input/VrInputBindingEngine.h"

#include <algorithm>
#include <cmath>

namespace il2vr {

namespace {

uint32_t buildControllerButtonBitmask(const ControllerState& c) noexcept {
    if (!c.valid) {
        return 0;
    }
    uint32_t m = 0;
    if (c.triggerClick) m |= (1u << 1);
    if (c.grip)         m |= (1u << 2);
    if (c.primaryBtn)   m |= (1u << 3);
    if (c.menuBtn)      m |= (1u << 4); // Y (Left) / B (Right) -> k_EButton_ApplicationMenu
    if (c.secondaryBtn) m |= (1u << 5); // L3 / R3 thumbstick click!
    if (c.joyY > 0.70f)  m |= (1u << 6);
    if (c.joyY < -0.70f) m |= (1u << 7);
    if (c.joyX < -0.70f) m |= (1u << 8);
    if (c.joyX > 0.70f)  m |= (1u << 9);
    return m;
}

} // namespace

bool VrInputBindingEngine::isSlotPressed(const VrFrameState& state, const VrInputSlot& slot) noexcept {
    if (!slot.isValid()) {
        return false;
    }
    const ControllerState& c = (slot.hand == VrHand::Left) ? state.left : state.right;
    if (!c.valid) {
        return false;
    }
    // IMPORTANT: OpenVrBackend maps A/X -> primaryBtn, B/Y -> menuBtn, L3/R3 -> secondaryBtn.
    switch (slot.button) {
    case VrButtonId::Trigger:    return c.triggerClick;
    case VrButtonId::Grip:       return c.grip;
    case VrButtonId::Primary:    return c.primaryBtn;
    case VrButtonId::Secondary:  return c.menuBtn;
    case VrButtonId::ThumbClick: return c.secondaryBtn;
    case VrButtonId::StickUp:    return c.joyY > 0.70f;
    case VrButtonId::StickDown:  return c.joyY < -0.70f;
    case VrButtonId::StickLeft:  return c.joyX < -0.70f;
    case VrButtonId::StickRight: return c.joyX > 0.70f;
    case VrButtonId::None:
    default:                     return false;
    }
}

void VrInputBindingEngine::reset() noexcept {
    runtimes_.fill(EntryRuntime{});
}

bool VrInputBindingEngine::isTriggerHeldNow(const VrFrameState& state, const KeybindTrigger& trig) noexcept {
    if (!trig.hasInput()) {
        return false;
    }
    if (trig.isTwoButton()) {
        return isSlotPressed(state, trig.primary) && isSlotPressed(state, trig.secondary);
    }
    return isSlotPressed(state, trig.primary);
}

void VrInputBindingEngine::dispatchEntryActivation(
    const KeybindEntry& entry,
    bool suppressWeapons,
    VrBindingEvalResult& out) {
    out.lastTriggeredLabel = entry.labelEn;
    auto pulseHand = [&](VrHand h, uint16_t us) {
        if (h == VrHand::Left) {
            out.pulseLeft = true;
            out.pulseLeftUs = std::max(out.pulseLeftUs, us);
        } else {
            out.pulseRight = true;
            out.pulseRightUs = std::max(out.pulseRightUs, us);
        }
    };

    switch (entry.outputType) {
    case ActionOutputType::CompanionAction:
        switch (entry.companionAction) {
        case CompanionActionId::ToggleHud:          out.toggleHud = true; break;
        case CompanionActionId::ToggleOriginMode:   out.toggleOriginMode = true; break;
        case CompanionActionId::RecenterSeat:       out.recenterSeat = true; break;
        case CompanionActionId::ToggleVrMouse:      out.toggleVrMouse = true; break;
        case CompanionActionId::ToggleCalloutLayer: out.toggleCalloutLayer = true; break;
        case CompanionActionId::FlightStickHold:    out.stickGripHeld = true; break;
        case CompanionActionId::FlightThrottleHold: out.throttleGripHeld = true; break;
        case CompanionActionId::None:
        default: break;
        }
        pulseHand(entry.trigger.primary.hand, 2200);
        if (entry.trigger.isTwoButton()) {
            pulseHand(entry.trigger.secondary.hand, 2200);
        }
        break;

    case ActionOutputType::VJoyButton:
        if (!suppressWeapons && entry.vjoyButton >= 1 && entry.vjoyButton <= 16) {
            out.vjoyButtonsMask |= static_cast<uint16_t>(1u << (entry.vjoyButton - 1u));
        }
        break;

    case ActionOutputType::Il2KeyboardCombo:
        if (!suppressWeapons && !entry.keyCombo.empty()) {
            out.keyCombosTapped.push_back(entry.keyCombo);
            pulseHand(entry.trigger.primary.hand, 1600);
            if (entry.trigger.isTwoButton()) {
                pulseHand(entry.trigger.secondary.hand, 1600);
            }
        }
        break;
    }
}

VrBindingEvalResult VrInputBindingEngine::update(
    const VrFrameState& vrState,
    const KeybindProfile& profile,
    float dtSeconds,
    bool suppressWeapons,
    bool suppressLeftPrimary) {
    VrBindingEvalResult out{};
    const float dt = std::clamp(dtSeconds, 0.0f, 0.25f);
    if (!vrState.runtimeRunning) {
        reset();
        return out;
    }

    const size_t count = std::min(profile.entries.size(), kMaxEntries);

    // Precompute which single slots ALSO have a Double/Triple or Two-Button LongHold/Simultaneous sharing the button,
    // so simple Tap1x waits cleanly a short 220ms window only when ambiguous, and triggers instantly (0ms) otherwise.
    auto slotHasMultiClick = [&](const VrInputSlot& slot) -> bool {
        for (size_t i = 0; i < count; ++i) {
            const auto& e = profile.entries[i];
            if (!e.enabled || !e.trigger.hasInput() || e.trigger.isTwoButton()) {
                continue;
            }
            if (e.trigger.primary == slot &&
                (e.trigger.gesture == VrGesture::Double2x || e.trigger.gesture == VrGesture::Triple3x)) {
                return true;
            }
        }
        return false;
    };

    auto slotParticipatesInTwoButton = [&](const VrInputSlot& slot) -> bool {
        for (size_t i = 0; i < count; ++i) {
            const auto& e = profile.entries[i];
            if (!e.enabled || !e.trigger.isTwoButton()) {
                continue;
            }
            if ((e.trigger.primary == slot || e.trigger.secondary == slot) &&
                isTriggerHeldNow(vrState, e.trigger)) {
                return true;
            }
        }
        return false;
    };

    auto slotHasSingleLongHold = [&](const VrInputSlot& slot) -> bool {
        for (size_t i = 0; i < count; ++i) {
            const auto& e = profile.entries[i];
            if (!e.enabled || !e.trigger.hasInput() || e.trigger.isTwoButton()) {
                continue;
            }
            if (e.trigger.primary == slot && e.trigger.gesture == VrGesture::LongHold) {
                return true;
            }
        }
        return false;
    };

    // Pass 1: Check 2-button simultaneous triggers first so their constituent single buttons are suppressed while held together.
    for (size_t i = 0; i < count; ++i) {
        const auto& entry = profile.entries[i];
        EntryRuntime& rt = runtimes_[i];
        if (!entry.enabled || !entry.trigger.hasInput()) {
            rt = EntryRuntime{};
            continue;
        }
        if (suppressLeftPrimary &&
            entry.trigger.primary.hand == VrHand::Left &&
            entry.trigger.primary.button == VrButtonId::Primary) {
            rt.prevTriggerDown = isTriggerHeldNow(vrState, entry.trigger);
            continue;
        }

        const bool downNow = isTriggerHeldNow(vrState, entry.trigger);
        const bool risingEdge = downNow && !rt.prevTriggerDown;

        // Tick deferred tap timer if waiting for disambiguation.
        if (rt.tapDeferredPending) {
            rt.tapDeferRemaining -= dt;
            if (rt.tapDeferRemaining <= 0.0f) {
                rt.tapDeferredPending = false;
                // Only fire deferred tap if slot wasn't claimed by 2-button combo or long-hold.
                if (!downNow && !slotParticipatesInTwoButton(entry.trigger.primary)) {
                    dispatchEntryActivation(entry, suppressWeapons, out);
                }
            }
        }

        if (rt.windowRemaining > 0.0f) {
            rt.windowRemaining -= dt;
            if (rt.windowRemaining <= 0.0f) {
                rt.clickCount = 0;
            }
        }

        switch (entry.trigger.gesture) {
        case VrGesture::WhileHeld:
            if (downNow && (entry.trigger.isTwoButton() || !slotParticipatesInTwoButton(entry.trigger.primary))) {
                if (entry.outputType == ActionOutputType::CompanionAction) {
                    if (entry.companionAction == CompanionActionId::FlightStickHold) {
                        out.stickGripHeld = true;
                    } else if (entry.companionAction == CompanionActionId::FlightThrottleHold) {
                        out.throttleGripHeld = true;
                    }
                } else if (entry.outputType == ActionOutputType::VJoyButton) {
                    if (!suppressWeapons && entry.vjoyButton >= 1 && entry.vjoyButton <= 16) {
                        out.vjoyButtonsMask |= static_cast<uint16_t>(1u << (entry.vjoyButton - 1u));
                    }
                } else if (!suppressWeapons && entry.outputType == ActionOutputType::Il2KeyboardCombo && !entry.keyCombo.empty()) {
                    out.keyCombosHeld.push_back(entry.keyCombo);
                }
            }
            break;

        case VrGesture::LongHold:
            if (!entry.trigger.isTwoButton() && slotParticipatesInTwoButton(entry.trigger.primary)) {
                rt.holdElapsed = 0.0f;
                rt.longHoldFired = false;
                break;
            }
            if (downNow) {
                rt.holdElapsed += dt;
                if (!rt.longHoldFired && rt.holdElapsed >= std::max(entry.trigger.holdSeconds, 0.15f)) {
                    rt.longHoldFired = true;
                    rt.tapDeferredPending = false;
                    dispatchEntryActivation(entry, suppressWeapons, out);
                }
            } else {
                rt.holdElapsed = 0.0f;
                rt.longHoldFired = false;
            }
            break;

        case VrGesture::Tap1x:
            if (!entry.trigger.isTwoButton() && slotParticipatesInTwoButton(entry.trigger.primary)) {
                rt.tapDeferredPending = false;
                rt.clickCount = 0;
                break;
            }
            if (risingEdge) {
                const bool ambiguous =
                    !entry.trigger.isTwoButton() &&
                    (slotHasMultiClick(entry.trigger.primary) || slotHasSingleLongHold(entry.trigger.primary));
                if (!ambiguous) {
                    dispatchEntryActivation(entry, suppressWeapons, out);
                } else {
                    // Wait brief release/second-tap window so 2x/3x or LongHold doesn't misfire Tap1x.
                    rt.tapDeferredPending = true;
                    rt.tapDeferRemaining = 0.24f;
                }
            }
            if (!downNow && rt.prevTriggerDown && rt.tapDeferredPending && rt.tapDeferRemaining > 0.0f) {
                // Released quickly — let defer timer expire (unless a 2nd press cancels it).
            }
            if (risingEdge && rt.windowRemaining > 0.0f) {
                rt.tapDeferredPending = false;
            }
            if (risingEdge) {
                rt.windowRemaining = 0.28f;
            }
            break;

        case VrGesture::Double2x:
            if (risingEdge) {
                rt.clickCount = (rt.windowRemaining > 0.0f) ? (rt.clickCount + 1) : 1;
                rt.windowRemaining = 0.45f;
                // Cancel any pending Tap1x on the same slot
                for (size_t k = 0; k < count; ++k) {
                    if (profile.entries[k].trigger.primary == entry.trigger.primary &&
                        profile.entries[k].trigger.gesture == VrGesture::Tap1x) {
                        runtimes_[k].tapDeferredPending = false;
                    }
                }
                if (rt.clickCount == 2) {
                    rt.clickCount = 0;
                    rt.windowRemaining = 0.0f;
                    dispatchEntryActivation(entry, suppressWeapons, out);
                } else {
                    const uint16_t tickUs = 550;
                    if (entry.trigger.primary.hand == VrHand::Left) {
                        out.pulseLeft = true;
                        out.pulseLeftUs = std::max(out.pulseLeftUs, tickUs);
                    } else {
                        out.pulseRight = true;
                        out.pulseRightUs = std::max(out.pulseRightUs, tickUs);
                    }
                }
            }
            break;

        case VrGesture::Triple3x:
            if (risingEdge) {
                rt.clickCount = (rt.windowRemaining > 0.0f) ? (rt.clickCount + 1) : 1;
                rt.windowRemaining = 0.65f;
                if (rt.clickCount >= 3) {
                    rt.clickCount = 0;
                    rt.windowRemaining = 0.0f;
                    dispatchEntryActivation(entry, suppressWeapons, out);
                } else {
                    const uint16_t tickUs = 600;
                    if (entry.trigger.primary.hand == VrHand::Left) {
                        out.pulseLeft = true;
                        out.pulseLeftUs = std::max(out.pulseLeftUs, tickUs);
                    } else {
                        out.pulseRight = true;
                        out.pulseRightUs = std::max(out.pulseRightUs, tickUs);
                    }
                }
            }
            break;
        }

        rt.prevTriggerDown = downNow;
    }

    return out;
}

void VrInputBindingEngine::startCapture(VrGesture desiredGesture, float desiredHoldSec) noexcept {
    captureActive_ = true;
    captureGesture_ = desiredGesture;
    captureHoldSec_ = desiredHoldSec;
    captureTimeout_ = 5.0f;
    captureWindowTimer_ = 0.0f;
    captureFirst_ = {};
    captureSecond_ = {};
    captureBaselineMaskLeft_ = 0;
    captureBaselineMaskRight_ = 0;
}

void VrInputBindingEngine::cancelCapture() noexcept {
    captureActive_ = false;
    captureFirst_ = {};
    captureSecond_ = {};
}

VrCaptureResult VrInputBindingEngine::pollCapture(const VrFrameState& vrState, float dtSeconds) noexcept {
    VrCaptureResult res{};
    if (!captureActive_) {
        return res;
    }
    captureTimeout_ -= dtSeconds;
    if (captureTimeout_ <= 0.0f) {
        cancelCapture();
        return res;
    }

    const uint32_t leftMask = buildControllerButtonBitmask(vrState.left);
    const uint32_t rightMask = buildControllerButtonBitmask(vrState.right);
    if (captureBaselineMaskLeft_ == 0 && captureBaselineMaskRight_ == 0 && !captureFirst_.isValid()) {
        // Ignore whatever trigger was still held down when user clicked "Capture" on HUD!
        if ((leftMask != 0 || rightMask != 0) && captureTimeout_ > 4.75f) {
            return res;
        }
    }

    auto findNewlyPressedSlot = [&](VrHand hand, uint32_t mask, const VrInputSlot& ignore) -> VrInputSlot {
        for (int b = 1; b <= 9; ++b) {
            if ((mask & (1u << b)) == 0) {
                continue;
            }
            const VrInputSlot candidate{hand, static_cast<VrButtonId>(b)};
            if (candidate != ignore) {
                return candidate;
            }
        }
        return {};
    };

    if (!captureFirst_.isValid()) {
        VrInputSlot first = findNewlyPressedSlot(VrHand::Left, leftMask, {});
        if (!first.isValid()) {
            first = findNewlyPressedSlot(VrHand::Right, rightMask, {});
        }
        if (first.isValid()) {
            captureFirst_ = first;
            captureWindowTimer_ = 0.35f; // Briefly allow adding a 2nd simultaneous button!
        }
        return res;
    }

    if (!captureSecond_.isValid()) {
        VrInputSlot second = findNewlyPressedSlot(VrHand::Left, leftMask, captureFirst_);
        if (!second.isValid()) {
            second = findNewlyPressedSlot(VrHand::Right, rightMask, captureFirst_);
        }
        if (second.isValid()) {
            captureSecond_ = second;
        }
    }

    captureWindowTimer_ -= dtSeconds;
    const bool firstStillDown = isSlotPressed(vrState, captureFirst_);
    if (captureSecond_.isValid() || (captureWindowTimer_ <= 0.0f && !firstStillDown) || captureWindowTimer_ <= -0.25f) {
        res.completed = true;
        res.trigger.primary = captureFirst_;
        if (captureSecond_.isValid()) {
            res.trigger.secondary = captureSecond_;
            res.trigger.relation = VrComboRelation::Simultaneous;
            res.trigger.gesture = (captureGesture_ == VrGesture::LongHold) ? VrGesture::LongHold : VrGesture::Tap1x;
        } else {
            res.trigger.secondary = {};
            res.trigger.relation = VrComboRelation::Single;
            res.trigger.gesture = captureGesture_;
        }
        res.trigger.holdSeconds = captureHoldSec_;
        cancelCapture();
    }
    return res;
}

} // namespace il2vr
