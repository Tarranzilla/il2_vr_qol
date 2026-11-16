#pragma once

#include <atomic>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace il2vr {

class TelemetryListener {
public:
    TelemetryListener() = default;
    ~TelemetryListener();

    TelemetryListener(const TelemetryListener&) = delete;
    TelemetryListener& operator=(const TelemetryListener&) = delete;

    bool start(const std::string& bindIp, uint16_t port);
    void stop();

    bool isRunning() const { return running_.load(); }
    uint64_t packetCount() const { return packetCount_.load(); }
    std::vector<uint8_t> lastPacket() const;
    std::vector<std::vector<uint8_t>> drainPackets();
    uint16_t boundPort() const { return port_; }

    static bool sendTestPacket(
        const std::string& targetIp,
        uint16_t port,
        const std::vector<uint8_t>& payload);

private:
    void listenLoop();

    std::atomic<bool> running_{false};
    std::atomic<uint64_t> packetCount_{0};
    uintptr_t socketHandle_ = ~static_cast<uintptr_t>(0);
    uint16_t port_ = 0;
    std::thread thread_;

    mutable std::mutex mutex_;
    std::vector<uint8_t> lastPacket_;
    std::deque<std::vector<uint8_t>> pendingPackets_;
};

} // namespace il2vr
