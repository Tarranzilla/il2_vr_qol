#include "core/MathTypes.h"
#include "output/KeyboardInjector.h"
#include "output/VJoyClient.h"
#include "seat/SeatManager.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <filesystem>

using Catch::Approx;

TEST_CASE("MathTypes horizontal pose and yaw extraction", "[math]") {
    const auto pose = il2vr::makeHorizontalPose({1.0f, 2.0f, -3.0f}, 45.0f);
    CHECK(pose.m[0][3] == Approx(1.0f));
    CHECK(pose.m[1][3] == Approx(2.0f));
    CHECK(pose.m[2][3] == Approx(-3.0f));
    CHECK(il2vr::extractYawDeg(pose) == Approx(45.0f).margin(0.01f));

    const auto negPose = il2vr::makeHorizontalPose({0.0f, 0.0f, 0.0f}, -120.0f);
    CHECK(il2vr::extractYawDeg(negPose) == Approx(-120.0f).margin(0.01f));
}

TEST_CASE("MathTypes seat pose composition and pilot offset inversion", "[math]") {
    const auto base = il2vr::makeHorizontalPose({0.5f, 1.1f, -0.4f}, 0.0f);
    const il2vr::SeatOffset offset{0.02f, 0.05f, -0.03f, 10.0f};

    const auto direct = il2vr::composeSeatZeroPose(base, offset);
    CHECK(direct.m[0][3] == Approx(0.52f).margin(1e-4f));
    CHECK(direct.m[1][3] == Approx(1.15f).margin(1e-4f));
    CHECK(direct.m[2][3] == Approx(-0.43f).margin(1e-4f));
    CHECK(il2vr::extractYawDeg(direct) == Approx(10.0f).margin(0.01f));

    // When pilotOffsetToSeatedZeroPose is applied and HMD stays at `base`,
    // the HMD's position in seated space must equal `offset`.
    const auto seatedToRaw = il2vr::pilotOffsetToSeatedZeroPose(base, offset);
    const auto hmdInSeated = il2vr::multiply(il2vr::invertRigid(seatedToRaw), base);
    CHECK(hmdInSeated.m[0][3] == Approx(offset.x).margin(1e-4f));
    CHECK(hmdInSeated.m[1][3] == Approx(offset.y).margin(1e-4f));
    CHECK(hmdInSeated.m[2][3] == Approx(offset.z).margin(1e-4f));
    CHECK(il2vr::extractYawDeg(hmdInSeated) == Approx(offset.yawDeg).margin(0.05f));
}

TEST_CASE("Deadzone and expo axis shaping", "[controls]") {
    CHECK(il2vr::applyDeadzoneAndExpo(0.01f, 0.05f, 0.3f) == Approx(0.0f));
    CHECK(il2vr::applyDeadzoneAndExpo(-0.05f, 0.05f, 0.3f) == Approx(0.0f));
    CHECK(il2vr::applyDeadzoneAndExpo(1.0f, 0.05f, 0.3f) == Approx(1.0f));
    CHECK(il2vr::applyDeadzoneAndExpo(-1.0f, 0.05f, 0.3f) == Approx(-1.0f));
    CHECK(il2vr::applyDeadzoneAndExpo(0.5f, 0.0f, 0.0f) == Approx(0.5f));
    CHECK(il2vr::applyDeadzoneAndExpo(0.5f, 0.0f, 1.0f) == Approx(0.125f));
}

TEST_CASE("OneEuroFilter smooths noisy step and resets", "[controls]") {
    il2vr::OneEuroFilter filter(1.0f, 0.05f, 1.0f);
    CHECK(filter.filter(0.0f, 0.011f) == Approx(0.0f));
    const float firstStep = filter.filter(1.0f, 0.011f);
    CHECK(firstStep > 0.0f);
    CHECK(firstStep < 1.0f);

    filter.reset();
    CHECK(filter.filter(0.5f, 0.011f) == Approx(0.5f));
}

TEST_CASE("Ray intersections for Box and Sphere", "[cockpit]") {
    const il2vr::Ray ray{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}};

    const auto boxHit = il2vr::intersectRayBox(ray, {0.0f, 0.0f, -1.0f}, {0.1f, 0.1f, 0.1f});
    REQUIRE(boxHit.has_value());
    CHECK(*boxHit == Approx(0.9f).margin(1e-4f));

    const auto boxMiss = il2vr::intersectRayBox(ray, {0.5f, 0.0f, -1.0f}, {0.1f, 0.1f, 0.1f});
    CHECK_FALSE(boxMiss.has_value());

    const auto sphereHit = il2vr::intersectRaySphere(ray, {0.0f, 0.0f, -2.0f}, 0.25f);
    REQUIRE(sphereHit.has_value());
    CHECK(*sphereHit == Approx(1.75f).margin(1e-4f));

    const auto sphereInside = il2vr::intersectRaySphere(ray, {0.0f, 0.0f, 0.0f}, 0.25f);
    REQUIRE(sphereInside.has_value());
    CHECK(*sphereInside == Approx(0.0f));

    const auto sphereMiss = il2vr::intersectRaySphere(ray, {0.0f, 1.0f, -2.0f}, 0.25f);
    CHECK_FALSE(sphereMiss.has_value());
}

TEST_CASE("Ray intersection against HUD quad", "[cockpit]") {
    const auto quadPose = il2vr::makeHorizontalPose({0.0f, 0.0f, -1.0f}, 0.0f);
    const float width = 0.8f;
    const float height = 0.4f;

    const auto centerHit = il2vr::intersectRayHudQuad(
        il2vr::Ray{{0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        quadPose,
        width,
        height);
    REQUIRE(centerHit.has_value());
    CHECK(centerHit->u == Approx(0.5f).margin(1e-4f));
    CHECK(centerHit->v == Approx(0.5f).margin(1e-4f));
    CHECK(centerHit->distance == Approx(1.0f).margin(1e-4f));
    CHECK(centerHit->worldPoint.z == Approx(-1.0f).margin(1e-4f));

    const auto topRightHit = il2vr::intersectRayHudQuad(
        il2vr::Ray{{0.2f, 0.1f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        quadPose,
        width,
        height);
    REQUIRE(topRightHit.has_value());
    CHECK(topRightHit->u == Approx(0.75f).margin(1e-4f));
    CHECK(topRightHit->v == Approx(0.25f).margin(1e-4f));

    const auto cornerMiss = il2vr::intersectRayHudQuad(
        il2vr::Ray{{0.5f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},
        quadPose,
        width,
        height);
    CHECK_FALSE(cornerMiss.has_value());

    const auto behindMiss = il2vr::intersectRayHudQuad(
        il2vr::Ray{{0.0f, 0.0f, -2.0f}, {0.0f, 0.0f, -1.0f}},
        quadPose,
        width,
        height);
    CHECK_FALSE(behindMiss.has_value());
}

TEST_CASE("SeatManager recenterFromRawHmdPose resets accumulated offset", "[seat]") {
    il2vr::SeatManager seat;
    seat.nudgeStep(2, 1, -1, 3, false);
    REQUIRE(seat.offset().yawDeg == Approx(15.0f).margin(1e-4f));
    REQUIRE(seat.offset().x == Approx(0.04f).margin(1e-5f));

    const auto headPose = il2vr::makeHorizontalPose({0.3f, 1.2f, -0.5f}, -45.0f);
    REQUIRE(seat.recenterFromRawHmdPose(headPose));
    CHECK(seat.offset().x == Approx(0.0f));
    CHECK(seat.offset().y == Approx(0.0f));
    CHECK(seat.offset().z == Approx(0.0f));
    CHECK(seat.offset().yawDeg == Approx(0.0f));
    CHECK(seat.recenterBase().m[0][3] == Approx(0.3f).margin(1e-5f));
    CHECK(il2vr::extractYawDeg(seat.recenterBase()) == Approx(-45.0f).margin(0.01f));
}

TEST_CASE("SeatManager nudge steps and JSON persistence", "[seat]") {
    il2vr::SeatManager seat;
    seat.nudgeStep(1, -1, 0, 2, false); // coarse: +2cm X, -2cm Y, +10deg Yaw
    seat.nudgeStep(0, 1, -1, 0, true);  // fine: +2mm Y, -2mm Z

    CHECK(seat.offset().x == Approx(0.02f).margin(1e-5f));
    CHECK(seat.offset().y == Approx(-0.018f).margin(1e-5f));
    CHECK(seat.offset().z == Approx(-0.002f).margin(1e-5f));
    CHECK(seat.offset().yawDeg == Approx(10.0f).margin(1e-4f));

    const auto tempFile = std::filesystem::temp_directory_path() / "il2vr_unit_seat.json";
    REQUIRE(seat.saveProfile(tempFile));

    il2vr::SeatManager loaded;
    REQUIRE(loaded.loadProfile(tempFile));
    CHECK(loaded.offset().x == Approx(seat.offset().x).margin(1e-5f));
    CHECK(loaded.offset().y == Approx(seat.offset().y).margin(1e-5f));
    CHECK(loaded.offset().z == Approx(seat.offset().z).margin(1e-5f));
    CHECK(loaded.offset().yawDeg == Approx(seat.offset().yawDeg).margin(1e-4f));

    loaded.resetOffset();
    CHECK(loaded.offset().x == Approx(0.0f));
    CHECK(loaded.offset().y == Approx(0.0f));

    std::error_code ec;
    std::filesystem::remove(tempFile, ec);
}

TEST_CASE("VJoyClient axis normalization", "[output]") {
    CHECK(il2vr::VJoyClient::normalizedToVJoyAxis(-1.0f) == 1);
    CHECK(il2vr::VJoyClient::normalizedToVJoyAxis(0.0f) == 16384);
    CHECK(il2vr::VJoyClient::normalizedToVJoyAxis(1.0f) == 32768);
    CHECK(il2vr::VJoyClient::normalizedToVJoyAxis(2.5f) == 32768);
    CHECK(il2vr::VJoyClient::normalizedToVJoyAxis(-2.5f) == 1);

    CHECK(il2vr::VJoyClient::throttleNormalizedToVJoyAxis(0.0f) == 1);
    CHECK(il2vr::VJoyClient::throttleNormalizedToVJoyAxis(1.0f) == 32768);
}

TEST_CASE("KeyboardInjector key resolution", "[output]") {
    const auto g = il2vr::KeyboardInjector::resolveKey("G");
    REQUIRE(g.has_value());
    CHECK(g->scanCode != 0);
    CHECK_FALSE(g->extended);

    const auto ralt = il2vr::KeyboardInjector::resolveKey("RAlt");
    REQUIRE(ralt.has_value());
    CHECK(ralt->scanCode != 0);
    CHECK(ralt->extended);

    const auto rctrl = il2vr::KeyboardInjector::resolveKey("RCtrl");
    REQUIRE(rctrl.has_value());
    CHECK(rctrl->scanCode != 0);
    CHECK(rctrl->extended);

    CHECK_FALSE(il2vr::KeyboardInjector::resolveKey("NonExistentKeyXYZ").has_value());
}

TEST_CASE("SeatManager relative recenter preserves original zero pose without 180 flip", "[seat]") {
    class MockVrBackend final : public il2vr::IVrBackend {
    public:
        bool init(bool /*allowAutoLaunch*/) override { initialized = true; return true; }
        void shutdown() override { initialized = false; }
        bool isInitialized() const override { return initialized; }
        std::string lastError() const override { return {}; }
        il2vr::VrFrameState pollState() override { return frame; }
        std::optional<il2vr::Mat34> getSeatedZeroPose() override { return zeroPose; }
        bool setSeatedZeroPose(const il2vr::Mat34& pose, bool /*commitLive*/) override {
            zeroPose = pose;
            return true;
        }
        void triggerHapticPulse(il2vr::VrHand /*hand*/, uint16_t /*durationMicroSec*/) override {}

        bool initialized = true;
        il2vr::Mat34 zeroPose = il2vr::makeHorizontalPose({-0.2f, 0.0f, 0.4f}, -18.5f);
        il2vr::VrFrameState frame{};
    };

    MockVrBackend vr;
    const il2vr::Mat34 initialZero = vr.zeroPose;
    const il2vr::Mat34 initialHmdRaw = il2vr::makeHorizontalPose({0.1f, 1.25f, -0.3f}, 15.0f);

    il2vr::SeatManager seat;
    seat.attachBackend(&vr);
    REQUIRE(seat.hasOriginalBackup());
    seat.setInitialHmdRawPose(initialHmdRaw);

    // Recentering without moving the head must preserve the exact standing zero pose (no 180 deg flip).
    REQUIRE(seat.recenterFromRawHmdPose(initialHmdRaw));
    CHECK(il2vr::extractYawDeg(vr.zeroPose) == Approx(-18.5f).margin(0.05f));
    CHECK(vr.zeroPose.m[0][3] == Approx(initialZero.m[0][3]).margin(1e-4f));
    CHECK(vr.zeroPose.m[1][3] == Approx(initialZero.m[1][3]).margin(1e-4f));
    CHECK(vr.zeroPose.m[2][3] == Approx(initialZero.m[2][3]).margin(1e-4f));

    // Moving HMD and recentering must map the new physical HMD pose to the exact same standing pose.
    const il2vr::Mat34 movedHmdRaw = il2vr::makeHorizontalPose({0.35f, 1.40f, -0.55f}, 40.0f);
    REQUIRE(seat.recenterFromRawHmdPose(movedHmdRaw));

    const il2vr::Mat34 expectedInStanding = il2vr::multiply(il2vr::invertRigid(initialZero), initialHmdRaw);
    const il2vr::Mat34 actualInStanding = il2vr::multiply(il2vr::invertRigid(vr.zeroPose), movedHmdRaw);
    CHECK(actualInStanding.m[0][3] == Approx(expectedInStanding.m[0][3]).margin(1e-4f));
    CHECK(actualInStanding.m[1][3] == Approx(expectedInStanding.m[1][3]).margin(1e-4f));
    CHECK(actualInStanding.m[2][3] == Approx(expectedInStanding.m[2][3]).margin(1e-4f));
    CHECK(il2vr::extractYawDeg(actualInStanding) == Approx(il2vr::extractYawDeg(expectedInStanding)).margin(0.05f));

    // Prevent SeatManager destructor from running commit during teardown assertions (restore returns true cleanly).
}

TEST_CASE("VTOL VR 6DoF stick input mapping and pilot frame orientation", "[controls]") {
    const il2vr::VtolStickConfig cfg{0.10f, 0.0f, 0.0f, 30.0f, true};
    const il2vr::Mat34 startPose = il2vr::makeHorizontalPose({0.2f, 0.8f, -0.3f}, 0.0f);

    // Full pull back (+Z in OpenVR) -> +1.0 Pitch, 0 Roll.
    const il2vr::Mat34 pullBack = il2vr::makeHorizontalPose({0.2f, 0.8f, -0.2f}, 0.0f);
    const auto pullOut = il2vr::computeVtolStickInput(startPose, pullBack, 0.0f, cfg);
    CHECK(pullOut.rollX == Approx(0.0f).margin(1e-4f));
    CHECK(pullOut.pitchY == Approx(1.0f).margin(1e-4f));
    CHECK(pullOut.rudderRz == Approx(0.0f).margin(1e-4f));

    // Half push right (+0.05 m X) and twist wrist clockwise (-15 deg yaw) -> +0.5 Roll, +0.5 Rudder.
    const il2vr::Mat34 rollTwist = il2vr::makeHorizontalPose({0.25f, 0.8f, -0.3f}, -15.0f);
    const auto twistOut = il2vr::computeVtolStickInput(startPose, rollTwist, 0.0f, cfg);
    CHECK(twistOut.rollX == Approx(0.5f).margin(1e-4f));
    CHECK(twistOut.pitchY == Approx(0.0f).margin(1e-4f));
    CHECK(twistOut.rudderRz == Approx(0.5f).margin(1e-3f));

    // Rotated pilot facing +90 deg CCW (toward -X): pilot's right is world -Z.
    const il2vr::Mat34 pilotRightWorldMinusZ = il2vr::makeHorizontalPose({0.2f, 0.8f, -0.4f}, 0.0f);
    const auto rotOut = il2vr::computeVtolStickInput(startPose, pilotRightWorldMinusZ, 90.0f, cfg);
    CHECK(rotOut.rollX == Approx(1.0f).margin(1e-4f));
    CHECK(rotOut.pitchY == Approx(0.0f).margin(1e-4f));
}

TEST_CASE("VTOL VR latched virtual throttle lever mapping", "[controls]") {
    const il2vr::VtolThrottleConfig cfg{0.16f};
    const il2vr::Vec3 startPos{-0.25f, 0.85f, -0.20f};

    // Push forward (-Z at yaw 0) by 8 cm from 40% -> +50% -> 90%.
    CHECK(il2vr::computeVtolThrottleInput(0.40f, startPos, {-0.25f, 0.85f, -0.28f}, 0.0f, cfg)
        == Approx(0.90f).margin(1e-4f));

    // Pull back (+Z at yaw 0) by 16 cm from 40% -> clamps at 0%.
    CHECK(il2vr::computeVtolThrottleInput(0.40f, startPos, {-0.25f, 0.85f, -0.04f}, 0.0f, cfg)
        == Approx(0.0f).margin(1e-4f));

    // When pilot faces -X (yaw +90 deg), pushing forward (-X) by 8 cm from 20% -> 70%.
    CHECK(il2vr::computeVtolThrottleInput(0.20f, startPos, {-0.33f, 0.85f, -0.20f}, 90.0f, cfg)
        == Approx(0.70f).margin(1e-4f));
}

#include "core/LocaleManager.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <set>

TEST_CASE("TripleClickDetector detects 3 rapid presses and resets on timeout", "[controls]") {
    il2vr::TripleClickDetector detector(0.45f);

    CHECK_FALSE(detector.update(true, 0.02f));
    CHECK(detector.clickCount() == 1);
    // Holding button down must not trigger extra clicks.
    CHECK_FALSE(detector.update(true, 0.10f));
    CHECK(detector.clickCount() == 1);

    CHECK_FALSE(detector.update(false, 0.05f));
    CHECK_FALSE(detector.update(true, 0.10f));
    CHECK(detector.clickCount() == 2);
    CHECK_FALSE(detector.update(false, 0.05f));

    // Third click within window triggers once and resets count.
    CHECK(detector.update(true, 0.10f));
    CHECK(detector.clickCount() == 0);
    CHECK_FALSE(detector.update(false, 0.05f));

    // Two clicks followed by timeout resets instead of triggering on third delayed click.
    CHECK_FALSE(detector.update(true, 0.05f));
    CHECK_FALSE(detector.update(false, 0.05f));
    CHECK_FALSE(detector.update(true, 0.05f));
    CHECK_FALSE(detector.update(false, 0.05f));
    CHECK(detector.clickCount() == 2);
    CHECK_FALSE(detector.update(true, 0.50f));
    CHECK(detector.clickCount() == 1);
}

TEST_CASE("Euler angle extraction and anchored throttle mapping", "[math][controls]") {
    const il2vr::EulerAnglesDeg idAng = il2vr::extractEulerAnglesDeg(il2vr::Mat34::identity());
    CHECK(idAng.pitchDeg == Approx(0.0f).margin(1e-3f));
    CHECK(idAng.yawDeg == Approx(0.0f).margin(1e-3f));
    CHECK(idAng.rollDeg == Approx(0.0f).margin(1e-3f));

    const il2vr::Mat34 yaw90 = il2vr::makeHorizontalPose({0.0f, 1.0f, 0.0f}, 90.0f);
    const il2vr::EulerAnglesDeg yawAng = il2vr::extractEulerAnglesDeg(yaw90);
    CHECK(yawAng.yawDeg == Approx(90.0f).margin(1e-3f));
    CHECK(yawAng.pitchDeg == Approx(0.0f).margin(1e-3f));
    CHECK(yawAng.rollDeg == Approx(0.0f).margin(1e-3f));

    il2vr::VtolThrottleConfig cfg;
    cfg.rangeM = 0.16f;
    const il2vr::Vec3 anchor{-0.24f, 0.55f, -0.25f};
    CHECK(il2vr::computeAnchoredThrottleInput(anchor, anchor, 0.0f, cfg) == Approx(0.50f).margin(1e-4f));
    CHECK(il2vr::computeAnchoredThrottleInput(anchor, {-0.24f, 0.55f, -0.33f}, 0.0f, cfg) == Approx(1.0f).margin(1e-4f));
    CHECK(il2vr::computeAnchoredThrottleInput(anchor, {-0.24f, 0.55f, -0.17f}, 0.0f, cfg) == Approx(0.0f).margin(1e-4f));
    CHECK(il2vr::computeAnchoredThrottleInput(anchor, {-0.28f, 0.55f, -0.25f}, 90.0f, cfg) == Approx(0.75f).margin(1e-4f));
}

TEST_CASE("LocaleManager loads pt-BR and en-US with 100% key parity", "[i18n]") {
    const auto localesDir = il2vr::LocaleManager::findLocalesDirectory();
    REQUIRE_FALSE(localesDir.empty());

    il2vr::LocaleManager mgr;
    REQUIRE(mgr.discoverLocales(localesDir));
    REQUIRE(mgr.availableLocales().size() >= 2);

    REQUIRE(mgr.loadLocale("pt-BR"));
    CHECK(mgr.activeLocaleCode() == "pt-BR");
    CHECK(mgr.get("tab.languages", "") == "Idiomas");
    CHECK(mgr.get("missing.key", "fallback_val") == "fallback_val");

    REQUIRE(mgr.loadLocale("en-US"));
    CHECK(mgr.activeLocaleCode() == "en-US");
    CHECK(mgr.get("tab.languages", "") == "Languages");

    auto loadKeys = [](const std::filesystem::path& filePath) {
        std::ifstream in(filePath);
        REQUIRE(in.is_open());
        nlohmann::json root;
        in >> root;
        REQUIRE(root.contains("strings"));
        REQUIRE(root["strings"].is_object());
        std::set<std::string> keys;
        for (const auto& [k, v] : root["strings"].items()) {
            REQUIRE(v.is_string());
            REQUIRE_FALSE(v.get<std::string>().empty());
            keys.insert(k);
        }
        return keys;
    };

    const auto ptKeys = loadKeys(localesDir / "pt-BR.json");
    const auto enKeys = loadKeys(localesDir / "en-US.json");
    CHECK_FALSE(ptKeys.empty());
    CHECK(ptKeys == enKeys);
}
