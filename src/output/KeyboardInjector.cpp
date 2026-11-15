#include "output/KeyboardInjector.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <string>
#include <thread>
#include <unordered_map>

namespace il2vr {

namespace {

std::string toLower(std::string_view s) {
    std::string out(s);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

std::optional<std::pair<uint16_t, bool>> lookupVk(std::string_view name) {
    if (name.size() == 1) {
        const char ch = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
        if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) {
            return std::pair<uint16_t, bool>{static_cast<uint16_t>(ch), false};
        }
    }

    const std::string lower = toLower(name);
    static const std::unordered_map<std::string, std::pair<uint16_t, bool>> kMap = {
        {"lshift", {VK_LSHIFT, false}},
        {"rshift", {VK_RSHIFT, false}},
        {"shift", {VK_LSHIFT, false}},
        {"lctrl", {VK_LCONTROL, false}},
        {"rctrl", {VK_RCONTROL, true}},
        {"ctrl", {VK_LCONTROL, false}},
        {"lalt", {VK_LMENU, false}},
        {"ralt", {VK_RMENU, true}},
        {"alt", {VK_LMENU, false}},
        {"space", {VK_SPACE, false}},
        {"enter", {VK_RETURN, false}},
        {"return", {VK_RETURN, false}},
        {"esc", {VK_ESCAPE, false}},
        {"escape", {VK_ESCAPE, false}},
        {"tab", {VK_TAB, false}},
        {"backspace", {VK_BACK, false}},
        {"numpad0", {VK_NUMPAD0, false}},
        {"numpad1", {VK_NUMPAD1, false}},
        {"numpad2", {VK_NUMPAD2, false}},
        {"numpad3", {VK_NUMPAD3, false}},
        {"numpad4", {VK_NUMPAD4, false}},
        {"numpad5", {VK_NUMPAD5, false}},
        {"numpad6", {VK_NUMPAD6, false}},
        {"numpad7", {VK_NUMPAD7, false}},
        {"numpad8", {VK_NUMPAD8, false}},
        {"numpad9", {VK_NUMPAD9, false}},
        {"numpadadd", {VK_ADD, false}},
        {"numpadsubtract", {VK_SUBTRACT, false}},
        {"numpaddecimal", {VK_DECIMAL, false}},
        {"f1", {VK_F1, false}},
        {"f2", {VK_F2, false}},
        {"f3", {VK_F3, false}},
        {"f4", {VK_F4, false}},
        {"f5", {VK_F5, false}},
        {"f6", {VK_F6, false}},
        {"f7", {VK_F7, false}},
        {"f8", {VK_F8, false}},
        {"f9", {VK_F9, false}},
        {"f10", {VK_F10, false}},
        {"f11", {VK_F11, false}},
        {"f12", {VK_F12, false}},
        {"up", {VK_UP, true}},
        {"down", {VK_DOWN, true}},
        {"left", {VK_LEFT, true}},
        {"right", {VK_RIGHT, true}},
        {"insert", {VK_INSERT, true}},
        {"delete", {VK_DELETE, true}},
        {"home", {VK_HOME, true}},
        {"end", {VK_END, true}},
        {"pageup", {VK_PRIOR, true}},
        {"pagedown", {VK_NEXT, true}},
    };

    const auto it = kMap.find(lower);
    if (it != kMap.end()) {
        return it->second;
    }
    return std::nullopt;
}

} // namespace

std::optional<KeySpec> KeyboardInjector::resolveKey(std::string_view name) {
    const auto vkPair = lookupVk(name);
    if (!vkPair.has_value()) {
        return std::nullopt;
    }
    const UINT scan = MapVirtualKeyW(vkPair->first, MAPVK_VK_TO_VSC);
    if (scan == 0) {
        return std::nullopt;
    }
    return KeySpec{
        vkPair->first,
        static_cast<uint16_t>(scan & 0xFFu),
        vkPair->second
    };
}

bool KeyboardInjector::sendKeyComboState(const std::vector<std::string>& combo, bool down) {
    if (combo.empty()) {
        return false;
    }

    std::vector<KeySpec> specs;
    specs.reserve(combo.size());
    for (const auto& name : combo) {
        const auto resolved = resolveKey(name);
        if (!resolved.has_value()) {
            return false;
        }
        specs.push_back(*resolved);
    }

    if (!down) {
        std::reverse(specs.begin(), specs.end());
    }

    std::vector<INPUT> inputs;
    inputs.reserve(specs.size());
    for (const auto& spec : specs) {
        INPUT in{};
        in.type = INPUT_KEYBOARD;
        in.ki.wVk = 0;
        in.ki.wScan = spec.scanCode;
        in.ki.dwFlags = KEYEVENTF_SCANCODE;
        if (spec.extended) {
            in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        }
        if (!down) {
            in.ki.dwFlags |= KEYEVENTF_KEYUP;
        }
        inputs.push_back(in);
    }

    const UINT sent = SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
    return sent == inputs.size();
}

bool KeyboardInjector::sendKeyTap(const std::vector<std::string>& combo, int holdMs) {
    if (!sendKeyComboState(combo, true)) {
        return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(std::max(holdMs, 1)));
    return sendKeyComboState(combo, false);
}

bool KeyboardInjector::sendMouseMoveNormalized(float normX0To1, float normY0To1) {
    const float cx = std::clamp(normX0To1, 0.0f, 1.0f);
    const float cy = std::clamp(normY0To1, 0.0f, 1.0f);
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dx = static_cast<LONG>(std::lround(cx * 65535.0f));
    in.mi.dy = static_cast<LONG>(std::lround(cy * 65535.0f));
    in.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    return SendInput(1, &in, sizeof(INPUT)) == 1;
}

bool KeyboardInjector::sendMouseButton(bool leftButton, bool pressed) {
    INPUT in{};
    in.type = INPUT_MOUSE;
    if (leftButton) {
        in.mi.dwFlags = pressed ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    } else {
        in.mi.dwFlags = pressed ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
    }
    return SendInput(1, &in, sizeof(INPUT)) == 1;
}

bool KeyboardInjector::sendMouseWheel(int wheelDelta) {
    if (wheelDelta == 0) {
        return true;
    }
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = MOUSEEVENTF_WHEEL;
    in.mi.mouseData = static_cast<DWORD>(wheelDelta);
    return SendInput(1, &in, sizeof(INPUT)) == 1;
}

} // namespace il2vr
