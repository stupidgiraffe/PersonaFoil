#pragma once

#include "util/json.hpp"
#include <cstdint>
#include <string>
#include <vector>

namespace inst::bridge {
    constexpr int kSchemaVersion = 1;
    constexpr std::size_t kMaxSources = 64;
    constexpr std::size_t kMaxReleases = 20000;
    constexpr std::size_t kMaxActions = 4096;
    constexpr std::size_t kMaxAlternatives = 8;
    constexpr std::size_t kMaxWarnings = 64;
    constexpr std::size_t kMaxLanguages = 16;

    enum class ContentType { Base, Update, Dlc, Multi };
    enum class SourceHealth { Unknown, Online, Degraded, Offline, AuthRequired };
    enum class ActionReason { Update, Missing };

    struct Source {
        std::string id;
        std::string name;
        std::string kind;
        SourceHealth health = SourceHealth::Unknown;
        int priority = 0;
    };

    struct Release {
        std::string id;
        std::string titleId;
        ContentType contentType = ContentType::Base;
        std::uint64_t version = 0;
        std::uint64_t size = 0;
        std::string displayName;
        std::string sourceId;
        std::string downloadId;
        std::string sha256;
        std::string region;
        std::vector<std::string> languages;
    };

    struct Catalog {
        std::string generatedAt;
        std::vector<Source> sources;
        std::vector<Release> releases;
    };

    struct InstalledTitle {
        std::string titleId;
        ContentType contentType = ContentType::Base;
        std::uint64_t version = 0;
    };

    struct Alternative {
        std::string releaseId;
        std::string sourceId;
        std::string downloadId;
        std::uint64_t version = 0;
        std::uint64_t size = 0;
    };

    struct Action {
        std::string titleId;
        ContentType contentType = ContentType::Base;
        std::uint64_t installedVersion = 0;
        std::uint64_t latestVersion = 0;
        ActionReason reason = ActionReason::Update;
        std::vector<Alternative> alternatives;
    };

    struct ReconcileResult {
        std::vector<Action> actions;
        std::vector<std::string> warnings;
    };

    const char* ContentTypeName(ContentType value);
    const char* SourceHealthName(SourceHealth value);
    const char* ActionReasonName(ActionReason value);

    bool ParseCatalog(const std::string& body, Catalog& out, std::string& error);
    bool ParseReconcile(const std::string& body, ReconcileResult& out, std::string& error);
    nlohmann::json BuildReconcileRequest(const std::vector<InstalledTitle>& installed, std::string& error);
}
