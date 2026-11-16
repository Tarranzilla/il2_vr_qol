#include "telemetry/Il2TelemetryParser.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace il2vr {

namespace {

uint16_t readU16(const uint8_t* p) noexcept {
    uint16_t v = 0;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

uint32_t readU32(const uint8_t* p) noexcept {
    uint32_t v = 0;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

float readF32(const uint8_t* p) noexcept {
    float v = 0.0f;
    std::memcpy(&v, p, sizeof(v));
    return std::isfinite(v) ? v : 0.0f;
}

} // namespace

float Il2TelemetrySnapshot::maxRpm() const noexcept {
    float best = 0.0f;
    const int n = std::clamp(engineCount, 0, 4);
    for (int i = 0; i < n; ++i) {
        best = std::max(best, rpm[i]);
    }
    return best;
}

float Il2TelemetrySnapshot::maxManifoldPressurePa() const noexcept {
    float best = 0.0f;
    const int n = std::clamp(engineCount, 0, 4);
    for (int i = 0; i < n; ++i) {
        best = std::max(best, manifoldPressurePa[i]);
    }
    return best;
}

void Il2TelemetrySnapshot::clearPendingEvents() noexcept {
    pendingGunShots = 0;
    pendingOrdnanceReleases = 0;
    pendingHits = 0;
    pendingDamages = 0;
    pendingPeakImpact = 0.0f;
}

void Il2TelemetrySnapshot::clearLiveTelemetryState() noexcept {
    hasMotion = false;
    hasState = false;
    paused = false;
    frozen = false;
    identicalTickRun = 0;
    engineCount = 0;
    for (float& v : rpm) {
        v = 0.0f;
    }
    for (float& v : manifoldPressurePa) {
        v = 0.0f;
    }
    engineShakeFreq = 0.0f;
    engineShakeAmp = 0.0f;
    indicatedAirspeedMps = 0.0f;
    for (float& v : motionAcc) {
        v = 0.0f;
    }
    for (float& v : stateAcc) {
        v = 0.0f;
    }
    stallBuffetFreq = 0.0f;
    stallBuffetAmp = 0.0f;
    gearPos = 0.0f;
    maxGearPressure = 0.0f;
    flapsPos = 0.0f;
    airBrakePos = 0.0f;
    clearPendingEvents();
}

bool Il2TelemetryParser::feedPacket(const std::vector<uint8_t>& packet, Il2TelemetrySnapshot& snapshot) {
    if (packet.size() < 8) {
        return false;
    }

    const uint8_t* data = packet.data();
    const size_t size = packet.size();
    const uint32_t packetId = readU32(data);

    if (packetId == kMotionPacketId) {
        if (size < 44) {
            return false;
        }
        snapshot.hasMotion = true;
        snapshot.lastMotionTick = readU32(data + 4);
        snapshot.motionPacketCount += 1;
        // [8..31] = yaw, pitch, roll, spinX, spinY, spinZ; [32..43] = accX, accY, accZ.
        snapshot.motionAcc[0] = readF32(data + 32);
        snapshot.motionAcc[1] = readF32(data + 36);
        snapshot.motionAcc[2] = readF32(data + 40);
        return true;
    }

    if (packetId != kStateAndEventsPacketId || size < 11) {
        return false;
    }

    const uint16_t packetSize = readU16(data + 4);
    const uint32_t tick = readU32(data + 6);
    const uint8_t stateCount = data[10];

    if (snapshot.hasState && tick == snapshot.lastStateTick) {
        snapshot.identicalTickRun += 1;
    } else {
        snapshot.identicalTickRun = 0;
    }
    snapshot.hasState = true;
    snapshot.lastStateTick = tick;
    snapshot.statePacketCount += 1;
    snapshot.paused = (packetSize == 12);
    snapshot.frozen = (snapshot.identicalTickRun >= 8);

    size_t offset = 11;
    for (uint8_t i = 0; i < stateCount; ++i) {
        if (offset + 3 > size) {
            return true;
        }
        const uint16_t stateType = readU16(data + offset);
        const uint8_t floatCount = data[offset + 2];
        offset += 3;

        const size_t byteLen = static_cast<size_t>(floatCount) * 4u;
        if (offset + byteLen > size) {
            return true;
        }

        switch (stateType) {
        case 0: { // RPM
            snapshot.engineCount = std::min<int>(floatCount, 4);
            for (int j = 0; j < snapshot.engineCount; ++j) {
                snapshot.rpm[j] = readF32(data + offset + static_cast<size_t>(j) * 4u);
            }
            break;
        }
        case 1: { // IntakeManifoldPressurePa
            const int n = std::min<int>(floatCount, 4);
            for (int j = 0; j < n; ++j) {
                snapshot.manifoldPressurePa[j] = readF32(data + offset + static_cast<size_t>(j) * 4u);
            }
            break;
        }
        case 2: { // EngineShakeFrequency
            float best = 0.0f;
            const int n = std::min<int>(floatCount, 4);
            for (int j = 0; j < n; ++j) {
                best = std::max(best, readF32(data + offset + static_cast<size_t>(j) * 4u));
            }
            snapshot.engineShakeFreq = best;
            break;
        }
        case 3: { // EngineShakeAmplitude
            float best = 0.0f;
            const int n = std::min<int>(floatCount, 4);
            for (int j = 0; j < n; ++j) {
                best = std::max(best, readF32(data + offset + static_cast<size_t>(j) * 4u));
            }
            snapshot.engineShakeAmp = best;
            break;
        }
        case 4: { // LandingGearPosition
            if (floatCount >= 1) {
                snapshot.gearPos = readF32(data + offset);
            }
            break;
        }
        case 5: { // LandingGearPressure
            float best = 0.0f;
            const int n = std::min<int>(floatCount, 4);
            for (int j = 0; j < n; ++j) {
                best = std::max(best, readF32(data + offset + static_cast<size_t>(j) * 4u));
            }
            snapshot.maxGearPressure = best;
            break;
        }
        case 6: { // IndicatedAirspeedMetresSecond
            if (floatCount >= 1) {
                snapshot.indicatedAirspeedMps = readF32(data + offset);
            }
            break;
        }
        case 8: { // Acceleration
            if (floatCount >= 3) {
                snapshot.stateAcc[0] = readF32(data + offset);
                snapshot.stateAcc[1] = readF32(data + offset + 4u);
                snapshot.stateAcc[2] = readF32(data + offset + 8u);
            }
            break;
        }
        case 9: { // StallBuffet
            if (floatCount >= 2) {
                snapshot.stallBuffetFreq = readF32(data + offset);
                snapshot.stallBuffetAmp = readF32(data + offset + 4u);
            }
            break;
        }
        case 11: { // FlapsPosition
            if (floatCount >= 1) {
                snapshot.flapsPos = readF32(data + offset);
            }
            break;
        }
        case 12: { // AirBrakePosition
            if (floatCount >= 1) {
                snapshot.airBrakePos = readF32(data + offset);
            }
            break;
        }
        default:
            break;
        }
        offset += byteLen;
    }

    if (offset >= size) {
        return true;
    }

    const uint8_t eventCount = data[offset++];
    for (uint8_t i = 0; i < eventCount; ++i) {
        if (offset + 3 > size) {
            return true;
        }
        const uint16_t eventType = readU16(data + offset);
        const uint8_t eventBytes = data[offset + 2];
        offset += 3;

        if (offset + eventBytes > size) {
            return true;
        }

        switch (eventType) {
        case 0: { // VehicleName
            std::string name(reinterpret_cast<const char*>(data + offset), eventBytes);
            while (!name.empty() && name.back() == '\0') {
                name.pop_back();
            }
            if (!name.empty()) {
                snapshot.aircraftName = std::move(name);
            }
            break;
        }
        case 4: // BombRelease
        case 5: // RocketLaunch
            snapshot.pendingOrdnanceReleases += 1;
            snapshot.pendingPeakImpact = std::max(snapshot.pendingPeakImpact, 0.45f);
            break;
        case 7: { // Hit (offset 3f + force 3f = 24 bytes)
            snapshot.pendingHits += 1;
            float forceMag = 0.6f;
            if (eventBytes >= 24) {
                const float fx = readF32(data + offset + 12u);
                const float fy = readF32(data + offset + 16u);
                const float fz = readF32(data + offset + 20u);
                const float rawMag = std::sqrt(fx * fx + fy * fy + fz * fz);
                forceMag = std::clamp(rawMag * 0.05f + 0.45f, 0.45f, 1.0f);
            }
            snapshot.pendingPeakImpact = std::max(snapshot.pendingPeakImpact, forceMag);
            break;
        }
        case 8: { // Damage (offset 3f + float0 = 16 bytes)
            snapshot.pendingDamages += 1;
            float dmgMag = 0.70f;
            if (eventBytes >= 16) {
                const float f0 = readF32(data + offset + 12u);
                dmgMag = std::clamp(f0 * 0.08f + 0.55f, 0.55f, 1.0f);
            }
            snapshot.pendingPeakImpact = std::max(snapshot.pendingPeakImpact, dmgMag);
            break;
        }
        case 9: { // GunFired
            snapshot.pendingGunShots += 1;
            if (eventBytes >= 1) {
                snapshot.lastGunIndex = data[offset];
            }
            break;
        }
        default:
            break;
        }

        offset += eventBytes;
    }

    return true;
}

std::string updateStartupCfgTelemetryText(const std::string& content, uint16_t port, bool& outChanged) {
    const std::string nl = (content.find("\r\n") != std::string::npos) ? "\r\n" : "\n";
    std::vector<std::string> lines;
    {
        std::istringstream iss(content);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            lines.push_back(std::move(line));
        }
    }

    auto buildBlock = [&](const char* keyName) {
        std::vector<std::string> block;
        block.push_back(std::string("[KEY = ") + keyName + "]");
        block.push_back("\taddr = \"127.0.0.1\"");
        block.push_back("\tdecimation = 1");
        block.push_back("\tenable = true");
        block.push_back("\tport = " + std::to_string(port));
        block.push_back("[END]");
        return block;
    };

    auto ensureSection = [&](const char* keyName) {
        const std::string header = std::string("[KEY = ") + keyName + "]";
        int startIdx = -1;
        int endIdx = -1;
        for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
            if (lines[i].find(header) != std::string::npos) {
                startIdx = i;
                for (int j = i + 1; j < static_cast<int>(lines.size()); ++j) {
                    if (lines[j].find("[END]") != std::string::npos) {
                        endIdx = j;
                        break;
                    }
                }
                break;
            }
        }

        const std::vector<std::string> desired = buildBlock(keyName);
        if (startIdx >= 0 && endIdx > startIdx) {
            bool matches = (endIdx - startIdx + 1) == static_cast<int>(desired.size());
            if (matches) {
                for (size_t k = 0; k < desired.size(); ++k) {
                    if (lines[static_cast<size_t>(startIdx) + k] != desired[k]) {
                        matches = false;
                        break;
                    }
                }
            }
            if (!matches) {
                lines.erase(lines.begin() + startIdx, lines.begin() + endIdx + 1);
                lines.insert(lines.begin() + startIdx, desired.begin(), desired.end());
                outChanged = true;
            }
        } else {
            if (!lines.empty() && !lines.back().empty()) {
                lines.emplace_back();
            }
            lines.insert(lines.end(), desired.begin(), desired.end());
            outChanged = true;
        }
    };

    outChanged = false;
    ensureSection("motiondevice");
    ensureSection("telemetrydevice");

    if (!outChanged) {
        return content;
    }

    std::ostringstream oss;
    for (size_t i = 0; i < lines.size(); ++i) {
        oss << lines[i];
        if (i + 1 < lines.size() || (!content.empty() && content.back() == '\n')) {
            oss << nl;
        }
    }
    return oss.str();
}

bool configureIl2StartupCfgTelemetry(
    const std::string& startupCfgPath,
    uint16_t port,
    bool& outModified,
    std::string& outStatus) {
    outModified = false;
    const std::filesystem::path cfgPath(startupCfgPath);
    std::error_code ec;
    if (!std::filesystem::exists(cfgPath, ec)) {
        outStatus = "data/startup.cfg nao encontrado";
        return false;
    }

    std::string raw;
    {
        std::ifstream in(cfgPath, std::ios::binary);
        if (!in.is_open()) {
            outStatus = "Falha ao ler data/startup.cfg";
            return false;
        }
        std::ostringstream ss;
        ss << in.rdbuf();
        raw = ss.str();
    }

    bool changed = false;
    const std::string updated = updateStartupCfgTelemetryText(raw, port, changed);
    if (!changed) {
        outStatus = "Telemetria ja configurada em startup.cfg (porta " + std::to_string(port) + ")";
        return true;
    }

    const std::filesystem::path backupPath(startupCfgPath + ".bak_il2vr");
    if (!std::filesystem::exists(backupPath, ec)) {
        std::filesystem::copy_file(cfgPath, backupPath, std::filesystem::copy_options::skip_existing, ec);
    }

    std::ofstream out(cfgPath, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        outStatus = "Falha ao gravar data/startup.cfg";
        return false;
    }
    out.write(updated.data(), static_cast<std::streamsize>(updated.size()));
    if (!out.good()) {
        outStatus = "Erro de escrita em data/startup.cfg";
        return false;
    }

    outModified = true;
    outStatus = "Telemetria ativada em startup.cfg (reinicie o IL-2 uma vez)";
    return true;
}

} // namespace il2vr
