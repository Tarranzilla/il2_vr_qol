#include "telemetry/TelemetryListener.h"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

int main(int argc, char** argv) {
    bool selfTest = true;
    uint16_t port = 14321;
    int listenSeconds = 10;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--self-test") {
            selfTest = true;
        } else if (arg == "--listen" && i + 2 < argc) {
            selfTest = false;
            port = static_cast<uint16_t>(std::atoi(argv[++i]));
            listenSeconds = std::atoi(argv[++i]);
        }
    }

    il2vr::TelemetryListener listener;
    if (!listener.start("127.0.0.1", selfTest ? 0 : port)) {
        std::cerr << "[S5] Falha ao iniciar socket UDP.\n";
        return 1;
    }

    const uint16_t activePort = listener.boundPort();
    std::cout << "[S5] Escutando UDP em 127.0.0.1:" << activePort << "\n";

    if (selfTest) {
        const std::vector<uint8_t> dummy = {0x49, 0x4C, 0x32, 0x54, 0x00, 0x00, 0x80, 0x3F};
        if (!il2vr::TelemetryListener::sendTestPacket("127.0.0.1", activePort, dummy)) {
            std::cerr << "[S5] Falha ao enviar pacote loopback.\n";
            return 1;
        }

        for (int i = 0; i < 50 && listener.packetCount() == 0; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        const auto pkt = listener.lastPacket();
        listener.stop();

        if (listener.packetCount() == 0 || pkt != dummy) {
            std::cerr << "[S5] Pacote loopback nao recebido corretamente.\n";
            return 1;
        }
        std::cout << "[S5] Self-test ok (recebido " << pkt.size() << " bytes).\n";
        return 0;
    }

    uint64_t lastPrinted = 0;
    const auto start = std::chrono::steady_clock::now();
    while (true) {
        const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        if (elapsed >= static_cast<double>(listenSeconds)) {
            break;
        }
        const uint64_t count = listener.packetCount();
        if (count != lastPrinted) {
            lastPrinted = count;
            const auto pkt = listener.lastPacket();
            std::cout << "[S5] pkt #" << count << " (" << pkt.size() << " bytes) hex:";
            for (size_t b = 0; b < std::min<size_t>(pkt.size(), 32); ++b) {
                std::cout << " " << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(pkt[b]);
            }
            std::cout << std::dec << "\n";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    std::cout << "[S5] Total de pacotes: " << listener.packetCount() << "\n";
    listener.stop();
    return 0;
}
