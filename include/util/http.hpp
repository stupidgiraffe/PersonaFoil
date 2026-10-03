#pragma once

#include <curl/curl.h>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace inst::http {
    struct ByteRange { std::uint64_t first; std::uint64_t last; };
    enum class Error { None, InvalidRequest, Connectivity, Timeout, Tls, Http, Redirect, TooLarge, Io, Decode };
    struct Result {
        CURLcode curlCode = CURLE_OK;
        long status = 0;
        std::string body;
        std::string effectiveUrl;
        std::string contentType;
        std::string diagnostic;
        std::map<std::string, std::string> headers;
        Error error = Error::None;
        bool ok() const { return error == Error::None && curlCode == CURLE_OK && status >= 200 && status < 300; }
    };
    struct Request {
        bool head = false;
        std::optional<ByteRange> range;
        long timeoutMs = 15000;
        std::size_t maxBytes = 16 * 1024 * 1024;
        std::string userAgent = "PersonaFoil";
        std::string username;
        std::string password;
        std::vector<std::string> headers;
        // Credentials and custom headers belong to this origin, even across redirects.
        std::string credentialOrigin;
        bool verifyTls = true;
        std::function<void(std::uint64_t, std::uint64_t)> progress;
        // Streaming writers receive only successful bodies (206 for a range).
        std::function<std::size_t(const char*, std::size_t)> writer;
    };
    std::string CanonicalUrl(const std::string& url);
    std::string Origin(const std::string& url);
    std::string ResolveUrl(const std::string& base, const std::string& relative);
    std::string Describe(const Result& result);
    Result Get(const std::string& url, const Request& request = {});
    Result GetFile(const std::string& url, const std::string& path, const Request& request = {});
}
