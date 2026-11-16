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

TEST_CASE("Curved virtual mouse screen intersection and upright alignment", "[math][mouse]") {
    const il2vr::Vec3 center{0.1f, 1.0f, -0.85f};
    const il2vr::Mat34 tilted = il2vr::composeScreenPose(center, 15.0f, -18.0f, false);
    const il2vr::EulerAnglesDeg tiltedAng = il2vr::extractEulerAnglesDeg(tilted);
    CHECK(tiltedAng.yawDeg == Approx(15.0f).margin(1e-2f));
    CHECK(tiltedAng.pitchDeg == Approx(-18.0f).margin(1e-2f));

    const il2vr::Mat34 upright = il2vr::makeUprightPose(tilted);
    const il2vr::EulerAnglesDeg upAng = il2vr::extractEulerAnglesDeg(upright);
    CHECK(upAng.yawDeg == Approx(15.0f).margin(1e-2f));
    CHECK(upAng.pitchDeg == Approx(0.0f).margin(1e-3f));
    CHECK(upAng.rollDeg == Approx(0.0f).margin(1e-3f));
    CHECK(upright.m[0][3] == Approx(center.x).margin(1e-4f));
    CHECK(upright.m[1][3] == Approx(center.y).margin(1e-4f));
    CHECK(upright.m[2][3] == Approx(center.z).margin(1e-4f));

    const il2vr::Mat34 flatPose = il2vr::makeHorizontalPose({0.0f, 1.0f, -1.0f}, 0.0f);
    const il2vr::Ray rayCenter{{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}};
    const auto hitFlat = il2vr::intersectRayCurvedScreen(rayCenter, flatPose, 1.60f, 0.90f, 0.0f, 0.0f);
    const auto hitQuad = il2vr::intersectRayHudQuad(rayCenter, flatPose, 1.60f, 0.90f);
    REQUIRE(hitFlat.has_value());
    REQUIRE(hitQuad.has_value());
    CHECK(hitFlat->u == Approx(hitQuad->u).margin(1e-4f));
    CHECK(hitFlat->v == Approx(hitQuad->v).margin(1e-4f));

    const float curvature = 0.25f;
    const auto hitCurvedCenter = il2vr::intersectRayCurvedScreen(rayCenter, flatPose, 1.60f, 0.90f, curvature, 0.0f);
    REQUIRE(hitCurvedCenter.has_value());
    CHECK(hitCurvedCenter->u == Approx(0.5f).margin(1e-4f));
    CHECK(hitCurvedCenter->v == Approx(0.5f).margin(1e-4f));
    CHECK(hitCurvedCenter->worldPoint.z == Approx(-1.0f).margin(1e-4f));

    // Ray aimed at right border of curved cylinder (theta = +pi * curvature).
    constexpr float kPi = 3.14159265358979323846f;
    const float radius = 1.60f / (2.0f * kPi * curvature);
    const float thetaEdge = kPi * curvature;
    const float edgeX = radius * std::sin(thetaEdge);
    const float edgeZ = -1.0f + radius * (1.0f - std::cos(thetaEdge));
    const float len = std::sqrt(edgeX * edgeX + edgeZ * edgeZ);
    const il2vr::Ray rayEdge{{0.0f, 1.0f, 0.0f}, {edgeX / len, 0.0f, edgeZ / len}};
    const auto hitEdge = il2vr::intersectRayCurvedScreen(rayEdge, flatPose, 1.60f, 0.90f, curvature, 0.0f);
    REQUIRE(hitEdge.has_value());
    CHECK(hitEdge->u == Approx(1.0f).margin(1e-3f));
    CHECK(hitEdge->worldPoint.x == Approx(edgeX).margin(1e-3f));
    CHECK(hitEdge->worldPoint.z == Approx(edgeZ).margin(1e-3f));

    // Grab Bar geometry scales curvature proportionally so both share cylinder radius.
    const il2vr::MouseGrabBarGeometry bar = il2vr::computeMouseGrabBarGeometry(flatPose, 1.60f, 0.90f, curvature);
    CHECK(bar.pose.m[1][3] < flatPose.m[1][3] - 0.45f);
    CHECK(bar.curvature == Approx(curvature * (bar.widthM / 1.60f)).margin(1e-4f));
}

#include "haptics/HapticsMixer.h"
#include "telemetry/Il2TelemetryParser.h"

namespace {

void pushU16(std::vector<uint8_t>& buf, uint16_t v) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
    buf.insert(buf.end(), p, p + sizeof(v));
}

void pushU32(std::vector<uint8_t>& buf, uint32_t v) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
    buf.insert(buf.end(), p, p + sizeof(v));
}

void pushF32(std::vector<uint8_t>& buf, float v) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
    buf.insert(buf.end(), p, p + sizeof(v));
}

} // namespace

TEST_CASE("Il2TelemetryParser decodes Motion, StateAndEvents, and guards truncated input", "[telemetry]") {
    il2vr::Il2TelemetrySnapshot snap{};

    // Truncated / unknown packets fail safely without crashing.
    CHECK_FALSE(il2vr::Il2TelemetryParser::feedPacket({}, snap));
    CHECK_FALSE(il2vr::Il2TelemetryParser::feedPacket({1, 2, 3, 4, 5, 6, 7, 8}, snap));

    // Motion packet (44 bytes): packetId=0x494C0100, tick=120, 9 floats.
    std::vector<uint8_t> motionPkt;
    pushU32(motionPkt, il2vr::Il2TelemetryParser::kMotionPacketId);
    pushU32(motionPkt, 120);
    for (int i = 0; i < 6; ++i) {
        pushF32(motionPkt, 0.0f);
    }
    pushF32(motionPkt, 1.5f);
    pushF32(motionPkt, 2.5f);
    pushF32(motionPkt, 9.8f);
    REQUIRE(motionPkt.size() == 44);
    REQUIRE(il2vr::Il2TelemetryParser::feedPacket(motionPkt, snap));
    CHECK(snap.hasMotion);
    CHECK(snap.lastMotionTick == 120u);
    CHECK(snap.motionAcc[2] == Approx(9.8f).margin(1e-4f));

    // StateAndEvents packet: 3 states (RPM, IAS, StallBuffet) + 2 events (VehicleName, GunFired).
    std::vector<uint8_t> statePkt;
    pushU32(statePkt, il2vr::Il2TelemetryParser::kStateAndEventsPacketId);
    pushU16(statePkt, 64); // not paused
    pushU32(statePkt, 200);
    statePkt.push_back(3); // stateCount

    // State 0: RPM (1 engine = 2400.0)
    pushU16(statePkt, 0);
    statePkt.push_back(1);
    pushF32(statePkt, 2400.0f);

    // State 6: IAS (95.0 m/s)
    pushU16(statePkt, 6);
    statePkt.push_back(1);
    pushF32(statePkt, 95.0f);

    // State 9: StallBuffet (freq=18.0, amp=0.65)
    pushU16(statePkt, 9);
    statePkt.push_back(2);
    pushF32(statePkt, 18.0f);
    pushF32(statePkt, 0.65f);

    // Event count = 2
    statePkt.push_back(2);
    // Event 0: VehicleName = "Bf109"
    pushU16(statePkt, 0);
    statePkt.push_back(5);
    statePkt.insert(statePkt.end(), {'B', 'f', '1', '0', '9'});
    // Event 9: GunFired (gunIndex=1)
    pushU16(statePkt, 9);
    statePkt.push_back(1);
    statePkt.push_back(1);

    REQUIRE(il2vr::Il2TelemetryParser::feedPacket(statePkt, snap));
    CHECK(snap.hasState);
    CHECK_FALSE(snap.paused);
    CHECK(snap.maxRpm() == Approx(2400.0f).margin(1e-2f));
    CHECK(snap.indicatedAirspeedMps == Approx(95.0f).margin(1e-2f));
    CHECK(snap.stallBuffetAmp == Approx(0.65f).margin(1e-3f));
    CHECK(snap.aircraftName == "Bf109");
    CHECK(snap.pendingGunShots == 1u);
    CHECK(snap.lastGunIndex == 1);
}

TEST_CASE("startup.cfg telemetry block updater is idempotent and preserves existing settings", "[telemetry]") {
    const std::string original = "[KEY = graphics]\r\n\tfullscreen = 1\r\n[END]\r\n";
    bool changed = false;
    const std::string updated = il2vr::updateStartupCfgTelemetryText(original, 29373, changed);
    CHECK(changed);
    CHECK(updated.find("[KEY = motiondevice]") != std::string::npos);
    CHECK(updated.find("[KEY = telemetrydevice]") != std::string::npos);
    CHECK(updated.find("port = 29373") != std::string::npos);
    CHECK(updated.find("fullscreen = 1") != std::string::npos);

    bool secondChanged = true;
    const std::string again = il2vr::updateStartupCfgTelemetryText(updated, 29373, secondChanged);
    CHECK_FALSE(secondChanged);
    CHECK(again == updated);
}

TEST_CASE("HapticsMixer routes independent channels and respects master and UI priority", "[haptics]") {
    il2vr::HapticsMixer mixer;
    il2vr::Il2TelemetrySnapshot snap{};
    snap.hasState = true;
    snap.engineCount = 1;
    snap.rpm[0] = 2400.0f;
    snap.pendingGunShots = 1;

    // Default gunsHand=0 (Right): gun fire vibrates right hand only.
    auto out = mixer.update(snap, 0.8f, 0.011f);
    CHECK(out.gunsLevel > 0.5f);
    CHECK(out.rightDurationUs > 0);
    CHECK(out.leftDurationUs == 0);

    // Turning guns off while keeping buffet on isolates channels independently.
    snap.clearPendingEvents();
    auto cfg = mixer.config();
    cfg.gunsEnabled = false;
    cfg.buffetEnabled = true;
    mixer.setConfig(cfg);
    snap.stallBuffetAmp = 0.80f;
    for (int i = 0; i < 6; ++i) {
        out = mixer.update(snap, 0.8f, 0.016f);
    }
    CHECK(out.gunsLevel == 0.0f);
    CHECK(out.buffetLevel > 0.4f);
    CHECK(out.leftDurationUs > 0);
    CHECK(out.rightDurationUs > 0);

    // UI priority pulse suppresses telemetry pulses on that hand.
    mixer.notifyInteractionPulse(true, 0.05f);
    out = mixer.update(snap, 0.8f, 0.011f);
    CHECK(out.leftDurationUs == 0);
    CHECK(out.rightDurationUs > 0);

    // Master off silences both hands.
    cfg.masterEnabled = false;
    mixer.setConfig(cfg);
    out = mixer.update(snap, 0.8f, 0.011f);
    CHECK(out.leftDurationUs == 0);
    CHECK(out.rightDurationUs == 0);
}

TEST_CASE("HapticsMixer timed test buffet stops cleanly and never loops infinitely", "[haptics]") {
    il2vr::HapticsMixer mixer;
    il2vr::Il2TelemetrySnapshot emptySnap{};

    mixer.triggerTestBuffet(0.85f, 0.45f);
    REQUIRE(mixer.hasActiveTest());

    il2vr::HapticsFrameOutput out{};
    for (int i = 0; i < 10; ++i) {
        out = mixer.update(emptySnap, 0.0f, 0.020f);
    }
    CHECK(out.buffetLevel > 0.30f);
    CHECK(out.leftDurationUs > 0);
    CHECK(out.rightDurationUs > 0);

    // Advance past duration + decay tail: must return cleanly to 0 on both controllers.
    for (int i = 0; i < 40; ++i) {
        out = mixer.update(emptySnap, 0.0f, 0.025f);
    }
    CHECK_FALSE(mixer.hasActiveTest());
    CHECK(out.buffetLevel == 0.0f);
    CHECK(out.leftDurationUs == 0);
    CHECK(out.rightDurationUs == 0);
}

TEST_CASE("HapticsMixer feels takeoff ground-roll, high-G turns, and silences when frozen or cleared", "[haptics]") {
    il2vr::HapticsMixer mixer;
    il2vr::Il2TelemetrySnapshot snap{};
    snap.hasState = true;
    snap.engineCount = 1;
    snap.rpm[0] = 2200.0f;
    snap.maxGearPressure = 0.65f;
    snap.indicatedAirspeedMps = 28.0f;
    snap.stateAcc[0] = 0.0f;
    snap.stateAcc[1] = 9.81f;
    snap.stateAcc[2] = 0.0f;

    il2vr::HapticsFrameOutput out{};
    for (int i = 0; i < 12; ++i) {
        out = mixer.update(snap, 0.9f, 0.020f);
    }
    CHECK(out.buffetLevel > 0.15f);
    CHECK(out.leftDurationUs > 0);

    // Identical tick >= 8 marks snapshot frozen (menu/paused MP), silencing live telemetry.
    snap.frozen = true;
    for (int i = 0; i < 20; ++i) {
        out = mixer.update(snap, 0.9f, 0.025f);
    }
    CHECK(out.buffetLevel == 0.0f);
    CHECK(out.leftDurationUs == 0);
    CHECK(out.rightDurationUs == 0);

    snap.clearLiveTelemetryState();
    CHECK_FALSE(snap.hasState);
    CHECK(snap.maxRpm() == 0.0f);
    CHECK(snap.stallBuffetAmp == 0.0f);
}
