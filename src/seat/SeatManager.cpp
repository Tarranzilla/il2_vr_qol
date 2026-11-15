#include "seat/SeatManager.h"

#include <nlohmann/json.hpp>

#include <fstream>

namespace il2vr {

SeatManager::~SeatManager() {
    restoreOriginalPose();
}

void SeatManager::attachBackend(IVrBackend* backend) {
    backend_ = backend;
    if (backend_ != nullptr && backend_->isInitialized() && !originalBackup_.has_value()) {
        captureOriginalBackup();
    }
}

bool SeatManager::captureOriginalBackup() {
    if (backend_ == nullptr || !backend_->isInitialized()) {
        return false;
    }
    auto current = backend_->getSeatedZeroPose();
    if (!current.has_value()) {
        return false;
    }
    originalBackup_ = *current;
    recenterBase_ = *current;
    return true;
}

bool SeatManager::restoreOriginalPose() {
    if (!originalBackup_.has_value() || backend_ == nullptr || !backend_->isInitialized()) {
        return false;
    }
    const bool ok = backend_->setSeatedZeroPose(*originalBackup_, true);
    if (ok) {
        recenterBase_ = *originalBackup_;
        offset_ = SeatOffset{};
    }
    return ok;
}

void SeatManager::setInitialHmdRawPose(const Mat34& rawHmdPose) {
    const Vec3 pos{rawHmdPose.m[0][3], rawHmdPose.m[1][3], rawHmdPose.m[2][3]};
    initialHmdRawPose_ = makeHorizontalPose(pos, extractYawDeg(rawHmdPose));
    hasInitialHmdRawPose_ = true;
}

bool SeatManager::recenterFromRawHmdPose(const Mat34& currentHmdRawPose) {
    const Vec3 curPos{currentHmdRawPose.m[0][3], currentHmdRawPose.m[1][3], currentHmdRawPose.m[2][3]};
    const float curYaw = extractYawDeg(currentHmdRawPose);
    const Mat34 curHoriz = makeHorizontalPose(curPos, curYaw);

    if (originalBackup_.has_value()) {
        if (!hasInitialHmdRawPose_) {
            initialHmdRawPose_ = curHoriz;
            hasInitialHmdRawPose_ = true;
        }
        const Mat34 initHorizInv = invertRigid(initialHmdRawPose_);
        const Vec3 origPos{originalBackup_->m[0][3], originalBackup_->m[1][3], originalBackup_->m[2][3]};
        const float origYaw = extractYawDeg(*originalBackup_);
        const Mat34 origHoriz = makeHorizontalPose(origPos, origYaw);
        recenterBase_ = multiply(multiply(curHoriz, initHorizInv), origHoriz);
    } else {
        recenterBase_ = curHoriz;
    }

    offset_ = SeatOffset{};
    return applyCurrentPose();
}

bool SeatManager::recenterNow() {
    if (backend_ == nullptr || !backend_->isInitialized()) {
        return false;
    }
    const VrFrameState frame = backend_->pollState();
    if (!frame.hmdValid) {
        return false;
    }
    if (!originalBackup_.has_value()) {
        captureOriginalBackup();
    }
    if (!hasInitialHmdRawPose_) {
        setInitialHmdRawPose(frame.hmdRawPose);
    }
    return recenterFromRawHmdPose(frame.hmdRawPose);
}

void SeatManager::nudge(float dx, float dy, float dz, float dYawDeg) {
    offset_.x += dx;
    offset_.y += dy;
    offset_.z += dz;
    offset_.yawDeg += dYawDeg;
    applyCurrentPose();
}

void SeatManager::nudgeStep(int dirX, int dirY, int dirZ, int dirYaw, bool fine) {
    const float tStep = fine ? steps_.fineTranslationM : steps_.coarseTranslationM;
    const float yStep = fine ? steps_.fineYawDeg : steps_.coarseYawDeg;
    nudge(
        static_cast<float>(dirX) * tStep,
        static_cast<float>(dirY) * tStep,
        static_cast<float>(dirZ) * tStep,
        static_cast<float>(dirYaw) * yStep);
}

void SeatManager::setOffset(const SeatOffset& offset) {
    offset_ = offset;
    applyCurrentPose();
}

void SeatManager::resetOffset() {
    offset_ = SeatOffset{};
    applyCurrentPose();
}

bool SeatManager::applyCurrentPose() {
    if (backend_ == nullptr || !backend_->isInitialized()) {
        return true;
    }
    const Mat34 target = pilotOffsetToSeatedZeroPose(recenterBase_, offset_);
    return backend_->setSeatedZeroPose(target, true);
}

bool SeatManager::saveProfile(const std::filesystem::path& path) const {
    if (path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
    }

    const Vec3 basePos{recenterBase_.m[0][3], recenterBase_.m[1][3], recenterBase_.m[2][3]};
    const float baseYaw = extractYawDeg(recenterBase_);

    nlohmann::json j = {
        {"schemaVersion", 1},
        {"profile", "default"},
        {"universe", "standing"},
        {"recenterBase", {
            {"position", {basePos.x, basePos.y, basePos.z}},
            {"yawDeg", baseYaw}
        }},
        {"offset", {
            {"x", offset_.x},
            {"y", offset_.y},
            {"z", offset_.z},
            {"yawDeg", offset_.yawDeg}
        }},
        {"steps", {
            {"coarse", {{"translationM", steps_.coarseTranslationM}, {"yawDeg", steps_.coarseYawDeg}}},
            {"fine", {{"translationM", steps_.fineTranslationM}, {"yawDeg", steps_.fineYawDeg}}}
        }}
    };

    std::ofstream out(path);
    if (!out.is_open()) {
        return false;
    }
    out << j.dump(2) << '\n';
    return out.good();
}

bool SeatManager::loadProfile(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return false;
    }

    nlohmann::json j;
    try {
        in >> j;
    } catch (...) {
        return false;
    }

    if (j.contains("offset") && j["offset"].is_object()) {
        const auto& off = j["offset"];
        offset_.x = off.value("x", 0.0f);
        offset_.y = off.value("y", 0.0f);
        offset_.z = off.value("z", 0.0f);
        offset_.yawDeg = off.value("yawDeg", 0.0f);
    }

    if (j.contains("recenterBase") && j["recenterBase"].is_object()) {
        const auto& rb = j["recenterBase"];
        Vec3 pos{};
        if (rb.contains("position") && rb["position"].is_array() && rb["position"].size() == 3) {
            pos.x = rb["position"][0].get<float>();
            pos.y = rb["position"][1].get<float>();
            pos.z = rb["position"][2].get<float>();
        }
        const float yawDeg = rb.value("yawDeg", 0.0f);
        recenterBase_ = makeHorizontalPose(pos, yawDeg);
    }

    if (j.contains("steps") && j["steps"].is_object()) {
        const auto& st = j["steps"];
        if (st.contains("coarse") && st["coarse"].is_object()) {
            steps_.coarseTranslationM = st["coarse"].value("translationM", steps_.coarseTranslationM);
            steps_.coarseYawDeg = st["coarse"].value("yawDeg", steps_.coarseYawDeg);
        }
        if (st.contains("fine") && st["fine"].is_object()) {
            steps_.fineTranslationM = st["fine"].value("translationM", steps_.fineTranslationM);
            steps_.fineYawDeg = st["fine"].value("yawDeg", steps_.fineYawDeg);
        }
    }

    return applyCurrentPose();
}

} // namespace il2vr
