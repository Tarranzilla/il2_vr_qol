#include "input/Il2ActionsCatalog.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace il2vr {

namespace {

std::string trimCopy(const std::string& s) {
    size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) {
        ++b;
    }
    size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) {
        --e;
    }
    return s.substr(b, e - b);
}

std::string toLowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

std::string mapSingleIl2KeyToken(const std::string& rawToken) {
    std::string tok = toLowerCopy(trimCopy(rawToken));
    if (tok.rfind("key_", 0) != 0) {
        return {};
    }
    tok = tok.substr(4);
    if (tok == "lcontrol") return "lctrl";
    if (tok == "rcontrol") return "rctrl";
    if (tok == "lmenu") return "lalt";
    if (tok == "rmenu") return "ralt";
    if (tok == "lshift" || tok == "rshift" || tok == "lwin" || tok == "rwin") return tok;
    if (tok == "return") return "enter";
    if (tok == "escape") return "esc";
    if (tok == "add") return "numpadadd";
    if (tok == "subtract") return "numpadsubtract";
    if (tok == "decimal") return "numpaddecimal";
    if (tok == "prior") return "pageup";
    if (tok == "next") return "pagedown";
    if (tok == "slash") return "slash";
    if (tok == "comma") return "comma";
    if (tok == "period") return "period";
    if (tok == "semicolon") return "semicolon";
    if (tok == "apostrophe") return "apostrophe";
    if (tok == "grave") return "grave";
    if (tok == "equals") return "equals";
    if (tok == "minus") return "minus";
    return tok;
}

} // namespace

const std::vector<Il2CatalogItem>& Il2ActionsCatalog::builtInItems() {
    static const std::vector<Il2CatalogItem> kItems = {
        // Priority 1..19: Main Flight & Combat Actions (displayed first)
        {"gear", "Landing Gear (Up/Down)", "Trem de Pouso (Subir/Descer)", ActionCategory::MainFlight, 1, {"g"}, 0, false},
        {"flaps_dn", "Flaps Down / Extend", "Flaps - Baixar", ActionCategory::MainFlight, 2, {"f"}, 0, false},
        {"flaps_up", "Flaps Up / Retract", "Flaps - Recolher", ActionCategory::MainFlight, 3, {"lshift", "f"}, 0, false},
        {"throw_bombs_command", "Drop Bombs / Stores", "Soltar Bombas / Cargas", ActionCategory::MainFlight, 4, {"b"}, 0, false},
        {"launch_rockets_command", "Launch Rockets", "Disparar Foguetes", ActionCategory::MainFlight, 5, {"r"}, 0, false},
        {"bc_eject", "Bail Out / Eject", "Pular de Paraquedas / Ejetar", ActionCategory::MainFlight, 6, {"lctrl", "e"}, 0, true},
        {"all_guns_fire", "Fire All Guns", "Disparar Todas as Armas", ActionCategory::MainFlight, 7, {"space"}, 1, false},
        {"weapon_group_1_fire", "Fire Weapon Group 1 (MG)", "Armas Grupo 1 (Metralhadoras)", ActionCategory::MainFlight, 8, {"ralt", "space"}, 1, false},
        {"weapon_group_2_fire", "Fire Weapon Group 2 (Cannons)", "Armas Grupo 2 (Canhoes)", ActionCategory::MainFlight, 9, {"lalt", "space"}, 2, false},
        {"canopy_open_close", "Canopy Open / Close", "Abrir / Fechar Canopy", ActionCategory::MainFlight, 10, {"ralt", "c"}, 0, false},
        {"airbrakes", "Airbrakes / Dive Brakes", "Freio Aerodinamico / Dive", ActionCategory::MainFlight, 11, {"ralt", "b"}, 0, false},
        {"rpc_wheel_brakes", "Wheel Brakes", "Freios das Rodas", ActionCategory::MainFlight, 12, {"slash"}, 3, false},
        {"parking_brakes", "Parking Brakes Toggle", "Freio de Estacionamento", ActionCategory::MainFlight, 13, {"lshift", "slash"}, 0, false},
        {"tailwheel_lock", "Tailwheel Lock / Unlock", "Travar / Destravar Bequilha", ActionCategory::MainFlight, 14, {"lshift", "g"}, 0, false},
        {"bombdoors_toggle", "Bomb Bay Doors Toggle", "Portas de Bombas (Abrir/Fechar)", ActionCategory::MainFlight, 15, {"n"}, 0, false},
        {"all_guns_reload", "Reload All Guns", "Recarregar Armas", ActionCategory::MainFlight, 16, {"lalt", "r"}, 0, false},

        // Priority 20..49: Engine, Radiators, Trim & Cockpit Systems
        {"all_engines_switch", "Engine Start / Stop", "Ligar / Desligar Motor", ActionCategory::EngineSystems, 20, {"e"}, 0, true},
        {"autoradiators", "Auto Radiators Toggle", "Radiadores Automaticos", ActionCategory::EngineSystems, 21, {"lshift", "r"}, 0, false},
        {"autorpmlimiter", "Auto RPM Limiter Toggle", "Limitador RPM Automatico", ActionCategory::EngineSystems, 22, {"lshift", "n"}, 0, false},
        {"autoaltitudinalcorrection", "Auto Mixture / Supercharger", "Mistura / Supercharger Auto", ActionCategory::EngineSystems, 23, {"lshift", "m"}, 0, false},
        {"all_engines_boost", "Engine Boost Toggle", "Boost / WEP do Motor", ActionCategory::EngineSystems, 24, {"lshift", "b"}, 0, false},
        {"pitch_trim_switch_up", "Elevator Trim Up", "Trim Profundor Cima", ActionCategory::EngineSystems, 25, {"rctrl", "down"}, 0, false},
        {"pitch_trim_switch_dn", "Elevator Trim Down", "Trim Profundor Baixo", ActionCategory::EngineSystems, 26, {"rctrl", "up"}, 0, false},
        {"trims_reset", "Reset All Trimmers", "Zerar Todos os Trims", ActionCategory::EngineSystems, 27, {"lctrl", "t"}, 0, false},
        {"throw_bombs_mode", "Bomb Drop Mode Toggle", "Modo de Soltura de Bombas", ActionCategory::EngineSystems, 28, {"lwin", "b"}, 0, false},
        {"throw_bombs_delay", "Bomb Fuse Delay Toggle", "Atraso da Espoleta de Bomba", ActionCategory::EngineSystems, 29, {"lctrl", "b"}, 0, false},
        {"landing_lights", "Landing Lights Toggle", "Luzes de Pouso", ActionCategory::EngineSystems, 30, {"rshift", "l"}, 0, false},
        {"cockpit_lights", "Cockpit Lights Toggle", "Luzes do Cockpit", ActionCategory::EngineSystems, 31, {"l"}, 0, false},
        {"autopilot_device", "Autopilot On / Off", "Piloto Automatico", ActionCategory::EngineSystems, 32, {"ralt", "a"}, 0, false},
        {"cam_reset", "IL-2 VR View Recenter", "Recenter Padrao IL-2 (Num5)", ActionCategory::EngineSystems, 33, {"numpad5"}, 0, false},
    };
    return kItems;
}

std::vector<std::string> Il2ActionsCatalog::translateIl2KeyTokenCombo(const std::string& il2Spec) {
    std::string trimmed = trimCopy(il2Spec);
    if (trimmed.empty()) {
        return {};
    }
    // Dual-axis lines look like "key_down/key_up"; take first branch if single combo needed.
    const size_t slashPos = trimmed.find('/');
    if (slashPos != std::string::npos) {
        trimmed = trimmed.substr(0, slashPos);
    }

    std::vector<std::string> out;
    std::istringstream iss(trimmed);
    std::string part;
    while (std::getline(iss, part, '+')) {
        const std::string mapped = mapSingleIl2KeyToken(part);
        if (mapped.empty()) {
            return {};
        }
        out.push_back(mapped);
    }
    return out;
}

std::unordered_map<std::string, std::vector<std::string>> Il2ActionsCatalog::parseActionsText(const std::string& content) {
    std::unordered_map<std::string, std::vector<std::string>> result;
    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
        const size_t c1 = line.find(',');
        if (c1 == std::string::npos) {
            continue;
        }
        const size_t c2 = line.find(',', c1 + 1);
        if (c2 == std::string::npos) {
            continue;
        }
        const std::string actionId = toLowerCopy(trimCopy(line.substr(0, c1)));
        const std::string bindingSpec = trimCopy(line.substr(c1 + 1, c2 - c1 - 1));
        if (actionId.empty() || bindingSpec.empty()) {
            continue;
        }
        if (bindingSpec.find("key_") == std::string::npos) {
            continue;
        }
        if (result.find(actionId) != result.end()) {
            continue; // Keep first valid keyboard combo per actionId.
        }
        auto combo = translateIl2KeyTokenCombo(bindingSpec);
        if (!combo.empty()) {
            result.emplace(actionId, std::move(combo));
        }
    }
    return result;
}

std::unordered_map<std::string, std::vector<std::string>> Il2ActionsCatalog::loadUserActionKeyOverrides(
    const std::filesystem::path& il2InputDir) {
    std::unordered_map<std::string, std::vector<std::string>> merged;
    if (il2InputDir.empty()) {
        return merged;
    }

    const auto loadInto = [&](const std::filesystem::path& filePath, bool overwriteExisting) {
        std::error_code ec;
        if (!std::filesystem::exists(filePath, ec)) {
            return;
        }
        std::ifstream in(filePath, std::ios::binary);
        if (!in.is_open()) {
            return;
        }
        std::ostringstream ss;
        ss << in.rdbuf();
        auto parsed = parseActionsText(ss.str());
        for (auto& kv : parsed) {
            if (overwriteExisting || merged.find(kv.first) == merged.end()) {
                merged[kv.first] = std::move(kv.second);
            }
        }
    };

    loadInto(il2InputDir / "global.actions", false);
    loadInto(il2InputDir / "current.actions", true);
    return merged;
}

} // namespace il2vr
