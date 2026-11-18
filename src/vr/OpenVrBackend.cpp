#include "vr/OpenVrBackend.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <openvr.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace il2vr {

namespace {

Mat34 fromOvr(const vr::HmdMatrix34_t& src) {
    Mat34 dst{};
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 4; ++c) {
            dst.m[r][c] = src.m[r][c];
        }
    }
    return dst;
}

vr::HmdMatrix34_t toOvr(const Mat34& src) {
    vr::HmdMatrix34_t dst{};
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 4; ++c) {
            dst.m[r][c] = src.m[r][c];
        }
    }
    return dst;
}

Vec3 crossVec(const Vec3& a, const Vec3& b) {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

Vec3 normalizeVec(const Vec3& v, const Vec3& fallback) {
    const float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
    if (lenSq < 1e-8f) {
        return fallback;
    }
    const float invLen = 1.0f / std::sqrt(lenSq);
    return {v.x * invLen, v.y * invLen, v.z * invLen};
}

ControllerState readController(
    vr::IVRSystem* sys,
    vr::TrackedDeviceIndex_t idx,
    const std::array<vr::TrackedDevicePose_t, vr::k_unMaxTrackedDeviceCount>& seatedPoses,
    const std::array<vr::TrackedDevicePose_t, vr::k_unMaxTrackedDeviceCount>& rawPoses) {
    ControllerState out{};
    if (idx == vr::k_unTrackedDeviceIndexInvalid || idx >= vr::k_unMaxTrackedDeviceCount) {
        return out;
    }
    const auto& pose = seatedPoses[idx];
    if (!pose.bDeviceIsConnected || !pose.bPoseIsValid) {
        return out;
    }

    out.valid = true;
    out.pose = fromOvr(pose.mDeviceToAbsoluteTracking);
    if (rawPoses[idx].bDeviceIsConnected && rawPoses[idx].bPoseIsValid) {
        out.rawPose = fromOvr(rawPoses[idx].mDeviceToAbsoluteTracking);
    } else {
        out.rawPose = out.pose;
    }

    vr::VRControllerState_t cs{};
    if (sys->GetControllerState(idx, &cs, sizeof(cs))) {
        out.buttonPressedMask = cs.ulButtonPressed;
        out.buttonTouchedMask = cs.ulButtonTouched;
        out.grip = (cs.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_Grip)) != 0;
        out.triggerClick = (cs.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Trigger)) != 0;
        out.primaryBtn = (cs.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_A)) != 0;
        out.menuBtn = (cs.ulButtonPressed & vr::ButtonMaskFromId(vr::k_EButton_ApplicationMenu)) != 0;
        out.secondaryBtn = (cs.ulButtonPressed & (
            vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad) |
            vr::ButtonMaskFromId(vr::k_EButton_IndexController_JoyStick))) != 0;

        out.joyX = cs.rAxis[0].x;
        out.joyY = cs.rAxis[0].y;
        out.triggerValue = cs.rAxis[1].x;
    }
    return out;
}

} // namespace

OpenVrBackend::~OpenVrBackend() {
    shutdown();
}

bool OpenVrBackend::init(bool allowAutoLaunch) {
    if (system_ != nullptr) {
        return true;
    }

    lastError_.clear();
    vr::EVRInitError err = vr::VRInitError_None;

    if (!allowAutoLaunch) {
        // Check SteamVR IPC pipe directly so we never call VR_Init while SteamVR is closed
        // (repeated failed VR_Init calls leak Windows TLS slots inside openvr_api.dll).
        const bool pipePresent =
            GetFileAttributesA("\\\\.\\pipe\\SteamVR_Namespace") != INVALID_FILE_ATTRIBUTES ||
            WaitNamedPipeA("\\\\.\\pipe\\SteamVR_Namespace", 1) != FALSE;
        if (!pipePresent) {
            lastError_ = "Aguardando SteamVR (pipe SteamVR_Namespace inativo).";
            return false;
        }
    }

    err = vr::VRInitError_None;
    system_ = vr::VR_Init(&err, vr::VRApplication_Overlay);
    if (system_ != nullptr) {
        overlayMode_ = true;
        createHudOverlay();
        createLaserOverlay();
        createGizmoOverlays();
        createMouseOverlays();
        createCalloutOverlays();
        return true;
    }

    // Fallback to Background application mode if Overlay mode failed.
    vr::EVRInitError fallbackErr = vr::VRInitError_None;
    system_ = vr::VR_Init(&fallbackErr, vr::VRApplication_Background);
    if (system_ != nullptr) {
        overlayMode_ = false;
        createHudOverlay();
        createLaserOverlay();
        createGizmoOverlays();
        createMouseOverlays();
        createCalloutOverlays();
        return true;
    }

    const char* desc = vr::VR_GetVRInitErrorAsEnglishDescription(err);
    lastError_ = desc ? desc : "Falha ao inicializar OpenVR.";
    return false;
}

void OpenVrBackend::shutdown() {
    destroyCalloutOverlays();
    destroyMouseOverlays();
    destroyGizmoOverlays();
    destroyLaserOverlay();
    destroyHudOverlay();
    hudAnchored_ = false;
    if (system_ != nullptr) {
        vr::VR_Shutdown();
        system_ = nullptr;
        overlayMode_ = false;
    }
}

VrFrameState OpenVrBackend::pollState() {
    VrFrameState state{};
    if (system_ == nullptr) {
        return state;
    }

    vr::VREvent_t ev{};
    while (system_->PollNextEvent(&ev, sizeof(ev))) {
        if (ev.eventType == vr::VREvent_Quit) {
            system_->AcknowledgeQuit_Exiting();
            shutdown();
            return state;
        }
    }

    state.runtimeRunning = true;

    std::array<vr::TrackedDevicePose_t, vr::k_unMaxTrackedDeviceCount> rawPoses{};
    std::array<vr::TrackedDevicePose_t, vr::k_unMaxTrackedDeviceCount> seatedPoses{};

    system_->GetDeviceToAbsoluteTrackingPose(
        vr::TrackingUniverseRawAndUncalibrated,
        0.0f,
        rawPoses.data(),
        static_cast<uint32_t>(rawPoses.size()));

    // IL-2 executes in TrackingUniverseStanding on non-Vive headsets (e.g. Meta Quest 3).
    // Querying Standing here keeps poses, overlays, and the game cockpit in the same universe.
    system_->GetDeviceToAbsoluteTrackingPose(
        vr::TrackingUniverseStanding,
        0.0f,
        seatedPoses.data(),
        static_cast<uint32_t>(seatedPoses.size()));

    const auto& hmdRaw = rawPoses[vr::k_unTrackedDeviceIndex_Hmd];
    const auto& hmdSeated = seatedPoses[vr::k_unTrackedDeviceIndex_Hmd];
    if (hmdRaw.bDeviceIsConnected && hmdRaw.bPoseIsValid) {
        state.hmdValid = true;
        state.hmdRawPose = fromOvr(hmdRaw.mDeviceToAbsoluteTracking);
        if (hmdSeated.bPoseIsValid) {
            state.hmdSeatedPose = fromOvr(hmdSeated.mDeviceToAbsoluteTracking);
        }
    }

    const auto leftIdx = system_->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_LeftHand);
    const auto rightIdx = system_->GetTrackedDeviceIndexForControllerRole(vr::TrackedControllerRole_RightHand);
    state.left = readController(system_, leftIdx, seatedPoses, rawPoses);
    state.right = readController(system_, rightIdx, seatedPoses, rawPoses);

    return state;
}

std::optional<Mat34> OpenVrBackend::getSeatedZeroPose() {
    if (system_ == nullptr) {
        return std::nullopt;
    }
    auto* chapSetup = vr::VRChaperoneSetup();
    if (chapSetup == nullptr) {
        return std::nullopt;
    }

    chapSetup->RevertWorkingCopy();

    vr::HmdMatrix34_t mat{};
    if (!chapSetup->GetWorkingStandingZeroPoseToRawTrackingPose(&mat)) {
        if (!chapSetup->GetWorkingSeatedZeroPoseToRawTrackingPose(&mat)) {
            return std::nullopt;
        }
    }

    return fromOvr(mat);
}

bool OpenVrBackend::setSeatedZeroPose(const Mat34& pose, bool commitLive) {
    if (system_ == nullptr) {
        return false;
    }
    auto* chapSetup = vr::VRChaperoneSetup();
    if (chapSetup == nullptr) {
        return false;
    }

    // Mutate StandingZeroPose because IL-2 renders in TrackingUniverseStanding for Quest 3.
    // Mixing Seated and Standing poses causes a ~180 deg yaw jump, so we read/write Standing only.
    chapSetup->RevertWorkingCopy();

    const vr::HmdMatrix34_t mat = toOvr(pose);
    chapSetup->SetWorkingStandingZeroPoseToRawTrackingPose(&mat);

    if (commitLive) {
        return chapSetup->CommitWorkingCopy(vr::EChaperoneConfigFile_Live);
    }
    return true;
}

void OpenVrBackend::triggerHapticPulse(VrHand hand, uint16_t durationMicroSec) {
    if (system_ == nullptr) {
        return;
    }
    const auto role = (hand == VrHand::Left)
        ? vr::TrackedControllerRole_LeftHand
        : vr::TrackedControllerRole_RightHand;
    const auto idx = system_->GetTrackedDeviceIndexForControllerRole(role);
    if (idx != vr::k_unTrackedDeviceIndexInvalid) {
        system_->TriggerHapticPulse(idx, 0, durationMicroSec);
    }
}

void OpenVrBackend::createHudOverlay() {
    auto* overlay = vr::VROverlay();
    if (overlay == nullptr || overlayHandle_ != 0) {
        return;
    }

    vr::VROverlayHandle_t handle = vr::k_ulOverlayHandleInvalid;
    const auto err = overlay->CreateOverlay("il2vr.qol.hud", "IL-2 VR QoL HUD", &handle);
    if (err != vr::VROverlayError_None || handle == vr::k_ulOverlayHandleInvalid) {
        return;
    }

    overlayHandle_ = handle;
    overlay->SetOverlayWidthInMeters(overlayHandle_, hudWidthM_);
    overlay->SetOverlayAlpha(overlayHandle_, 0.94f);
    overlay->SetOverlayInputMethod(overlayHandle_, vr::VROverlayInputMethod_None);

    // Default seated anchor until HMD tracking becomes valid.
    anchorHudToHead(Mat34::identity(), 0.65f, -0.10f, hudWidthM_, hudHeightM_ / hudWidthM_);
    hudAnchored_ = false;

    if (overlayVisible_) {
        overlay->ShowOverlay(overlayHandle_);
    } else {
        overlay->HideOverlay(overlayHandle_);
    }
}

void OpenVrBackend::destroyHudOverlay() {
    if (overlayHandle_ != 0) {
        if (auto* overlay = vr::VROverlay()) {
            overlay->DestroyOverlay(overlayHandle_);
        }
        overlayHandle_ = 0;
    }
}

void OpenVrBackend::createLaserOverlay() {
    auto* overlay = vr::VROverlay();
    if (overlay == nullptr || laserOverlayHandle_ != 0) {
        return;
    }

    vr::VROverlayHandle_t handle = vr::k_ulOverlayHandleInvalid;
    const auto err = overlay->CreateOverlay("il2vr.qol.laser", "IL-2 VR QoL Laser", &handle);
    if (err != vr::VROverlayError_None || handle == vr::k_ulOverlayHandleInvalid) {
        return;
    }

    laserOverlayHandle_ = handle;
    overlay->SetOverlayAlpha(laserOverlayHandle_, 0.95f);
    overlay->SetOverlayWidthInMeters(laserOverlayHandle_, 0.50f);
    overlay->SetOverlayInputMethod(laserOverlayHandle_, vr::VROverlayInputMethod_None);

    // 256x2 RGBA bitmap gives aspect H/W = 1/128 (~4mm thick at 0.5m length).
    constexpr uint32_t kW = 256;
    constexpr uint32_t kH = 2;
    std::vector<uint8_t> pixels(kW * kH * 4, 0);
    for (uint32_t i = 0; i < kW * kH; ++i) {
        pixels[i * 4 + 0] = 60;
        pixels[i * 4 + 1] = 255;
        pixels[i * 4 + 2] = 195;
        pixels[i * 4 + 3] = 235;
    }
    overlay->SetOverlayRaw(laserOverlayHandle_, pixels.data(), kW, kH, 4);
    overlay->HideOverlay(laserOverlayHandle_);
}

void OpenVrBackend::destroyLaserOverlay() {
    if (laserOverlayHandle_ != 0) {
        if (auto* overlay = vr::VROverlay()) {
            overlay->DestroyOverlay(laserOverlayHandle_);
        }
        laserOverlayHandle_ = 0;
    }
}

void OpenVrBackend::applyHudTransform() {
    if (overlayHandle_ == 0) {
        return;
    }
    if (auto* overlay = vr::VROverlay()) {
        overlay->SetOverlayWidthInMeters(overlayHandle_, hudWidthM_);
        const vr::HmdMatrix34_t mat = toOvr(hudSeatedPose_);
        overlay->SetOverlayTransformAbsolute(
            overlayHandle_,
            vr::TrackingUniverseStanding,
            &mat);
    }
}

void OpenVrBackend::anchorHudToHead(
    const Mat34& hmdSeatedPose,
    float distanceM,
    float downOffsetM,
    float widthM,
    float aspectHOverW) {
    const Vec3 eye{hmdSeatedPose.m[0][3], hmdSeatedPose.m[1][3], hmdSeatedPose.m[2][3]};
    const float yawDeg = extractYawDeg(hmdSeatedPose);

    // Local -Z of horizontal pose is forward along gaze direction.
    const Mat34 horizHead = makeHorizontalPose(eye, yawDeg);
    const Vec3 horizFwd{-horizHead.m[0][2], 0.0f, -horizHead.m[2][2]};
    const Vec3 center{
        eye.x + horizFwd.x * distanceM,
        eye.y + downOffsetM,
        eye.z + horizFwd.z * distanceM
    };

    // Tilt panel slightly upward (-10 deg around local +X) so it faces the pilot's eyes comfortably.
    constexpr float cosMinus10 = 0.9848078f;
    constexpr float sinMinus10 = -0.1736482f;
    Mat34 tilt{};
    tilt.m[0][0] = 1.0f;
    tilt.m[1][1] = cosMinus10;
    tilt.m[1][2] = -sinMinus10;
    tilt.m[2][1] = sinMinus10;
    tilt.m[2][2] = cosMinus10;

    hudSeatedPose_ = multiply(makeHorizontalPose(center, yawDeg), tilt);
    hudWidthM_ = std::max(widthM, 0.20f);
    hudHeightM_ = hudWidthM_ * std::clamp(aspectHOverW, 0.25f, 2.0f);
    hudAnchored_ = true;
    applyHudTransform();
}

void OpenVrBackend::placeHudAtWorldPointFacingViewer(
    const Vec3& centerPos,
    const Vec3& viewerPos,
    float widthM,
    float aspectHOverW) {
    const float dx = viewerPos.x - centerPos.x;
    const float dy = centerPos.y - viewerPos.y;
    const float dz = viewerPos.z - centerPos.z;
    const float horizDist = std::sqrt(dx * dx + dz * dz);

    const float yawDeg = (horizDist > 1e-4f)
        ? (std::atan2(dx, dz) * (180.0f / 3.14159265358979323846f))
        : extractYawDeg(hudSeatedPose_);
    const float pitchRad = std::clamp(std::atan2(dy, std::max(horizDist, 0.10f)), -0.45f, 0.45f);
    const float c = std::cos(pitchRad);
    const float s = std::sin(pitchRad);

    Mat34 tilt{};
    tilt.m[0][0] = 1.0f;
    tilt.m[1][1] = c;
    tilt.m[1][2] = -s;
    tilt.m[2][1] = s;
    tilt.m[2][2] = c;

    hudSeatedPose_ = multiply(makeHorizontalPose(centerPos, yawDeg), tilt);
    hudWidthM_ = std::max(widthM, 0.20f);
    hudHeightM_ = hudWidthM_ * std::clamp(aspectHOverW, 0.25f, 2.0f);
    hudAnchored_ = true;
    applyHudTransform();
}

void OpenVrBackend::setHudAspectRatio(float aspectHOverW) {
    hudHeightM_ = hudWidthM_ * std::clamp(aspectHOverW, 0.25f, 2.0f);
}

void OpenVrBackend::createGizmoOverlays() {
    auto* overlay = vr::VROverlay();
    if (overlay == nullptr) {
        return;
    }

    constexpr uint32_t kSize = 64;
    auto makeRgbaTexture = [&](bool isControllerArrows) {
        std::vector<uint8_t> px(kSize * kSize * 4, 0);
        auto setPx = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
            if (x < 0 || x >= static_cast<int>(kSize) || y < 0 || y >= static_cast<int>(kSize)) {
                return;
            }
            const size_t idx = (static_cast<size_t>(y) * kSize + static_cast<size_t>(x)) * 4;
            px[idx + 0] = r;
            px[idx + 1] = g;
            px[idx + 2] = b;
            px[idx + 3] = a;
        };

        const float cx = 31.5f;
        const float cy = 31.5f;
        for (int y = 0; y < static_cast<int>(kSize); ++y) {
            for (int x = 0; x < static_cast<int>(kSize); ++x) {
                const float dx = static_cast<float>(x) - cx;
                const float dy = static_cast<float>(y) - cy;
                const float dist = std::sqrt(dx * dx + dy * dy);
                if (dist >= 27.0f && dist <= 30.0f) {
                    setPx(x, y, 220, 235, 255, 210);
                }
                if (dist <= 3.0f) {
                    setPx(x, y, 255, 255, 255, 230);
                }
            }
        }

        if (isControllerArrows) {
            // Forward (-Z -> up on XZ rotated plane): Cyan/Green arrow
            for (int y = 3; y <= 31; ++y) {
                for (int x = 30; x <= 33; ++x) {
                    setPx(x, y, 40, 245, 210, 245);
                }
            }
            for (int dy = 0; dy < 9; ++dy) {
                for (int dx = -dy; dx <= dy; ++dx) {
                    setPx(32 + dx, 2 + dy, 40, 255, 220, 255);
                }
            }
            // Right (+X -> right on XZ plane): Orange/Red arrow
            for (int x = 32; x <= 59; ++x) {
                for (int y = 30; y <= 33; ++y) {
                    setPx(x, y, 255, 115, 65, 245);
                }
            }
            for (int dx = 0; dx < 8; ++dx) {
                for (int dy = -dx; dy <= dx; ++dy) {
                    setPx(61 - dx, 32 + dy, 255, 130, 80, 255);
                }
            }
        } else {
            // Crosshair reticle for anchored stick & throttle
            for (int i = 10; i <= 53; ++i) {
                for (int w = 30; w <= 33; ++w) {
                    setPx(i, w, 255, 215, 60, 230);
                    setPx(w, i, 255, 215, 60, 230);
                }
            }
        }
        return px;
    };

    const auto createOne = [&](uint64_t& handle, const char* key, const char* name, float widthM, std::vector<uint8_t>& px) {
        if (handle != 0) {
            return;
        }
        vr::VROverlayHandle_t h = vr::k_ulOverlayHandleInvalid;
        if (overlay->CreateOverlay(key, name, &h) == vr::VROverlayError_None && h != vr::k_ulOverlayHandleInvalid) {
            handle = h;
            overlay->SetOverlayWidthInMeters(h, widthM);
            overlay->SetOverlayAlpha(h, 0.90f);
            overlay->SetOverlayInputMethod(h, vr::VROverlayInputMethod_None);
            overlay->SetOverlayRaw(h, px.data(), kSize, kSize, 4);
            overlay->HideOverlay(h);
        }
    };

    auto ctrlTex = makeRgbaTexture(true);
    auto anchorTex = makeRgbaTexture(false);
    createOne(leftGizmoOverlayHandle_, "il2vr.qol.gizmo.left", "IL-2 VR Left Gizmo", 0.14f, ctrlTex);
    createOne(rightGizmoOverlayHandle_, "il2vr.qol.gizmo.right", "IL-2 VR Right Gizmo", 0.14f, ctrlTex);
    createOne(stickAnchorOverlayHandle_, "il2vr.qol.anchor.stick", "IL-2 VR Stick Anchor", 0.11f, anchorTex);
    createOne(throttleAnchorOverlayHandle_, "il2vr.qol.anchor.throttle", "IL-2 VR Throttle Anchor", 0.11f, anchorTex);
}

void OpenVrBackend::destroyGizmoOverlays() {
    auto* overlay = vr::VROverlay();
    const auto destroyOne = [&](uint64_t& handle) {
        if (handle != 0) {
            if (overlay != nullptr) {
                overlay->DestroyOverlay(handle);
            }
            handle = 0;
        }
    };
    destroyOne(leftGizmoOverlayHandle_);
    destroyOne(rightGizmoOverlayHandle_);
    destroyOne(stickAnchorOverlayHandle_);
    destroyOne(throttleAnchorOverlayHandle_);
}

void OpenVrBackend::updateHandGizmoOverlays(
    bool visible,
    const ControllerState& left,
    const ControllerState& right) {
    auto* overlay = vr::VROverlay();
    if (overlay == nullptr) {
        return;
    }

    // Rotate overlay quad -90 deg around local +X so quad +Y points along controller forward (-Z).
    Mat34 flatXZ{};
    flatXZ.m[0][0] = 1.0f;
    flatXZ.m[1][2] = 1.0f;
    flatXZ.m[2][1] = -1.0f;

    const auto updateOne = [&](uint64_t handle, const ControllerState& c) {
        if (handle == 0) {
            return;
        }
        if (!visible || !c.valid) {
            overlay->HideOverlay(handle);
            return;
        }
        const vr::HmdMatrix34_t mat = toOvr(multiply(c.pose, flatXZ));
        overlay->SetOverlayTransformAbsolute(handle, vr::TrackingUniverseStanding, &mat);
        overlay->ShowOverlay(handle);
    };

    updateOne(leftGizmoOverlayHandle_, left);
    updateOne(rightGizmoOverlayHandle_, right);
}

void OpenVrBackend::updateControlAnchorOverlays(
    bool visible,
    const Mat34& stickPose,
    const Mat34& throttlePose,
    bool stickHighlighted,
    bool throttleHighlighted) {
    auto* overlay = vr::VROverlay();
    if (overlay == nullptr) {
        return;
    }
    const auto updateOne = [&](uint64_t handle, const Mat34& pose, bool highlight) {
        if (handle == 0) {
            return;
        }
        if (!visible) {
            overlay->HideOverlay(handle);
            return;
        }
        const vr::HmdMatrix34_t mat = toOvr(pose);
        overlay->SetOverlayWidthInMeters(handle, highlight ? 0.14f : 0.10f);
        overlay->SetOverlayAlpha(handle, highlight ? 0.98f : 0.65f);
        overlay->SetOverlayTransformAbsolute(handle, vr::TrackingUniverseStanding, &mat);
        overlay->ShowOverlay(handle);
    };
    updateOne(stickAnchorOverlayHandle_, stickPose, stickHighlighted);
    updateOne(throttleAnchorOverlayHandle_, throttlePose, throttleHighlighted);
}

void OpenVrBackend::createMouseOverlays() {
    auto* overlay = vr::VROverlay();
    if (overlay == nullptr) {
        return;
    }

    if (mouseScreenOverlayHandle_ == 0) {
        vr::VROverlayHandle_t h = vr::k_ulOverlayHandleInvalid;
        if (overlay->CreateOverlay("il2vr.qol.mouse.screen", "IL-2 VR Mouse Screen", &h) == vr::VROverlayError_None
            && h != vr::k_ulOverlayHandleInvalid) {
            mouseScreenOverlayHandle_ = h;
            constexpr uint32_t kW = 256;
            constexpr uint32_t kH = 144;
            std::vector<uint8_t> px(kW * kH * 4, 0);
            auto setPx = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
                if (x < 0 || x >= static_cast<int>(kW) || y < 0 || y >= static_cast<int>(kH)) {
                    return;
                }
                const size_t idx = (static_cast<size_t>(y) * kW + static_cast<size_t>(x)) * 4;
                px[idx + 0] = r;
                px[idx + 1] = g;
                px[idx + 2] = b;
                px[idx + 3] = a;
            };

            for (int y = 0; y < static_cast<int>(kH); ++y) {
                for (int x = 0; x < static_cast<int>(kW); ++x) {
                    const int dxEdge = std::min(x, static_cast<int>(kW) - 1 - x);
                    const int dyEdge = std::min(y, static_cast<int>(kH) - 1 - y);
                    const bool outerBorder = (dxEdge <= 1) || (dyEdge <= 1);
                    const bool inCorner = (dxEdge < 24 && dyEdge <= 2) || (dyEdge < 24 && dxEdge <= 2);
                    if (inCorner) {
                        setPx(x, y, 110, 245, 255, 210);
                    } else if (outerBorder) {
                        setPx(x, y, 80, 220, 255, 110);
                    }
                }
            }
            const int midX = static_cast<int>(kW) / 2;
            const int midY = static_cast<int>(kH) / 2;
            for (int d = -4; d <= 4; ++d) {
                setPx(midX + d, midY, 120, 235, 255, 90);
                setPx(midX, midY + d, 120, 235, 255, 90);
            }

            overlay->SetOverlayWidthInMeters(h, 1.60f);
            overlay->SetOverlayAlpha(h, 0.90f);
            overlay->SetOverlayInputMethod(h, vr::VROverlayInputMethod_None);
            overlay->SetOverlayRaw(h, px.data(), kW, kH, 4);
            overlay->HideOverlay(h);
        }
    }

    if (mouseCursorOverlayHandle_ == 0) {
        vr::VROverlayHandle_t h = vr::k_ulOverlayHandleInvalid;
        if (overlay->CreateOverlay("il2vr.qol.mouse.cursor", "IL-2 VR Mouse Cursor", &h) == vr::VROverlayError_None
            && h != vr::k_ulOverlayHandleInvalid) {
            mouseCursorOverlayHandle_ = h;
            constexpr uint32_t kSize = 64;
            std::vector<uint8_t> px(kSize * kSize * 4, 0);
            const float cx = 31.5f;
            const float cy = 31.5f;
            for (int y = 0; y < static_cast<int>(kSize); ++y) {
                for (int x = 0; x < static_cast<int>(kSize); ++x) {
                    const float dx = static_cast<float>(x) - cx;
                    const float dy = static_cast<float>(y) - cy;
                    const float dist = std::sqrt(dx * dx + dy * dy);
                    const size_t idx = (static_cast<size_t>(y) * kSize + static_cast<size_t>(x)) * 4;
                    if (dist <= 6.0f) {
                        px[idx + 0] = 255;
                        px[idx + 1] = 230;
                        px[idx + 2] = 75;
                        px[idx + 3] = 250;
                    } else if (dist >= 22.0f && dist <= 26.0f) {
                        px[idx + 0] = 80;
                        px[idx + 1] = 245;
                        px[idx + 2] = 255;
                        px[idx + 3] = 245;
                    } else if ((std::fabs(dx) <= 1.5f && dist >= 10.0f && dist <= 20.0f)
                        || (std::fabs(dy) <= 1.5f && dist >= 10.0f && dist <= 20.0f)) {
                        px[idx + 0] = 255;
                        px[idx + 1] = 255;
                        px[idx + 2] = 255;
                        px[idx + 3] = 220;
                    }
                }
            }

            overlay->SetOverlayWidthInMeters(h, 0.048f);
            overlay->SetOverlayAlpha(h, 0.98f);
            overlay->SetOverlayInputMethod(h, vr::VROverlayInputMethod_None);
            overlay->SetOverlayRaw(h, px.data(), kSize, kSize, 4);
            overlay->HideOverlay(h);
        }
    }

    if (mouseGrabBarOverlayHandle_ == 0) {
        vr::VROverlayHandle_t h = vr::k_ulOverlayHandleInvalid;
        if (overlay->CreateOverlay("il2vr.qol.mouse.grabbar", "IL-2 VR Mouse Grab Bar", &h) == vr::VROverlayError_None
            && h != vr::k_ulOverlayHandleInvalid) {
            mouseGrabBarOverlayHandle_ = h;
            overlay->SetOverlayWidthInMeters(h, 0.65f);
            overlay->SetOverlayAlpha(h, 0.94f);
            overlay->SetOverlayInputMethod(h, vr::VROverlayInputMethod_None);
            grabBarHoverMove_ = false;
            grabBarHoverClose_ = false;
            applyGrabBarTexture(false, false);
            overlay->HideOverlay(h);
        }
    }
}

void OpenVrBackend::applyGrabBarTexture(bool hoverMove, bool hoverClose) {
    if (mouseGrabBarOverlayHandle_ == 0) {
        return;
    }
    auto* overlay = vr::VROverlay();
    if (overlay == nullptr) {
        return;
    }

    constexpr uint32_t kW = 256;
    constexpr uint32_t kH = 40;
    std::vector<uint8_t> px(kW * kH * 4, 0);

    for (int y = 0; y < static_cast<int>(kH); ++y) {
        for (int x = 0; x < static_cast<int>(kW); ++x) {
            const float fx = static_cast<float>(x);
            const float fy = static_cast<float>(y);

            // Left/center move pill: horizontal capsule from x=20 to x=184, y=19.5, r=15.
            const float pxClamped = std::clamp(fx, 20.0f, 184.0f);
            const float dPill = std::sqrt((fx - pxClamped) * (fx - pxClamped) + (fy - 19.5f) * (fy - 19.5f));

            // Right close circle: center=(230, 19.5), r=15.
            const float dClose = std::sqrt((fx - 230.0f) * (fx - 230.0f) + (fy - 19.5f) * (fy - 19.5f));

            uint8_t r = 0, g = 0, b = 0, a = 0;
            if (dPill <= 15.5f) {
                if (dPill >= 13.8f) {
                    r = hoverMove ? 140 : 95;
                    g = hoverMove ? 245 : 210;
                    b = 255;
                    a = 240;
                } else {
                    r = hoverMove ? 36 : 20;
                    g = hoverMove ? 95 : 36;
                    b = hoverMove ? 165 : 58;
                    a = hoverMove ? 242 : 215;
                }

                // 4-way move cross icon centered at (102, 19.5).
                const float mx = fx - 102.0f;
                const float my = fy - 19.5f;
                const bool hStem = (std::fabs(my) <= 1.4f && std::fabs(mx) <= 9.0f);
                const bool vStem = (std::fabs(mx) <= 1.4f && std::fabs(my) <= 9.0f);
                const bool rArrow = (mx >= 5.0f && mx <= 10.0f && std::fabs(my) <= (10.0f - mx + 0.5f));
                const bool lArrow = (mx <= -5.0f && mx >= -10.0f && std::fabs(my) <= (10.0f + mx + 0.5f));
                const bool dArrow = (my >= 5.0f && my <= 10.0f && std::fabs(mx) <= (10.0f - my + 0.5f));
                const bool uArrow = (my <= -5.0f && my >= -10.0f && std::fabs(mx) <= (10.0f + my + 0.5f));
                if (hStem || vStem || rArrow || lArrow || dArrow || uArrow) {
                    r = 240;
                    g = 250;
                    b = 255;
                    a = 250;
                }
            } else if (dClose <= 15.5f) {
                if (dClose >= 13.8f) {
                    r = 255;
                    g = hoverClose ? 150 : 110;
                    b = hoverClose ? 150 : 115;
                    a = 245;
                } else {
                    r = hoverClose ? 190 : 88;
                    g = hoverClose ? 44 : 28;
                    b = hoverClose ? 54 : 34;
                    a = hoverClose ? 248 : 220;
                }

                // Simplified X close icon centered at (230, 19.5).
                const float cx = fx - 230.0f;
                const float cy = fy - 19.5f;
                if (std::fabs(cx) <= 6.2f && std::fabs(cy) <= 6.2f
                    && (std::fabs(cx - cy) <= 1.7f || std::fabs(cx + cy) <= 1.7f)) {
                    r = 255;
                    g = 245;
                    b = 245;
                    a = 252;
                }
            }

            const size_t idx = (static_cast<size_t>(y) * kW + static_cast<size_t>(x)) * 4;
            px[idx + 0] = r;
            px[idx + 1] = g;
            px[idx + 2] = b;
            px[idx + 3] = a;
        }
    }

    overlay->SetOverlayRaw(mouseGrabBarOverlayHandle_, px.data(), kW, kH, 4);
}

void OpenVrBackend::destroyMouseOverlays() {
    auto* overlay = vr::VROverlay();
    const auto destroyOne = [&](uint64_t& handle) {
        if (handle != 0) {
            if (overlay != nullptr) {
                overlay->DestroyOverlay(handle);
            }
            handle = 0;
        }
    };
    destroyOne(mouseScreenOverlayHandle_);
    destroyOne(mouseCursorOverlayHandle_);
    destroyOne(mouseGrabBarOverlayHandle_);
}

void OpenVrBackend::updateMouseScreenOverlays(
    bool visible,
    const Mat34& screenPose,
    float widthM,
    float heightM,
    float curvature,
    bool cursorVisible,
    const Mat34& cursorWorldPose,
    bool cursorPressed,
    bool grabBarHoverMove,
    bool grabBarHoverClose) {
    auto* overlay = vr::VROverlay();
    if (overlay == nullptr) {
        return;
    }

    if (!visible) {
        if (mouseScreenOverlayHandle_ != 0) {
            overlay->HideOverlay(mouseScreenOverlayHandle_);
        }
        if (mouseCursorOverlayHandle_ != 0) {
            overlay->HideOverlay(mouseCursorOverlayHandle_);
        }
        if (mouseGrabBarOverlayHandle_ != 0) {
            overlay->HideOverlay(mouseGrabBarOverlayHandle_);
        }
        return;
    }

    const float clampedWidth = std::max(widthM, 0.40f);
    const float clampedCurv = std::clamp(curvature, 0.0f, 0.45f);

    if (mouseScreenOverlayHandle_ != 0) {
        const vr::HmdMatrix34_t mat = toOvr(screenPose);
        overlay->SetOverlayWidthInMeters(mouseScreenOverlayHandle_, clampedWidth);
        overlay->SetOverlayCurvature(mouseScreenOverlayHandle_, clampedCurv);
        overlay->SetOverlayTransformAbsolute(
            mouseScreenOverlayHandle_,
            vr::TrackingUniverseStanding,
            &mat);
        overlay->ShowOverlay(mouseScreenOverlayHandle_);
    }

    if (mouseGrabBarOverlayHandle_ != 0) {
        if (grabBarHoverMove != grabBarHoverMove_ || grabBarHoverClose != grabBarHoverClose_) {
            grabBarHoverMove_ = grabBarHoverMove;
            grabBarHoverClose_ = grabBarHoverClose;
            applyGrabBarTexture(grabBarHoverMove_, grabBarHoverClose_);
        }
        const MouseGrabBarGeometry bar = computeMouseGrabBarGeometry(
            screenPose,
            clampedWidth,
            std::max(heightM, 0.25f),
            clampedCurv);
        const vr::HmdMatrix34_t barMat = toOvr(bar.pose);
        overlay->SetOverlayWidthInMeters(mouseGrabBarOverlayHandle_, bar.widthM);
        overlay->SetOverlayCurvature(mouseGrabBarOverlayHandle_, bar.curvature);
        overlay->SetOverlayTransformAbsolute(
            mouseGrabBarOverlayHandle_,
            vr::TrackingUniverseStanding,
            &barMat);
        overlay->ShowOverlay(mouseGrabBarOverlayHandle_);
    }

    if (mouseCursorOverlayHandle_ != 0) {
        if (!cursorVisible) {
            overlay->HideOverlay(mouseCursorOverlayHandle_);
        } else {
            Mat34 cursorPose = cursorWorldPose;
            cursorPose.m[0][3] += cursorWorldPose.m[0][2] * 0.002f;
            cursorPose.m[1][3] += cursorWorldPose.m[1][2] * 0.002f;
            cursorPose.m[2][3] += cursorWorldPose.m[2][2] * 0.002f;
            const vr::HmdMatrix34_t mat = toOvr(cursorPose);
            overlay->SetOverlayWidthInMeters(mouseCursorOverlayHandle_, cursorPressed ? 0.036f : 0.048f);
            overlay->SetOverlayAlpha(mouseCursorOverlayHandle_, 0.98f);
            overlay->SetOverlayTransformAbsolute(
                mouseCursorOverlayHandle_,
                vr::TrackingUniverseStanding,
                &mat);
            overlay->ShowOverlay(mouseCursorOverlayHandle_);
        }
    }
}

bool buildSegmentBeamPose(
    const Vec3& startSeated,
    const Vec3& endSeated,
    const Vec3& viewerSeatedPos,
    Mat34& outPose,
    float& outLen) {
    const Vec3 diff{
        endSeated.x - startSeated.x,
        endSeated.y - startSeated.y,
        endSeated.z - startSeated.z
    };
    const float len = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
    if (len < 0.015f) {
        return false;
    }

    const Vec3 axisX{diff.x / len, diff.y / len, diff.z / len};
    const Vec3 mid{
        (startSeated.x + endSeated.x) * 0.5f,
        (startSeated.y + endSeated.y) * 0.5f,
        (startSeated.z + endSeated.z) * 0.5f
    };
    const Vec3 toEye = normalizeVec(
        {viewerSeatedPos.x - mid.x, viewerSeatedPos.y - mid.y, viewerSeatedPos.z - mid.z},
        {0.0f, 0.0f, 1.0f});

    Vec3 rawY = crossVec(toEye, axisX);
    if ((rawY.x * rawY.x + rawY.y * rawY.y + rawY.z * rawY.z) < 1e-6f) {
        rawY = crossVec({0.0f, 1.0f, 0.0f}, axisX);
        if ((rawY.x * rawY.x + rawY.y * rawY.y + rawY.z * rawY.z) < 1e-6f) {
            rawY = crossVec({1.0f, 0.0f, 0.0f}, axisX);
        }
    }
    const Vec3 axisY = normalizeVec(rawY, {0.0f, 1.0f, 0.0f});
    const Vec3 axisZ = normalizeVec(crossVec(axisX, axisY), {0.0f, 0.0f, 1.0f});

    outPose = Mat34{};
    outPose.m[0][0] = axisX.x; outPose.m[0][1] = axisY.x; outPose.m[0][2] = axisZ.x; outPose.m[0][3] = mid.x;
    outPose.m[1][0] = axisX.y; outPose.m[1][1] = axisY.y; outPose.m[1][2] = axisZ.y; outPose.m[1][3] = mid.y;
    outPose.m[2][0] = axisX.z; outPose.m[2][1] = axisY.z; outPose.m[2][2] = axisZ.z; outPose.m[2][3] = mid.z;
    outLen = len;
    return true;
}

void OpenVrBackend::updateLaserOverlay(
    bool visible,
    const Vec3& startSeated,
    const Vec3& endSeated,
    const Vec3& viewerSeatedPos) {
    if (laserOverlayHandle_ == 0) {
        return;
    }
    auto* overlay = vr::VROverlay();
    if (overlay == nullptr) {
        return;
    }

    if (!visible) {
        overlay->HideOverlay(laserOverlayHandle_);
        return;
    }

    Mat34 beamPose{};
    float len = 0.0f;
    if (!buildSegmentBeamPose(startSeated, endSeated, viewerSeatedPos, beamPose, len)) {
        overlay->HideOverlay(laserOverlayHandle_);
        return;
    }

    const vr::HmdMatrix34_t ovrBeam = toOvr(beamPose);
    overlay->SetOverlayWidthInMeters(laserOverlayHandle_, len);
    overlay->SetOverlayTransformAbsolute(
        laserOverlayHandle_,
        vr::TrackingUniverseStanding,
        &ovrBeam);
    overlay->ShowOverlay(laserOverlayHandle_);
}

namespace {

std::wstring utf8ToWide(const std::string& s) {
    if (s.empty()) {
        return L"";
    }
    const int needed = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    if (needed <= 1) {
        return L"";
    }
    std::wstring out(static_cast<size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), needed);
    out.pop_back();
    return out;
}

void renderCalloutCardRgba(
    const CalloutCardSpec& card,
    std::vector<uint8_t>& outRgba,
    uint32_t& outW,
    uint32_t& outH) {
    outW = 480;
    outH = 128;
    outRgba.assign(static_cast<size_t>(outW) * outH * 4u, 0);

    HDC screenDc = GetDC(nullptr);
    HDC memDc = CreateCompatibleDC(screenDc);
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = static_cast<LONG>(outW);
    bmi.bmiHeader.biHeight = -static_cast<LONG>(outH); // top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(memDc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (bmp == nullptr || bits == nullptr) {
        if (bmp != nullptr) {
            DeleteObject(bmp);
        }
        DeleteDC(memDc);
        ReleaseDC(nullptr, screenDc);
        return;
    }

    HGDIOBJ oldBmp = SelectObject(memDc, bmp);

    // Fill rounded card background & crisp border.
    const COLORREF bgCol = RGB(14, 22, 34);
    const COLORREF borderCol = card.isTwoButton ? RGB(245, 165, 35) : (card.isCompanion ? RGB(56, 189, 248) : RGB(74, 222, 128));
    HBRUSH bgBrush = CreateSolidBrush(bgCol);
    HPEN borderPen = CreatePen(PS_SOLID, 4, borderCol);
    HGDIOBJ oldBrush = SelectObject(memDc, bgBrush);
    HGDIOBJ oldPen = SelectObject(memDc, borderPen);
    RoundRect(memDc, 2, 2, static_cast<int>(outW) - 2, static_cast<int>(outH) - 2, 24, 24);

    // Mode pill top-right.
    const COLORREF pillCol = card.isTwoButton ? RGB(150, 88, 12) : (card.isCompanion ? RGB(20, 92, 138) : RGB(22, 108, 62));
    HBRUSH pillBrush = CreateSolidBrush(pillCol);
    HPEN pillPen = CreatePen(PS_SOLID, 2, borderCol);
    SelectObject(memDc, pillBrush);
    SelectObject(memDc, pillPen);
    RoundRect(memDc, 250, 12, static_cast<int>(outW) - 14, 52, 14, 14);

    SetBkMode(memDc, TRANSPARENT);
    HFONT titleFont = CreateFontW(30, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT subFont = CreateFontW(23, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT pillFont = CreateFontW(20, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    HGDIOBJ oldFont = SelectObject(memDc, titleFont);
    SetTextColor(memDc, RGB(242, 248, 255));
    RECT rTitle{18, 14, 244, 62};
    const std::wstring wTitle = utf8ToWide(card.title);
    DrawTextW(memDc, wTitle.c_str(), -1, &rTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    SelectObject(memDc, pillFont);
    SetTextColor(memDc, RGB(255, 250, 230));
    RECT rPill{254, 14, static_cast<LONG>(outW) - 18, 50};
    const std::wstring wMode = utf8ToWide(card.modeBadge);
    DrawTextW(memDc, wMode.c_str(), -1, &rPill, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    SelectObject(memDc, subFont);
    SetTextColor(memDc, card.isTwoButton ? RGB(255, 215, 130) : RGB(155, 225, 255));
    RECT rSub{18, 68, static_cast<LONG>(outW) - 18, 114};
    const std::wstring wSub = utf8ToWide(card.inputLabel + "  ->  " + card.outputLabel);
    DrawTextW(memDc, wSub.c_str(), -1, &rSub, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    GdiFlush();

    // Convert BGRA DIB -> OpenVR RGBA with clean rounded-rectangle corner alpha.
    const uint8_t* src = static_cast<const uint8_t*>(bits);
    const float hw = static_cast<float>(outW) * 0.5f - 2.0f;
    const float hh = static_cast<float>(outH) * 0.5f - 2.0f;
    const float radius = 12.0f;
    for (uint32_t y = 0; y < outH; ++y) {
        const float py = std::fabs(static_cast<float>(y) + 0.5f - static_cast<float>(outH) * 0.5f) - (hh - radius);
        for (uint32_t x = 0; x < outW; ++x) {
            const float px = std::fabs(static_cast<float>(x) + 0.5f - static_cast<float>(outW) * 0.5f) - (hw - radius);
            const float qx = std::max(px, 0.0f);
            const float qy = std::max(py, 0.0f);
            const float dist = std::sqrt(qx * qx + qy * qy) - radius;

            const size_t idx = (static_cast<size_t>(y) * outW + static_cast<size_t>(x)) * 4u;
            const uint8_t b = src[idx + 0];
            const uint8_t g = src[idx + 1];
            const uint8_t r = src[idx + 2];
            uint8_t a = 0;
            if (dist <= 0.0f) {
                a = 242;
            } else if (dist < 1.5f) {
                a = static_cast<uint8_t>(242.0f * (1.0f - dist / 1.5f));
            }
            outRgba[idx + 0] = r;
            outRgba[idx + 1] = g;
            outRgba[idx + 2] = b;
            outRgba[idx + 3] = a;
        }
    }

    SelectObject(memDc, oldFont);
    DeleteObject(titleFont);
    DeleteObject(subFont);
    DeleteObject(pillFont);
    SelectObject(memDc, oldBrush);
    SelectObject(memDc, oldPen);
    DeleteObject(bgBrush);
    DeleteObject(borderPen);
    DeleteObject(pillBrush);
    DeleteObject(pillPen);
    SelectObject(memDc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(memDc);
    ReleaseDC(nullptr, screenDc);
}

} // namespace

void OpenVrBackend::createCalloutOverlays() {
    auto* overlay = vr::VROverlay();
    if (overlay == nullptr) {
        return;
    }

    // 1. Glowing circular dot texture for button pins (24x24).
    constexpr uint32_t kPinSz = 24;
    std::vector<uint8_t> pinPx(kPinSz * kPinSz * 4u, 0);
    for (uint32_t y = 0; y < kPinSz; ++y) {
        for (uint32_t x = 0; x < kPinSz; ++x) {
            const float dx = static_cast<float>(x) + 0.5f - static_cast<float>(kPinSz) * 0.5f;
            const float dy = static_cast<float>(y) + 0.5f - static_cast<float>(kPinSz) * 0.5f;
            const float rDist = std::sqrt(dx * dx + dy * dy);
            const size_t idx = (static_cast<size_t>(y) * kPinSz + x) * 4u;
            if (rDist <= 6.5f) {
                pinPx[idx + 0] = 255;
                pinPx[idx + 1] = 255;
                pinPx[idx + 2] = 255;
                pinPx[idx + 3] = 245;
            } else if (rDist <= 10.5f) {
                const float t = 1.0f - (rDist - 6.5f) / 4.0f;
                pinPx[idx + 0] = 255;
                pinPx[idx + 1] = 255;
                pinPx[idx + 2] = 255;
                pinPx[idx + 3] = static_cast<uint8_t>(180.0f * t);
            }
        }
    }

    for (size_t i = 0; i < calloutPinHandles_.size(); ++i) {
        if (calloutPinHandles_[i] != 0) {
            continue;
        }
        vr::VROverlayHandle_t h = vr::k_ulOverlayHandleInvalid;
        const std::string key = "il2vr.qol.callout.pin." + std::to_string(i);
        if (overlay->CreateOverlay(key.c_str(), key.c_str(), &h) == vr::VROverlayError_None &&
            h != vr::k_ulOverlayHandleInvalid) {
            calloutPinHandles_[i] = h;
            overlay->SetOverlayWidthInMeters(h, 0.015f);
            overlay->SetOverlayInputMethod(h, vr::VROverlayInputMethod_None);
            overlay->SetOverlayRaw(h, pinPx.data(), kPinSz, kPinSz, 4);
            overlay->HideOverlay(h);
        }
    }

    // 2. Leader line beam texture (512x4 white tintable hairline ~2mm thick at 25cm length).
    constexpr uint32_t kLineW = 512;
    constexpr uint32_t kLineH = 4;
    std::vector<uint8_t> linePx(kLineW * kLineH * 4u, 255);
    for (uint32_t y = 0; y < kLineH; ++y) {
        const uint8_t a = (y == 1 || y == 2) ? 235 : 95;
        for (uint32_t x = 0; x < kLineW; ++x) {
            const size_t idx = (static_cast<size_t>(y) * kLineW + x) * 4u;
            linePx[idx + 3] = a;
        }
    }

    for (size_t i = 0; i < calloutLineHandles_.size(); ++i) {
        if (calloutLineHandles_[i] != 0) {
            continue;
        }
        vr::VROverlayHandle_t h = vr::k_ulOverlayHandleInvalid;
        const std::string key = "il2vr.qol.callout.line." + std::to_string(i);
        if (overlay->CreateOverlay(key.c_str(), key.c_str(), &h) == vr::VROverlayError_None &&
            h != vr::k_ulOverlayHandleInvalid) {
            calloutLineHandles_[i] = h;
            overlay->SetOverlayWidthInMeters(h, 0.15f);
            overlay->SetOverlayAlpha(h, 0.90f);
            overlay->SetOverlayInputMethod(h, vr::VROverlayInputMethod_None);
            overlay->SetOverlayRaw(h, linePx.data(), kLineW, kLineH, 4);
            overlay->HideOverlay(h);
        }
    }

    // 3. Callout card overlay slots.
    for (size_t i = 0; i < calloutCardHandles_.size(); ++i) {
        calloutCardCacheKeys_[i].clear();
        if (calloutCardHandles_[i] != 0) {
            continue;
        }
        vr::VROverlayHandle_t h = vr::k_ulOverlayHandleInvalid;
        const std::string key = "il2vr.qol.callout.card." + std::to_string(i);
        if (overlay->CreateOverlay(key.c_str(), key.c_str(), &h) == vr::VROverlayError_None &&
            h != vr::k_ulOverlayHandleInvalid) {
            calloutCardHandles_[i] = h;
            overlay->SetOverlayWidthInMeters(h, 0.165f);
            overlay->SetOverlayAlpha(h, 0.96f);
            overlay->SetOverlayInputMethod(h, vr::VROverlayInputMethod_None);
            overlay->HideOverlay(h);
        }
    }
}

void OpenVrBackend::destroyCalloutOverlays() {
    auto* overlay = vr::VROverlay();
    const auto destroyArr = [&](auto& arr) {
        for (auto& handle : arr) {
            if (handle != 0) {
                if (overlay != nullptr) {
                    overlay->DestroyOverlay(handle);
                }
                handle = 0;
            }
        }
    };
    destroyArr(calloutPinHandles_);
    destroyArr(calloutLineHandles_);
    destroyArr(calloutCardHandles_);
    for (auto& k : calloutCardCacheKeys_) {
        k.clear();
    }
}

void OpenVrBackend::updateControllerCalloutOverlays(
    bool visible,
    const CalloutScene& scene,
    const Vec3& viewerSeatedPos) {
    auto* overlay = vr::VROverlay();
    if (overlay == nullptr) {
        return;
    }

    if (!visible) {
        for (const auto h : calloutPinHandles_) if (h != 0) overlay->HideOverlay(h);
        for (const auto h : calloutLineHandles_) if (h != 0) overlay->HideOverlay(h);
        for (const auto h : calloutCardHandles_) if (h != 0) overlay->HideOverlay(h);
        return;
    }

    // 1. Pins facing viewer.
    for (size_t i = 0; i < calloutPinHandles_.size(); ++i) {
        const uint64_t h = calloutPinHandles_[i];
        if (h == 0) {
            continue;
        }
        if (i >= scene.pins.size()) {
            overlay->HideOverlay(h);
            continue;
        }
        const auto& pin = scene.pins[i];
        const Vec3 toEye = normalizeVec(
            {viewerSeatedPos.x - pin.worldPos.x, viewerSeatedPos.y - pin.worldPos.y, viewerSeatedPos.z - pin.worldPos.z},
            {0.0f, 0.0f, 1.0f});
        const Vec3 right = normalizeVec(crossVec({0.0f, 1.0f, 0.0f}, toEye), {1.0f, 0.0f, 0.0f});
        const Vec3 up = normalizeVec(crossVec(toEye, right), {0.0f, 1.0f, 0.0f});
        Mat34 pinPose{};
        pinPose.m[0][0] = right.x; pinPose.m[0][1] = up.x; pinPose.m[0][2] = toEye.x; pinPose.m[0][3] = pin.worldPos.x;
        pinPose.m[1][0] = right.y; pinPose.m[1][1] = up.y; pinPose.m[1][2] = toEye.y; pinPose.m[1][3] = pin.worldPos.y;
        pinPose.m[2][0] = right.z; pinPose.m[2][1] = up.z; pinPose.m[2][2] = toEye.z; pinPose.m[2][3] = pin.worldPos.z;
        const vr::HmdMatrix34_t ovrMat = toOvr(pinPose);
        const float r = static_cast<float>((pin.rgbaColor >> 16) & 0xFFu) / 255.0f;
        const float g = static_cast<float>((pin.rgbaColor >> 8) & 0xFFu) / 255.0f;
        const float b = static_cast<float>(pin.rgbaColor & 0xFFu) / 255.0f;
        overlay->SetOverlayColor(h, r, g, b);
        overlay->SetOverlayTransformAbsolute(h, vr::TrackingUniverseStanding, &ovrMat);
        overlay->ShowOverlay(h);
    }

    // 2. Leader lines connecting button pin(s) -> Tarja card border.
    for (size_t i = 0; i < calloutLineHandles_.size(); ++i) {
        const uint64_t h = calloutLineHandles_[i];
        if (h == 0) {
            continue;
        }
        if (i >= scene.lines.size()) {
            overlay->HideOverlay(h);
            continue;
        }
        const auto& ln = scene.lines[i];
        Mat34 segPose{};
        float len = 0.0f;
        if (!buildSegmentBeamPose(ln.startWorld, ln.endWorld, viewerSeatedPos, segPose, len)) {
            overlay->HideOverlay(h);
            continue;
        }
        const vr::HmdMatrix34_t ovrBeam = toOvr(segPose);
        const float r = static_cast<float>((ln.rgbaColor >> 16) & 0xFFu) / 255.0f;
        const float g = static_cast<float>((ln.rgbaColor >> 8) & 0xFFu) / 255.0f;
        const float b = static_cast<float>(ln.rgbaColor & 0xFFu) / 255.0f;
        overlay->SetOverlayColor(h, r, g, b);
        overlay->SetOverlayWidthInMeters(h, len);
        overlay->SetOverlayTransformAbsolute(h, vr::TrackingUniverseStanding, &ovrBeam);
        overlay->ShowOverlay(h);
    }

    // 3. Callout Tarjas (re-rasterize RGBA texture ONLY when card text/mode changes).
    for (size_t i = 0; i < calloutCardHandles_.size(); ++i) {
        const uint64_t h = calloutCardHandles_[i];
        if (h == 0) {
            continue;
        }
        if (i >= scene.cards.size()) {
            overlay->HideOverlay(h);
            continue;
        }
        const auto& card = scene.cards[i];
        const std::string cacheKey =
            card.entryId + "|" + card.title + "|" + card.inputLabel + "|" + card.modeBadge + "|" + card.outputLabel;
        if (calloutCardCacheKeys_[i] != cacheKey) {
            std::vector<uint8_t> rgba;
            uint32_t w = 0;
            uint32_t hPx = 0;
            renderCalloutCardRgba(card, rgba, w, hPx);
            if (!rgba.empty()) {
                overlay->SetOverlayRaw(h, rgba.data(), w, hPx, 4);
                calloutCardCacheKeys_[i] = cacheKey;
            }
        }
        const vr::HmdMatrix34_t ovrCard = toOvr(card.pose);
        overlay->SetOverlayWidthInMeters(h, card.widthM);
        overlay->SetOverlayTransformAbsolute(h, vr::TrackingUniverseStanding, &ovrCard);
        overlay->ShowOverlay(h);
    }
}

void OpenVrBackend::setOverlayVisible(bool visible) {
    overlayVisible_ = visible;
    if (overlayHandle_ != 0) {
        if (auto* overlay = vr::VROverlay()) {
            if (overlayVisible_) {
                overlay->ShowOverlay(overlayHandle_);
            } else {
                overlay->HideOverlay(overlayHandle_);
            }
        }
    }
}

void OpenVrBackend::submitOverlayTextureD3D11(void* d3d11Texture2D) {
    if (overlayHandle_ == 0 || !overlayVisible_ || d3d11Texture2D == nullptr) {
        return;
    }
    auto* overlay = vr::VROverlay();
    if (overlay == nullptr) {
        return;
    }
    const vr::Texture_t tex{
        d3d11Texture2D,
        vr::TextureType_DirectX,
        vr::ColorSpace_Auto
    };
    overlay->SetOverlayTexture(overlayHandle_, &tex);
}

} // namespace il2vr
