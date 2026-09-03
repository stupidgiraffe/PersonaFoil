#pragma once

#include <functional>
#include <string>

namespace inst::update {
    enum class CheckStatus { Error, UpToDate, UpdateAvailable };

    struct ReleaseInfo {
        std::string version;
        std::string nroUrl;
        std::string checksumsUrl;
        std::string notes;
    };

    struct CheckResult {
        CheckStatus status = CheckStatus::Error;
        ReleaseInfo release;
        std::string error;
    };

    struct InstallResult {
        bool success = false;
        std::string error;
        std::string installedPath;
        std::string backupPath;
    };

    using ProgressCallback = std::function<void(const std::string& stage, double percent)>;

    void SetRunningNroPath(const std::string& path);
    std::string GetRunningNroPath();
    CheckResult CheckForUpdate(const std::string& currentVersion);
    InstallResult InstallUpdate(const ReleaseInfo& release, const ProgressCallback& progress = {});
}
