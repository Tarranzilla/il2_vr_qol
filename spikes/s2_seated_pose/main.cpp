#include "core/MathTypes.h"
#include "vr/OpenVrBackend.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

namespace {

void printMat34(const char* label, const il2vr::Mat34& m) {
    std::cout << label << ":\n";
    for (int r = 0; r < 3; ++r) {
        std::cout << "  [" << m.m[r][0] << ", " << m.m[r][1] << ", "
                  << m.m[r][2] << ", " << m.m[r][3] << "]\n";
    }
}

} // namespace

int main(int argc, char** argv) {
    bool statusOnly = true;
    float nudgeY = 0.0f;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--status") {
            statusOnly = true;
        } else if (arg == "--nudge-y" && i + 1 < argc) {
            statusOnly = false;
            nudgeY = std::strtof(argv[++i], nullptr);
        }
    }

    il2vr::OpenVrBackend vr;
    if (!vr.init(false)) {
        std::cout << "[S2] SteamVR offline / nao conectado: " << vr.lastError() << "\n";
        return statusOnly ? 0 : 1;
    }

    std::cout << "[S2] OpenVR conectado (overlay=" << (vr.isOverlayMode() ? "sim" : "nao") << ").\n";
    const auto original = vr.getSeatedZeroPose();
    if (!original.has_value()) {
        std::cerr << "[S2] Falha ao ler SeatedZeroPose do ChaperoneSetup.\n";
        return 1;
    }

    printMat34("[S2] SeatedZeroPose atual", *original);
    const auto frame = vr.pollState();
    if (frame.hmdValid) {
        std::cout << "[S2] HMD raw pos=("
                  << frame.hmdRawPose.m[0][3] << ", "
                  << frame.hmdRawPose.m[1][3] << ", "
                  << frame.hmdRawPose.m[2][3] << "), yaw="
                  << il2vr::extractYawDeg(frame.hmdRawPose) << " deg\n";
    } else {
        std::cout << "[S2] HMD pose ainda nao valida.\n";
    }

    if (!statusOnly) {
        il2vr::SeatOffset pilotOffset{};
        pilotOffset.y = nudgeY;
        const il2vr::Mat34 modified = il2vr::pilotOffsetToSeatedZeroPose(*original, pilotOffset);
        std::cout << "[S2] Aplicando delta piloto Y=" << nudgeY << " m por 2s...\n";
        if (!vr.setSeatedZeroPose(modified, true)) {
            std::cerr << "[S2] Falha ao aplicar SeatedZeroPose.\n";
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
        std::cout << "[S2] Restaurando SeatedZeroPose original...\n";
        vr.setSeatedZeroPose(*original, true);
    }

    vr.shutdown();
    return 0;
}
