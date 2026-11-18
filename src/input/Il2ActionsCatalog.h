#pragma once

#include "input/KeybindTypes.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace il2vr {

struct Il2CatalogItem {
    std::string actionId;
    std::string labelEn;
    std::string labelPt;
    ActionCategory category = ActionCategory::AllIl2;
    int priorityRank = 100;
    std::vector<std::string> defaultKeyCombo;
    uint8_t suggestedVJoyButton = 0;
    bool recommendLongHold = false;
};

class Il2ActionsCatalog {
public:
    static const std::vector<Il2CatalogItem>& builtInItems();

    // Reads IL-2's data/input/current.actions (or global.actions) to map actionId -> real keyboard combo.
    static std::unordered_map<std::string, std::vector<std::string>> parseActionsText(const std::string& content);
    static std::unordered_map<std::string, std::vector<std::string>> loadUserActionKeyOverrides(
        const std::filesystem::path& il2InputDir);

    // Translates an IL-2 token string like "key_lcontrol+key_e" to {"lctrl", "e"}.
    static std::vector<std::string> translateIl2KeyTokenCombo(const std::string& il2Spec);
};

} // namespace il2vr
