#pragma once

#include <optional>

namespace il2vr {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Quat {
    float w = 1.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

// Row-major 3x4 affine transform matching OpenVR's HmdMatrix34_t
// (+X right, +Y up, -Z forward).
struct Mat34 {
    float m[3][4] = {
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f}
    };

    static Mat34 identity();
};

struct Pose3D {
    Vec3 position{};
    Quat orientation{};
};

struct SeatOffset {
    float x = 0.0f;       // meters (+X right)
    float y = 0.0f;       // meters (+Y up)
    float z = 0.0f;       // meters (+Z back, -Z forward)
    float yawDeg = 0.0f;  // degrees (+CCW around +Y)
};

struct Ray {
    Vec3 origin{};
    Vec3 dir{0.0f, 0.0f, -1.0f};
};

struct HudQuadHit {
    float u = 0.0f;         // [0, 1] left -> right
    float v = 0.0f;         // [0, 1] top -> bottom (ImGui convention)
    float distance = 0.0f;  // distance along ray (> 0)
    Vec3 worldPoint{};      // 3D hit point in quadPose's coordinate space
};

Mat34 multiply(const Mat34& a, const Mat34& b);
Mat34 invertRigid(const Mat34& m);
Vec3 transformPoint(const Mat34& m, const Vec3& p);
Vec3 transformDirection(const Mat34& m, const Vec3& d);

float extractYawDeg(const Mat34& m);
Mat34 makeHorizontalPose(const Vec3& pos, float yawDeg);

// Direct composition: RecenterBase * T(offset).
Mat34 composeSeatZeroPose(const Mat34& recenterBase, const SeatOffset& offset);

// Converts a pilot-space viewpoint offset (e.g. +Y = raise eye level in cockpit)
// into the SteamVR SeatedZeroPoseToRawTrackingPose matrix.
Mat34 pilotOffsetToSeatedZeroPose(const Mat34& recenterBase, const SeatOffset& pilotOffset);

// Axis shaping for virtual stick / throttle.
float applyDeadzoneAndExpo(float input, float deadzone, float expo);

struct VtolStickConfig {
    float rangeM = 0.10f;
    float deadzone = 0.04f;
    float expo = 0.25f;
    float maxTwistDeg = 30.0f;
    bool useWristTwistRudder = true;
};

struct VtolStickOutput {
    float rollX = 0.0f;
    float pitchY = 0.0f;
    float rudderRz = 0.0f;
    float deflectionMag = 0.0f;
};

struct VtolThrottleConfig {
    float rangeM = 0.16f;
};

VtolStickOutput computeVtolStickInput(
    const Mat34& grabStartPose,
    const Mat34& currentPose,
    float referenceYawDeg,
    const VtolStickConfig& cfg) noexcept;

float computeVtolThrottleInput(
    float grabStartThrottle,
    const Vec3& grabStartPos,
    const Vec3& currentPos,
    float referenceYawDeg,
    const VtolThrottleConfig& cfg) noexcept;

class OneEuroFilter {
public:
    OneEuroFilter(float minCutoff = 1.0f, float beta = 0.05f, float dCutoff = 1.0f);

    float filter(float value, float dtSeconds);
    void reset();

    void setParams(float minCutoff, float beta, float dCutoff = 1.0f);

private:
    static float alpha(float cutoff, float dtSeconds);

    float minCutoff_;
    float beta_;
    float dCutoff_;
    bool initialized_ = false;
    float xPrev_ = 0.0f;
    float dxPrev_ = 0.0f;
};

std::optional<float> intersectRayBox(const Ray& ray, const Vec3& center, const Vec3& halfExtents);
std::optional<float> intersectRaySphere(const Ray& ray, const Vec3& center, float radius);
std::optional<HudQuadHit> intersectRayHudQuad(
    const Ray& ray,
    const Mat34& quadPose,
    float widthM,
    float heightM) noexcept;

} // namespace il2vr
