#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace il2vr {

struct Il2TelemetrySnapshot {
    bool hasMotion = false;
    bool hasState = false;
    bool paused = false;
    bool frozen = false;

    uint32_t lastMotionTick = 0;
    uint32_t lastStateTick = 0;
    uint32_t identicalTickRun = 0;
    uint64_t motionPacketCount = 0;
    uint64_t statePacketCount = 0;

    std::string aircraftName;
    int engineCount = 0;
    float rpm[4]{};
    float manifoldPressurePa[4]{};
    float engineShakeFreq = 0.0f;
    float engineShakeAmp = 0.0f;
    float indicatedAirspeedMps = 0.0f;

    // Body-local linear acceleration [X lateral, Y longitudinal/vertical depending on source, Z] in m/s^2.
    float motionAcc[3]{};
    float stateAcc[3]{};

    float stallBuffetFreq = 0.0f;
    float stallBuffetAmp = 0.0f;
    float gearPos = 0.0f;
    float maxGearPressure = 0.0f;
    float flapsPos = 0.0f;
    float airBrakePos = 0.0f;

    // Accumulators updated per parsed StateAndEvents packet (caller can consume & clear each frame).
    uint32_t pendingGunShots = 0;
    uint8_t lastGunIndex = 0;
    uint32_t pendingOrdnanceReleases = 0;
    uint32_t pendingHits = 0;
    uint32_t pendingDamages = 0;
    float pendingPeakImpact = 0.0f;

    float maxRpm() const noexcept;
    float maxManifoldPressurePa() const noexcept;
    void clearPendingEvents() noexcept;
    void clearLiveTelemetryState() noexcept;
};

class Il2TelemetryParser {
public:
    static constexpr uint32_t kMotionPacketId = 0x494C0100u;
    static constexpr uint32_t kStateAndEventsPacketId = 0x54000101u;

    static bool feedPacket(const std::vector<uint8_t>& packet, Il2TelemetrySnapshot& snapshot);
};

// Safely ensures [KEY = motiondevice] and [KEY = telemetrydevice] are enabled in IL-2 data/startup.cfg,
// creating a one-time backup `startup.cfg.bak_il2vr` before editing.
std::string updateStartupCfgTelemetryText(const std::string& content, uint16_t port, bool& outChanged);
bool configureIl2StartupCfgTelemetry(
    const std::string& startupCfgPath,
    uint16_t port,
    bool& outModified,
    std::string& outStatus);

} // namespace il2vr
