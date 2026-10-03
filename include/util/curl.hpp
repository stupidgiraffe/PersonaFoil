#pragma once
#include <string>
#include "util/http.hpp"
#include <cstdint>
#include <functional>

namespace inst::curl {
    using DownloadProgressCallback = std::function<void(std::uint64_t downloaded, std::uint64_t total)>;
    const std::string& getDefaultUserAgent();
    const std::string& getDownloadUserAgent();
    const std::string& getUserAgent();
    bool downloadFile(const std::string ourUrl, const char *pagefilename, long timeout = 5000, bool writeProgress = false);
    bool downloadFileWithProgress(const std::string ourUrl, const char *pagefilename, long timeout, const DownloadProgressCallback& progressCb);
    bool downloadFileRangeWithProgress(const std::string ourUrl, const char *pagefilename, std::uint64_t start, std::uint64_t endInclusive, long timeout, const DownloadProgressCallback& progressCb = {});
    bool downloadFileRangeToOffsetWithProgress(const std::string ourUrl, const char *pagefilename, std::uint64_t fileOffset, std::uint64_t start, std::uint64_t endInclusive, long timeout, const DownloadProgressCallback& progressCb = {});
    bool downloadFileWithAuth(const std::string ourUrl, const char *pagefilename, const std::string& user, const std::string& pass, long timeout = 5000);
    bool downloadImageWithAuth(const std::string ourUrl, const char *pagefilename, const std::string& user, const std::string& pass, long timeout = 5000);
    std::string downloadToBuffer(const std::string& url, long timeout = 5000);
    std::string downloadToBuffer(const std::string& url, inst::http::ByteRange range, long timeout = 5000);
}
