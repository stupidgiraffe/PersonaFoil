#include "util/http.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <mutex>

namespace inst::http {
    namespace {
        bool Init() {
            static std::once_flag once;
            static bool ok = false;
            std::call_once(once, [] { ok = curl_global_init(CURL_GLOBAL_ALL) == CURLE_OK; });
            return ok;
        }
        std::string Part(CURLU* url, CURLUPart part, unsigned flags = 0) {
            char* value = nullptr;
            if (curl_url_get(url, part, &value, flags) != CURLUE_OK) return {};
            std::string copy(value);
            curl_free(value);
            return copy;
        }
        bool SetUrl(CURLU* parsed, const std::string& url) {
            if (url.size() > 4096 || url.find_first_of("\r\n\t ") != std::string::npos) return false;
            if (curl_url_set(parsed, CURLUPART_URL, url.c_str(), 0) != CURLUE_OK) return false;
            const auto scheme = Part(parsed, CURLUPART_SCHEME);
            return (scheme == "http" || scheme == "https") && Part(parsed, CURLUPART_USER).empty() && Part(parsed, CURLUPART_PASSWORD).empty();
        }
        struct Sink {
            Result* result;
            FILE* file;
            std::size_t bytes = 0;
            std::size_t maxBytes;
            bool tooLarge = false;
            bool writerFailed = false;
            const Request* request;
            std::string location;
            long status = 0;
        };
        size_t Write(char* data, size_t size, size_t count, void* context) {
            auto& sink = *static_cast<Sink*>(context);
            if (size != 0 && count > std::numeric_limits<size_t>::max() / size) return 0;
            const size_t bytes = size * count;
            if (bytes > sink.maxBytes - sink.bytes) { sink.tooLarge = true; return 0; }
            sink.bytes += bytes;
            if (sink.request->writer) {
                if (sink.status < 200 || sink.status >= 300 || (sink.request->range && sink.status != 206)) {
                    sink.result->body.append(data, std::min(bytes, std::size_t(4096) - std::min(sink.result->body.size(), std::size_t(4096))));
                    return bytes;
                }
                try {
                    const auto written = sink.request->writer(data, bytes);
                    if (written != bytes) sink.writerFailed = true;
                    return written;
                } catch (...) { sink.writerFailed = true; return 0; }
            }
            if (sink.file) return fwrite(data, 1, bytes, sink.file);
            try { sink.result->body.append(data, bytes); } catch (...) { return 0; }
            return bytes;
        }
        size_t Header(char* data, size_t size, size_t count, void* context) {
            if (size != 0 && count > std::numeric_limits<size_t>::max() / size) return 0;
            const size_t bytes = size * count;
            if (bytes > 8192) return 0;
            auto& sink = *static_cast<Sink*>(context);
            auto& location = sink.location;
            std::string line(data, bytes);
            if (line.rfind("HTTP/", 0) == 0) {
                location.clear();
                sink.result->headers.clear();
                const auto space = line.find(' ');
                if (space != std::string::npos) sink.status = std::strtol(line.c_str() + space + 1, nullptr, 10);
            }
            auto colon = line.find(':');
            if (colon != std::string::npos) {
                auto name = line.substr(0, colon);
                std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::tolower(c); });
                const auto start = line.find_first_not_of(" \t", colon + 1);
                const auto end = line.find_last_not_of(" \t\r\n");
                const auto value = start == std::string::npos || end < start ? "" : line.substr(start, end - start + 1);
                if (sink.result->headers.size() >= 128 && !sink.result->headers.count(name)) return 0;
                sink.result->headers[name] = value;
                if (name == "location") {
                    location = value;
                }
            }
            return bytes;
        }
        int Progress(void* data, curl_off_t total, curl_off_t now, curl_off_t, curl_off_t) {
            auto& request = *static_cast<const Request*>(data);
            if (request.progress) request.progress(now > 0 ? now : 0, total > 0 ? total : 0);
            return 0;
        }
        Result Perform(const std::string& rawUrl, const Request& request, const std::string& path) {
            Result result;
            std::string url = CanonicalUrl(rawUrl);
            if (url.empty() || (request.range && request.range->last < request.range->first)) {
                result.error = Error::InvalidRequest;
                result.diagnostic = "Invalid HTTP URL or byte range.";
                return result;
            }
            for (const auto& header : request.headers) {
                if (header.find_first_of("\r\n") != std::string::npos || header.find(':') == std::string::npos) {
                    result.error = Error::InvalidRequest;
                    return result;
                }
            }
            if (!Init()) { result.curlCode = CURLE_FAILED_INIT; result.error = Error::Connectivity; return result; }
            const std::string boundary = request.credentialOrigin.empty() ? Origin(url) : Origin(request.credentialOrigin);
            const bool requireHttps = request.verifyTls && Origin(url).rfind("https://", 0) == 0;
            const auto started = std::chrono::steady_clock::now();
            for (int hop = 0; hop <= 5; ++hop) {
                result = Result{};
                result.effectiveUrl = url;
                CURL* curl = curl_easy_init();
                if (!curl) { result.curlCode = CURLE_FAILED_INIT; result.error = Error::Connectivity; return result; }
                FILE* file = path.empty() ? nullptr : fopen(path.c_str(), "wb");
                if (!path.empty() && !file) { curl_easy_cleanup(curl); result.error = Error::Io; return result; }
                Sink sink{&result, file, 0, request.maxBytes, false, false, &request, {}, 0};
                std::array<char, CURL_ERROR_SIZE> errorBuffer{};
                curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
                curl_easy_setopt(curl, CURLOPT_NOBODY, request.head ? 1L : 0L);
#if LIBCURL_VERSION_NUM >= 0x075500
                curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "http,https");
#else
                curl_easy_setopt(curl, CURLOPT_PROTOCOLS, CURLPROTO_HTTP | CURLPROTO_HTTPS);
#endif
                curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, request.verifyTls ? 1L : 0L);
                curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, request.verifyTls ? 2L : 0L);
#ifdef __SWITCH__
                if (request.verifyTls) curl_easy_setopt(curl, CURLOPT_CAINFO, "romfs:/cacert.pem");
#endif
                curl_easy_setopt(curl, CURLOPT_USERAGENT, request.userAgent.c_str());
                if (request.timeoutMs > 0) {
                    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
                    if (elapsed >= request.timeoutMs) {
                        if (file) fclose(file);
                        curl_easy_cleanup(curl);
                        result.error = Error::Timeout; result.curlCode = CURLE_OPERATION_TIMEDOUT;
                        if (!path.empty()) std::remove(path.c_str());
                        return result;
                    }
                    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, request.timeoutMs - static_cast<long>(elapsed));
                }
                curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 10000L);
                curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);
                curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 45L);
                curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer.data());
                curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, Header);
                curl_easy_setopt(curl, CURLOPT_HEADERDATA, &sink);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, Write);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sink);
                curl_easy_setopt(curl, CURLOPT_NOPROGRESS, request.progress ? 0L : 1L);
                curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, Progress);
                curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &request);
                std::string range;
                if (request.range) {
                    range = std::to_string(request.range->first) + "-" + std::to_string(request.range->last);
                    curl_easy_setopt(curl, CURLOPT_RANGE, range.c_str());
                }
                curl_slist* headers = nullptr;
                std::string auth;
                if (Origin(url) == boundary) {
                    for (const auto& h : request.headers) headers = curl_slist_append(headers, h.c_str());
                    if (!request.username.empty() || !request.password.empty()) {
                        auth = request.username + ":" + request.password;
                        curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
                        curl_easy_setopt(curl, CURLOPT_USERPWD, auth.c_str());
                    }
                }
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                result.curlCode = curl_easy_perform(curl);
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &result.status);
                char* value = nullptr;
                curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &value);
                if (value) result.contentType = value;
                curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &value);
                if (value) { const auto effective = CanonicalUrl(value); result.effectiveUrl = effective.empty() ? url : effective; }
                const std::string redirect = ResolveUrl(url, sink.location);
                // Copy all curl-owned strings before cleanup.
                result.diagnostic = errorBuffer.data();
                if (headers) curl_slist_free_all(headers);
                curl_easy_cleanup(curl);
                if (file) {
                    const bool flushed = fflush(file) == 0;
                    const bool closed = fclose(file) == 0;
                    if (!flushed || !closed) result.error = Error::Io;
                }
                if (sink.tooLarge) result.error = Error::TooLarge;
                else if (sink.writerFailed) result.error = Error::Io;
                else if (result.curlCode == CURLE_OPERATION_TIMEDOUT) result.error = Error::Timeout;
                else if (result.curlCode == CURLE_PEER_FAILED_VERIFICATION || result.curlCode == CURLE_SSL_CACERT_BADFILE || result.curlCode == CURLE_SSL_CONNECT_ERROR) result.error = Error::Tls;
                else if (result.curlCode != CURLE_OK) result.error = Error::Connectivity;
                if (result.error != Error::None) break;
                if (result.status >= 300 && result.status < 400) {
                    const std::string next = sink.location.empty() ? "" : CanonicalUrl(redirect);
                    if (hop == 5 || next.empty() || (requireHttps && Origin(next).rfind("https://", 0) != 0)) {
                        result.error = Error::Redirect; break;
                    }
                    url = next;
                    continue;
                }
                if (result.status < 200 || result.status >= 300) result.error = Error::Http;
                else if (request.range && result.status != 206) {
                    result.error = Error::Http; result.diagnostic = "Server did not honor the byte range.";
                }
                break;
            }
            if (!result.ok() && !path.empty()) std::remove(path.c_str());
            return result;
        }
    }
    std::string CanonicalUrl(const std::string& url) {
        CURLU* parsed = curl_url();
        if (!parsed) return {};
        std::string result;
        if (SetUrl(parsed, url)) {
            std::string host = Part(parsed, CURLUPART_HOST);
            std::transform(host.begin(), host.end(), host.begin(), [](unsigned char c) { return std::tolower(c); });
            curl_url_set(parsed, CURLUPART_HOST, host.c_str(), 0);
            curl_url_set(parsed, CURLUPART_FRAGMENT, nullptr, 0);
            result = Part(parsed, CURLUPART_URL, CURLU_NO_DEFAULT_PORT);
        }
        curl_url_cleanup(parsed);
        return result;
    }
    std::string Origin(const std::string& url) {
        CURLU* parsed = curl_url();
        if (!parsed) return {};
        std::string result;
        if (SetUrl(parsed, url)) {
            std::string host = Part(parsed, CURLUPART_HOST);
            std::transform(host.begin(), host.end(), host.begin(), [](unsigned char c) { return std::tolower(c); });
            result = Part(parsed, CURLUPART_SCHEME) + "://" + host + ":" + Part(parsed, CURLUPART_PORT, CURLU_DEFAULT_PORT);
        }
        curl_url_cleanup(parsed);
        return result;
    }
    std::string ResolveUrl(const std::string& base, const std::string& relative) {
        CURLU* parsed = curl_url();
        if (!parsed) return {};
        std::string result;
        if (SetUrl(parsed, base) && curl_url_set(parsed, CURLUPART_URL, relative.c_str(), 0) == CURLUE_OK)
            result = CanonicalUrl(Part(parsed, CURLUPART_URL));
        curl_url_cleanup(parsed);
        return result;
    }
    std::string Describe(const Result& result) {
        switch (result.error) {
            case Error::InvalidRequest: return "Invalid HTTP address or request.";
            case Error::Connectivity: return "Cannot connect. Check Wi-Fi and the host address.";
            case Error::Timeout: return "Request timed out. Try again later.";
            case Error::Tls: return "TLS connection failed. Check the console clock and server certificate.";
            case Error::Redirect: return "Unsafe or excessive redirects. Check the source address.";
            case Error::TooLarge: return "Response exceeds the size limit.";
            case Error::Io: return "Could not write the downloaded file.";
            case Error::Http:
                if (result.status == 401) return "HTTP 401: sign in with the correct credentials.";
                if (result.status == 403) return "HTTP 403: access denied by this source.";
                if (result.status == 404) return "HTTP 404: source not found.";
                if (result.status == 429) return "HTTP 429: rate limited. Try again later.";
                if (result.status >= 500) return "HTTP " + std::to_string(result.status) + ": server unavailable.";
                return "HTTP " + std::to_string(result.status) + ": request rejected.";
            case Error::Decode: return "Encrypted payload could not be decoded by this build.";
            case Error::None: return {};
        }
        return "Request failed.";
    }
    Result Get(const std::string& url, const Request& request) { return Perform(url, request, {}); }
    Result GetFile(const std::string& url, const std::string& path, const Request& request) { return Perform(url, request, path); }
}
