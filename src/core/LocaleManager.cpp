#include "core/LocaleManager.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <fstream>

namespace il2vr {

bool LocaleManager::parseLocaleFile(
    const std::filesystem::path& path,
    LocaleInfo& outInfo,
    std::unordered_map<std::string, std::string>& outStrings) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return false;
    }

    nlohmann::json root;
    try {
        in >> root;
    } catch (...) {
        return false;
    }
    if (!root.is_object()) {
        return false;
    }

    outInfo = LocaleInfo{};
    outInfo.path = path;
    if (root.contains("meta") && root["meta"].is_object()) {
        const auto& meta = root["meta"];
        outInfo.code = meta.value("code", path.stem().string());
        outInfo.name = meta.value("name", outInfo.code);
    } else {
        outInfo.code = path.stem().string();
        outInfo.name = outInfo.code;
    }

    outStrings.clear();
    if (root.contains("strings") && root["strings"].is_object()) {
        for (const auto& [k, v] : root["strings"].items()) {
            if (v.is_string()) {
                outStrings[k] = v.get<std::string>();
            }
        }
    }
    return !outInfo.code.empty();
}

bool LocaleManager::discoverLocales(const std::filesystem::path& localesDir) {
    localesDir_ = localesDir;
    available_.clear();
    fallbackStrings_.clear();

    std::error_code ec;
    if (localesDir.empty() || !std::filesystem::exists(localesDir, ec)) {
        return false;
    }

    for (const auto& entry : std::filesystem::directory_iterator(localesDir, ec)) {
        if (!entry.is_regular_file(ec) || entry.path().extension() != ".json") {
            continue;
        }
        LocaleInfo info{};
        std::unordered_map<std::string, std::string> parsed;
        if (parseLocaleFile(entry.path(), info, parsed)) {
            if (info.code == "pt-BR" || fallbackStrings_.empty()) {
                if (info.code == "pt-BR") {
                    fallbackStrings_ = parsed;
                }
            }
            available_.push_back(std::move(info));
        }
    }

    std::sort(available_.begin(), available_.end(), [](const LocaleInfo& a, const LocaleInfo& b) {
        return a.code < b.code;
    });

    if (fallbackStrings_.empty() && !available_.empty()) {
        LocaleInfo dummy{};
        parseLocaleFile(available_.front().path, dummy, fallbackStrings_);
    }

    return !available_.empty();
}

bool LocaleManager::loadLocale(const std::string& code) {
    if (available_.empty()) {
        return false;
    }

    const LocaleInfo* target = nullptr;
    for (const auto& item : available_) {
        if (item.code == code) {
            target = &item;
            break;
        }
    }
    if (target == nullptr) {
        target = &available_.front();
    }

    LocaleInfo info{};
    std::unordered_map<std::string, std::string> loaded;
    if (!parseLocaleFile(target->path, info, loaded)) {
        return false;
    }

    activeCode_ = info.code;
    activeName_ = info.name;
    strings_ = std::move(loaded);
    return true;
}

std::string LocaleManager::get(const std::string& key, const std::string& fallback) const {
    if (const auto it = strings_.find(key); it != strings_.end()) {
        return it->second;
    }
    if (const auto itFb = fallbackStrings_.find(key); itFb != fallbackStrings_.end()) {
        return itFb->second;
    }
    return fallback;
}

const char* LocaleManager::tr(const std::string& key, const char* fallback) const noexcept {
    if (const auto it = strings_.find(key); it != strings_.end()) {
        return it->second.c_str();
    }
    if (const auto itFb = fallbackStrings_.find(key); itFb != fallbackStrings_.end()) {
        return itFb->second.c_str();
    }
    return (fallback != nullptr) ? fallback : "";
}

std::filesystem::path LocaleManager::findLocalesDirectory() {
    std::error_code ec;
    const std::array<std::filesystem::path, 4> relCandidates = {
        std::filesystem::path("config") / "locales",
        std::filesystem::path("..") / "config" / "locales",
        std::filesystem::path("..") / ".." / "config" / "locales",
        std::filesystem::path("..") / ".." / ".." / "config" / "locales"
    };
    for (const auto& cand : relCandidates) {
        if (std::filesystem::exists(cand, ec)) {
            return std::filesystem::weakly_canonical(cand, ec);
        }
    }

    wchar_t exePathBuf[MAX_PATH]{};
    const DWORD len = GetModuleFileNameW(nullptr, exePathBuf, MAX_PATH);
    if (len > 0 && len < MAX_PATH) {
        std::filesystem::path cur = std::filesystem::path(exePathBuf).parent_path();
        for (int depth = 0; depth < 5 && !cur.empty(); ++depth) {
            const auto candidate = cur / "config" / "locales";
            if (std::filesystem::exists(candidate, ec)) {
                return candidate;
            }
            cur = cur.parent_path();
        }
    }
    return {};
}

} // namespace il2vr
