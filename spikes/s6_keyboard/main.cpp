#include "output/KeyboardInjector.h"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

int main(int argc, char** argv) {
    bool selfTest = true;
    std::vector<std::string> comboToSend;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--self-test") {
            selfTest = true;
        } else if (arg == "--send-tap") {
            selfTest = false;
            for (int j = i + 1; j < argc; ++j) {
                comboToSend.emplace_back(argv[j]);
            }
            break;
        }
    }

    if (selfTest) {
        const std::vector<std::string> lagg3Keys = {
            "G", "F", "LShift", "S", "E", "RAlt", "C", "RCtrl", "L"
        };
        for (const auto& name : lagg3Keys) {
            const auto spec = il2vr::KeyboardInjector::resolveKey(name);
            if (!spec.has_value() || spec->scanCode == 0) {
                std::cerr << "[S6] Falha ao resolver tecla: " << name << "\n";
                return 1;
            }
            std::cout << "[S6] " << name << " -> vk=0x" << std::hex << spec->vk
                      << " scan=0x" << spec->scanCode << std::dec
                      << " ext=" << (spec->extended ? "1" : "0") << "\n";
        }
        std::cout << "[S6] Self-test ok (todas as teclas do perfil LaGG-3 resolvidas).\n";
        return 0;
    }

    if (comboToSend.empty()) {
        std::cerr << "[S6] Informe ao menos uma tecla apos --send-tap.\n";
        return 1;
    }

    std::cout << "[S6] Enviando combo em 3 segundos (foque a janela de teste/jogo)...\n";
    std::this_thread::sleep_for(std::chrono::seconds(3));
    if (!il2vr::KeyboardInjector::sendKeyTap(comboToSend, 50)) {
        std::cerr << "[S6] Falha no SendInput.\n";
        return 1;
    }
    std::cout << "[S6] SendInput enviado com sucesso.\n";
    return 0;
}
