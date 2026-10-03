#include "util/update.hpp"
#include "util/json.hpp"
#include "util/remote_core.hpp"
#include "util/update_core.hpp"
#include <algorithm>

namespace inst::update {
    namespace {
        std::string NormalizeReleaseNotes(std::string text)
        {
            if (text.empty()) return "No changelog available for this release.";
            text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
            while (!text.empty() && (text.back() == '\n' || text.back() == ' ' || text.back() == '\t')) text.pop_back();
            constexpr std::size_t kMax = 3500;
            if (text.size() > kMax) text = text.substr(0, kMax) + "\n\n[Changelog truncated]";
            return text.empty() ? "No changelog available for this release." : text;
        }

    }
    CheckResult ParseReleaseResponse(const inst::http::Result& response, const std::string& currentVersion)
    {
        CheckResult result;
        try {
            if (response.curlCode == CURLE_OK && response.status == 404) {
                result.status = CheckStatus::NoRelease;
                return result;
            }
            if (!response.ok()) {
                result.error = response.status == 403 || response.status == 429
                    ? "GitHub API access denied or rate limited. Try again later."
                    : inst::http::Describe(response);
                return result;
            }
            const std::string& jsonData = response.body;
            if (jsonData.empty()) {
                result.error = "GitHub returned no release metadata.";
                return result;
            }
            const nlohmann::json release = inst::remote::ParseDocument(jsonData);
            if (!release.is_object()) {
                result.error = "GitHub release metadata was not an object.";
                return result;
            }
            if (release.value("draft", false) || release.value("prerelease", false)) {
                result.error = "Latest GitHub release is not a stable release.";
                return result;
            }
            if (!release.contains("tag_name") || !release["tag_name"].is_string()) {
                result.error = "GitHub release metadata is missing tag_name.";
                return result;
            }

            SemanticVersion current;
            SemanticVersion latest;
            std::string versionError;
            const std::string tag = release["tag_name"].get<std::string>();
            if (!ParseStableSemver(currentVersion, current, &versionError)) {
                result.error = "Current PersonaFoil version is invalid: " + versionError;
                return result;
            }
            if (!ParseStableSemver(tag, latest, &versionError)) {
                result.error = "Latest release tag is not a supported stable version: " + versionError;
                return result;
            }
            if (CompareSemanticVersions(latest, current) <= 0) {
                result.status = CheckStatus::UpToDate;
                result.release.version = tag;
                return result;
            }

            if (!release.contains("assets") || !release["assets"].is_array()) {
                result.error = "GitHub release metadata has no asset list.";
                return result;
            }
            std::vector<ReleaseAsset> assets;
            for (const auto& item : release["assets"]) {
                if (!item.is_object() || !item.contains("name") || !item["name"].is_string() ||
                    !item.contains("browser_download_url") || !item["browser_download_url"].is_string()) continue;
                assets.push_back({item["name"].get<std::string>(), item["browser_download_url"].get<std::string>()});
            }

            std::string assetError;
            if (!SelectRequiredReleaseAssets(assets, result.release.nroUrl, result.release.checksumsUrl, &assetError)) {
                result.error = assetError;
                return result;
            }
            result.release.version = tag;
            result.release.notes = release.contains("body") && release["body"].is_string()
                ? NormalizeReleaseNotes(release["body"].get<std::string>())
                : "No changelog available for this release.";
            result.status = CheckStatus::UpdateAvailable;
            return result;
        } catch (const std::exception&) {
            result.error = "Could not parse GitHub release metadata.";
        } catch (...) {
            result.error = "Could not check GitHub releases.";
        }
        return result;
    }

}
