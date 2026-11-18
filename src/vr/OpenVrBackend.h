#pragma once

#include "vr/ControllerCalloutLayout.h"
#include "vr/IVrBackend.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace vr {
class IVRSystem;
}

namespace il2vr {

class OpenVrBackend final : public IVrBackend {
public:
    OpenVrBackend() = default;
    ~OpenVrBackend() override;

    OpenVrBackend(const OpenVrBackend&) = delete;
    OpenVrBackend& operator=(const OpenVrBackend&) = delete;

    bool init(bool allowAutoLaunch = false) override;
    void shutdown() override;
    bool isInitialized() const override { return system_ != nullptr; }
    std::string lastError() const override { return lastError_; }

    VrFrameState pollState() override;
    std::optional<Mat34> getSeatedZeroPose() override;
    bool setSeatedZeroPose(const Mat34& pose, bool commitLive = true) override;
    void triggerHapticPulse(VrHand hand, uint16_t durationMicroSec) override;

    void setOverlayVisible(bool visible) override;
    bool isOverlayVisible() const override { return overlayHandle_ != 0 && overlayVisible_; }
    void submitOverlayTextureD3D11(void* d3d11Texture2D) override;

    bool isOverlayMode() const { return overlayMode_; }
    bool hasHudOverlay() const { return overlayHandle_ != 0; }

    void anchorHudToHead(
        const Mat34& hmdSeatedPose,
        float distanceM = 0.65f,
        float downOffsetM = -0.10f,
        float widthM = 0.62f,
        float aspectHOverW = 680.0f / 960.0f);
    void placeHudAtWorldPointFacingViewer(
        const Vec3& centerPos,
        const Vec3& viewerPos,
        float widthM,
        float aspectHOverW);
    void setHudAspectRatio(float aspectHOverW);
    bool isHudAnchored() const noexcept { return hudAnchored_; }
    const Mat34& hudSeatedPose() const noexcept { return hudSeatedPose_; }
    float hudWidthM() const noexcept { return hudWidthM_; }
    float hudHeightM() const noexcept { return hudHeightM_; }

    void updateLaserOverlay(
        bool visible,
        const Vec3& startSeated,
        const Vec3& endSeated,
        const Vec3& viewerSeatedPos);
    void updateHandGizmoOverlays(
        bool visible,
        const ControllerState& left,
        const ControllerState& right);
    void updateControlAnchorOverlays(
        bool visible,
        const Mat34& stickPose,
        const Mat34& throttlePose,
        bool stickHighlighted,
        bool throttleHighlighted);
    void updateMouseScreenOverlays(
        bool visible,
        const Mat34& screenPose,
        float widthM,
        float heightM,
        float curvature,
        bool cursorVisible,
        const Mat34& cursorWorldPose,
        bool cursorPressed,
        bool grabBarHoverMove = false,
        bool grabBarHoverClose = false);
    void updateControllerCalloutOverlays(
        bool visible,
        const CalloutScene& scene,
        const Vec3& viewerSeatedPos);

private:
    void createHudOverlay();
    void destroyHudOverlay();
    void createLaserOverlay();
    void destroyLaserOverlay();
    void createGizmoOverlays();
    void destroyGizmoOverlays();
    void createMouseOverlays();
    void destroyMouseOverlays();
    void createCalloutOverlays();
    void destroyCalloutOverlays();
    void applyGrabBarTexture(bool hoverMove, bool hoverClose);
    void applyHudTransform();

    vr::IVRSystem* system_ = nullptr;
    bool overlayMode_ = false;
    uint64_t overlayHandle_ = 0;
    uint64_t laserOverlayHandle_ = 0;
    uint64_t leftGizmoOverlayHandle_ = 0;
    uint64_t rightGizmoOverlayHandle_ = 0;
    uint64_t stickAnchorOverlayHandle_ = 0;
    uint64_t throttleAnchorOverlayHandle_ = 0;
    uint64_t mouseScreenOverlayHandle_ = 0;
    uint64_t mouseCursorOverlayHandle_ = 0;
    uint64_t mouseGrabBarOverlayHandle_ = 0;
    std::array<uint64_t, ControllerCalloutLayout::kMaxPins> calloutPinHandles_{};
    std::array<uint64_t, ControllerCalloutLayout::kMaxLines> calloutLineHandles_{};
    std::array<uint64_t, ControllerCalloutLayout::kMaxCards> calloutCardHandles_{};
    std::array<std::string, ControllerCalloutLayout::kMaxCards> calloutCardCacheKeys_{};
    bool grabBarHoverMove_ = false;
    bool grabBarHoverClose_ = false;
    bool overlayVisible_ = true;
    bool hudAnchored_ = false;
    Mat34 hudSeatedPose_ = Mat34::identity();
    float hudWidthM_ = 0.62f;
    float hudHeightM_ = 0.62f * (680.0f / 960.0f);
    std::string lastError_;
};

} // namespace il2vr
