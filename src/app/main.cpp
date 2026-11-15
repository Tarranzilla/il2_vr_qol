#include "core/MathTypes.h"
#include "output/KeyboardInjector.h"
#include "output/VJoyClient.h"
#include "seat/SeatManager.h"
#include "telemetry/TelemetryListener.h"
#include "vr/OpenVrBackend.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <imgui_impl_win32.h>
#include <spdlog/spdlog.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <d3d11.h>
#include <dxgi.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {

ID3D11Device* g_d3dDevice = nullptr;
ID3D11DeviceContext* g_d3dContext = nullptr;
IDXGISwapChain* g_swapChain = nullptr;
ID3D11RenderTargetView* g_renderTargetView = nullptr;
UINT g_resizeWidth = 0;
UINT g_resizeHeight = 0;

void createRenderTarget() {
    ID3D11Texture2D* backBuffer = nullptr;
    if (SUCCEEDED(g_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))) && backBuffer != nullptr) {
        g_d3dDevice->CreateRenderTargetView(backBuffer, nullptr, &g_renderTargetView);
        backBuffer->Release();
    }
}

void cleanupRenderTarget() {
    if (g_renderTargetView != nullptr) {
        g_renderTargetView->Release();
        g_renderTargetView = nullptr;
    }
}

bool createDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createFlags = 0;
    D3D_FEATURE_LEVEL featureLevel{};
    const D3D_FEATURE_LEVEL featureLevels[2] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createFlags,
        featureLevels,
        2,
        D3D11_SDK_VERSION,
        &sd,
        &g_swapChain,
        &g_d3dDevice,
        &featureLevel,
        &g_d3dContext);

    if (hr == DXGI_ERROR_UNSUPPORTED) {
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr,
            D3D_DRIVER_TYPE_WARP,
            nullptr,
            createFlags,
            featureLevels,
            2,
            D3D11_SDK_VERSION,
            &sd,
            &g_swapChain,
            &g_d3dDevice,
            &featureLevel,
            &g_d3dContext);
    }
    if (FAILED(hr)) {
        return false;
    }
    createRenderTarget();
    return true;
}

void cleanupDeviceD3D() {
    cleanupRenderTarget();
    if (g_swapChain != nullptr) {
        g_swapChain->Release();
        g_swapChain = nullptr;
    }
    if (g_d3dContext != nullptr) {
        g_d3dContext->Release();
        g_d3dContext = nullptr;
    }
    if (g_d3dDevice != nullptr) {
        g_d3dDevice->Release();
        g_d3dDevice = nullptr;
    }
}

#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif

enum class HotkeyAction {
    None = 0,
    Recenter,
    ResetOffset,
    ToggleHud,
    VisibleTest,
    StepFwdCoarse,
    StepFwdFine,
    StepBackCoarse,
    StepBackFine,
    StepLeftCoarse,
    StepLeftFine,
    StepRightCoarse,
    StepRightFine,
    StepUpCoarse,
    StepUpFine,
    StepDownCoarse,
    StepDownFine,
    YawLeftCoarse,
    YawLeftFine,
    YawRightCoarse,
    YawRightFine
};

struct HotkeyBinding {
    int id = 0;
    UINT modifiers = 0;
    UINT vk = 0;
    HotkeyAction action = HotkeyAction::None;
};

std::vector<HotkeyAction> g_pendingHotkeys;

const std::vector<HotkeyBinding>& getHotkeyBindings() {
    static const std::vector<HotkeyBinding> kBindings = {
        {101, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_NUMPAD5, HotkeyAction::Recenter},
        {102, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'R', HotkeyAction::Recenter},
        {103, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_HOME, HotkeyAction::Recenter},

        {104, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_NUMPAD0, HotkeyAction::ResetOffset},
        {105, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, '0', HotkeyAction::ResetOffset},
        {106, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_BACK, HotkeyAction::ResetOffset},

        {107, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'O', HotkeyAction::ToggleHud},
        {108, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'M', HotkeyAction::ToggleHud},
        {109, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_F1, HotkeyAction::ToggleHud},
        {110, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'T', HotkeyAction::VisibleTest},

        // Translation & Yaw (Coarse: Ctrl+Alt)
        {111, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_NUMPAD8, HotkeyAction::StepUpCoarse},
        {112, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_UP, HotkeyAction::StepUpCoarse},
        {113, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'W', HotkeyAction::StepUpCoarse},

        {114, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_NUMPAD2, HotkeyAction::StepDownCoarse},
        {115, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_DOWN, HotkeyAction::StepDownCoarse},
        {116, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'S', HotkeyAction::StepDownCoarse},

        {117, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_NUMPAD4, HotkeyAction::StepLeftCoarse},
        {118, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_LEFT, HotkeyAction::StepLeftCoarse},
        {119, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'A', HotkeyAction::StepLeftCoarse},

        {120, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_NUMPAD6, HotkeyAction::StepRightCoarse},
        {121, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_RIGHT, HotkeyAction::StepRightCoarse},
        {122, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'D', HotkeyAction::StepRightCoarse},

        {123, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_PRIOR, HotkeyAction::StepFwdCoarse},
        {124, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'I', HotkeyAction::StepFwdCoarse},
        {125, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_ADD, HotkeyAction::StepFwdCoarse},

        {126, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_NEXT, HotkeyAction::StepBackCoarse},
        {127, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'K', HotkeyAction::StepBackCoarse},
        {128, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_SUBTRACT, HotkeyAction::StepBackCoarse},

        {129, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'Q', HotkeyAction::YawLeftCoarse},
        {130, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_OEM_COMMA, HotkeyAction::YawLeftCoarse},
        {131, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_NUMPAD7, HotkeyAction::YawLeftCoarse},

        {132, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'E', HotkeyAction::YawRightCoarse},
        {133, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_OEM_PERIOD, HotkeyAction::YawRightCoarse},
        {134, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, VK_NUMPAD9, HotkeyAction::YawRightCoarse},

        // Translation & Yaw (Fine: Ctrl+Alt+Shift)
        {211, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, VK_NUMPAD8, HotkeyAction::StepUpFine},
        {212, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, VK_UP, HotkeyAction::StepUpFine},
        {213, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, 'W', HotkeyAction::StepUpFine},

        {214, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, VK_NUMPAD2, HotkeyAction::StepDownFine},
        {215, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, VK_DOWN, HotkeyAction::StepDownFine},
        {216, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, 'S', HotkeyAction::StepDownFine},

        {217, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, VK_NUMPAD4, HotkeyAction::StepLeftFine},
        {218, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, VK_LEFT, HotkeyAction::StepLeftFine},
        {219, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, 'A', HotkeyAction::StepLeftFine},

        {220, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, VK_NUMPAD6, HotkeyAction::StepRightFine},
        {221, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, VK_RIGHT, HotkeyAction::StepRightFine},
        {222, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, 'D', HotkeyAction::StepRightFine},

        {223, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, VK_PRIOR, HotkeyAction::StepFwdFine},
        {224, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, 'I', HotkeyAction::StepFwdFine},
        {225, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, VK_ADD, HotkeyAction::StepFwdFine},

        {226, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, VK_NEXT, HotkeyAction::StepBackFine},
        {227, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, 'K', HotkeyAction::StepBackFine},
        {228, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, VK_SUBTRACT, HotkeyAction::StepBackFine},

        {229, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, 'Q', HotkeyAction::YawLeftFine},
        {230, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, VK_OEM_COMMA, HotkeyAction::YawLeftFine},
        {231, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, VK_NUMPAD7, HotkeyAction::YawLeftFine},

        {232, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, 'E', HotkeyAction::YawRightFine},
        {233, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, VK_OEM_PERIOD, HotkeyAction::YawRightFine},
        {234, MOD_CONTROL | MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, VK_NUMPAD9, HotkeyAction::YawRightFine}
    };
    return kBindings;
}

std::vector<int> registerGlobalHotkeys(HWND hwnd) {
    std::vector<int> registered;
    for (const auto& b : getHotkeyBindings()) {
        if (RegisterHotKey(hwnd, b.id, b.modifiers, b.vk) != FALSE) {
            registered.push_back(b.id);
        }
    }
    return registered;
}

void unregisterGlobalHotkeys(HWND hwnd, const std::vector<int>& ids) {
    for (int id : ids) {
        UnregisterHotKey(hwnd, id);
    }
}

il2vr::Vec3 normalizeOrDefault(const il2vr::Vec3& v, const il2vr::Vec3& fallback) {
    const float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
    if (lenSq < 1e-8f) {
        return fallback;
    }
    const float invLen = 1.0f / std::sqrt(lenSq);
    return {v.x * invLen, v.y * invLen, v.z * invLen};
}

il2vr::Ray makeControllerRay(const il2vr::Mat34& ctrlSeatedPose, float pitchOffsetDeg) {
    const float rad = pitchOffsetDeg * (3.14159265358979323846f / 180.0f);
    const il2vr::Vec3 localDir{0.0f, std::sin(rad), -std::cos(rad)};
    const il2vr::Vec3 worldDir = normalizeOrDefault(
        il2vr::transformDirection(ctrlSeatedPose, localDir),
        {0.0f, 0.0f, -1.0f});
    return {
        {ctrlSeatedPose.m[0][3], ctrlSeatedPose.m[1][3], ctrlSeatedPose.m[2][3]},
        worldDir
    };
}

LRESULT WINAPI wndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
        return TRUE;
    }

    switch (msg) {
    case WM_HOTKEY: {
        const int id = static_cast<int>(wParam);
        for (const auto& b : getHotkeyBindings()) {
            if (b.id == id) {
                g_pendingHotkeys.push_back(b.action);
                break;
            }
        }
        return 0;
    }
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED) {
            return 0;
        }
        g_resizeWidth = static_cast<UINT>(LOWORD(lParam));
        g_resizeHeight = static_cast<UINT>(HIWORD(lParam));
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) {
            return 0;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

int runSmokeTest() {
    spdlog::info("=== IL-2 VR QoL Companion: Smoke Test (Fase 0) ===");

    il2vr::OpenVrBackend vr;
    if (vr.init(false)) {
        spdlog::info("OpenVR conectado (overlay={}).", vr.isOverlayMode());
    } else {
        spdlog::info("OpenVR nao ativo (esperado se SteamVR estiver fechado): {}", vr.lastError());
    }

    il2vr::SeatManager seat;
    seat.attachBackend(&vr);
    seat.nudgeStep(0, 1, 0, 0, true);
    const auto tempProfile = std::filesystem::temp_directory_path() / "il2vr_smoke_seat.json";
    if (!seat.saveProfile(tempProfile)) {
        spdlog::error("Falha ao salvar perfil de teste em {}", tempProfile.string());
        return 1;
    }
    il2vr::SeatManager seatReload;
    if (!seatReload.loadProfile(tempProfile)) {
        spdlog::error("Falha ao carregar perfil de teste");
        return 1;
    }
    std::error_code ec;
    std::filesystem::remove(tempProfile, ec);
    spdlog::info("SeatManager ok (offset.y={:.4f} m).", seatReload.offset().y);

    il2vr::VJoyClient vjoy;
    if (vjoy.load()) {
        spdlog::info(
            "vJoy carregado ({}) enabled={} version=0x{:x} dev1_status={}",
            vjoy.loadedPath(),
            vjoy.isDriverEnabled(),
            static_cast<unsigned>(vjoy.driverVersion()),
            static_cast<int>(vjoy.deviceStatus(1)));
    } else {
        spdlog::warn("vJoyInterface.dll nao encontrado.");
    }

    il2vr::TelemetryListener telemetry;
    if (!telemetry.start("127.0.0.1", 0)) {
        spdlog::error("Falha ao iniciar TelemetryListener UDP.");
        return 1;
    }
    const uint16_t port = telemetry.boundPort();
    const std::vector<uint8_t> payload = {0x49, 0x4C, 0x32, 0x01};
    il2vr::TelemetryListener::sendTestPacket("127.0.0.1", port, payload);
    for (int i = 0; i < 20 && telemetry.packetCount() == 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (telemetry.packetCount() == 0) {
        spdlog::error("TelemetryListener nao recebeu pacote loopback.");
        return 1;
    }
    spdlog::info("TelemetryListener ok (porta={}, pacotes={}).", port, telemetry.packetCount());
    telemetry.stop();

    if (!il2vr::KeyboardInjector::resolveKey("G").has_value() ||
        !il2vr::KeyboardInjector::resolveKey("LShift").has_value()) {
        spdlog::error("Falha ao resolver teclas basicas no KeyboardInjector.");
        return 1;
    }
    spdlog::info("KeyboardInjector ok.");
    spdlog::info("Smoke test concluido com sucesso.");
    return 0;
}

void playBeepAsync(DWORD freqHz, DWORD durationMs) {
    std::thread([freqHz, durationMs]() {
        Beep(freqHz, durationMs);
    }).detach();
}

uint32_t findProcessIdByName(const wchar_t* exeName) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        return 0;
    }
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    uint32_t foundPid = 0;
    if (Process32FirstW(snap, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, exeName) == 0) {
                foundPid = entry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snap, &entry));
    }
    CloseHandle(snap);
    return foundPid;
}

std::string toLowerAscii(std::string s) {
    for (char& ch : s) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return s;
}

std::filesystem::path detectIl2InputDir() {
    const std::filesystem::path defaultDir =
        L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\IL-2 Sturmovik Battle of Stalingrad\\data\\input";
    std::error_code ec;
    if (std::filesystem::exists(defaultDir, ec)) {
        return defaultDir;
    }
    return {};
}

int detectIl2VJoyConfigId(const std::filesystem::path& inputDir) {
    if (inputDir.empty()) {
        return 1;
    }
    std::ifstream in(inputDir / "devices.txt", std::ios::binary);
    if (!in.is_open()) {
        return 1;
    }
    std::string line;
    while (std::getline(in, line)) {
        const std::string lower = toLowerAscii(line);
        if (lower.find("vjoy") != std::string::npos) {
            const size_t comma = line.find(',');
            if (comma != std::string::npos && comma > 0) {
                try {
                    const int parsed = std::stoi(line.substr(0, comma));
                    if (parsed >= 0 && parsed < 32) {
                        return parsed;
                    }
                } catch (...) {
                }
            }
        }
    }
    return 1;
}

bool readFileTextBinary(const std::filesystem::path& p, std::string& out) {
    std::ifstream in(p, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

bool writeFileTextBinary(const std::filesystem::path& p, const std::string& content) {
    std::ofstream out(p, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    out.write(content.data(), static_cast<std::streamsize>(content.size()));
    return out.good();
}

std::string formatActionsLine(const std::string& actionName, const std::string& bindingToken) {
    std::string line = actionName + ",";
    if (line.size() < 50) {
        line.append(50 - line.size(), ' ');
    } else {
        line.push_back(' ');
    }
    line += bindingToken + ",";
    if (line.size() < 100) {
        line.append(100 - line.size(), ' ');
    } else {
        line.push_back(' ');
    }
    line += "0|";
    return line;
}

bool syncActionsFileForVJoy(const std::filesystem::path& filePath, const std::string& joyPrefix, bool& outChanged) {
    std::string raw;
    if (!readFileTextBinary(filePath, raw)) {
        return false;
    }

    const std::string newline = (raw.find("\r\n") != std::string::npos) ? "\r\n" : "\n";
    std::vector<std::string> lines;
    {
        std::istringstream iss(raw);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            lines.push_back(std::move(line));
        }
    }

    struct ActionRule {
        const char* action;
        const char* suffix;
        const char* matchRegex;
        bool hasJoy = false;
    };
    std::vector<ActionRule> rules = {
        {"rpc_pitch", "axis_y", R"(joy[0-9]+_axis_y)"},
        {"rpc_roll", "axis_x", R"(joy[0-9]+_axis_x)"},
        {"rpc_yaw", "axis_t", R"(joy[0-9]+_axis_t)"},
        {"rpc_all_engines_throttles", "axis_z", R"(joy[0-9]+_axis_[zp])"},
        {"rpc_wheel_brakes", "b2", R"(joy[0-9]+_b2)"},
        {"all_guns_fire", "b0", R"(joy[0-9]+_b0)"},
        {"weapon_group_1_fire", "b0", R"(joy[0-9]+_b0)"},
        {"weapon_group_2_fire", "b1", R"(joy[0-9]+_b1)"}
    };

    bool changed = false;
    for (std::string& line : lines) {
        for (auto& r : rules) {
            const std::string prefix = std::string(r.action) + ",";
            if (line.rfind(prefix, 0) == 0) {
                const std::string desiredToken = joyPrefix + "_" + r.suffix;
                const std::regex re(r.matchRegex);
                if (std::regex_search(line, re)) {
                    std::string next = std::regex_replace(line, re, desiredToken);
                    if (std::string(r.action) == "rpc_all_engines_throttles") {
                        next = std::regex_replace(next, std::regex(R"(,\s*1\|)"), ",                                      0|");
                    }
                    if (next != line) {
                        line = std::move(next);
                        changed = true;
                    }
                    r.hasJoy = true;
                }
            }
        }
    }

    for (const auto& r : rules) {
        if (r.hasJoy) {
            continue;
        }
        const std::string prefix = std::string(r.action) + ",";
        for (size_t i = 0; i < lines.size(); ++i) {
            if (lines[i].rfind(prefix, 0) == 0) {
                lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(i + 1),
                    formatActionsLine(r.action, joyPrefix + "_" + r.suffix));
                changed = true;
                break;
            }
        }
    }

    if (!changed) {
        return true;
    }

    std::string rebuilt;
    rebuilt.reserve(raw.size() + 256);
    for (size_t i = 0; i < lines.size(); ++i) {
        rebuilt += lines[i];
        if (i + 1 < lines.size() || (!raw.empty() && raw.back() == '\n')) {
            rebuilt += newline;
        }
    }
    if (!writeFileTextBinary(filePath, rebuilt)) {
        return false;
    }
    outChanged = true;
    return true;
}

bool syncCurrentMapForVJoy(const std::filesystem::path& filePath, const std::string& joyPrefix, bool& outChanged) {
    std::string raw;
    if (!readFileTextBinary(filePath, raw)) {
        return false;
    }

    const std::string newline = (raw.find("\r\n") != std::string::npos) ? "\r\n" : "\n";
    std::vector<std::string> lines;
    {
        std::istringstream iss(raw);
        std::string line;
        while (std::getline(iss, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            lines.push_back(std::move(line));
        }
    }

    struct MapRule {
        const char* action;
        const char* suffix;
        const char* matchRegex;
        const char* fallbackAnchor;
        bool hasJoy = false;
    };
    std::vector<MapRule> rules = {
        {"rpc_pitch_p", "axis_y", R"(joy[0-9]+_axis_y)", R"(action("rpc_pitch_p",)"},
        {"rpc_roll_p", "axis_x", R"(joy[0-9]+_axis_x)", R"(action("rpc_roll_p",)"},
        {"rpc_yaw_p", "axis_t", R"(joy[0-9]+_axis_t)", R"(action("rpc_yaw_p",)"},
        {"rpc_all_engines_throttles_p", "axis_z", R"(joy[0-9]+_axis_[pz])", R"(action("rpc_all_engines_throttles_)"},
        {"rpc_wheel_brakes_p", "b2", R"(joy[0-9]+_b2)", R"(action("rpc_wheel_brakes_p",)"},
        {"all_guns_fire", "b0", R"(joy[0-9]+_b0)", R"(action("all_guns_fire",)"},
        {"weapon_group_1_fire", "b0", R"(joy[0-9]+_b0)", R"(action("weapon_group_1_fire",)"},
        {"weapon_group_2_fire", "b1", R"(joy[0-9]+_b1)", R"(action("weapon_group_2_fire",)"},
    };

    bool changed = false;
    for (std::string& line : lines) {
        if (line.find(R"(action("rpc_all_engines_throttles_n","State","joy)") != std::string::npos) {
            const std::string desiredLine =
                std::string(R"(action("rpc_all_engines_throttles_p","State",")") + joyPrefix + R"(_axis_z");)";
            if (line != desiredLine) {
                line = desiredLine;
                changed = true;
            }
            rules[3].hasJoy = true;
            continue;
        }
        for (auto& r : rules) {
            const std::string actionTag = std::string(R"(action(")") + r.action + R"(","State",")";
            if (line.rfind(actionTag, 0) == 0) {
                const std::regex re(r.matchRegex);
                if (std::regex_search(line, re)) {
                    const std::string next = std::regex_replace(line, re, joyPrefix + "_" + r.suffix);
                    if (next != line) {
                        line = next;
                        changed = true;
                    }
                    r.hasJoy = true;
                }
            }
        }
    }

    for (const auto& r : rules) {
        if (r.hasJoy) {
            continue;
        }
        const std::string desiredLine =
            std::string(R"(action(")") + r.action + R"(","State",")" + joyPrefix + "_" + r.suffix + R"(");)";
        for (size_t i = 0; i < lines.size(); ++i) {
            if (lines[i].rfind(r.fallbackAnchor, 0) == 0) {
                lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(i + 1), desiredLine);
                changed = true;
                break;
            }
        }
    }

    if (!changed) {
        return true;
    }

    std::string rebuilt;
    rebuilt.reserve(raw.size() + 256);
    for (size_t i = 0; i < lines.size(); ++i) {
        rebuilt += lines[i];
        if (i + 1 < lines.size() || (!raw.empty() && raw.back() == '\n')) {
            rebuilt += newline;
        }
    }
    if (!writeFileTextBinary(filePath, rebuilt)) {
        return false;
    }
    outChanged = true;
    return true;
}

bool syncIl2InputProfilesForVJoy(const std::filesystem::path& inputDir, int vjoyConfigId, std::string& outSummary) {
    if (inputDir.empty()) {
        outSummary = "Pasta data/input do IL-2 nao encontrada";
        return false;
    }
    const std::string joyPrefix = "joy" + std::to_string(vjoyConfigId);
    bool anyChanged = false;
    int syncedCount = 0;

    const std::array<std::filesystem::path, 3> actionFiles = {
        inputDir / "custom" / "Quest3.actions",
        inputDir / "global.actions",
        inputDir / "current.actions"
    };
    for (const auto& f : actionFiles) {
        std::error_code ec;
        if (std::filesystem::exists(f, ec)) {
            if (syncActionsFileForVJoy(f, joyPrefix, anyChanged)) {
                ++syncedCount;
            }
        }
    }
    const std::filesystem::path mapFile = inputDir / "current.map";
    std::error_code ec;
    if (std::filesystem::exists(mapFile, ec)) {
        if (syncCurrentMapForVJoy(mapFile, joyPrefix, anyChanged)) {
            ++syncedCount;
        }
    }

    outSummary = anyChanged
        ? ("Perfis IL-2 atualizados para " + joyPrefix + " (" + std::to_string(syncedCount) + " arquivos)")
        : ("Perfis IL-2 ja sincronizados com " + joyPrefix);
    return syncedCount > 0;
}

} // namespace

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--smoke-test") {
            return runSmokeTest();
        }
    }

    WNDCLASSEXW wc{
        sizeof(wc),
        CS_CLASSDC,
        wndProc,
        0L,
        0L,
        GetModuleHandleW(nullptr),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        L"IL2VRQoLClass",
        nullptr
    };
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowW(
        wc.lpszClassName,
        L"IL-2 VR QoL Companion - Status & Diagnostico (Windows + VR)",
        WS_OVERLAPPEDWINDOW,
        60,
        40,
        1080,
        780,
        nullptr,
        nullptr,
        wc.hInstance,
        nullptr);

    if (!createDeviceD3D(hwnd)) {
        cleanupDeviceD3D();
        UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    const std::vector<int> registeredHotkeyIds = registerGlobalHotkeys(hwnd);

    // Explicitly use SW_SHOWNORMAL + SWP_SHOWWINDOW because launched background hosts
    // set STARTF_USESHOWWINDOW = SW_HIDE, which makes SW_SHOWDEFAULT hide the window.
    ShowWindow(hwnd, SW_SHOWNORMAL);
    SetWindowPos(hwnd, HWND_TOP, 60, 40, 1080, 780, SWP_SHOWWINDOW);
    SetForegroundWindow(hwnd);
    UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    {
        ImGuiStyle& style = ImGui::GetStyle();
        style.FramePadding = ImVec2(10.0f, 7.0f);
        style.ItemSpacing = ImVec2(8.0f, 7.0f);
        style.GrabMinSize = 18.0f;
        style.ScrollbarSize = 18.0f;
        ImGui::GetIO().FontGlobalScale = 1.10f;
    }
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_d3dDevice, g_d3dContext);

    il2vr::OpenVrBackend vr;
    bool showVrHud = true;
    vr.setOverlayVisible(showVrHud);
    if (vr.init(false)) {
        vr.setOverlayVisible(showVrHud);
    }

    il2vr::SeatManager seat;
    seat.attachBackend(&vr);

    il2vr::VJoyClient vjoy;
    vjoy.load();
    bool mirrorVrControllers = false;
    if (vjoy.isLibraryLoaded() && vjoy.isDriverEnabled()) {
        if (vjoy.acquire(1)) {
            mirrorVrControllers = true;
        }
    }

    il2vr::TelemetryListener telemetry;
    int telemetryPort = 4321;

    const std::filesystem::path il2InputDir = detectIl2InputDir();
    int il2VJoyConfigId = detectIl2VJoyConfigId(il2InputDir);
    std::string il2SyncStatus;
    if (!il2InputDir.empty()) {
        syncIl2InputProfilesForVJoy(il2InputDir, il2VJoyConfigId, il2SyncStatus);
    } else {
        il2SyncStatus = "Pasta data/input do IL-2 nao detectada";
    }
    bool vjoyAxesReady = vjoy.hasRequiredFlightAxes(1);
    std::string vjoyAxesDesc = vjoy.describeEnabledAxes(1);

    bool fineStep = false;
    std::string lastHotkeyAction = il2SyncStatus;
    float testStickX = 0.0f;
    float testStickY = 0.0f;
    float testThrottle = 0.5f;
    float testRudder = 0.0f;

    // Mode selection: false = VTOL VR 6DoF flight controls, true = 3D World Origin Grab (Grip drags origin).
    bool originGrabModeEnabled = false;

    // 3D Space Drag state (Grip in mid-air, translation only - zero rotation).
    bool spaceDragActive = false;
    il2vr::VrHand spaceDragHand = il2vr::VrHand::Left;
    il2vr::Vec3 dragStartHandRawPos{};
    il2vr::SeatOffset dragStartOffset{};
    il2vr::Mat34 dragStartRecenterBase = il2vr::Mat34::identity();
    bool invertSpaceDrag = false;
    float spaceDragScale = 1.0f;

    // VTOL VR 6DoF virtual stick (Right hand) & latched throttle lever (Left hand).
    il2vr::VtolStickConfig vtolStickCfg{};
    il2vr::VtolThrottleConfig vtolThrottleCfg{};
    float vtolStickRangeCm = vtolStickCfg.rangeM * 100.0f;
    float vtolThrottleRangeCm = vtolThrottleCfg.rangeM * 100.0f;
    il2vr::OneEuroFilter stickFilterX(2.5f, 0.45f, 1.0f);
    il2vr::OneEuroFilter stickFilterY(2.5f, 0.45f, 1.0f);
    il2vr::OneEuroFilter rudderFilterRz(2.5f, 0.35f, 1.0f);

    bool vtolStickGrabbed = false;
    il2vr::Mat34 stickGrabStartPose = il2vr::Mat34::identity();
    float stickGrabRefYawDeg = 0.0f;
    bool stickHighDeflectionHapticLatched = false;

    bool vtolThrottleGrabbed = false;
    il2vr::Vec3 throttleGrabStartPos{};
    float throttleGrabStartVal = testThrottle;
    float throttleGrabRefYawDeg = 0.0f;
    int lastThrottleDetent = 1;

    // VR HUD Laser Raycaster state.
    float rayPitchOffsetDeg = -22.0f;
    il2vr::VrHand preferredRayHand = il2vr::VrHand::Right;
    bool vrHudWasHit = false;
    bool prevRayClickDown = false;
    bool pendingHudReanchor = false;

    // Isolated vJoy Auto-Bind / Calibration sweep state (eliminates 6DoF crosstalk in IL-2 wizard).
    enum class AutoBindAxis { None, RollX, PitchY, ThrottleZ, RudderRz };
    AutoBindAxis autoBindAxis = AutoBindAxis::None;
    float autoBindElapsedSec = 0.0f;
    constexpr float kAutoBindDurationSec = 4.2f;

    using Clock = std::chrono::steady_clock;
    const auto appStartTime = Clock::now();
    auto lastVrConnectAttempt = Clock::now() - std::chrono::seconds(2);
    auto lastProcessCheckTime = Clock::now() - std::chrono::seconds(3);
    auto lastStatusWriteTime = Clock::now() - std::chrono::seconds(2);
    uint32_t il2Pid = 0;
    uint32_t prevIl2Pid = 0;
    auto lastDragCommitTime = Clock::now() - std::chrono::seconds(1);
    auto prevFrameTime = Clock::now();
    bool prevLeftMenuBtn = false;
    bool prevLeftStickBtn = false;
    bool prevRightStickBtn = false;

    auto sendIl2NativeRecenter = [&]() {
        std::thread([]() {
            il2vr::KeyboardInjector::sendKeyTap({"Numpad5"}, 50);
        }).detach();
        lastHotkeyAction = "NumPad 5 enviado (Recenter Nativo IL-2)";
        playBeepAsync(920, 45);
    };

    auto startAutoBind = [&](AutoBindAxis axis, const char* label) {
        autoBindAxis = axis;
        autoBindElapsedSec = 0.0f;
        mirrorVrControllers = true;
        if (!vjoy.isAcquired() && vjoy.isDriverEnabled()) {
            vjoy.acquire(1);
        }
        lastHotkeyAction = std::string("Auto-Bind iniciado: ") + label;
        playBeepAsync(880, 60);
    };

    auto toggleOriginGrabMode = [&]() {
        originGrabModeEnabled = !originGrabModeEnabled;
        spaceDragActive = false;
        vtolStickGrabbed = false;
        vtolThrottleGrabbed = false;
        lastHotkeyAction = originGrabModeEnabled
            ? "Modo: AJUSTAR ORIGEM (Grip Arrasta Mundo)"
            : "Modo: PILOTAGEM VTOL VR";
        if (vr.isInitialized()) {
            vr.triggerHapticPulse(il2vr::VrHand::Left, originGrabModeEnabled ? 2800 : 1800);
        }
        playBeepAsync(originGrabModeEnabled ? 660 : 880, 55);
    };

    auto toggleVisibleTestOffset = [&]() {
        auto off = seat.offset();
        if (std::abs(off.y - 0.25f) < 0.05f) {
            off.y = 0.0f;
            lastHotkeyAction = "Teste Visivel (+25 cm -> 0 cm)";
            playBeepAsync(600, 60);
        } else {
            off.y = 0.25f;
            lastHotkeyAction = "Teste Visivel (+25 cm Altura)";
            playBeepAsync(1050, 70);
        }
        seat.setOffset(off);
    };

    auto applyHotkeyAction = [&](HotkeyAction action, const il2vr::VrFrameState& vrState) {
        auto beepNudge = [](bool fine) {
            playBeepAsync(fine ? 880 : 720, 35);
        };
        switch (action) {
        case HotkeyAction::Recenter:
            if (vrState.hmdValid) {
                if (!seat.hasOriginalBackup()) {
                    seat.captureOriginalBackup();
                }
                seat.recenterFromRawHmdPose(vrState.hmdRawPose);
            } else {
                seat.recenterNow();
            }
            pendingHudReanchor = showVrHud;
            lastHotkeyAction = "Recentralizar (Hotkey)";
            playBeepAsync(1000, 60);
            break;
        case HotkeyAction::ResetOffset:
            seat.resetOffset();
            pendingHudReanchor = showVrHud;
            lastHotkeyAction = "Zerar Offset (Hotkey)";
            playBeepAsync(600, 60);
            break;
        case HotkeyAction::ToggleHud:
            showVrHud = !showVrHud;
            vr.setOverlayVisible(showVrHud);
            if (showVrHud && vrState.hmdValid) {
                pendingHudReanchor = true;
            }
            lastHotkeyAction = showVrHud ? "HUD VR: Ligado (Hotkey)" : "HUD VR: Desligado (Hotkey)";
            playBeepAsync(showVrHud ? 950 : 520, 50);
            break;
        case HotkeyAction::VisibleTest:
            toggleVisibleTestOffset();
            pendingHudReanchor = showVrHud;
            if (vr.isInitialized()) {
                vr.triggerHapticPulse(il2vr::VrHand::Right, 2500);
            }
            break;
        case HotkeyAction::StepFwdCoarse:
            seat.nudgeStep(0, 0, -1, 0, false);
            pendingHudReanchor = showVrHud;
            lastHotkeyAction = "Frente (grosso)";
            beepNudge(false);
            break;
        case HotkeyAction::StepFwdFine:
            seat.nudgeStep(0, 0, -1, 0, true);
            pendingHudReanchor = showVrHud;
            lastHotkeyAction = "Frente (fino)";
            beepNudge(true);
            break;
        case HotkeyAction::StepBackCoarse:
            seat.nudgeStep(0, 0, 1, 0, false);
            pendingHudReanchor = showVrHud;
            lastHotkeyAction = "Tras (grosso)";
            beepNudge(false);
            break;
        case HotkeyAction::StepBackFine:
            seat.nudgeStep(0, 0, 1, 0, true);
            pendingHudReanchor = showVrHud;
            lastHotkeyAction = "Tras (fino)";
            beepNudge(true);
            break;
        case HotkeyAction::StepLeftCoarse:
            seat.nudgeStep(-1, 0, 0, 0, false);
            pendingHudReanchor = showVrHud;
            lastHotkeyAction = "Esquerda (grosso)";
            beepNudge(false);
            break;
        case HotkeyAction::StepLeftFine:
            seat.nudgeStep(-1, 0, 0, 0, true);
            pendingHudReanchor = showVrHud;
            lastHotkeyAction = "Esquerda (fino)";
            beepNudge(true);
            break;
        case HotkeyAction::StepRightCoarse:
            seat.nudgeStep(1, 0, 0, 0, false);
            pendingHudReanchor = showVrHud;
            lastHotkeyAction = "Direita (grosso)";
            beepNudge(false);
            break;
        case HotkeyAction::StepRightFine:
            seat.nudgeStep(1, 0, 0, 0, true);
            pendingHudReanchor = showVrHud;
            lastHotkeyAction = "Direita (fino)";
            beepNudge(true);
            break;
        case HotkeyAction::StepUpCoarse:
            seat.nudgeStep(0, 1, 0, 0, false);
            pendingHudReanchor = showVrHud;
            lastHotkeyAction = "Subir (grosso)";
            beepNudge(false);
            break;
        case HotkeyAction::StepUpFine:
            seat.nudgeStep(0, 1, 0, 0, true);
            pendingHudReanchor = showVrHud;
            lastHotkeyAction = "Subir (fino)";
            beepNudge(true);
            break;
        case HotkeyAction::StepDownCoarse:
            seat.nudgeStep(0, -1, 0, 0, false);
            pendingHudReanchor = showVrHud;
            lastHotkeyAction = "Descer (grosso)";
            beepNudge(false);
            break;
        case HotkeyAction::StepDownFine:
            seat.nudgeStep(0, -1, 0, 0, true);
            pendingHudReanchor = showVrHud;
            lastHotkeyAction = "Descer (fino)";
            beepNudge(true);
            break;
        case HotkeyAction::YawLeftCoarse:
            seat.nudgeStep(0, 0, 0, 1, false);
            pendingHudReanchor = showVrHud;
            lastHotkeyAction = "Yaw Esq (grosso)";
            beepNudge(false);
            break;
        case HotkeyAction::YawLeftFine:
            seat.nudgeStep(0, 0, 0, 1, true);
            pendingHudReanchor = showVrHud;
            lastHotkeyAction = "Yaw Esq (fino)";
            beepNudge(true);
            break;
        case HotkeyAction::YawRightCoarse:
            seat.nudgeStep(0, 0, 0, -1, false);
            pendingHudReanchor = showVrHud;
            lastHotkeyAction = "Yaw Dir (grosso)";
            beepNudge(false);
            break;
        case HotkeyAction::YawRightFine:
            seat.nudgeStep(0, 0, 0, -1, true);
            pendingHudReanchor = showVrHud;
            lastHotkeyAction = "Yaw Dir (fino)";
            beepNudge(true);
            break;
        case HotkeyAction::None:
            break;
        }
    };

    bool running = true;
    while (running) {
        MSG msg{};
        while (PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) {
                running = false;
            }
        }
        if (!running) {
            break;
        }

        if (g_resizeWidth != 0 && g_resizeHeight != 0) {
            cleanupRenderTarget();
            g_swapChain->ResizeBuffers(0, g_resizeWidth, g_resizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_resizeWidth = 0;
            g_resizeHeight = 0;
            createRenderTarget();
        }

        const auto now = Clock::now();
        const float dtSeconds = std::clamp(
            std::chrono::duration<float>(now - prevFrameTime).count(),
            0.001f,
            0.1f);
        prevFrameTime = now;

        if ((now - lastProcessCheckTime) >= std::chrono::seconds(2)) {
            lastProcessCheckTime = now;
            prevIl2Pid = il2Pid;
            il2Pid = findProcessIdByName(L"Il-2.exe");
            if ((!vjoy.isAcquired() || vjoy.deviceStatus(1) != il2vr::VJoyStatus::Own) && vjoy.isDriverEnabled()) {
                vjoy.relinquish();
                if (vjoy.acquire(1)) {
                    mirrorVrControllers = true;
                }
            }
            vjoyAxesReady = vjoy.hasRequiredFlightAxes(1);
            vjoyAxesDesc = vjoy.describeEnabledAxes(1);
            if (!il2InputDir.empty()) {
                il2VJoyConfigId = detectIl2VJoyConfigId(il2InputDir);
                if (prevIl2Pid != 0 && il2Pid == 0) {
                    syncIl2InputProfilesForVJoy(il2InputDir, il2VJoyConfigId, il2SyncStatus);
                    lastHotkeyAction = il2SyncStatus;
                }
            }
        }

        if (!vr.isInitialized() && (now - lastVrConnectAttempt) >= std::chrono::seconds(2)) {
            lastVrConnectAttempt = now;
            if (vr.init(false)) {
                vr.setOverlayVisible(showVrHud);
                seat.attachBackend(&vr);
                pendingHudReanchor = showVrHud;
            }
        }

        il2vr::VrFrameState vrState{};
        if (vr.isInitialized()) {
            vrState = vr.pollState();
        }

        if (!g_pendingHotkeys.empty()) {
            const auto pending = std::move(g_pendingHotkeys);
            g_pendingHotkeys.clear();
            for (const auto action : pending) {
                applyHotkeyAction(action, vrState);
            }
        }

        if (vr.isInitialized() && vrState.runtimeRunning) {
            if (!seat.hasOriginalBackup()) {
                seat.captureOriginalBackup();
            }
            if (vrState.hmdValid && !seat.hasInitialHmdRawPose()) {
                seat.setInitialHmdRawPose(vrState.hmdRawPose);
            }

            // Left controller Y button (k_EButton_ApplicationMenu) -> Recenter origin to head pose.
            const bool leftMenuNow = vrState.left.valid && vrState.left.menuBtn;
            if (leftMenuNow && !prevLeftMenuBtn && vrState.hmdValid) {
                if (seat.recenterFromRawHmdPose(vrState.hmdRawPose)) {
                    vrState = vr.pollState();
                    lastHotkeyAction = "Origem recentralizada na cabeca (Botao Y)";
                    pendingHudReanchor = showVrHud;
                    vr.triggerHapticPulse(il2vr::VrHand::Left, 3000);
                    playBeepAsync(1040, 65);
                }
            }
            prevLeftMenuBtn = leftMenuNow;

            // Right thumbstick click (R3) -> Toggle HUD & anchor in front (leaves Button B free for IL-2!).
            const bool rightStickNow = vrState.right.valid && vrState.right.secondaryBtn;
            if (rightStickNow && !prevRightStickBtn) {
                showVrHud = !showVrHud;
                vr.setOverlayVisible(showVrHud);
                if (showVrHud && vrState.hmdValid) {
                    pendingHudReanchor = true;
                }
                lastHotkeyAction = showVrHud ? "HUD VR Aberto (R3)" : "HUD VR Oculto (R3)";
                vr.triggerHapticPulse(il2vr::VrHand::Right, 2400);
                playBeepAsync(showVrHud ? 950 : 520, 50);
            }
            prevRightStickBtn = rightStickNow;

            // Left thumbstick click (L3) -> Toggle between VTOL VR Flight Mode and Origin Grab Mode.
            const bool leftStickNow = vrState.left.valid && vrState.left.secondaryBtn;
            if (leftStickNow && !prevLeftStickBtn) {
                toggleOriginGrabMode();
            }
            prevLeftStickBtn = leftStickNow;

            const bool leftGripNow = vrState.left.valid && vrState.left.grip;
            const bool rightGripNow = vrState.right.valid && vrState.right.grip;

            auto rawPosOf = [](const il2vr::ControllerState& c) -> il2vr::Vec3 {
                return {c.rawPose.m[0][3], c.rawPose.m[1][3], c.rawPose.m[2][3]};
            };

            if (originGrabModeEnabled) {
                // 3D Space Drag ("Agarrar no ar" via Left or Right Grip) -> 3D translation ONLY, zero yaw change.
                if (!spaceDragActive) {
                    if (leftGripNow || rightGripNow) {
                        spaceDragActive = true;
                        spaceDragHand = leftGripNow ? il2vr::VrHand::Left : il2vr::VrHand::Right;
                        const auto& ctrl = (spaceDragHand == il2vr::VrHand::Left) ? vrState.left : vrState.right;
                        dragStartHandRawPos = rawPosOf(ctrl);
                        dragStartOffset = seat.offset();
                        dragStartRecenterBase = seat.recenterBase();
                        lastDragCommitTime = now - std::chrono::milliseconds(40);
                        lastHotkeyAction = "Arraste 3D (Grip) iniciado";
                        vr.triggerHapticPulse(spaceDragHand, 1800);
                    }
                } else {
                    const bool activeGripStillDown =
                        (spaceDragHand == il2vr::VrHand::Left) ? leftGripNow : rightGripNow;
                    const bool otherGripDown =
                        (spaceDragHand == il2vr::VrHand::Left) ? rightGripNow : leftGripNow;

                    auto computeDraggedOffset = [&](const il2vr::ControllerState& ctrl) -> il2vr::SeatOffset {
                        const il2vr::Vec3 curRaw = rawPosOf(ctrl);
                        const il2vr::Vec3 deltaRaw{
                            curRaw.x - dragStartHandRawPos.x,
                            curRaw.y - dragStartHandRawPos.y,
                            curRaw.z - dragStartHandRawPos.z
                        };
                        const il2vr::Vec3 deltaPilot = il2vr::transformDirection(
                            il2vr::invertRigid(dragStartRecenterBase), deltaRaw);
                        const float sign = invertSpaceDrag ? 1.0f : -1.0f;
                        il2vr::SeatOffset next = dragStartOffset;
                        next.x += sign * spaceDragScale * deltaPilot.x;
                        next.y += sign * spaceDragScale * deltaPilot.y;
                        next.z += sign * spaceDragScale * deltaPilot.z;
                        return next;
                    };

                    if (activeGripStillDown) {
                        if ((now - lastDragCommitTime) >= std::chrono::milliseconds(33)) {
                            const auto& ctrl = (spaceDragHand == il2vr::VrHand::Left) ? vrState.left : vrState.right;
                            seat.setOffset(computeDraggedOffset(ctrl));
                            if (showVrHud) {
                                vrState = vr.pollState();
                                pendingHudReanchor = true;
                            }
                            lastDragCommitTime = now;
                        }
                    } else if (otherGripDown) {
                        spaceDragHand = (spaceDragHand == il2vr::VrHand::Left)
                            ? il2vr::VrHand::Right
                            : il2vr::VrHand::Left;
                        const auto& ctrl = (spaceDragHand == il2vr::VrHand::Left) ? vrState.left : vrState.right;
                        dragStartHandRawPos = rawPosOf(ctrl);
                        dragStartOffset = seat.offset();
                        dragStartRecenterBase = seat.recenterBase();
                        lastDragCommitTime = now;
                    } else {
                        const auto& ctrl = (spaceDragHand == il2vr::VrHand::Left) ? vrState.left : vrState.right;
                        if (ctrl.valid) {
                            seat.setOffset(computeDraggedOffset(ctrl));
                            if (showVrHud) {
                                vrState = vr.pollState();
                                pendingHudReanchor = true;
                            }
                        }
                        spaceDragActive = false;
                        lastHotkeyAction = "Arraste 3D (Grip) concluido";
                        vr.triggerHapticPulse(spaceDragHand, 1200);
                    }
                }
            } else {
                spaceDragActive = false;

                // VTOL VR 6DoF Virtual Flight Controls:
                // Right Grip grabs 6DoF virtual control stick (returns to center when released).
                if (rightGripNow) {
                    if (!vtolStickGrabbed) {
                        vtolStickGrabbed = true;
                        stickGrabStartPose = vrState.right.pose;
                        stickGrabRefYawDeg = vrState.hmdValid
                            ? il2vr::extractYawDeg(vrState.hmdSeatedPose)
                            : il2vr::extractYawDeg(vrState.right.pose);
                        stickFilterX.reset();
                        stickFilterY.reset();
                        rudderFilterRz.reset();
                        stickHighDeflectionHapticLatched = false;
                        vr.triggerHapticPulse(il2vr::VrHand::Right, 1600);
                    }
                    const il2vr::VtolStickOutput stickOut = il2vr::computeVtolStickInput(
                        stickGrabStartPose,
                        vrState.right.pose,
                        stickGrabRefYawDeg,
                        vtolStickCfg);
                    testStickX = stickFilterX.filter(stickOut.rollX, dtSeconds);
                    testStickY = stickFilterY.filter(stickOut.pitchY, dtSeconds);
                    const float thumbRudder = (vrState.left.valid && std::abs(vrState.left.joyX) > 0.08f)
                        ? vrState.left.joyX
                        : 0.0f;
                    const float combinedRudder = (std::abs(thumbRudder) > 0.08f)
                        ? thumbRudder
                        : (vtolStickCfg.useWristTwistRudder ? stickOut.rudderRz : 0.0f);
                    testRudder = rudderFilterRz.filter(combinedRudder, dtSeconds);

                    if (stickOut.deflectionMag > 0.85f) {
                        if (!stickHighDeflectionHapticLatched) {
                            vr.triggerHapticPulse(il2vr::VrHand::Right, 1100);
                            stickHighDeflectionHapticLatched = true;
                        }
                    } else if (stickOut.deflectionMag < 0.78f) {
                        stickHighDeflectionHapticLatched = false;
                    }
                } else {
                    if (vtolStickGrabbed) {
                        vtolStickGrabbed = false;
                        vr.triggerHapticPulse(il2vr::VrHand::Right, 1000);
                    }
                    const float thumbRoll = (vrState.right.valid && std::abs(vrState.right.joyX) > 0.12f)
                        ? vrState.right.joyX
                        : 0.0f;
                    const float thumbPitch = (vrState.right.valid && std::abs(vrState.right.joyY) > 0.12f)
                        ? -vrState.right.joyY
                        : 0.0f;
                    testStickX = stickFilterX.filter(thumbRoll, dtSeconds);
                    testStickY = stickFilterY.filter(thumbPitch, dtSeconds);
                    const float thumbRudder = (vrState.left.valid && std::abs(vrState.left.joyX) > 0.08f)
                        ? vrState.left.joyX
                        : 0.0f;
                    testRudder = rudderFilterRz.filter(thumbRudder, dtSeconds);
                }

                // Left Grip grabs virtual throttle lever (latches value when released).
                if (leftGripNow) {
                    const il2vr::Vec3 curLeftPos{
                        vrState.left.pose.m[0][3],
                        vrState.left.pose.m[1][3],
                        vrState.left.pose.m[2][3]
                    };
                    if (!vtolThrottleGrabbed) {
                        vtolThrottleGrabbed = true;
                        throttleGrabStartPos = curLeftPos;
                        throttleGrabStartVal = testThrottle;
                        throttleGrabRefYawDeg = vrState.hmdValid
                            ? il2vr::extractYawDeg(vrState.hmdSeatedPose)
                            : il2vr::extractYawDeg(vrState.left.pose);
                        vr.triggerHapticPulse(il2vr::VrHand::Left, 1600);
                    }
                    testThrottle = il2vr::computeVtolThrottleInput(
                        throttleGrabStartVal,
                        throttleGrabStartPos,
                        curLeftPos,
                        throttleGrabRefYawDeg,
                        vtolThrottleCfg);

                    int newDetent = -1;
                    if (testThrottle <= 0.02f) {
                        newDetent = 0;
                    } else if (std::abs(testThrottle - 0.50f) <= 0.025f) {
                        newDetent = 1;
                    } else if (testThrottle >= 0.98f) {
                        newDetent = 2;
                    }
                    if (newDetent != -1 && newDetent != lastThrottleDetent) {
                        vr.triggerHapticPulse(il2vr::VrHand::Left, (newDetent == 1) ? 1400 : 2000);
                    }
                    lastThrottleDetent = newDetent;
                } else {
                    if (vtolThrottleGrabbed) {
                        vtolThrottleGrabbed = false;
                        vr.triggerHapticPulse(il2vr::VrHand::Left, 900);
                    }
                    if (vrState.left.valid && std::abs(vrState.left.joyY) > 0.22f) {
                        testThrottle = std::clamp(
                            testThrottle + vrState.left.joyY * 0.45f * dtSeconds,
                            0.0f,
                            1.0f);
                    }
                }

                if (mirrorVrControllers && vjoy.isAcquired()) {
                    vjoy.setAxisNormalized(il2vr::VJoyAxis::X, testStickX);
                    vjoy.setAxisNormalized(il2vr::VJoyAxis::Y, testStickY);
                    vjoy.setThrottleNormalized(il2vr::VJoyAxis::Z, testThrottle);
                    vjoy.setAxisNormalized(il2vr::VJoyAxis::Rz, testRudder);
                }
            }
        } else {
            prevLeftMenuBtn = false;
            prevLeftStickBtn = false;
            prevRightStickBtn = false;
            spaceDragActive = false;
            vtolStickGrabbed = false;
            vtolThrottleGrabbed = false;
        }

        if (autoBindAxis != AutoBindAxis::None) {
            autoBindElapsedSec += dtSeconds;
            const float t = autoBindElapsedSec;
            float v = 0.0f;
            if (t < 1.4f) {
                v = 0.0f;
            } else if (t < 2.2f) {
                v = std::clamp((t - 1.4f) / 0.45f, 0.0f, 1.0f);
            } else if (t < 3.2f) {
                v = 1.0f - 2.0f * std::clamp((t - 2.2f) / 0.65f, 0.0f, 1.0f);
            } else {
                v = -1.0f + std::clamp((t - 3.2f) / 0.60f, 0.0f, 1.0f);
            }

            testStickX = 0.0f;
            testStickY = 0.0f;
            testRudder = 0.0f;
            testThrottle = 0.5f;

            switch (autoBindAxis) {
            case AutoBindAxis::RollX:     testStickX = v; break;
            case AutoBindAxis::PitchY:    testStickY = v; break;
            case AutoBindAxis::ThrottleZ: testThrottle = (v + 1.0f) * 0.5f; break;
            case AutoBindAxis::RudderRz:  testRudder = v; break;
            case AutoBindAxis::None:      break;
            }

            if (vjoy.isAcquired()) {
                vjoy.setAxisNormalized(il2vr::VJoyAxis::X, testStickX);
                vjoy.setAxisNormalized(il2vr::VJoyAxis::Y, testStickY);
                vjoy.setThrottleNormalized(il2vr::VJoyAxis::Z, testThrottle);
                vjoy.setAxisNormalized(il2vr::VJoyAxis::Rz, testRudder);
            }

            if (autoBindElapsedSec >= kAutoBindDurationSec) {
                autoBindAxis = AutoBindAxis::None;
                testStickX = 0.0f;
                testStickY = 0.0f;
                testRudder = 0.0f;
                testThrottle = 0.5f;
                lastHotkeyAction = "Auto-Bind concluido!";
                playBeepAsync(1180, 80);
            }
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();

        // Update HUD anchor & VR controller raycaster between Win32_NewFrame and ImGui::NewFrame
        // so ImGui uses exact DisplaySize and receives VR laser mouse input cleanly.
        std::optional<il2vr::HudQuadHit> vrHudHit;
        il2vr::VrHand activeRayHand = preferredRayHand;
        if (vr.isInitialized() && vrState.runtimeRunning) {
            const ImVec2 disp = ImGui::GetIO().DisplaySize;
            const float aspectHOverW = (disp.x > 1.0f && disp.y > 1.0f)
                ? (disp.y / disp.x)
                : (680.0f / 960.0f);
            vr.setHudAspectRatio(aspectHOverW);

            if ((pendingHudReanchor || (showVrHud && !vr.isHudAnchored())) && vrState.hmdValid) {
                vr.anchorHudToHead(vrState.hmdSeatedPose, 0.65f, -0.10f, 0.62f, aspectHOverW);
                pendingHudReanchor = false;
            }

            const bool anyGrabActive = spaceDragActive || vtolStickGrabbed || vtolThrottleGrabbed;
            if (showVrHud && vrState.hmdValid && !anyGrabActive) {
                auto tryHand = [&](il2vr::VrHand hand) -> std::optional<std::pair<il2vr::Ray, il2vr::HudQuadHit>> {
                    const auto& ctrl = (hand == il2vr::VrHand::Left) ? vrState.left : vrState.right;
                    if (!ctrl.valid) {
                        return std::nullopt;
                    }
                    const il2vr::Ray ray = makeControllerRay(ctrl.pose, rayPitchOffsetDeg);
                    const auto hit = il2vr::intersectRayHudQuad(
                        ray, vr.hudSeatedPose(), vr.hudWidthM(), vr.hudHeightM());
                    if (!hit.has_value()) {
                        return std::nullopt;
                    }
                    return std::make_pair(ray, *hit);
                };

                const il2vr::VrHand otherHand = (preferredRayHand == il2vr::VrHand::Right)
                    ? il2vr::VrHand::Left
                    : il2vr::VrHand::Right;
                auto best = tryHand(preferredRayHand);
                if (best.has_value()) {
                    activeRayHand = preferredRayHand;
                } else {
                    best = tryHand(otherHand);
                    if (best.has_value()) {
                        activeRayHand = otherHand;
                        preferredRayHand = otherHand;
                    }
                }

                if (best.has_value()) {
                    vrHudHit = best->second;
                    const float mouseX = vrHudHit->u * disp.x;
                    const float mouseY = vrHudHit->v * disp.y;
                    const auto& pointingCtrl = (activeRayHand == il2vr::VrHand::Left)
                        ? vrState.left
                        : vrState.right;
                    const bool clickDown = pointingCtrl.triggerClick || pointingCtrl.primaryBtn;

                    ImGuiIO& io = ImGui::GetIO();
                    io.MouseDrawCursor = false;
                    io.AddMousePosEvent(mouseX, mouseY);
                    io.AddMouseButtonEvent(0, clickDown);

                    if (!vrHudWasHit || (clickDown && !prevRayClickDown)) {
                        vr.triggerHapticPulse(activeRayHand, clickDown ? 1500 : 700);
                    }
                    prevRayClickDown = clickDown;

                    const il2vr::Vec3 eyePos{
                        vrState.hmdSeatedPose.m[0][3],
                        vrState.hmdSeatedPose.m[1][3],
                        vrState.hmdSeatedPose.m[2][3]
                    };
                    vr.updateLaserOverlay(true, best->first.origin, vrHudHit->worldPoint, eyePos);
                } else {
                    if (vrHudWasHit) {
                        ImGui::GetIO().AddMouseButtonEvent(0, false);
                    }
                    prevRayClickDown = false;
                    vr.updateLaserOverlay(false, {}, {}, {});
                }
            } else {
                if (vrHudWasHit) {
                    ImGui::GetIO().AddMouseButtonEvent(0, false);
                }
                prevRayClickDown = false;
                vr.updateLaserOverlay(false, {}, {}, {});
            }
            vrHudWasHit = vrHudHit.has_value();

            if (autoBindAxis == AutoBindAxis::None && !originGrabModeEnabled && mirrorVrControllers && vjoy.isAcquired()) {
                const bool rightBlockedByHud =
                    showVrHud && vrHudHit.has_value() && activeRayHand == il2vr::VrHand::Right;
                const bool leftBlockedByHud =
                    showVrHud && vrHudHit.has_value() && activeRayHand == il2vr::VrHand::Left;
                vjoy.setButton(1, !rightBlockedByHud && vrState.right.valid && vrState.right.triggerClick);
                vjoy.setButton(2, !rightBlockedByHud && vrState.right.valid && vrState.right.primaryBtn);
                vjoy.setButton(3, !leftBlockedByHud && vrState.left.valid && vrState.left.triggerClick);
            } else if (mirrorVrControllers && vjoy.isAcquired()) {
                vjoy.setButton(1, false);
                vjoy.setButton(2, false);
                vjoy.setButton(3, false);
            }
        } else {
            if (vrHudWasHit) {
                ImGui::GetIO().AddMouseButtonEvent(0, false);
            }
            vrHudWasHit = false;
            prevRayClickDown = false;
        }

        ImGui::NewFrame();

        const ImVec2 canvas = ImGui::GetIO().DisplaySize;
        const float halfW = std::max(300.0f, (canvas.x - 24.0f) * 0.5f);
        const float fullH = std::max(300.0f, canvas.y - 16.0f);

        ImGui::SetNextWindowPos(ImVec2(8.0f, 8.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(halfW, fullH), ImGuiCond_Always);
        ImGui::Begin(
            "Pilar 1 - Posicionamento & Origem IL-2",
            nullptr,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

        // Prominent Mode Toggle button (clickable with VR laser or L3).
        ImGui::PushStyleColor(
            ImGuiCol_Button,
            originGrabModeEnabled ? ImVec4(0.75f, 0.38f, 0.08f, 1.0f) : ImVec4(0.10f, 0.50f, 0.42f, 1.0f));
        ImGui::PushStyleColor(
            ImGuiCol_ButtonHovered,
            originGrabModeEnabled ? ImVec4(0.88f, 0.48f, 0.14f, 1.0f) : ImVec4(0.15f, 0.65f, 0.55f, 1.0f));
        const char* modeBtnLabel = originGrabModeEnabled
            ? "MODO: AJUSTAR ORIGEM (Grip = Arrastar Mundo) [L3]"
            : "MODO: PILOTAGEM VTOL VR (Grip = Manche/Manete) [L3]";
        if (ImGui::Button(modeBtnLabel, ImVec2(-1.0f, 40.0f))) {
            toggleOriginGrabMode();
        }
        ImGui::PopStyleColor(2);

        if (il2Pid != 0) {
            ImGui::TextColored(
                ImVec4(0.35f, 0.95f, 0.55f, 1.0f),
                "SteamVR: %s (Ovl:%s) | IL-2: RODANDO (PID %u) | HUD: %s",
                vr.isInitialized() ? "CONECTADO" : "AGUARDANDO",
                vr.isOverlayMode() ? "Sim" : "Nao",
                il2Pid,
                showVrHud ? "VISIVEL" : "OCULTO");
        } else {
            ImGui::Text(
                "SteamVR: %s (Ovl:%s) | IL-2: Aguardando | HUD: %s",
                vr.isInitialized() ? "CONECTADO" : "AGUARDANDO",
                vr.isOverlayMode() ? "Sim" : "Nao",
                showVrHud ? "VISIVEL" : "OCULTO");
        }
        if (!vr.isInitialized()) {
            ImGui::TextWrapped("Status: %s", vr.lastError().c_str());
            if (ImGui::Button("Conectar ao SteamVR Agora", ImVec2(-1.0f, 32.0f))) {
                if (vr.init(false)) {
                    vr.setOverlayVisible(showVrHud);
                    seat.attachBackend(&vr);
                    pendingHudReanchor = showVrHud;
                }
            }
        } else {
            if (vrState.hmdValid) {
                ImGui::Text("HMD(%.2f, %.2f, %.2f) Yaw=%.1f deg | Arraste: %s",
                    vrState.hmdSeatedPose.m[0][3],
                    vrState.hmdSeatedPose.m[1][3],
                    vrState.hmdSeatedPose.m[2][3],
                    il2vr::extractYawDeg(vrState.hmdSeatedPose),
                    spaceDragActive ? "ATIVO" : (originGrabModeEnabled ? "Pronto" : "Off(VTOL)"));
            } else {
                ImGui::Text("HMD: Aguardando tracking | Arraste: %s",
                    spaceDragActive ? "ATIVO" : (originGrabModeEnabled ? "Pronto" : "Off(VTOL)"));
            }
            ImGui::Text("Ctrl Esq: %s (0x%llX) | Ctrl Dir: %s (0x%llX)",
                vrState.left.valid ? "Ok" : "--",
                static_cast<unsigned long long>(vrState.left.buttonPressedMask),
                vrState.right.valid ? "Ok" : "--",
                static_cast<unsigned long long>(vrState.right.buttonPressedMask));
        }

        ImGui::Separator();
        if (ImGui::Button("Recentralizar Origem na Cabeca (Botao Y)", ImVec2(-1.0f, 34.0f))) {
            if (vr.isInitialized() && vrState.hmdValid) {
                if (!seat.hasOriginalBackup()) {
                    seat.captureOriginalBackup();
                }
                if (!seat.hasInitialHmdRawPose()) {
                    seat.setInitialHmdRawPose(vrState.hmdRawPose);
                }
                seat.recenterFromRawHmdPose(vrState.hmdRawPose);
                vrState = vr.pollState();
                pendingHudReanchor = showVrHud;
                lastHotkeyAction = "Origem recentralizada na cabeca (Botao HUD)";
            } else {
                seat.recenterNow();
            }
        }
        const float btnW = (halfW - 28.0f) * 0.5f;
        if (ImGui::Button("NumPad 5 -> Recenter IL-2", ImVec2(btnW, 30.0f))) {
            sendIl2NativeRecenter();
        }
        ImGui::SameLine();
        if (ImGui::Button("Painel na Frente", ImVec2(btnW, 30.0f))) {
            if (vr.isInitialized() && vrState.hmdValid) {
                vr.anchorHudToHead(
                    vrState.hmdSeatedPose,
                    0.65f,
                    -0.10f,
                    0.62f,
                    canvas.y / std::max(canvas.x, 1.0f));
                lastHotkeyAction = "Painel HUD reposicionado na sua frente";
            }
        }
        if (ImGui::Checkbox("Mostrar HUD VR (R3 / Ctrl+Alt+O)", &showVrHud)) {
            vr.setOverlayVisible(showVrHud);
            if (showVrHud && vrState.hmdValid) {
                pendingHudReanchor = true;
            }
        }

        ImGui::Checkbox("Inverter Arraste 3D (Grip)", &invertSpaceDrag);
        ImGui::SliderFloat("Vel. Arraste", &spaceDragScale, 0.5f, 2.0f, "%.2fx");
        ImGui::SliderFloat("Inclinacao Laser", &rayPitchOffsetDeg, -45.0f, 0.0f, "%.0f deg");

        ImGui::Separator();
        const auto& off = seat.offset();
        ImGui::Text("Offset: X=%.1fcm Y=%.1fcm Z=%.1fcm Yaw=%.1f",
            off.x * 100.0f, off.y * 100.0f, off.z * 100.0f, off.yawDeg);
        ImGui::Checkbox("Passo Fino (2 mm / 0.5 deg)", &fineStep);

        if (ImGui::Button("Frente (-Z)", ImVec2(btnW, 28.0f))) { seat.nudgeStep(0, 0, -1, 0, fineStep); pendingHudReanchor = showVrHud; }
        ImGui::SameLine();
        if (ImGui::Button("Tras (+Z)", ImVec2(btnW, 28.0f))) { seat.nudgeStep(0, 0, 1, 0, fineStep); pendingHudReanchor = showVrHud; }

        if (ImGui::Button("Esquerda (-X)", ImVec2(btnW, 28.0f))) { seat.nudgeStep(-1, 0, 0, 0, fineStep); pendingHudReanchor = showVrHud; }
        ImGui::SameLine();
        if (ImGui::Button("Direita (+X)", ImVec2(btnW, 28.0f))) { seat.nudgeStep(1, 0, 0, 0, fineStep); pendingHudReanchor = showVrHud; }

        if (ImGui::Button("Subir (+Y)", ImVec2(btnW, 28.0f))) { seat.nudgeStep(0, 1, 0, 0, fineStep); pendingHudReanchor = showVrHud; }
        ImGui::SameLine();
        if (ImGui::Button("Descer (-Y)", ImVec2(btnW, 28.0f))) { seat.nudgeStep(0, -1, 0, 0, fineStep); pendingHudReanchor = showVrHud; }

        if (ImGui::Button("Girar Esq (+Yaw)", ImVec2(btnW, 28.0f))) { seat.nudgeStep(0, 0, 0, 1, fineStep); pendingHudReanchor = showVrHud; }
        ImGui::SameLine();
        if (ImGui::Button("Girar Dir (-Yaw)", ImVec2(btnW, 28.0f))) { seat.nudgeStep(0, 0, 0, -1, fineStep); pendingHudReanchor = showVrHud; }

        if (ImGui::Button("Zerar Offsets", ImVec2(btnW, 30.0f))) {
            seat.resetOffset();
            pendingHudReanchor = showVrHud;
            lastHotkeyAction = "Offsets zerados";
        }
        ImGui::SameLine();
        if (ImGui::Button("Restaurar Original", ImVec2(btnW, 30.0f))) {
            seat.restoreOriginalPose();
            pendingHudReanchor = showVrHud;
            lastHotkeyAction = "Chaperone original restaurado";
        }

        if (ImGui::Button("Teste Visivel (+25 cm Altura)", ImVec2(-1.0f, 28.0f))) {
            toggleVisibleTestOffset();
            pendingHudReanchor = showVrHud;
        }

        ImGui::Text("Ultima acao: %s", lastHotkeyAction.c_str());
        ImGui::End();

        const float rightX = halfW + 16.0f;
        const float topRightH = fullH * 0.72f;
        ImGui::SetNextWindowPos(ImVec2(rightX, 8.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(halfW, topRightH), ImGuiCond_Always);
        ImGui::Begin(
            "Pilar 2 - Pilotagem VTOL VR (vJoy)",
            nullptr,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
        ImGui::Text("vJoy Driver: %s | Device 1: %s | IL-2: joy%d",
            vjoy.isDriverEnabled() ? "Sim" : "Nao",
            vjoy.isAcquired() ? "Adquirido" : "Nao adquirido",
            il2VJoyConfigId);
        ImGui::TextColored(
            vjoyAxesReady ? ImVec4(0.35f, 0.95f, 0.55f, 1.0f) : ImVec4(1.0f, 0.35f, 0.30f, 1.0f),
            "Eixos vJoy: %s",
            vjoyAxesDesc.c_str());

        if (!vjoyAxesReady) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.18f, 0.14f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.25f, 0.20f, 1.0f));
            if (ImGui::Button("ATIVAR EIXOS Y/Z/Rz NO VJOY (Admin)", ImVec2(-1.0f, 32.0f))) {
                const bool ok = vjoy.configureAllAxesWithAdmin(1);
                vjoyAxesReady = vjoy.hasRequiredFlightAxes(1);
                vjoyAxesDesc = vjoy.describeEnabledAxes(1);
                lastHotkeyAction = ok
                    ? "vJoy Device 1 reconfigurado com todos os 8 eixos!"
                    : "Reconfiguracao vJoy pendente (reabra vJoy como Admin)";
                playBeepAsync(ok ? 1100 : 520, 80);
            }
            ImGui::PopStyleColor(2);
        }

        if (!vjoy.isAcquired()) {
            if (ImGui::Button("Adquirir vJoy Device 1", ImVec2(-1.0f, 32.0f))) {
                if (vjoy.acquire(1)) {
                    mirrorVrControllers = true;
                }
            }
        } else {
            if (ImGui::Button("Liberar vJoy")) {
                vjoy.reset();
                vjoy.relinquish();
                mirrorVrControllers = false;
            }
            ImGui::SameLine();
            ImGui::Checkbox("Enviar para vJoy", &mirrorVrControllers);
            ImGui::SameLine();
            if (ImGui::Button("Sincronizar IL-2")) {
                if (!il2InputDir.empty()) {
                    il2VJoyConfigId = detectIl2VJoyConfigId(il2InputDir);
                    syncIl2InputProfilesForVJoy(il2InputDir, il2VJoyConfigId, il2SyncStatus);
                }
                lastHotkeyAction = il2SyncStatus;
                playBeepAsync(920, 50);
            }
        }

        ImGui::Text("Manche (Dir Grip): %s | Manete (Esq Grip): %s",
            vtolStickGrabbed ? "AGARRADO" : "SOLTO",
            vtolThrottleGrabbed ? "AGARRADA" : "TRAVADA");

        if (ImGui::SliderFloat("Curso Manche (cm)", &vtolStickRangeCm, 5.0f, 25.0f, "%.0f cm")) {
            vtolStickCfg.rangeM = vtolStickRangeCm * 0.01f;
        }
        if (ImGui::SliderFloat("Curso Manete (cm)", &vtolThrottleRangeCm, 8.0f, 30.0f, "%.0f cm")) {
            vtolThrottleCfg.rangeM = vtolThrottleRangeCm * 0.01f;
        }
        ImGui::Checkbox("Leme por Torcao do Pulso Dir (Rz)", &vtolStickCfg.useWristTwistRudder);

        ImGui::Separator();
        char axisOverlay[64]{};
        std::snprintf(axisOverlay, sizeof(axisOverlay), "Roll (X): %+.2f", testStickX);
        ImGui::ProgressBar((testStickX + 1.0f) * 0.5f, ImVec2(-1.0f, 20.0f), axisOverlay);

        std::snprintf(axisOverlay, sizeof(axisOverlay), "Pitch (Y): %+.2f", testStickY);
        ImGui::ProgressBar((testStickY + 1.0f) * 0.5f, ImVec2(-1.0f, 20.0f), axisOverlay);

        std::snprintf(axisOverlay, sizeof(axisOverlay), "Leme (Rz): %+.2f", testRudder);
        ImGui::ProgressBar((testRudder + 1.0f) * 0.5f, ImVec2(-1.0f, 20.0f), axisOverlay);

        if (ImGui::SliderFloat("Acelerador (Z)", &testThrottle, 0.0f, 1.0f, "%.2f")) {
            if (vjoy.isAcquired() && mirrorVrControllers) {
                vjoy.setThrottleNormalized(il2vr::VJoyAxis::Z, testThrottle);
            }
        }
        std::snprintf(axisOverlay, sizeof(axisOverlay), "Potencia: %.0f%%", testThrottle * 100.0f);
        ImGui::ProgressBar(testThrottle, ImVec2(-1.0f, 22.0f), axisOverlay);

        ImGui::Separator();
        ImGui::Text("Auto-Bind IL-2 (varredura limpa sem tremor):");
        const float bindBtnW = (halfW - 40.0f) * 0.25f;
        if (ImGui::Button("Bind Roll X", ImVec2(bindBtnW, 28.0f))) {
            startAutoBind(AutoBindAxis::RollX, "Roll (X)");
        }
        ImGui::SameLine();
        if (ImGui::Button("Bind Pitch Y", ImVec2(bindBtnW, 28.0f))) {
            startAutoBind(AutoBindAxis::PitchY, "Pitch (Y)");
        }
        ImGui::SameLine();
        if (ImGui::Button("Bind Manete Z", ImVec2(bindBtnW, 28.0f))) {
            startAutoBind(AutoBindAxis::ThrottleZ, "Acelerador (Z)");
        }
        ImGui::SameLine();
        if (ImGui::Button("Bind Leme Rz", ImVec2(bindBtnW, 28.0f))) {
            startAutoBind(AutoBindAxis::RudderRz, "Leme (Rz)");
        }
        if (autoBindAxis != AutoBindAxis::None) {
            const char* phaseLabel = (autoBindElapsedSec < 1.4f)
                ? "Centro (aguarde...)"
                : (autoBindElapsedSec < 2.2f)
                    ? "Varrendo +100% (Max)"
                    : (autoBindElapsedSec < 3.2f)
                        ? "Varrendo -100% (Min)"
                        : "Voltando ao Centro...";
            ImGui::ProgressBar(
                std::clamp(autoBindElapsedSec / kAutoBindDurationSec, 0.0f, 1.0f),
                ImVec2(-1.0f, 20.0f),
                phaseLabel);
        }
        ImGui::End();

        ImGui::SetNextWindowPos(ImVec2(rightX, topRightH + 16.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(halfW, fullH - topRightH - 8.0f), ImGuiCond_Always);
        ImGui::Begin(
            "Guia Quest 3 & Telemetria UDP",
            nullptr,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
        ImGui::TextWrapped(
            "Quest 3:\n"
            "  R3 (Clique Analogico Dir): Abrir/Fechar este HUD\n"
            "  L3 (Clique Analogico Esq): Alternar Modo VTOL VR <-> Ajustar Origem\n"
            "  Segurar GRIP Dir/Esq (VTOL VR): Manche 6DoF / Manete Travada\n"
            "  Segurar GRIP (Modo Origem): Arrastar cockpit 3D (sem girar)\n"
            "  Botao Y (Esq): Centralizar Origem na cabeca | Botao B: Livre IL-2\n"
            "  Laser + Gatilho (ou A/X): Clicar/arrastar botoes e sliders");
        ImGui::Separator();
        ImGui::InputInt("Porta UDP", &telemetryPort);
        if (!telemetry.isRunning()) {
            if (ImGui::Button("Iniciar Listener UDP")) {
                telemetry.start("127.0.0.1", static_cast<uint16_t>(telemetryPort));
            }
        } else {
            if (ImGui::Button("Parar Listener UDP")) {
                telemetry.stop();
            }
        }
        ImGui::SameLine();
        ImGui::Text("Pacotes: %llu | FPS: %.1f",
            static_cast<unsigned long long>(telemetry.packetCount()),
            ImGui::GetIO().Framerate);
        ImGui::End();

        if (vrHudHit.has_value()) {
            const ImVec2 cursorPos(vrHudHit->u * canvas.x, vrHudHit->v * canvas.y);
            ImDrawList* fg = ImGui::GetForegroundDrawList();
            const ImU32 glowCol = prevRayClickDown
                ? IM_COL32(255, 220, 60, 255)
                : IM_COL32(50, 255, 195, 255);
            fg->AddCircleFilled(cursorPos, prevRayClickDown ? 8.0f : 6.0f, glowCol);
            fg->AddCircle(cursorPos, prevRayClickDown ? 13.0f : 11.0f, IM_COL32(255, 255, 255, 230), 0, 2.2f);
        }

        ImGui::Render();
        const float clearColor[4] = {0.10f, 0.12f, 0.15f, 1.0f};
        g_d3dContext->OMSetRenderTargets(1, &g_renderTargetView, nullptr);
        g_d3dContext->ClearRenderTargetView(g_renderTargetView, clearColor);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

        if (vr.isInitialized() && vr.isOverlayVisible()) {
            ID3D11Texture2D* backBuffer = nullptr;
            if (SUCCEEDED(g_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer))) && backBuffer != nullptr) {
                vr.submitOverlayTextureD3D11(backBuffer);
                backBuffer->Release();
            }
        }

        g_swapChain->Present(1, 0);

        if ((now - lastStatusWriteTime) >= std::chrono::seconds(1)) {
            lastStatusWriteTime = now;
            std::ofstream statusOut("il2vr_status.txt", std::ios::trunc);
            if (statusOut.is_open()) {
                const auto wallNow = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
                std::tm tmLocal{};
                localtime_s(&tmLocal, &wallNow);
                char timeBuf[32]{};
                std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", &tmLocal);
                const auto uptimeSec = std::chrono::duration_cast<std::chrono::seconds>(now - appStartTime).count();
                const auto& curOffset = seat.offset();

                statusOut
                    << "Timestamp: " << timeBuf << " (uptime=" << uptimeSec << "s)\n"
                    << "PID: " << GetCurrentProcessId() << "\n"
                    << "SteamVR: " << (vr.isInitialized() ? "Conectado" : "Desconectado")
                    << " | LastError: " << (vr.lastError().empty() ? "none" : vr.lastError()) << "\n"
                    << "OverlayMode: " << (vr.isOverlayMode() ? "Sim" : "Nao")
                    << " | HUDVisible: " << (showVrHud ? "Sim" : "Nao") << "\n"
                    << "IL2_PID: " << il2Pid << "\n"
                    << "Modo: " << (originGrabModeEnabled ? "AJUSTAR_ORIGEM" : "PILOTAGEM_VTOL_VR") << "\n"
                    << "HMD: valid=" << (vrState.hmdValid ? 1 : 0)
                    << " pos=(" << vrState.hmdSeatedPose.m[0][3] << ", "
                    << vrState.hmdSeatedPose.m[1][3] << ", "
                    << vrState.hmdSeatedPose.m[2][3] << ")"
                    << " yawDeg=" << il2vr::extractYawDeg(vrState.hmdSeatedPose) << "\n"
                    << "LeftCtrl: valid=" << (vrState.left.valid ? 1 : 0)
                    << " grip=" << (vrState.left.grip ? 1 : 0)
                    << " trigger=" << (vrState.left.triggerClick ? 1 : 0)
                    << " mask=0x" << std::hex << vrState.left.buttonPressedMask << std::dec << "\n"
                    << "RightCtrl: valid=" << (vrState.right.valid ? 1 : 0)
                    << " grip=" << (vrState.right.grip ? 1 : 0)
                    << " trigger=" << (vrState.right.triggerClick ? 1 : 0)
                    << " mask=0x" << std::hex << vrState.right.buttonPressedMask << std::dec << "\n"
                    << "Offset: x=" << curOffset.x << " y=" << curOffset.y << " z=" << curOffset.z
                    << " yawDeg=" << curOffset.yawDeg << "\n"
                    << "vJoy: acquired=" << (vjoy.isAcquired() ? 1 : 0)
                    << " mirror=" << (mirrorVrControllers ? 1 : 0)
                    << " X=" << testStickX
                    << " Y=" << testStickY
                    << " Z=" << testThrottle
                    << " Rz=" << testRudder << "\n"
                    << "EixosVJoy: " << vjoyAxesDesc
                    << " | IL2_vJoy_ID: joy" << il2VJoyConfigId << "\n"
                    << "UltimaAcao: " << lastHotkeyAction << "\n";
            }
        }
    }

    unregisterGlobalHotkeys(hwnd, registeredHotkeyIds);
    telemetry.stop();
    vjoy.relinquish();
    seat.restoreOriginalPose();
    vr.shutdown();

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    cleanupDeviceD3D();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}
