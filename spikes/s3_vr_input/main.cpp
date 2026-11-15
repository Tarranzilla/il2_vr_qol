#include "vr/OpenVrBackend.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace {

void printHand(const char* name, const il2vr::ControllerState& c) {
    if (!c.valid) {
        std::cout << name << ": desconectado";
        return;
    }
    std::cout << name
              << ": pos=(" << c.pose.m[0][3] << "," << c.pose.m[1][3] << "," << c.pose.m[2][3] << ")"
              << " grip=" << c.grip
              << " trig=" << c.triggerValue << (c.triggerClick ? "*" : "")
              << " joy=(" << c.joyX << "," << c.joyY << ")"
              << " A/X=" << c.primaryBtn
              << " Menu=" << c.menuBtn;
}

} // namespace

int main(int argc, char** argv) {
    int samples = 20;
    bool statusOnly = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--samples" && i + 1 < argc) {
            samples = std::atoi(argv[++i]);
        } else if (arg == "--status") {
            statusOnly = true;
            samples = 1;
        }
    }

    il2vr::OpenVrBackend vr;
    if (!vr.init(false)) {
        std::cout << "[S3] SteamVR offline / nao conectado: " << vr.lastError() << "\n";
        return statusOnly ? 0 : 1;
    }

    std::cout << "[S3] OpenVR conectado (overlay=" << (vr.isOverlayMode() ? "sim" : "nao") << ").\n";
    for (int i = 0; i < samples; ++i) {
        const auto frame = vr.pollState();
        std::cout << "[S3 #" << i << "] ";
        printHand("L", frame.left);
        std::cout << " | ";
        printHand("R", frame.right);
        std::cout << "\n";
        if (i + 1 < samples) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    vr.shutdown();
    return 0;
}
