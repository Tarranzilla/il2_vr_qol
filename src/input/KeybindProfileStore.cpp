#include "input/KeybindProfileStore.h"

#include "input/Il2ActionsCatalog.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <unordered_set>

namespace il2vr {

namespace {

std::string toUpperCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return s;
}

KeybindEntry makeCompanionEntry(
    const char* id,
    const char* actionId,
    const char* labelEn,
    const char* labelPt,
    int priority,
    CompanionActionId compAction,
    const KeybindTrigger& trigger,
    bool showInCallout = true) {
    KeybindEntry e{};
    e.id = id;
    e.actionId = actionId;
    e.labelEn = labelEn;
    e.labelPt = labelPt;
    e.category = ActionCategory::CompanionVr;
    e.priorityRank = priority;
    e.outputType = ActionOutputType::CompanionAction;
    e.companionAction = compAction;
    e.trigger = trigger;
    e.enabled = true;
    e.showInCallout = showInCallout;
    return e;
}

KeybindEntry makeCatalogEntry(
    const Il2CatalogItem& item,
    const KeybindTrigger& trigger,
    bool forceVJoy = false,
    bool showInCallout = true) {
    KeybindEntry e{};
    e.id = "il2." + item.actionId;
    e.actionId = item.actionId;
    e.labelEn = item.labelEn;
    e.labelPt = item.labelPt;
    e.category = item.category;
    e.priorityRank = item.priorityRank;
    if (forceVJoy && item.suggestedVJoyButton > 0) {
        e.outputType = ActionOutputType::VJoyButton;
        e.vjoyButton = item.suggestedVJoyButton;
        e.keyCombo = item.defaultKeyCombo;
    } else {
        e.outputType = ActionOutputType::Il2KeyboardCombo;
        e.keyCombo = item.defaultKeyCombo;
        e.vjoyButton = item.suggestedVJoyButton;
    }
    e.trigger = trigger;
    e.enabled = true;
    e.showInCallout = showInCallout;
    return e;
}

nlohmann::json slotToJson(const VrInputSlot& s) {
    return {
        {"hand", (s.hand == VrHand::Left) ? "left" : "right"},
        {"button", buttonIdToToken(s.button)}
    };
}

VrInputSlot slotFromJson(const nlohmann::json& j) {
    VrInputSlot s{};
    if (!j.is_object()) {
        return s;
    }
    const std::string h = j.value("hand", std::string("right"));
    s.hand = (h == "left") ? VrHand::Left : VrHand::Right;
    s.button = tokenToButtonId(j.value("button", std::string("none")));
    return s;
}

} // namespace

const char* buttonIdToToken(VrButtonId id) noexcept {
    switch (id) {
    case VrButtonId::Trigger:    return "trigger";
    case VrButtonId::Grip:       return "grip";
    case VrButtonId::Primary:    return "primary";
    case VrButtonId::Secondary:  return "secondary";
    case VrButtonId::ThumbClick: return "thumb_click";
    case VrButtonId::StickUp:    return "stick_up";
    case VrButtonId::StickDown:  return "stick_down";
    case VrButtonId::StickLeft:  return "stick_left";
    case VrButtonId::StickRight: return "stick_right";
    case VrButtonId::None:
    default:                     return "none";
    }
}

VrButtonId tokenToButtonId(const std::string& token) noexcept {
    if (token == "trigger") return VrButtonId::Trigger;
    if (token == "grip") return VrButtonId::Grip;
    if (token == "primary") return VrButtonId::Primary;
    if (token == "secondary") return VrButtonId::Secondary;
    if (token == "thumb_click") return VrButtonId::ThumbClick;
    if (token == "stick_up") return VrButtonId::StickUp;
    if (token == "stick_down") return VrButtonId::StickDown;
    if (token == "stick_left") return VrButtonId::StickLeft;
    if (token == "stick_right") return VrButtonId::StickRight;
    return VrButtonId::None;
}

const char* gestureToToken(VrGesture g) noexcept {
    switch (g) {
    case VrGesture::Tap1x:     return "tap_1x";
    case VrGesture::Double2x:  return "double_2x";
    case VrGesture::Triple3x:  return "triple_3x";
    case VrGesture::LongHold:  return "long_hold";
    case VrGesture::WhileHeld: return "while_held";
    default:                   return "tap_1x";
    }
}

VrGesture tokenToGesture(const std::string& token) noexcept {
    if (token == "double_2x") return VrGesture::Double2x;
    if (token == "triple_3x") return VrGesture::Triple3x;
    if (token == "long_hold") return VrGesture::LongHold;
    if (token == "while_held") return VrGesture::WhileHeld;
    return VrGesture::Tap1x;
}

const char* companionActionToToken(CompanionActionId a) noexcept {
    switch (a) {
    case CompanionActionId::ToggleHud:          return "toggle_hud";
    case CompanionActionId::ToggleOriginMode:   return "toggle_origin_mode";
    case CompanionActionId::RecenterSeat:       return "recenter_seat";
    case CompanionActionId::ToggleVrMouse:      return "toggle_vr_mouse";
    case CompanionActionId::FlightStickHold:    return "flight_stick_hold";
    case CompanionActionId::FlightThrottleHold: return "flight_throttle_hold";
    case CompanionActionId::ToggleCalloutLayer: return "toggle_callout_layer";
    case CompanionActionId::None:
    default:                                    return "none";
    }
}

CompanionActionId tokenToCompanionAction(const std::string& token) noexcept {
    if (token == "toggle_hud") return CompanionActionId::ToggleHud;
    if (token == "toggle_origin_mode") return CompanionActionId::ToggleOriginMode;
    if (token == "recenter_seat") return CompanionActionId::RecenterSeat;
    if (token == "toggle_vr_mouse") return CompanionActionId::ToggleVrMouse;
    if (token == "flight_stick_hold") return CompanionActionId::FlightStickHold;
    if (token == "flight_throttle_hold") return CompanionActionId::FlightThrottleHold;
    if (token == "toggle_callout_layer") return CompanionActionId::ToggleCalloutLayer;
    return CompanionActionId::None;
}

std::string formatVrSlotLabel(const VrInputSlot& slot, bool portuguese) {
    if (!slot.isValid()) {
        return portuguese ? "--" : "--";
    }
    const bool isLeft = (slot.hand == VrHand::Left);
    const char* handPrefix = isLeft ? (portuguese ? "Esq " : "L ") : (portuguese ? "Dir " : "R ");
    switch (slot.button) {
    case VrButtonId::Trigger:
        return std::string(handPrefix) + (portuguese ? "Gatilho" : "Trigger");
    case VrButtonId::Grip:
        return std::string(handPrefix) + "Grip";
    case VrButtonId::Primary:
        return isLeft ? (portuguese ? "X (Esq)" : "L X") : (portuguese ? "A (Dir)" : "R A");
    case VrButtonId::Secondary:
        return isLeft ? (portuguese ? "Y (Esq)" : "L Y") : (portuguese ? "B (Dir)" : "R B");
    case VrButtonId::ThumbClick:
        return isLeft ? "L3" : "R3";
    case VrButtonId::StickUp:
        return std::string(handPrefix) + (portuguese ? "Stick Cima" : "Stick Up");
    case VrButtonId::StickDown:
        return std::string(handPrefix) + (portuguese ? "Stick Baixo" : "Stick Down");
    case VrButtonId::StickLeft:
        return std::string(handPrefix) + (portuguese ? "Stick Esq" : "Stick Left");
    case VrButtonId::StickRight:
        return std::string(handPrefix) + (portuguese ? "Stick Dir" : "Stick Right");
    case VrButtonId::None:
    default:
        return "--";
    }
}

std::string formatTriggerModeBadge(const KeybindTrigger& trigger, bool portuguese) {
    if (!trigger.hasInput()) {
        return portuguese ? "SEM BIND" : "UNBOUND";
    }
    char buf[64]{};
    if (trigger.isTwoButton()) {
        if (trigger.gesture == VrGesture::LongHold) {
            std::snprintf(
                buf,
                sizeof(buf),
                portuguese ? "JUNTOS %.2fs" : "TOGETHER %.2fs",
                trigger.holdSeconds);
            return buf;
        }
        return portuguese ? "JUNTOS" : "TOGETHER";
    }
    switch (trigger.gesture) {
    case VrGesture::Tap1x:
        return portuguese ? "TOQUE 1x" : "TAP 1x";
    case VrGesture::Double2x:
        return portuguese ? "2x SEQUENCIA" : "2x SEQUENCE";
    case VrGesture::Triple3x:
        return portuguese ? "3x SEQUENCIA" : "3x SEQUENCE";
    case VrGesture::LongHold:
        std::snprintf(
            buf,
            sizeof(buf),
            portuguese ? "SEGURAR %.2fs" : "HOLD %.2fs",
            trigger.holdSeconds);
        return buf;
    case VrGesture::WhileHeld:
        return portuguese ? "SEGURADO" : "HELD";
    default:
        return "";
    }
}

std::string formatTriggerSummary(const KeybindTrigger& trigger, bool portuguese) {
    if (!trigger.hasInput()) {
        return portuguese ? "Sem vinculo" : "Unbound";
    }
    std::string input = formatVrSlotLabel(trigger.primary, portuguese);
    if (trigger.isTwoButton()) {
        input += " + " + formatVrSlotLabel(trigger.secondary, portuguese);
    }
    return input + " [" + formatTriggerModeBadge(trigger, portuguese) + "]";
}

std::string formatKeyComboLabel(const std::vector<std::string>& combo) {
    if (combo.empty()) {
        return "--";
    }
    std::string out;
    for (size_t i = 0; i < combo.size(); ++i) {
        if (i > 0) {
            out += '+';
        }
        out += toUpperCopy(combo[i]);
    }
    return out;
}

KeybindProfile KeybindProfileStore::builtInGlobalDefault() {
    KeybindProfile p{};
    p.id = kGlobalDefaultId;
    p.name = "Global Default (Immutable)";
    p.isImmutableDefault = true;

    // 1. Companion Native Actions moved to complex inputs, keeping X, Y, A, B completely free!
    p.entries.push_back(makeCompanionEntry(
        "companion.toggle_hud", "companion.toggle_hud",
        "Toggle VR HUD", "Abrir / Fechar HUD VR", 1,
        CompanionActionId::ToggleHud,
        {{VrHand::Left, VrButtonId::ThumbClick}, {}, VrComboRelation::Single, VrGesture::Triple3x, 0.0f}));
    p.entries.push_back(makeCompanionEntry(
        "companion.toggle_origin_mode", "companion.toggle_origin_mode",
        "Flight <-> Edit 3D Origin", "Voo <-> Editar Origem 3D", 2,
        CompanionActionId::ToggleOriginMode,
        {{VrHand::Right, VrButtonId::ThumbClick}, {}, VrComboRelation::Single, VrGesture::Triple3x, 0.0f}));
    p.entries.push_back(makeCompanionEntry(
        "companion.recenter_seat", "companion.recenter_seat",
        "Recenter VR Seat", "Recentralizar Assento VR", 3,
        CompanionActionId::RecenterSeat,
        {{VrHand::Left, VrButtonId::ThumbClick}, {}, VrComboRelation::Single, VrGesture::LongHold, 0.65f}));
    p.entries.push_back(makeCompanionEntry(
        "companion.toggle_vr_mouse", "companion.toggle_vr_mouse",
        "Toggle VR Menu Mouse", "Ligar / Desligar Mouse VR", 4,
        CompanionActionId::ToggleVrMouse,
        {{VrHand::Right, VrButtonId::ThumbClick}, {}, VrComboRelation::Single, VrGesture::LongHold, 0.65f}));
    p.entries.push_back(makeCompanionEntry(
        "companion.flight_stick_hold", "companion.flight_stick_hold",
        "6DoF Virtual Stick", "Manche Virtual 6DoF", 5,
        CompanionActionId::FlightStickHold,
        {{VrHand::Right, VrButtonId::Grip}, {}, VrComboRelation::Single, VrGesture::WhileHeld, 0.0f}));
    p.entries.push_back(makeCompanionEntry(
        "companion.flight_throttle_hold", "companion.flight_throttle_hold",
        "6DoF Virtual Throttle", "Manete Virtual 6DoF", 6,
        CompanionActionId::FlightThrottleHold,
        {{VrHand::Left, VrButtonId::Grip}, {}, VrComboRelation::Single, VrGesture::WhileHeld, 0.0f}));

    // 2. Expanded IL-2 Flight & Combat Controls utilizing freed X, Y, A, B + Triggers
    const auto& items = Il2ActionsCatalog::builtInItems();
    auto findTriggerForItem = [](const std::string& actionId, bool& outForceVJoy, bool& outShowCallout) -> KeybindTrigger {
        outForceVJoy = false;
        outShowCallout = true;
        if (actionId == "all_guns_fire") {
            outForceVJoy = true;
            return {{VrHand::Right, VrButtonId::Trigger}, {}, VrComboRelation::Single, VrGesture::WhileHeld, 0.0f};
        }
        if (actionId == "weapon_group_2_fire") {
            outForceVJoy = true;
            return {{VrHand::Right, VrButtonId::Primary}, {}, VrComboRelation::Single, VrGesture::WhileHeld, 0.0f};
        }
        if (actionId == "rpc_wheel_brakes") {
            outForceVJoy = true;
            return {{VrHand::Left, VrButtonId::Trigger}, {}, VrComboRelation::Single, VrGesture::WhileHeld, 0.0f};
        }
        if (actionId == "gear") {
            return {{VrHand::Right, VrButtonId::Secondary}, {}, VrComboRelation::Single, VrGesture::Tap1x, 0.0f};
        }
        if (actionId == "flaps_dn") {
            return {{VrHand::Left, VrButtonId::Primary}, {}, VrComboRelation::Single, VrGesture::Tap1x, 0.0f};
        }
        if (actionId == "flaps_up") {
            return {{VrHand::Left, VrButtonId::Primary}, {}, VrComboRelation::Single, VrGesture::Double2x, 0.0f};
        }
        if (actionId == "throw_bombs_command") {
            return {{VrHand::Right, VrButtonId::Primary}, {}, VrComboRelation::Single, VrGesture::Double2x, 0.0f};
        }
        if (actionId == "launch_rockets_command") {
            return {{VrHand::Right, VrButtonId::Primary}, {}, VrComboRelation::Single, VrGesture::LongHold, 0.50f};
        }
        if (actionId == "airbrakes") {
            return {{VrHand::Right, VrButtonId::Secondary}, {}, VrComboRelation::Single, VrGesture::Double2x, 0.0f};
        }
        if (actionId == "canopy_open_close") {
            return {{VrHand::Right, VrButtonId::Secondary}, {}, VrComboRelation::Single, VrGesture::LongHold, 0.60f};
        }
        if (actionId == "parking_brakes") {
            return {{VrHand::Left, VrButtonId::Secondary}, {}, VrComboRelation::Single, VrGesture::Tap1x, 0.0f};
        }
        if (actionId == "all_engines_switch") {
            return {{VrHand::Left, VrButtonId::Secondary}, {}, VrComboRelation::Single, VrGesture::LongHold, 0.60f};
        }
        if (actionId == "bc_eject") {
            return {
                {VrHand::Left, VrButtonId::Primary},
                {VrHand::Left, VrButtonId::Secondary},
                VrComboRelation::Simultaneous,
                VrGesture::LongHold,
                0.65f};
        }
        if (actionId == "all_guns_reload") {
            return {
                {VrHand::Right, VrButtonId::Primary},
                {VrHand::Right, VrButtonId::Secondary},
                VrComboRelation::Simultaneous,
                VrGesture::Tap1x,
                0.0f};
        }
        if (actionId == "bombdoors_toggle") {
            return {
                {VrHand::Left, VrButtonId::Trigger},
                {VrHand::Right, VrButtonId::Primary},
                VrComboRelation::Simultaneous,
                VrGesture::Tap1x,
                0.0f};
        }
        outShowCallout = false;
        return {};
    };

    for (const auto& item : items) {
        // Group 1 overlaps with all_guns_fire by default; leave secondary group unbound unless user assigns it.
        if (item.actionId == "weapon_group_1_fire") {
            p.entries.push_back(makeCatalogEntry(item, {}, true, false));
            continue;
        }
        bool forceVJoy = false;
        bool showCallout = true;
        const KeybindTrigger trig = findTriggerForItem(item.actionId, forceVJoy, showCallout);
        p.entries.push_back(makeCatalogEntry(item, trig, forceVJoy, showCallout));
    }

    return p;
}

KeybindProfile KeybindProfileStore::cloneAsEditable(const KeybindProfile& src, const std::string& newName) {
    KeybindProfile copy = src;
    copy.id.clear();
    copy.name = newName.empty() ? "Custom Profile" : newName;
    copy.isImmutableDefault = false;
    return copy;
}

std::string KeybindProfileStore::sanitizeSlug(const std::string& name) {
    std::string out;
    out.reserve(name.size());
    for (unsigned char c : name) {
        if (std::isalnum(c)) {
            out.push_back(static_cast<char>(std::tolower(c)));
        } else if (c == ' ' || c == '-' || c == '_') {
            if (!out.empty() && out.back() != '_') {
                out.push_back('_');
            }
        }
    }
    while (!out.empty() && out.back() == '_') {
        out.pop_back();
    }
    if (out.empty() || out == kGlobalDefaultId) {
        out = "custom_profile";
    }
    return out;
}

std::string KeybindProfileStore::makeUniqueId(const std::string& baseName) const {
    const std::string slug = sanitizeSlug(baseName);
    if (findById(slug) == nullptr) {
        return slug;
    }
    for (int i = 2; i < 1000; ++i) {
        const std::string candidate = slug + "_" + std::to_string(i);
        if (findById(candidate) == nullptr) {
            return candidate;
        }
    }
    return slug + "_custom";
}

void KeybindProfileStore::loadDirectory(const std::filesystem::path& profilesDir) {
    dir_ = profilesDir;
    profiles_.clear();
    profiles_.push_back(builtInGlobalDefault());

    if (dir_.empty()) {
        return;
    }
    std::error_code ec;
    if (!std::filesystem::exists(dir_, ec)) {
        return;
    }

    const KeybindProfile templateGlobal = builtInGlobalDefault();
    for (const auto& entry : std::filesystem::directory_iterator(dir_, ec)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".json") {
            continue;
        }
        std::ifstream in(entry.path());
        if (!in.is_open()) {
            continue;
        }
        try {
            nlohmann::json j;
            in >> j;
            if (!j.is_object()) {
                continue;
            }
            const std::string id = j.value("id", entry.path().stem().string());
            if (id.empty() || id == kGlobalDefaultId) {
                continue; // Never allow a disk file to replace immutable Global Default.
            }

            KeybindProfile prof = templateGlobal;
            prof.id = id;
            prof.name = j.value("name", id);
            prof.isImmutableDefault = false;

            if (j.contains("entries") && j["entries"].is_array()) {
                for (const auto& je : j["entries"]) {
                    if (!je.is_object()) {
                        continue;
                    }
                    const std::string eid = je.value("id", std::string{});
                    auto it = std::find_if(prof.entries.begin(), prof.entries.end(), [&](const KeybindEntry& e) {
                        return e.id == eid;
                    });
                    if (it == prof.entries.end()) {
                        continue;
                    }
                    it->enabled = je.value("enabled", it->enabled);
                    it->showInCallout = je.value("showInCallout", it->showInCallout);

                    const std::string outTypeStr = je.value("outputType", std::string{});
                    if (outTypeStr == "vjoy") {
                        it->outputType = ActionOutputType::VJoyButton;
                    } else if (outTypeStr == "keyboard") {
                        it->outputType = ActionOutputType::Il2KeyboardCombo;
                    } else if (outTypeStr == "companion") {
                        it->outputType = ActionOutputType::CompanionAction;
                    }
                    it->vjoyButton = static_cast<uint8_t>(std::clamp(je.value("vjoyButton", static_cast<int>(it->vjoyButton)), 0, 16));
                    if (je.contains("keyCombo") && je["keyCombo"].is_array()) {
                        it->keyCombo.clear();
                        for (const auto& k : je["keyCombo"]) {
                            if (k.is_string()) {
                                it->keyCombo.push_back(k.get<std::string>());
                            }
                        }
                    }
                    if (je.contains("trigger") && je["trigger"].is_object()) {
                        const auto& jt = je["trigger"];
                        it->trigger.primary = slotFromJson(jt.value("primary", nlohmann::json::object()));
                        it->trigger.secondary = slotFromJson(jt.value("secondary", nlohmann::json::object()));
                        it->trigger.relation = (jt.value("relation", std::string("single")) == "simultaneous")
                            ? VrComboRelation::Simultaneous
                            : VrComboRelation::Single;
                        it->trigger.gesture = tokenToGesture(jt.value("gesture", std::string("tap_1x")));
                        it->trigger.holdSeconds = std::clamp(jt.value("holdSeconds", 0.60f), 0.15f, 3.0f);
                    }
                }
            }
            profiles_.push_back(std::move(prof));
        } catch (...) {
        }
    }
}

bool KeybindProfileStore::saveProfile(const KeybindProfile& profile, std::string& outError) {
    if (profile.isImmutableDefault || profile.id == kGlobalDefaultId) {
        outError = "Global Default profile is immutable. Clone it into a new profile first.";
        return false;
    }
    if (dir_.empty()) {
        outError = "Profiles directory not configured.";
        return false;
    }

    KeybindProfile toSave = profile;
    toSave.isImmutableDefault = false;
    if (toSave.id.empty()) {
        toSave.id = makeUniqueId(toSave.name);
    }

    std::error_code ec;
    std::filesystem::create_directories(dir_, ec);
    const std::filesystem::path filePath = dir_ / (toSave.id + ".json");

    nlohmann::json jEntries = nlohmann::json::array();
    for (const auto& e : toSave.entries) {
        const char* outType = "keyboard";
        if (e.outputType == ActionOutputType::CompanionAction) {
            outType = "companion";
        } else if (e.outputType == ActionOutputType::VJoyButton) {
            outType = "vjoy";
        }
        jEntries.push_back({
            {"id", e.id},
            {"actionId", e.actionId},
            {"enabled", e.enabled},
            {"showInCallout", e.showInCallout},
            {"outputType", outType},
            {"vjoyButton", static_cast<int>(e.vjoyButton)},
            {"keyCombo", e.keyCombo},
            {"trigger", {
                {"primary", slotToJson(e.trigger.primary)},
                {"secondary", slotToJson(e.trigger.secondary)},
                {"relation", e.trigger.isTwoButton() ? "simultaneous" : "single"},
                {"gesture", gestureToToken(e.trigger.gesture)},
                {"holdSeconds", e.trigger.holdSeconds}
            }}
        });
    }

    const nlohmann::json root = {
        {"id", toSave.id},
        {"name", toSave.name},
        {"entries", jEntries}
    };

    std::ofstream out(filePath, std::ios::trunc);
    if (!out.is_open()) {
        outError = "Failed to write profile file: " + filePath.string();
        return false;
    }
    out << root.dump(2) << '\n';
    if (!out.good()) {
        outError = "Error flushing profile file.";
        return false;
    }

    KeybindProfile* existing = findMutableById(toSave.id);
    if (existing != nullptr) {
        *existing = toSave;
    } else {
        profiles_.push_back(toSave);
    }
    outError.clear();
    return true;
}

bool KeybindProfileStore::deleteProfile(const std::string& profileId, std::string& outError) {
    if (profileId == kGlobalDefaultId) {
        outError = "Global Default profile is immutable and cannot be deleted.";
        return false;
    }
    auto it = std::find_if(profiles_.begin(), profiles_.end(), [&](const KeybindProfile& p) {
        return p.id == profileId;
    });
    if (it == profiles_.end()) {
        outError = "Profile not found.";
        return false;
    }
    profiles_.erase(it);
    if (!dir_.empty()) {
        std::error_code ec;
        std::filesystem::remove(dir_ / (profileId + ".json"), ec);
    }
    outError.clear();
    return true;
}

const KeybindProfile* KeybindProfileStore::findById(const std::string& profileId) const noexcept {
    for (const auto& p : profiles_) {
        if (p.id == profileId) {
            return &p;
        }
    }
    return nullptr;
}

KeybindProfile* KeybindProfileStore::findMutableById(const std::string& profileId) noexcept {
    for (auto& p : profiles_) {
        if (p.id == profileId) {
            return &p;
        }
    }
    return nullptr;
}

std::vector<KeybindConflict> KeybindProfileStore::findConflicts(const KeybindProfile& profile) {
    std::vector<KeybindConflict> conflicts;
    for (size_t i = 0; i < profile.entries.size(); ++i) {
        const auto& a = profile.entries[i];
        if (!a.enabled || !a.trigger.hasInput()) {
            continue;
        }
        for (size_t j = i + 1; j < profile.entries.size(); ++j) {
            const auto& b = profile.entries[j];
            if (!b.enabled || !b.trigger.hasInput()) {
                continue;
            }
            const bool exactSame = (a.trigger == b.trigger);
            const bool swappedPair =
                a.trigger.isTwoButton() && b.trigger.isTwoButton() &&
                a.trigger.gesture == b.trigger.gesture &&
                a.trigger.primary == b.trigger.secondary &&
                a.trigger.secondary == b.trigger.primary;
            if (exactSame || swappedPair) {
                conflicts.push_back({i, j});
            }
        }
    }
    return conflicts;
}

void KeybindProfileStore::applyIl2ActionKeyOverrides(
    KeybindProfile& profile,
    const std::filesystem::path& il2InputDir) {
    const auto overrides = Il2ActionsCatalog::loadUserActionKeyOverrides(il2InputDir);
    if (overrides.empty()) {
        return;
    }
    for (auto& e : profile.entries) {
        if (e.outputType == ActionOutputType::CompanionAction) {
            continue;
        }
        const auto it = overrides.find(e.actionId);
        if (it != overrides.end() && !it->second.empty()) {
            e.keyCombo = it->second;
        }
    }
}

} // namespace il2vr
