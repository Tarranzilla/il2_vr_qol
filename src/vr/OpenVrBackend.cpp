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
        return true;
    }

    // Fallback to Background application mode if Overlay mode failed.
    vr::EVRInitError fallbackErr = vr::VRInitError_None;
    system_ = vr::VR_Init(&fallbackErr, vr::VRApplication_Background);
    if (system_ != nullptr) {
        overlayMode_ = false;
        createHudOverlay();
        createLaserOverlay();
        return true;
    }

    const char* desc = vr::VR_GetVRInitErrorAsEnglishDescription(err);
    lastError_ = desc ? desc : "Falha ao inicializar OpenVR.";
    return false;
}

void OpenVrBackend::shutdown() {
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

void OpenVrBackend::setHudAspectRatio(float aspectHOverW) {
    hudHeightM_ = hudWidthM_ * std::clamp(aspectHOverW, 0.25f, 2.0f);
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

    const Vec3 diff{
        endSeated.x - startSeated.x,
        endSeated.y - startSeated.y,
        endSeated.z - startSeated.z
    };
    const float len = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
    if (len < 0.02f) {
        overlay->HideOverlay(laserOverlayHandle_);
        return;
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

    Mat34 beamPose{};
    beamPose.m[0][0] = axisX.x; beamPose.m[0][1] = axisY.x; beamPose.m[0][2] = axisZ.x; beamPose.m[0][3] = mid.x;
    beamPose.m[1][0] = axisX.y; beamPose.m[1][1] = axisY.y; beamPose.m[1][2] = axisZ.y; beamPose.m[1][3] = mid.y;
    beamPose.m[2][0] = axisX.z; beamPose.m[2][1] = axisY.z; beamPose.m[2][2] = axisZ.z; beamPose.m[2][3] = mid.z;

    const vr::HmdMatrix34_t ovrBeam = toOvr(beamPose);
    overlay->SetOverlayWidthInMeters(laserOverlayHandle_, len);
    overlay->SetOverlayTransformAbsolute(
        laserOverlayHandle_,
        vr::TrackingUniverseStanding,
        &ovrBeam);
    overlay->ShowOverlay(laserOverlayHandle_);
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
    if (!overlayVisible_ && laserOverlayHandle_ != 0) {
        if (auto* overlay = vr::VROverlay()) {
            overlay->HideOverlay(laserOverlayHandle_);
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
