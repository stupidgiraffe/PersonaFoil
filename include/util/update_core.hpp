#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace inst::update {
    struct SemanticVersion { std::uint64_t major = 0; std::uint64_t minor = 0; std::uint64_t patch = 0; };
    struct ReleaseAsset { std::string name; std::string url; };
    bool ParseStableSemver(const std::string& value, SemanticVersion& out, std::string* error = nullptr);
    int CompareSemanticVersions(const SemanticVersion& lhs, const SemanticVersion& rhs);
    bool IsOfficialReleaseAssetUrl(const std::string& url);
    bool SelectRequiredReleaseAssets(const std::vector<ReleaseAsset>& assets, std::string& nroUrl, std::string& checksumsUrl, std::string* error = nullptr);
    bool ParseSha256Sums(const std::string& text, const std::string& targetFilename, std::string& expectedHash, std::string* error = nullptr);
    bool Sha256HexEqual(const std::string& lhs, const std::string& rhs);
}
