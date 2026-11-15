#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace il2vr {

struct KeySpec {
    uint16_t vk = 0;
    uint16_t scanCode = 0;
    bool extended = false;
};

class KeyboardInjector {
public:
    static std::optional<KeySpec> resolveKey(std::string_view name);
    static bool sendKeyComboState(const std::vector<std::string>& combo, bool down);
    static bool sendKeyTap(const std::vector<std::string>& combo, int holdMs = 40);
};

} // namespace il2vr
