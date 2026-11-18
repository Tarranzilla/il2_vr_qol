#pragma once

#include "input/KeybindTypes.h"

#include <filesystem>
#include <string>
#include <vector>

namespace il2vr {

struct KeybindConflict {
    size_t entryIndexA = 0;
    size_t entryIndexB = 0;
};

class KeybindProfileStore {
public:
    static constexpr const char* kGlobalDefaultId = "global_default";

    static KeybindProfile builtInGlobalDefault();
    static KeybindProfile cloneAsEditable(const KeybindProfile& src, const std::string& newName);

    void loadDirectory(const std::filesystem::path& profilesDir);
    bool saveProfile(const KeybindProfile& profile, std::string& outError);
    bool deleteProfile(const std::string& profileId, std::string& outError);

    const std::vector<KeybindProfile>& profiles() const noexcept { return profiles_; }
    const KeybindProfile* findById(const std::string& profileId) const noexcept;
    KeybindProfile* findMutableById(const std::string& profileId) noexcept;

    static std::vector<KeybindConflict> findConflicts(const KeybindProfile& profile);

    // Syncs IL-2 action default keystrokes with user overrides from data/input/current.actions.
    static void applyIl2ActionKeyOverrides(
        KeybindProfile& profile,
        const std::filesystem::path& il2InputDir);

private:
    static std::string sanitizeSlug(const std::string& name);
    std::string makeUniqueId(const std::string& baseName) const;

    std::filesystem::path dir_;
    std::vector<KeybindProfile> profiles_;
};

} // namespace il2vr
