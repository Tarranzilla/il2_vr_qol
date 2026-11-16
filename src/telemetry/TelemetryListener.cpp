#include "telemetry/TelemetryListener.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>

#include <array>

namespace il2vr {

namespace {

bool ensureWinsock() {
    static const bool ok = []() {
        WSADATA wsa{};
        return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
    }();
    return ok;
}

} // namespace

TelemetryListener::~TelemetryListener() {
    stop();
}

bool TelemetryListener::start(const std::string& bindIp, uint16_t port) {
    stop();
    lastError_.clear();
    if (!ensureWinsock()) {
        lastError_ = "WSAStartup failed";
        return false;
    }

    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        lastError_ = "socket() failed (WSA=" + std::to_string(WSAGetLastError()) + ")";
        return false;
    }

    DWORD timeoutMs = 50;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, bindIp.c_str(), &addr.sin_addr) != 1) {
        lastError_ = "Invalid bind IP: " + bindIp;
        closesocket(sock);
        return false;
    }

    if (bind(sock, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        lastError_ = "bind(" + bindIp + ":" + std::to_string(port) + ") failed (WSA=" + std::to_string(WSAGetLastError()) + ")";
        closesocket(sock);
        return false;
    }

    sockaddr_in actual{};
    int actualLen = sizeof(actual);
    if (getsockname(sock, reinterpret_cast<sockaddr*>(&actual), &actualLen) == 0) {
        port_ = ntohs(actual.sin_port);
    } else {
        port_ = port;
    }

    socketHandle_ = static_cast<uintptr_t>(sock);
    packetCount_.store(0);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        lastPacket_.clear();
        pendingPackets_.clear();
    }

    running_.store(true);
    thread_ = std::thread(&TelemetryListener::listenLoop, this);
    return true;
}

void TelemetryListener::stop() {
    running_.store(false);
    if (socketHandle_ != ~static_cast<uintptr_t>(0)) {
        closesocket(static_cast<SOCKET>(socketHandle_));
        socketHandle_ = ~static_cast<uintptr_t>(0);
    }
    if (thread_.joinable()) {
        thread_.join();
    }
    port_ = 0;
}

std::vector<uint8_t> TelemetryListener::lastPacket() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastPacket_;
}

std::vector<std::vector<uint8_t>> TelemetryListener::drainPackets() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::vector<uint8_t>> out;
    out.reserve(pendingPackets_.size());
    while (!pendingPackets_.empty()) {
        out.push_back(std::move(pendingPackets_.front()));
        pendingPackets_.pop_front();
    }
    return out;
}

void TelemetryListener::listenLoop() {
    std::array<uint8_t, 2048> buffer{};
    const SOCKET sock = static_cast<SOCKET>(socketHandle_);

    while (running_.load()) {
        sockaddr_in from{};
        int fromLen = sizeof(from);
        const int n = recvfrom(
            sock,
            reinterpret_cast<char*>(buffer.data()),
            static_cast<int>(buffer.size()),
            0,
            reinterpret_cast<sockaddr*>(&from),
            &fromLen);

        if (n > 0) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                lastPacket_.assign(buffer.begin(), buffer.begin() + n);
                if (pendingPackets_.size() >= 256) {
                    pendingPackets_.pop_front();
                }
                pendingPackets_.push_back(lastPacket_);
            }
            packetCount_.fetch_add(1);
        }
    }
}

bool TelemetryListener::sendTestPacket(
    const std::string& targetIp,
    uint16_t port,
    const std::vector<uint8_t>& payload) {
    if (!ensureWinsock()) {
        return false;
    }
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (inet_pton(AF_INET, targetIp.c_str(), &addr.sin_addr) != 1) {
        closesocket(sock);
        return false;
    }

    const int sent = sendto(
        sock,
        reinterpret_cast<const char*>(payload.data()),
        static_cast<int>(payload.size()),
        0,
        reinterpret_cast<const sockaddr*>(&addr),
        sizeof(addr));

    closesocket(sock);
    return sent == static_cast<int>(payload.size());
}

} // namespace il2vr
