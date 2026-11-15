#include "output/VJoyClient.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

int main(int argc, char** argv) {
    bool selfTest = true;
    int sweepSeconds = 0;
    uint32_t deviceId = 1;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--self-test") {
            selfTest = true;
        } else if (arg == "--sweep" && i + 1 < argc) {
            selfTest = false;
            sweepSeconds = std::atoi(argv[++i]);
        } else if (arg == "--device" && i + 1 < argc) {
            deviceId = static_cast<uint32_t>(std::atoi(argv[++i]));
        }
    }

    il2vr::VJoyClient vjoy;
    if (!vjoy.load()) {
        std::cerr << "[S4] Falha ao carregar vJoyInterface.dll.\n";
        return 1;
    }

    std::cout << "[S4] vJoy DLL carregada: " << vjoy.loadedPath() << "\n";
    if (!vjoy.isDriverEnabled()) {
        std::cerr << "[S4] Driver vJoy nao esta habilitado.\n";
        return 1;
    }

    std::cout << "[S4] Versao driver vJoy: 0x" << std::hex << vjoy.driverVersion() << std::dec << "\n";
    const auto status = vjoy.deviceStatus(deviceId);
    std::cout << "[S4] Status inicial Device " << deviceId << ": " << static_cast<int>(status) << "\n";

    if (!vjoy.acquire(deviceId)) {
        std::cerr << "[S4] Falha ao adquirir vJoy Device " << deviceId << ".\n";
        return 1;
    }
    std::cout << "[S4] Device " << deviceId << " adquirido com sucesso.\n";

    if (selfTest) {
        bool ok = true;
        ok &= vjoy.setAxisNormalized(il2vr::VJoyAxis::X, 0.5f);
        ok &= vjoy.setAxisNormalized(il2vr::VJoyAxis::Y, -0.5f);
        ok &= vjoy.setThrottleNormalized(il2vr::VJoyAxis::Z, 0.75f);
        ok &= vjoy.setAxisNormalized(il2vr::VJoyAxis::Rz, 0.25f);
        ok &= vjoy.setButton(1, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        ok &= vjoy.setButton(1, false);
        ok &= vjoy.reset();
        vjoy.relinquish();

        if (!ok) {
            std::cerr << "[S4] Erro durante envio de eixos/botoes.\n";
            return 1;
        }
        std::cout << "[S4] Self-test concluido (X, Y, Z, Rz, Btn1, Reset, Relinquish OK).\n";
        return 0;
    }

    std::cout << "[S4] Enviando senoide nos eixos X/Y/Z/Rz por " << sweepSeconds << "s...\n";
    const auto start = std::chrono::steady_clock::now();
    while (true) {
        const auto elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();
        if (elapsed >= static_cast<float>(sweepSeconds)) {
            break;
        }
        const float s = std::sin(elapsed * 2.0f);
        const float c = std::cos(elapsed * 2.0f);
        vjoy.setAxisNormalized(il2vr::VJoyAxis::X, s);
        vjoy.setAxisNormalized(il2vr::VJoyAxis::Y, c);
        vjoy.setThrottleNormalized(il2vr::VJoyAxis::Z, (s + 1.0f) * 0.5f);
        vjoy.setAxisNormalized(il2vr::VJoyAxis::Rz, -s);
        std::this_thread::sleep_for(std::chrono::milliseconds(11));
    }
    vjoy.reset();
    vjoy.relinquish();
    std::cout << "[S4] Sweep finalizado.\n";
    return 0;
}
