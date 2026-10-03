#include "util/bridge_core.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <unordered_set>

namespace inst::bridge {
    namespace {
        constexpr std::size_t kMaxText = 256;
        constexpr std::size_t kMaxWarning = 1024;

        bool Fail(std::string& error, const std::string& message) {
            error = message;
            return false;
        }

        bool SafeText(const std::string& value, std::size_t maxLen = kMaxText, bool allowEmpty = false) {
            if ((!allowEmpty && value.empty()) || value.size() > maxLen) return false;
            return std::none_of(value.begin(), value.end(), [](unsigned char c) {
                return c < 0x20 && c != '\t';
            });
        }

        bool ReadText(const nlohmann::json& node, const char* key, std::string& out,
                      std::string& error, std::size_t maxLen = kMaxText, bool allowEmpty = false) {
            if (!node.contains(key) || !node[key].is_string())
                return Fail(error, std::string("Missing or invalid ") + key + ".");
            out = node[key].get<std::string>();
            if (!SafeText(out, maxLen, allowEmpty))
                return Fail(error, std::string("Invalid ") + key + ".");
            return true;
        }

        bool ReadOptionalText(const nlohmann::json& node, const char* key, std::string& out,
                              std::string& error, std::size_t maxLen = kMaxText) {
            out.clear();
            if (!node.contains(key)) return true;
            if (!node[key].is_string()) return Fail(error, std::string("Invalid ") + key + ".");
            out = node[key].get<std::string>();
            if (!SafeText(out, maxLen, true)) return Fail(error, std::string("Invalid ") + key + ".");
            return true;
        }

        bool ReadU64(const nlohmann::json& node, const char* key, std::uint64_t& out, std::string& error) {
            if (!node.contains(key)) return Fail(error, std::string("Missing ") + key + ".");
            const auto& value = node[key];
            if (value.is_number_unsigned()) {
                out = value.get<std::uint64_t>();
                return true;
            }
            if (value.is_number_integer()) {
                const auto signedValue = value.get<std::int64_t>();
                if (signedValue >= 0) {
                    out = static_cast<std::uint64_t>(signedValue);
                    return true;
                }
            }
            return Fail(error, std::string("Invalid ") + key + ".");
        }

        bool NormalizeTitleId(std::string& value) {
            if (value.size() != 16) return false;
            for (char& c : value) {
                const unsigned char u = static_cast<unsigned char>(c);
                if (!std::isxdigit(u)) return false;
                c = static_cast<char>(std::toupper(u));
            }
            return true;
        }

        bool IsHex(const std::string& value, std::size_t size) {
            return value.size() == size && std::all_of(value.begin(), value.end(), [](unsigned char c) {
                return std::isxdigit(c) != 0;
            });
        }

        bool ParseContentType(const nlohmann::json& node, ContentType& out) {
            if (!node.is_string()) return false;
            const auto value = node.get<std::string>();
            if (value == "base") out = ContentType::Base;
            else if (value == "update") out = ContentType::Update;
            else if (value == "dlc") out = ContentType::Dlc;
            else if (value == "multi") out = ContentType::Multi;
            else return false;
            return true;
        }

        bool ParseHealth(const nlohmann::json& node, SourceHealth& out) {
            if (!node.is_string()) return false;
            const auto value = node.get<std::string>();
            if (value == "unknown") out = SourceHealth::Unknown;
            else if (value == "online") out = SourceHealth::Online;
            else if (value == "degraded") out = SourceHealth::Degraded;
            else if (value == "offline") out = SourceHealth::Offline;
            else if (value == "auth_required") out = SourceHealth::AuthRequired;
            else return false;
            return true;
        }

        bool ParseReason(const nlohmann::json& node, ActionReason& out) {
            if (!node.is_string()) return false;
            const auto value = node.get<std::string>();
            if (value == "update") out = ActionReason::Update;
            else if (value == "missing") out = ActionReason::Missing;
            else return false;
            return true;
        }

        bool ValidateRoot(const nlohmann::json& root, std::string& error) {
            if (!root.is_object()) return Fail(error, "Bridge document must be an object.");
            if (!root.contains("schema_version") || !root["schema_version"].is_number_integer() ||
                root["schema_version"].get<int>() != kSchemaVersion)
                return Fail(error, "Unsupported PersonaBridge schema version.");
            return true;
        }
    }

    const char* ContentTypeName(ContentType value) {
        switch (value) {
            case ContentType::Base: return "base";
            case ContentType::Update: return "update";
            case ContentType::Dlc: return "dlc";
            case ContentType::Multi: return "multi";
        }
        return "base";
    }

    const char* SourceHealthName(SourceHealth value) {
        switch (value) {
            case SourceHealth::Unknown: return "unknown";
            case SourceHealth::Online: return "online";
            case SourceHealth::Degraded: return "degraded";
            case SourceHealth::Offline: return "offline";
            case SourceHealth::AuthRequired: return "auth_required";
        }
        return "unknown";
    }

    const char* ActionReasonName(ActionReason value) {
        return value == ActionReason::Missing ? "missing" : "update";
    }

    bool ParseCatalog(const std::string& body, Catalog& out, std::string& error) {
        try {
            const auto root = nlohmann::json::parse(body);
            if (!ValidateRoot(root, error)) return false;
            if (!root.contains("sources") || !root["sources"].is_array() ||
                root["sources"].size() > kMaxSources)
                return Fail(error, "Invalid PersonaBridge sources.");
            if (!root.contains("releases") || !root["releases"].is_array() ||
                root["releases"].size() > kMaxReleases)
                return Fail(error, "Invalid PersonaBridge releases.");

            Catalog parsed;
            if (!ReadOptionalText(root, "generated_at", parsed.generatedAt, error)) return false;
            std::unordered_set<std::string> sourceIds;
            for (const auto& node : root["sources"]) {
                if (!node.is_object()) return Fail(error, "Invalid PersonaBridge source.");
                Source source;
                if (!ReadText(node, "id", source.id, error) ||
                    !ReadText(node, "name", source.name, error) ||
                    !ReadText(node, "kind", source.kind, error))
                    return false;
                if (!node.contains("health") || !ParseHealth(node["health"], source.health))
                    return Fail(error, "Invalid source health.");
                if (node.contains("priority")) {
                    if (!node["priority"].is_number_integer()) return Fail(error, "Invalid source priority.");
                    const auto priority = node["priority"].get<long long>();
                    if (priority < -100000 || priority > 100000) return Fail(error, "Invalid source priority.");
                    source.priority = static_cast<int>(priority);
                }
                if (!sourceIds.insert(source.id).second) return Fail(error, "Duplicate source id.");
                parsed.sources.push_back(std::move(source));
            }

            std::unordered_set<std::string> releaseIds;
            for (const auto& node : root["releases"]) {
                if (!node.is_object()) return Fail(error, "Invalid PersonaBridge release.");
                Release release;
                if (!ReadText(node, "id", release.id, error) ||
                    !ReadText(node, "title_id", release.titleId, error) ||
                    !ReadText(node, "source_id", release.sourceId, error) ||
                    !ReadText(node, "download_id", release.downloadId, error) ||
                    !ReadU64(node, "version", release.version, error) ||
                    !ReadU64(node, "size", release.size, error))
                    return false;
                if (!NormalizeTitleId(release.titleId)) return Fail(error, "Invalid title_id.");
                if (!node.contains("content_type") || !ParseContentType(node["content_type"], release.contentType))
                    return Fail(error, "Invalid content_type.");
                if (!sourceIds.count(release.sourceId)) return Fail(error, "Release references an unknown source.");
                if (!releaseIds.insert(release.id).second) return Fail(error, "Duplicate release id.");
                if (!ReadOptionalText(node, "display_name", release.displayName, error) ||
                    !ReadOptionalText(node, "sha256", release.sha256, error, 64) ||
                    !ReadOptionalText(node, "region", release.region, error, 32))
                    return false;
                if (!release.sha256.empty() && !IsHex(release.sha256, 64))
                    return Fail(error, "Invalid release sha256.");
                if (node.contains("languages")) {
                    if (!node["languages"].is_array() || node["languages"].size() > kMaxLanguages)
                        return Fail(error, "Invalid release languages.");
                    for (const auto& language : node["languages"]) {
                        if (!language.is_string()) return Fail(error, "Invalid release language.");
                        const auto value = language.get<std::string>();
                        if (!SafeText(value, 32)) return Fail(error, "Invalid release language.");
                        release.languages.push_back(value);
                    }
                }
                parsed.releases.push_back(std::move(release));
            }
            out = std::move(parsed);
            error.clear();
            return true;
        } catch (...) {
            return Fail(error, "Invalid PersonaBridge catalog JSON.");
        }
    }

    bool ParseReconcile(const std::string& body, ReconcileResult& out, std::string& error) {
        try {
            const auto root = nlohmann::json::parse(body);
            if (!ValidateRoot(root, error)) return false;
            if (!root.contains("actions") || !root["actions"].is_array() ||
                root["actions"].size() > kMaxActions)
                return Fail(error, "Invalid PersonaBridge actions.");

            ReconcileResult parsed;
            for (const auto& node : root["actions"]) {
                if (!node.is_object()) return Fail(error, "Invalid PersonaBridge action.");
                Action action;
                if (!ReadText(node, "title_id", action.titleId, error) ||
                    !ReadU64(node, "installed_version", action.installedVersion, error) ||
                    !ReadU64(node, "latest_version", action.latestVersion, error))
                    return false;
                if (!NormalizeTitleId(action.titleId)) return Fail(error, "Invalid action title_id.");
                if (!node.contains("content_type") || !ParseContentType(node["content_type"], action.contentType))
                    return Fail(error, "Invalid action content_type.");
                if (!node.contains("reason") || !ParseReason(node["reason"], action.reason))
                    return Fail(error, "Invalid action reason.");
                if (action.reason == ActionReason::Update && action.latestVersion <= action.installedVersion)
                    return Fail(error, "Update action is not newer than installed content.");
                if (!node.contains("alternatives") || !node["alternatives"].is_array() ||
                    node["alternatives"].empty() || node["alternatives"].size() > kMaxAlternatives)
                    return Fail(error, "Invalid action alternatives.");

                std::unordered_set<std::string> releaseIds;
                for (const auto& altNode : node["alternatives"]) {
                    if (!altNode.is_object()) return Fail(error, "Invalid action alternative.");
                    Alternative alternative;
                    if (!ReadText(altNode, "release_id", alternative.releaseId, error) ||
                        !ReadText(altNode, "source_id", alternative.sourceId, error) ||
                        !ReadText(altNode, "download_id", alternative.downloadId, error) ||
                        !ReadU64(altNode, "version", alternative.version, error) ||
                        !ReadU64(altNode, "size", alternative.size, error))
                        return false;
                    if (alternative.version != action.latestVersion)
                        return Fail(error, "Alternative version does not match action.");
                    if (!releaseIds.insert(alternative.releaseId).second)
                        return Fail(error, "Duplicate action alternative.");
                    action.alternatives.push_back(std::move(alternative));
                }
                parsed.actions.push_back(std::move(action));
            }

            if (root.contains("warnings")) {
                if (!root["warnings"].is_array() || root["warnings"].size() > kMaxWarnings)
                    return Fail(error, "Invalid PersonaBridge warnings.");
                for (const auto& warning : root["warnings"]) {
                    if (!warning.is_string()) return Fail(error, "Invalid PersonaBridge warning.");
                    const auto value = warning.get<std::string>();
                    if (!SafeText(value, kMaxWarning)) return Fail(error, "Invalid PersonaBridge warning.");
                    parsed.warnings.push_back(value);
                }
            }

            out = std::move(parsed);
            error.clear();
            return true;
        } catch (...) {
            return Fail(error, "Invalid PersonaBridge reconcile JSON.");
        }
    }

    nlohmann::json BuildReconcileRequest(const std::vector<InstalledTitle>& installed, std::string& error) {
        error.clear();
        if (installed.size() > kMaxReleases) {
            error = "Installed title inventory exceeds the PersonaBridge limit.";
            return {};
        }
        nlohmann::json root;
        root["schema_version"] = kSchemaVersion;
        root["installed"] = nlohmann::json::array();
        std::unordered_set<std::string> seen;
        for (const auto& item : installed) {
            std::string titleId = item.titleId;
            if (!NormalizeTitleId(titleId)) {
                error = "Invalid installed title id.";
                return {};
            }
            const std::string key = titleId + ":" + ContentTypeName(item.contentType);
            if (!seen.insert(key).second) {
                error = "Duplicate installed title record.";
                return {};
            }
            root["installed"].push_back({
                {"title_id", titleId},
                {"content_type", ContentTypeName(item.contentType)},
                {"version", item.version}
            });
        }
        return root;
    }
}
