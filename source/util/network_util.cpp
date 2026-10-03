#include "util/remote_core.hpp"
/*
Copyright (c) 2017-2018 Adubbz

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "util/network_util.hpp"

#include <switch.h>
#include <curl/curl.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <limits>
#include "util/curl.hpp"
#include "util/error.hpp"
#include "util/hauth.hpp"
#include "identity/identity.hpp"
#include "util/lang.hpp"
#include "util/config.hpp"
#include "ui/instPage.hpp"
#include "ui/MainApplication.hpp"

namespace inst::ui {
    extern MainApplication *mainApp;
}

namespace tin::network
{
    static std::string g_basic_auth_user;
    static std::string g_basic_auth_pass;
    static std::string g_basic_auth_origin;
    static bool g_basic_auth_set = false;

    static std::string GetUrlOrigin(const std::string& url) { return inst::http::Origin(url); }

    static bool CanUseBasicAuthForUrl(const std::string& url)
    {
        return g_basic_auth_set && !g_basic_auth_origin.empty() && GetUrlOrigin(url) == g_basic_auth_origin;
    }

    static std::string TrimCopy(const std::string& in)
    {
        size_t start = 0;
        while (start < in.size() && std::isspace(static_cast<unsigned char>(in[start])))
            start++;

        size_t end = in.size();
        while (end > start && std::isspace(static_cast<unsigned char>(in[end - 1])))
            end--;

        return in.substr(start, end - start);
    }

    static bool IsDigitsOnly(const std::string& in)
    {
        if (in.empty())
            return false;
        for (unsigned char c : in) {
            if (c < '0' || c > '9')
                return false;
        }
        return true;
    }

    static std::string StripUrlFragment(const std::string& in)
    {
        const auto pos = in.find('#');
        if (pos == std::string::npos)
            return in;
        return in.substr(0, pos);
    }

    static void BuildVersionAndRevision(std::string& outVersion, std::string& outRevision)
    {
        const std::string raw = inst::remote::ActiveCapabilities().customIndex() ? "20.0.2" : inst::config::appVersion;
        outVersion = raw.empty() ? "0.0" : raw;
        outRevision = "0";

        const std::size_t firstDot = raw.find('.');
        if (firstDot == std::string::npos)
            return;

        const std::size_t secondDot = raw.find('.', firstDot + 1);
        if (secondDot == std::string::npos) {
            outVersion = raw;
            return;
        }

        outVersion = raw.substr(0, secondDot);
        const std::string revisionToken = raw.substr(secondDot + 1);
        if (revisionToken.empty())
            return;

        std::size_t digitsEnd = 0;
        while (digitsEnd < revisionToken.size()) {
            const char c = revisionToken[digitsEnd];
            if (c < '0' || c > '9')
                break;
            digitsEnd++;
        }
        if (digitsEnd > 0)
            outRevision = revisionToken.substr(0, digitsEnd);
    }

    static std::string UrlDecode(const std::string& value)
    {
        CURL* curl = curl_easy_init();
        if (!curl)
            return value;

        int outLength = 0;
        char* decoded = curl_easy_unescape(curl, value.c_str(), value.size(), &outLength);
        std::string result = decoded ? std::string(decoded, outLength) : value;
        if (decoded)
            curl_free(decoded);
        curl_easy_cleanup(curl);
        return result;
    }

    static int StreamHttpRangeForUrl(const std::string& url, const std::vector<std::string>& requestHeaders,
        size_t offset, size_t size, const std::function<size_t (u8* bytes, size_t size)>& streamFunc,
        std::string* outErrorResponse)
    {
        if (size == 0)
            return 0;

        const std::string requestUrl = TrimCopy(StripUrlFragment(url));
        const bool legacyRequest = inst::remote::ActiveCapabilities().customIndex();
        const bool useBasicAuth = CanUseBasicAuthForUrl(requestUrl);
        inst::http::Request request;
        request.timeoutMs = 0;
        request.verifyTls = false;
        request.range = inst::http::ByteRange{offset, offset + size - 1};
        request.maxBytes = size;
        request.credentialOrigin = requestUrl;
        request.userAgent = legacyRequest ? "" : inst::curl::getUserAgent();
        if (useBasicAuth) { request.username = g_basic_auth_user; request.password = g_basic_auth_pass; }
        const bool configuredOrigin = inst::config::remoteUrl.empty() || GetUrlOrigin(requestUrl) == GetUrlOrigin(inst::config::remoteUrl);
        if (configuredOrigin) {
            const std::string hauthHeader = "HAUTH: " + inst::util::ComputeHauthFromUrl(requestUrl);
            const std::string uauthHeader = "UAUTH: " + inst::util::ComputeUauthFromUrl(
                requestUrl,
                useBasicAuth ? g_basic_auth_user : "",
                useBasicAuth ? g_basic_auth_pass : "");
            request.headers.push_back( hauthHeader);
            request.headers.push_back( uauthHeader);
            if (!legacyRequest) {
                std::string versionValue;
                std::string revisionValue;
                BuildVersionAndRevision(versionValue, revisionValue);
                const std::string themeHeader = "Theme: 0000000000000000000000000000000000000000000000000000000000000000";
                const std::string uidHeader = "UID: " + inst::identity::GetActiveUid();
                const std::string versionHeader = "Version: " + versionValue;
                const std::string revisionHeader = "Revision: " + revisionValue;
                const std::string languageHeader = "Language: " + Language::GetRemoteHeaderLanguage();
                request.headers.push_back( themeHeader);
                request.headers.push_back( languageHeader);
                request.headers.push_back( uidHeader);
                request.headers.push_back( versionHeader);
                request.headers.push_back( revisionHeader);
            }
        }
        for (const auto& header : requestHeaders) request.headers.push_back(header);

        bool callbackException = false;
        request.writer = [&](const char* data, std::size_t bytes) {
            if (inst::ui::instPage::isInstallCancelRequested()) return std::size_t(0);
            try { return streamFunc(reinterpret_cast<u8*>(const_cast<char*>(data)), bytes); }
            catch (...) { callbackException = true; return std::size_t(0); }
        };
        const auto response = inst::http::Get(requestUrl, request);
        const CURLcode rc = response.curlCode;
        const auto httpCode = response.status;
        if (outErrorResponse)
            *outErrorResponse = inst::http::Describe(response);

        if (callbackException)
            return 1999;

        if (response.ok() && httpCode == 206)
            return 0;

        LOG_DEBUG("Range request failed http=%ld curl=%d\n", httpCode, static_cast<int>(rc));

        if (httpCode != 0 && httpCode != 206)
            return static_cast<int>(httpCode);

        return 1000 + static_cast<int>(rc);
    }

    // Translates StreamDataRange/StreamHttpRangeForUrl return codes into a
    // human-readable cause so install logs stop showing an opaque "rc=1".
    static std::string DescribeRangeError(int rc, size_t sizeRead, size_t sizeExpected, const std::string& response = "")
    {
        std::stringstream ss;
        if (rc == 1999)
            ss << "write callback exception";
        else if (rc == 200)
            ss << "HTTP 200: server ignored the Range request (no partial content support)";
        else if (rc >= 100 && rc < 600)
            ss << "HTTP status " << rc;
        else if (rc >= 1000 && rc < 1999)
            ss << "curl error " << (rc - 1000) << ": " << curl_easy_strerror(static_cast<CURLcode>(rc - 1000));
        else if (rc == 0 && sizeRead != sizeExpected)
            ss << "short read";
        else
            ss << "rc=" << rc;

        if (sizeRead != sizeExpected)
            ss << ", got " << sizeRead << "/" << sizeExpected << " bytes";
        if (!response.empty())
            ss << "; server response: " << response;
        return ss.str();
    }

    HTTPHeader::HTTPHeader(std::string url) :
        m_url(url)
    {
    }

    void HTTPHeader::PerformRequest()
    {
        inst::http::Request request;
        request.head = true;
        request.verifyTls = false;
        request.userAgent = inst::curl::getUserAgent();
        request.credentialOrigin = g_basic_auth_origin;
        if (CanUseBasicAuthForUrl(m_url)) {
            request.username = g_basic_auth_user;
            request.password = g_basic_auth_pass;
        }
        const auto response = inst::http::Get(m_url, request);
        if (!response.ok()) THROW_FORMAT("Failed to retrieve HTTP header: %s\n", inst::http::Describe(response).c_str());
        m_values = response.headers;
    }

    bool HTTPHeader::HasValue(std::string key)
    {
        return m_values.count(key);
    }

    std::string HTTPHeader::GetValue(std::string key)
    {
        return m_values[key];
    }

    HTTPDownload::HTTPDownload(std::string url, std::vector<std::string> requestHeaders) :
        m_url(url), m_requestHeaders(std::move(requestHeaders)), m_header(url)
    {
        m_url = TrimCopy(m_url);
        const bool isJbod = StartsWithNoCase(m_url, "jbod:");
        if (isJbod) {
            m_isJbod = true;

            const std::string payload = m_url.substr(5);
            std::stringstream ss(payload);
            std::string token;
            std::vector<std::string> tokens;
            while (std::getline(ss, token, '/')) {
                if (!token.empty())
                    tokens.push_back(token);
            }

            if (tokens.size() < 2)
                THROW_FORMAT("Invalid JBOD URL format\n");

            size_t defaultChunkSize = 0;
            if (!ParseUnsignedSize(tokens[0], defaultChunkSize) || defaultChunkSize == 0)
                THROW_FORMAT("Invalid JBOD chunk size\n");

            size_t runningOffset = 0;
            bool sawAnyUrl = false;
            bool hasPendingSizeOverride = false;
            size_t pendingSizeOverride = 0;
            for (size_t i = 1; i < tokens.size(); i++) {
                if (IsDigitsOnly(tokens[i])) {
                    if (!ParseUnsignedSize(tokens[i], pendingSizeOverride) || pendingSizeOverride == 0)
                        THROW_FORMAT("Invalid JBOD part size override\n");
                    hasPendingSizeOverride = true;
                    continue;
                }

                std::string partUrl = UrlDecode(tokens[i]);
                partUrl = TrimCopy(StripUrlFragment(partUrl));
                if (partUrl.rfind("http://", 0) != 0 && partUrl.rfind("https://", 0) != 0)
                    THROW_FORMAT("Invalid JBOD part URL\n");

                const bool explicitSize = hasPendingSizeOverride;
                const size_t partSize = explicitSize ? pendingSizeOverride : defaultChunkSize;
                if (runningOffset > std::numeric_limits<size_t>::max() - partSize)
                    THROW_FORMAT("JBOD part sizes overflow\n");

                m_jbodSegments.push_back({partUrl, runningOffset, partSize, false});
                runningOffset += partSize;
                sawAnyUrl = true;
                hasPendingSizeOverride = false;
                pendingSizeOverride = 0;
            }

            if (!sawAnyUrl)
                THROW_FORMAT("Invalid JBOD URL format (no parts)\n");
            if (hasPendingSizeOverride)
                THROW_FORMAT("Invalid JBOD URL format (dangling size override)\n");

            m_jbodSize = runningOffset;
            if (!m_jbodSegments.empty()) {
                m_jbodSegments.back().openEnded = true;
                m_jbodSize = std::numeric_limits<size_t>::max();
            }
            m_rangesSupported = true;
            return;
        }

        m_rangesSupported = true;
    }

    size_t HTTPDownload::ParseHTMLData(char* bytes, size_t size, size_t numItems, void* userData)
    {
        auto streamFunc = *reinterpret_cast<std::function<size_t (u8* bytes, size_t size)>*>(userData);
        const size_t numBytes = size * numItems;
        try {
            if (streamFunc != nullptr)
                return streamFunc((u8*)bytes, numBytes);
            return numBytes;
        } catch (...) {
            return 0;
        }
    }

    void HTTPDownload::BufferDataRange(void* buffer, size_t offset, size_t size, std::function<void (size_t sizeRead)> progressFunc)
    {
        size_t sizeRead = 0;

        auto streamFunc = [&](u8* streamBuf, size_t streamBufSize) -> size_t
        {
            if (sizeRead + streamBufSize > size)
            {
                LOG_DEBUG("New read size 0x%lx would exceed total expected size 0x%lx\n", sizeRead + streamBufSize, size);
                return 0;
            }

            if (progressFunc != nullptr)
                progressFunc(sizeRead);

            memcpy(reinterpret_cast<u8*>(buffer) + sizeRead, streamBuf, streamBufSize);
            sizeRead += streamBufSize;
            return streamBufSize;
        };

        const int rc = this->StreamDataRange(offset, size, streamFunc);
        if (rc != 0 || sizeRead != size)
        {
            THROW_FORMAT("HTTP range read failed (%s)\n", DescribeRangeError(rc, sizeRead, size, m_lastErrorResponse).c_str());
        }
    }

    int HTTPDownload::StreamDataRange(size_t offset, size_t size, std::function<size_t (u8* bytes, size_t size)> streamFunc, std::function<bool()> retryConfirmFunc)
    {
        m_lastErrorResponse.clear();
        if (size == 0)
            return 0;

        if (!m_rangesSupported)
            THROW_FORMAT("Attempted range request when ranges aren't supported!\n");

        static constexpr int kMaxRetries = 3;
        static constexpr u64 kRetryDelayNs = 2000000000ULL;

        auto streamWithRetry = [&](const std::string& url, size_t requestOffset, size_t requestSize) -> int
        {
            size_t bytesReceived = 0;
            int lastRc = 1;

            auto trackingFunc = [&](u8* buf, size_t sz) -> size_t {
                size_t written = streamFunc(buf, sz);
                bytesReceived += written;
                return written;
            };

            while (true)
            {
                for (int attempt = 0; attempt <= kMaxRetries; attempt++)
                {
                    if (attempt > 0)
                    {
                        LOG_DEBUG("StreamDataRange: retry %d/%d, resuming at offset %zu+%zu\n",
                            attempt, kMaxRetries, requestOffset, bytesReceived);
                        svcSleepThread(kRetryDelayNs);
                    }

                    const size_t currentOffset = requestOffset + bytesReceived;
                    const size_t remaining = requestSize - bytesReceived;

                    if (remaining == 0)
                        return 0;

                    std::string errorResponse;
                    const int rc = StreamHttpRangeForUrl(url, m_requestHeaders, currentOffset, remaining, trackingFunc, &errorResponse);
                    if (!errorResponse.empty())
                        m_lastErrorResponse = std::move(errorResponse);
                    if (rc == 0)
                        return 0;

                    lastRc = rc;

                    // 200 = server ignored the Range header; 4xx/416 = request will
                    // never succeed. Retrying those only delays the same failure.
                    const bool fatal =
                        rc == 1999 ||
                        rc == 200 ||
                        (rc >= 400 && rc < 500) ||
                        rc == 1000 + CURLE_WRITE_ERROR;

                    if (fatal)
                    {
                        LOG_DEBUG("StreamDataRange: fatal error, aborting (url=%s rc=%d)\n",
                            url.c_str(), rc);
                        return rc;
                    }

                    LOG_DEBUG("StreamDataRange: retriable error (url=%s rc=%d), %d retries left\n",
                        url.c_str(), rc, kMaxRetries - attempt);
                }

                LOG_DEBUG("StreamDataRange: auto-retries exhausted for %s\n", url.c_str());
                if (retryConfirmFunc && retryConfirmFunc())
                {
                    LOG_DEBUG("StreamDataRange: user requested another retry cycle for %s\n", url.c_str());
                    continue;
                }
                break;
            }

            return lastRc;
        };

        if (!m_isJbod)
            return streamWithRetry(m_url, offset, size);

        size_t globalOffset = offset;
        size_t remaining = size;

        while (remaining > 0) {
            auto it = std::find_if(m_jbodSegments.begin(), m_jbodSegments.end(),
                [globalOffset](const JbodSegment& seg) {
                    if (globalOffset < seg.offset)
                        return false;
                    if (seg.openEnded)
                        return true;
                    return globalOffset < (seg.offset + seg.size);
                });
            if (it == m_jbodSegments.end()) {
                if (!m_jbodSegments.empty()) {
                    auto last = m_jbodSegments.end() - 1;
                    if (last->openEnded && globalOffset >= last->offset) {
                        it = last;
                    }
                }
                if (it == m_jbodSegments.end())
                    THROW_FORMAT("JBOD segment lookup failed\n");
            }

            const size_t localOffset = globalOffset - it->offset;
            size_t chunkRemaining = 0;
            if (it->openEnded) {
                chunkRemaining = remaining;
            } else {
                if (localOffset >= it->size)
                    THROW_FORMAT("JBOD segment offset out of range\n");
                chunkRemaining = it->size - localOffset;
            }
            const size_t readNow = std::min(remaining, chunkRemaining);

            const int rc = streamWithRetry(it->url, localOffset, readNow);
            if (rc != 0)
                return rc;

            globalOffset += readNow;
            remaining -= readNow;
        }

        return 0;
    }

    void SetBasicAuth(const std::string& user, const std::string& pass, const std::string& trustedOrigin)
    {
        g_basic_auth_user = user;
        g_basic_auth_pass = pass;
        g_basic_auth_origin = GetUrlOrigin(trustedOrigin);
        g_basic_auth_set = !g_basic_auth_origin.empty();
    }

    void ClearBasicAuth()
    {
        g_basic_auth_user.clear();
        g_basic_auth_pass.clear();
        g_basic_auth_origin.clear();
        g_basic_auth_set = false;
    }

    size_t WaitReceiveNetworkData(int sockfd, void* buf, size_t len)
    {
        int ret = 0;
        size_t read = 0;
        u64 lastRenderTick = armGetSystemTick();
        const u64 renderInterval = armGetSystemTickFreq() / 4;

        while ((((ret = recv(sockfd, (u8*)buf + read, len - read, 0)) > 0 && (read += ret) < len) || errno == EAGAIN))
        {
            errno = 0;
            inst::ui::mainApp->RefreshInputDevice();
            const u64 now = armGetSystemTick();
            if (now - lastRenderTick >= renderInterval) {
                lastRenderTick = now;
                inst::ui::mainApp->CallForRender();
            }
        }

        return read;
    }

    size_t WaitSendNetworkData(int sockfd, void* buf, size_t len)
    {
        int ret = 0;
        size_t written = 0;

        while (written < len)
        {
            inst::ui::mainApp->RefreshInputDevice();
            inst::ui::mainApp->UpdateButtons();
            u64 kDown = inst::ui::mainApp->GetButtonsDown();
            if (kDown & HidNpadButton_B)
                break;

            errno = 0;
            ret = send(sockfd, (u8*)buf + written, len - written, 0);

            if (ret < 0) {
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    sleep(5);
                    continue;
                }
                break;
            }

            written += ret;
        }

        return written;
    }

    void NSULDrop(std::string url)
    {
        CURL* curl = curl_easy_init();

        if (!curl)
        {
            THROW_FORMAT("Failed to initialize curl\n");
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DROP");
        const std::string& userAgent = inst::curl::getUserAgent();
        curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, false);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 50);

        curl_easy_perform(curl);

        curl_easy_cleanup(curl);
    }
}
