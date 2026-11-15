#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace il2vr {

struct LocaleInfo {
    std::string code;
    std::string name;
    std::filesystem::path path;
};

class LocaleManager {
public:
    bool discoverLocales(const std::filesystem::path& localesDir);
    bool loadLocale(const std::string& code);

    std::string get(const std::string& key, const std::string& fallback) const;
    const char* tr(const std::string& key, const char* fallback) const noexcept;

    const std::vector<LocaleInfo>& availableLocales() const noexcept { return available_; }
    const std::string& activeLocaleCode() const noexcept { return activeCode_; }
    const std::string& activeLocaleName() const noexcept { return activeName_; }
    const std::filesystem::path& localesDir() const noexcept { return localesDir_; }

    static std::filesystem::path findLocalesDirectory();

private:
    static bool parseLocaleFile(
        const std::filesystem::path& path,
        LocaleInfo& outInfo,
        std::unordered_map<std::string, std::string>& outStrings);

    std::filesystem::path localesDir_;
    std::vector<LocaleInfo> available_;
    std::string activeCode_ = "pt-BR";
    std::string activeName_ = "Portugues (Brasil)";
    std::unordered_map<std::string, std::string> strings_;
    std::unordered_map<std::string, std::string> fallbackStrings_;
};

} // namespace il2vr
