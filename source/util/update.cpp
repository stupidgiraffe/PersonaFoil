#include "util/update.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <system_error>

#include <switch.h>

#include "util/curl.hpp"
#include "util/json.hpp"
#include "util/update_core.hpp"

namespace inst::update {
    namespace {
        constexpr const char* kLatestReleaseEndpoint = "https://api.github.com/repos/stupidgiraffe/PersonaFoil/releases/latest";
        constexpr std::uintmax_t kMinNroSize = 1024 * 1024;
        constexpr std::uintmax_t kMaxNroSize = 256ull * 1024ull * 1024ull;
        std::mutex gPathMutex;
        std::string gRunningNroPath;

        std::string NormalizeReleaseNotes(std::string text)
        {
            if (text.empty()) return "No changelog available for this release.";
            text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
            while (!text.empty() && (text.back() == '\n' || text.back() == ' ' || text.back() == '\t')) text.pop_back();
            constexpr std::size_t kMax = 3500;
            if (text.size() > kMax) text = text.substr(0, kMax) + "\n\n[Changelog truncated]";
            return text.empty() ? "No changelog available for this release." : text;
        }

        bool SafeRunningNroPath(const std::string& path)
        {
            if (path.rfind("sdmc:/", 0) != 0 || path.find("..") != std::string::npos) return false;
            if (path.size() < 5 || path.substr(path.size() - 4) != ".nro") return false;
            return path.find('\r') == std::string::npos && path.find('\n') == std::string::npos;
        }

        bool HashFileSha256(const std::string& path, std::string& out, std::string& error)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file) {
                error = "Could not open the downloaded NRO for verification.";
                return false;
            }
            Sha256Context context;
            sha256ContextCreate(&context);
            std::array<char, 64 * 1024> buffer{};
            while (file) {
                file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
                const std::streamsize count = file.gcount();
                if (count > 0) sha256ContextUpdate(&context, buffer.data(), static_cast<std::size_t>(count));
            }
            if (!file.eof()) {
                error = "Could not read the downloaded NRO completely.";
                return false;
            }
            std::array<unsigned char, 32> hash{};
            sha256ContextGetHash(&context, hash.data());
            static constexpr char kHex[] = "0123456789abcdef";
            out.clear();
            out.reserve(64);
            for (unsigned char byte : hash) {
                out.push_back(kHex[(byte >> 4) & 0xf]);
                out.push_back(kHex[byte & 0xf]);
            }
            std::fill(hash.begin(), hash.end(), 0);
            return true;
        }

        void Progress(const ProgressCallback& callback, const std::string& stage, double percent)
        {
            if (callback) callback(stage, percent);
        }
    }

    void SetRunningNroPath(const std::string& path)
    {
        std::lock_guard<std::mutex> lock(gPathMutex);
        gRunningNroPath = path;
    }

    std::string GetRunningNroPath()
    {
        std::lock_guard<std::mutex> lock(gPathMutex);
        return gRunningNroPath;
    }

    CheckResult CheckForUpdate(const std::string& currentVersion)
    {
        CheckResult result;
        try {
            const std::string jsonData = inst::curl::downloadToBuffer(kLatestReleaseEndpoint, -1, -1, 8000L);
            if (jsonData.empty()) {
                result.error = "GitHub returned no release metadata.";
                return result;
            }
            const nlohmann::json release = nlohmann::json::parse(jsonData);
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
        } catch (const std::exception& e) {
            result.error = std::string("Could not parse GitHub release metadata: ") + e.what();
        } catch (...) {
            result.error = "Could not check GitHub releases.";
        }
        return result;
    }

    InstallResult InstallUpdate(const ReleaseInfo& release, const ProgressCallback& progress)
    {
        InstallResult result;
        const std::string running = GetRunningNroPath();
        if (!SafeRunningNroPath(running)) {
            result.error = "PersonaFoil cannot safely determine the running NRO path. Reinstall under sdmc:/switch/PersonaFoil/ and try again.";
            return result;
        }
        if (!IsOfficialReleaseAssetUrl(release.nroUrl) || !IsOfficialReleaseAssetUrl(release.checksumsUrl)) {
            result.error = "Update asset URL is not an official PersonaFoil GitHub Release URL.";
            return result;
        }

        const std::string staged = running + ".new";
        const std::string backup = running + ".bak";
        const std::string checksumTemp = running + ".sha256.tmp";
        std::error_code ec;
        std::filesystem::remove(staged, ec);
        ec.clear();
        std::filesystem::remove(checksumTemp, ec);

        Progress(progress, "Downloading checksum", 5.0);
        if (!inst::curl::downloadFile(release.checksumsUrl, checksumTemp.c_str(), 15000L, false)) {
            result.error = "Could not download SHA256SUMS.txt.";
            return result;
        }
        std::ifstream checksumFile(checksumTemp, std::ios::binary);
        if (!checksumFile) {
            std::filesystem::remove(checksumTemp, ec);
            result.error = "Could not read downloaded SHA256SUMS.txt.";
            return result;
        }
        std::ostringstream checksumBuffer;
        checksumBuffer << checksumFile.rdbuf();
        checksumFile.close();
        std::filesystem::remove(checksumTemp, ec);
        std::string expectedHash;
        std::string checksumError;
        if (!ParseSha256Sums(checksumBuffer.str(), "personafoil.nro", expectedHash, &checksumError)) {
            result.error = "Update checksum is invalid: " + checksumError;
            return result;
        }

        Progress(progress, "Downloading PersonaFoil", 15.0);
        if (!inst::curl::downloadFileWithProgress(release.nroUrl, staged.c_str(), 0L,
                [&](std::uint64_t downloaded, std::uint64_t total) {
                    double percent = 15.0;
                    if (total > 0) percent += std::min(65.0, (static_cast<double>(downloaded) / static_cast<double>(total)) * 65.0);
                    Progress(progress, "Downloading PersonaFoil", percent);
                })) {
            std::filesystem::remove(staged, ec);
            result.error = "Could not download personafoil.nro.";
            return result;
        }

        const std::uintmax_t size = std::filesystem::file_size(staged, ec);
        if (ec || size < kMinNroSize || size > kMaxNroSize) {
            std::filesystem::remove(staged, ec);
            result.error = "Downloaded personafoil.nro has an implausible file size.";
            return result;
        }

        Progress(progress, "Verifying SHA-256", 84.0);
        std::string actualHash;
        std::string hashError;
        if (!HashFileSha256(staged, actualHash, hashError)) {
            std::filesystem::remove(staged, ec);
            result.error = hashError;
            return result;
        }
        if (!Sha256HexEqual(expectedHash, actualHash)) {
            std::filesystem::remove(staged, ec);
            result.error = "Update verification failed. Installed PersonaFoil was not changed.";
            return result;
        }

        Progress(progress, "Installing verified NRO", 92.0);
        const bool hadCurrent = std::filesystem::exists(running, ec) && !ec;
        if (!hadCurrent) {
            std::filesystem::remove(staged, ec);
            result.error = "Running PersonaFoil NRO no longer exists; refusing to install.";
            return result;
        }
        std::filesystem::remove(backup, ec);
        ec.clear();
        std::filesystem::rename(running, backup, ec);
        if (ec) {
            std::filesystem::remove(staged, ec);
            result.error = "Could not preserve the current PersonaFoil NRO as a backup.";
            return result;
        }
        ec.clear();
        std::filesystem::rename(staged, running, ec);
        if (ec) {
            std::error_code restoreError;
            std::filesystem::rename(backup, running, restoreError);
            std::filesystem::remove(staged, restoreError);
            result.error = restoreError
                ? "Could not install the update and automatic rollback also failed. The backup remains at " + backup
                : "Could not install the update. The previous NRO was restored.";
            return result;
        }

        Progress(progress, "Update installed", 100.0);
        result.success = true;
        result.installedPath = running;
        result.backupPath = backup;
        return result;
    }
}
