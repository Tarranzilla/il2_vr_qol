#pragma once

#include "core/MathTypes.h"
#include "vr/IVrBackend.h"

#include <filesystem>
#include <optional>

namespace il2vr {

struct SeatSteps {
    float coarseTranslationM = 0.02f;  // 2 cm
    float coarseYawDeg = 5.0f;         // 5 deg
    float fineTranslationM = 0.002f;   // 2 mm
    float fineYawDeg = 0.5f;           // 0.5 deg
};

class SeatManager {
public:
    SeatManager() = default;
    ~SeatManager();

    SeatManager(const SeatManager&) = delete;
    SeatManager& operator=(const SeatManager&) = delete;

    void attachBackend(IVrBackend* backend);
    bool captureOriginalBackup();
    bool restoreOriginalPose();

    void setInitialHmdRawPose(const Mat34& rawHmdPose);
    bool hasInitialHmdRawPose() const { return hasInitialHmdRawPose_; }

    bool recenterFromRawHmdPose(const Mat34& currentHmdRawPose);
    bool recenterNow();

    void nudge(float dx, float dy, float dz, float dYawDeg);
    void nudgeStep(int dirX, int dirY, int dirZ, int dirYaw, bool fine);
    void setOffset(const SeatOffset& offset);
    void resetOffset();
    bool applyCurrentPose();

    const Mat34& recenterBase() const { return recenterBase_; }
    const SeatOffset& offset() const { return offset_; }
    const SeatSteps& steps() const { return steps_; }
    void setSteps(const SeatSteps& steps) { steps_ = steps; }
    bool hasOriginalBackup() const { return originalBackup_.has_value(); }

    bool saveProfile(const std::filesystem::path& path) const;
    bool loadProfile(const std::filesystem::path& path);

private:
    IVrBackend* backend_ = nullptr;
    Mat34 recenterBase_ = Mat34::identity();
    Mat34 initialHmdRawPose_ = Mat34::identity();
    bool hasInitialHmdRawPose_ = false;
    SeatOffset offset_{};
    SeatSteps steps_{};
    std::optional<Mat34> originalBackup_;
};

} // namespace il2vr
