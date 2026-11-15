#include "core/MathTypes.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace il2vr {

namespace {
constexpr float kPi = 3.14159265358979323846f;
constexpr float kDegToRad = kPi / 180.0f;
constexpr float kRadToDeg = 180.0f / kPi;

inline float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline Vec3 sub(const Vec3& a, const Vec3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

float wrapAngleDeg(float deg) {
    float d = std::fmod(deg + 180.0f, 360.0f);
    if (d < 0.0f) {
        d += 360.0f;
    }
    return d - 180.0f;
}
} // namespace

Mat34 Mat34::identity() {
    return Mat34{};
}

Mat34 multiply(const Mat34& a, const Mat34& b) {
    Mat34 r{};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            r.m[row][col] =
                a.m[row][0] * b.m[0][col] +
                a.m[row][1] * b.m[1][col] +
                a.m[row][2] * b.m[2][col];
        }
        r.m[row][3] =
            a.m[row][0] * b.m[0][3] +
            a.m[row][1] * b.m[1][3] +
            a.m[row][2] * b.m[2][3] +
            a.m[row][3];
    }
    return r;
}

Mat34 invertRigid(const Mat34& m) {
    Mat34 r{};
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            r.m[row][col] = m.m[col][row];
        }
    }
    const float tx = m.m[0][3];
    const float ty = m.m[1][3];
    const float tz = m.m[2][3];
    r.m[0][3] = -(r.m[0][0] * tx + r.m[0][1] * ty + r.m[0][2] * tz);
    r.m[1][3] = -(r.m[1][0] * tx + r.m[1][1] * ty + r.m[1][2] * tz);
    r.m[2][3] = -(r.m[2][0] * tx + r.m[2][1] * ty + r.m[2][2] * tz);
    return r;
}

Vec3 transformPoint(const Mat34& m, const Vec3& p) {
    return {
        m.m[0][0] * p.x + m.m[0][1] * p.y + m.m[0][2] * p.z + m.m[0][3],
        m.m[1][0] * p.x + m.m[1][1] * p.y + m.m[1][2] * p.z + m.m[1][3],
        m.m[2][0] * p.x + m.m[2][1] * p.y + m.m[2][2] * p.z + m.m[2][3]
    };
}

Vec3 transformDirection(const Mat34& m, const Vec3& d) {
    return {
        m.m[0][0] * d.x + m.m[0][1] * d.y + m.m[0][2] * d.z,
        m.m[1][0] * d.x + m.m[1][1] * d.y + m.m[1][2] * d.z,
        m.m[2][0] * d.x + m.m[2][1] * d.y + m.m[2][2] * d.z
    };
}

float extractYawDeg(const Mat34& m) {
    // Column 2 is local +Z (backward); forward is -col2.
    const float xzLenSq = m.m[0][2] * m.m[0][2] + m.m[2][2] * m.m[2][2];
    if (xzLenSq > 1e-6f) {
        return std::atan2(m.m[0][2], m.m[2][2]) * kRadToDeg;
    }
    // Fallback when looking nearly straight up/down: use local +X (column 0).
    return std::atan2(-m.m[2][0], m.m[0][0]) * kRadToDeg;
}

Mat34 makeHorizontalPose(const Vec3& pos, float yawDeg) {
    const float rad = yawDeg * kDegToRad;
    const float c = std::cos(rad);
    const float s = std::sin(rad);

    Mat34 r{};
    r.m[0][0] = c;
    r.m[0][1] = 0.0f;
    r.m[0][2] = s;
    r.m[0][3] = pos.x;

    r.m[1][0] = 0.0f;
    r.m[1][1] = 1.0f;
    r.m[1][2] = 0.0f;
    r.m[1][3] = pos.y;

    r.m[2][0] = -s;
    r.m[2][1] = 0.0f;
    r.m[2][2] = c;
    r.m[2][3] = pos.z;
    return r;
}

Mat34 composeSeatZeroPose(const Mat34& recenterBase, const SeatOffset& offset) {
    const Mat34 delta = makeHorizontalPose({offset.x, offset.y, offset.z}, offset.yawDeg);
    return multiply(recenterBase, delta);
}

Mat34 pilotOffsetToSeatedZeroPose(const Mat34& recenterBase, const SeatOffset& pilotOffset) {
    const Mat34 pilotDelta = makeHorizontalPose(
        {pilotOffset.x, pilotOffset.y, pilotOffset.z}, pilotOffset.yawDeg);
    return multiply(recenterBase, invertRigid(pilotDelta));
}

float applyDeadzoneAndExpo(float input, float deadzone, float expo) {
    const float clampedInput = std::clamp(input, -1.0f, 1.0f);
    const float dz = std::clamp(deadzone, 0.0f, 0.99f);
    const float ex = std::clamp(expo, 0.0f, 1.0f);

    const float absIn = std::abs(clampedInput);
    if (absIn <= dz) {
        return 0.0f;
    }

    const float sign = (clampedInput >= 0.0f) ? 1.0f : -1.0f;
    const float norm = std::clamp((absIn - dz) / (1.0f - dz), 0.0f, 1.0f);
    const float shaped = (1.0f - ex) * norm + ex * (norm * norm * norm);
    return sign * shaped;
}

VtolStickOutput computeVtolStickInput(
    const Mat34& grabStartPose,
    const Mat34& currentPose,
    float referenceYawDeg,
    const VtolStickConfig& cfg) noexcept {
    const float safeRangeM = std::max(cfg.rangeM, 0.01f);
    const float safeMaxTwistDeg = std::max(cfg.maxTwistDeg, 1.0f);

    const Mat34 refFrame = makeHorizontalPose({0.0f, 0.0f, 0.0f}, referenceYawDeg);
    const Vec3 rightAxis{refFrame.m[0][0], 0.0f, refFrame.m[2][0]};
    const Vec3 backAxis{refFrame.m[0][2], 0.0f, refFrame.m[2][2]};

    const Vec3 startPos{grabStartPose.m[0][3], grabStartPose.m[1][3], grabStartPose.m[2][3]};
    const Vec3 curPos{currentPose.m[0][3], currentPose.m[1][3], currentPose.m[2][3]};
    const Vec3 deltaWorld = sub(curPos, startPos);

    const float localRight = dot(deltaWorld, rightAxis);
    const float localBack = dot(deltaWorld, backAxis);

    const float rawRoll = std::clamp(localRight / safeRangeM, -1.0f, 1.0f);
    const float rawPitch = std::clamp(localBack / safeRangeM, -1.0f, 1.0f);

    VtolStickOutput out{};
    out.rollX = applyDeadzoneAndExpo(rawRoll, cfg.deadzone, cfg.expo);
    out.pitchY = applyDeadzoneAndExpo(rawPitch, cfg.deadzone, cfg.expo);

    if (cfg.useWristTwistRudder) {
        const float deltaYawDeg = wrapAngleDeg(extractYawDeg(currentPose) - extractYawDeg(grabStartPose));
        // OpenVR +yaw is CCW (left); clockwise wrist twist (right rudder) yields negative deltaYawDeg.
        const float rawYaw = std::clamp(-deltaYawDeg / safeMaxTwistDeg, -1.0f, 1.0f);
        out.rudderRz = applyDeadzoneAndExpo(rawYaw, cfg.deadzone, cfg.expo);
    }

    out.deflectionMag = std::clamp(
        std::sqrt(out.rollX * out.rollX + out.pitchY * out.pitchY),
        0.0f,
        1.0f);
    return out;
}

float computeVtolThrottleInput(
    float grabStartThrottle,
    const Vec3& grabStartPos,
    const Vec3& currentPos,
    float referenceYawDeg,
    const VtolThrottleConfig& cfg) noexcept {
    const float safeRangeM = std::max(cfg.rangeM, 0.01f);
    const Mat34 refFrame = makeHorizontalPose({0.0f, 0.0f, 0.0f}, referenceYawDeg);
    const Vec3 forwardAxis{-refFrame.m[0][2], 0.0f, -refFrame.m[2][2]};
    const Vec3 deltaWorld = sub(currentPos, grabStartPos);
    const float forwardDelta = dot(deltaWorld, forwardAxis);
    return std::clamp(grabStartThrottle + (forwardDelta / safeRangeM), 0.0f, 1.0f);
}

OneEuroFilter::OneEuroFilter(float minCutoff, float beta, float dCutoff)
    : minCutoff_(minCutoff), beta_(beta), dCutoff_(dCutoff) {}

float OneEuroFilter::alpha(float cutoff, float dtSeconds) {
    const float safeCutoff = std::max(cutoff, 1e-4f);
    const float tau = 1.0f / (2.0f * kPi * safeCutoff);
    return 1.0f / (1.0f + tau / dtSeconds);
}

float OneEuroFilter::filter(float value, float dtSeconds) {
    if (!initialized_ || dtSeconds <= 0.0f) {
        initialized_ = true;
        xPrev_ = value;
        dxPrev_ = 0.0f;
        return value;
    }

    const float dx = (value - xPrev_) / dtSeconds;
    const float aD = alpha(dCutoff_, dtSeconds);
    const float edx = aD * dx + (1.0f - aD) * dxPrev_;

    const float cutoff = minCutoff_ + beta_ * std::abs(edx);
    const float a = alpha(cutoff, dtSeconds);
    const float xHat = a * value + (1.0f - a) * xPrev_;

    xPrev_ = xHat;
    dxPrev_ = edx;
    return xHat;
}

void OneEuroFilter::reset() {
    initialized_ = false;
    xPrev_ = 0.0f;
    dxPrev_ = 0.0f;
}

void OneEuroFilter::setParams(float minCutoff, float beta, float dCutoff) {
    minCutoff_ = minCutoff;
    beta_ = beta;
    dCutoff_ = dCutoff;
}

std::optional<float> intersectRayBox(const Ray& ray, const Vec3& center, const Vec3& halfExtents) {
    if (halfExtents.x < 0.0f || halfExtents.y < 0.0f || halfExtents.z < 0.0f) {
        return std::nullopt;
    }

    float tMin = -std::numeric_limits<float>::infinity();
    float tMax = std::numeric_limits<float>::infinity();

    const float origins[3] = {ray.origin.x, ray.origin.y, ray.origin.z};
    const float dirs[3] = {ray.dir.x, ray.dir.y, ray.dir.z};
    const float centers[3] = {center.x, center.y, center.z};
    const float extents[3] = {halfExtents.x, halfExtents.y, halfExtents.z};

    for (int i = 0; i < 3; ++i) {
        const float boxMin = centers[i] - extents[i];
        const float boxMax = centers[i] + extents[i];
        if (std::abs(dirs[i]) < 1e-8f) {
            if (origins[i] < boxMin || origins[i] > boxMax) {
                return std::nullopt;
            }
        } else {
            const float invD = 1.0f / dirs[i];
            float t1 = (boxMin - origins[i]) * invD;
            float t2 = (boxMax - origins[i]) * invD;
            if (t1 > t2) {
                std::swap(t1, t2);
            }
            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            if (tMin > tMax) {
                return std::nullopt;
            }
        }
    }

    if (tMax < 0.0f) {
        return std::nullopt;
    }
    return (tMin >= 0.0f) ? tMin : 0.0f;
}

std::optional<float> intersectRaySphere(const Ray& ray, const Vec3& center, float radius) {
    if (radius < 0.0f) {
        return std::nullopt;
    }
    const float a = dot(ray.dir, ray.dir);
    if (a < 1e-12f) {
        return std::nullopt;
    }
    const Vec3 oc = sub(ray.origin, center);
    const float b = dot(oc, ray.dir);
    const float c = dot(oc, oc) - radius * radius;
    const float disc = b * b - a * c;
    if (disc < 0.0f) {
        return std::nullopt;
    }

    const float sqrtDisc = std::sqrt(disc);
    const float t0 = (-b - sqrtDisc) / a;
    const float t1 = (-b + sqrtDisc) / a;
    if (t1 < 0.0f) {
        return std::nullopt;
    }
    return (t0 >= 0.0f) ? t0 : 0.0f;
}

std::optional<HudQuadHit> intersectRayHudQuad(
    const Ray& ray,
    const Mat34& quadPose,
    float widthM,
    float heightM) noexcept {
    if (widthM <= 0.0f || heightM <= 0.0f) {
        return std::nullopt;
    }

    const Vec3 center{quadPose.m[0][3], quadPose.m[1][3], quadPose.m[2][3]};
    const Vec3 axisX{quadPose.m[0][0], quadPose.m[1][0], quadPose.m[2][0]};
    const Vec3 axisY{quadPose.m[0][1], quadPose.m[1][1], quadPose.m[2][1]};
    const Vec3 axisZ{quadPose.m[0][2], quadPose.m[1][2], quadPose.m[2][2]};

    const float denom = dot(ray.dir, axisZ);
    if (std::fabs(denom) < 1e-6f) {
        return std::nullopt;
    }

    const float t = dot(sub(center, ray.origin), axisZ) / denom;
    if (t <= 0.01f || t > 5.0f) {
        return std::nullopt;
    }

    const Vec3 hitPoint{
        ray.origin.x + ray.dir.x * t,
        ray.origin.y + ray.dir.y * t,
        ray.origin.z + ray.dir.z * t
    };
    const Vec3 rel = sub(hitPoint, center);
    const float localX = dot(rel, axisX);
    const float localY = dot(rel, axisY);

    const float u = (localX / widthM) + 0.5f;
    const float v = 0.5f - (localY / heightM);
    if (u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) {
        return std::nullopt;
    }

    return HudQuadHit{u, v, t, hitPoint};
}

TripleClickDetector::TripleClickDetector(float maxIntervalSec) noexcept
    : maxIntervalSec_(std::max(maxIntervalSec, 0.05f)) {}

bool TripleClickDetector::update(bool pressed, float dtSeconds) noexcept {
    if (count_ > 0) {
        elapsedSinceLastClick_ += std::max(dtSeconds, 0.0f);
        if (elapsedSinceLastClick_ > maxIntervalSec_) {
            count_ = 0;
            elapsedSinceLastClick_ = 0.0f;
        }
    }

    bool triggered = false;
    if (pressed && !prevPressed_) {
        ++count_;
        elapsedSinceLastClick_ = 0.0f;
        if (count_ >= 3) {
            triggered = true;
            count_ = 0;
        }
    }

    prevPressed_ = pressed;
    return triggered;
}

void TripleClickDetector::reset() noexcept {
    count_ = 0;
    elapsedSinceLastClick_ = 0.0f;
    prevPressed_ = false;
}

EulerAnglesDeg extractEulerAnglesDeg(const Mat34& m) noexcept {
    EulerAnglesDeg out{};
    out.pitchDeg = std::asin(std::clamp(-m.m[1][2], -1.0f, 1.0f)) * kRadToDeg;
    out.yawDeg = extractYawDeg(m);
    if (std::fabs(m.m[1][0]) > 1e-5f || std::fabs(m.m[1][1]) > 1e-5f) {
        out.rollDeg = std::atan2(-m.m[1][0], m.m[1][1]) * kRadToDeg;
    }
    return out;
}

float computeAnchoredThrottleInput(
    const Vec3& anchorPos,
    const Vec3& currentPos,
    float referenceYawDeg,
    const VtolThrottleConfig& cfg) noexcept {
    return computeVtolThrottleInput(0.50f, anchorPos, currentPos, referenceYawDeg, cfg);
}

} // namespace il2vr
