#include "util/remote_core.hpp"
#include <algorithm>
#include <mutex>
#include <stdexcept>
#include <chrono>
#include <unordered_set>

namespace inst::remote {
    namespace {
        std::mutex stateMutex;
        Capabilities active;
        AggregateReport lastReport;
        bool Parse(const std::string& body, nlohmann::json& out) {
            try { out = ParseDocument(body); return out.is_object(); } catch (...) { return false; }
        }
        std::string DirectoryUrl(const nlohmann::json& entry) {
            if (entry.is_string()) return entry.get<std::string>();
            if (entry.is_object()) for (const char* key : {"url", "path", "directory"})
                if (entry.contains(key) && entry[key].is_string()) return entry[key].get<std::string>();
            return {};
        }
        bool IsIndex(const nlohmann::json& json) {
            return json.contains("files") || json.contains("directories") || json.contains("paths") || json.contains("titledb");
        }
        bool NeedsCompatibilityRetry(const http::Result& response) {
            if (response.status == 401 || response.status == 403 || response.error == http::Error::Decode ||
                response.contentType.find("application/octet-stream") != std::string::npos) return true;
            nlohmann::json node;
            return response.ok() && Parse(response.body, node) && node.contains("error") &&
                node["error"].is_string() && !node["error"].get<std::string>().empty();
        }
    }
    nlohmann::json ParseDocument(const std::string& body) {
        return nlohmann::json::parse(body, [](int depth, nlohmann::json::parse_event_t, nlohmann::json&) {
            if (depth > 64) throw std::runtime_error("JSON nesting limit exceeded");
            return true;
        });
    }
    const char* CompatibilityName(Compatibility mode) {
        switch (mode) { case Compatibility::Auto: return "auto"; case Compatibility::Modern: return "modern"; case Compatibility::Tinfoil: return "tinfoil"; }
        return "auto";
    }
    Compatibility ReadCompatibility(const nlohmann::json& node, const char* newKey, const char* oldKey) {
        if (node.contains(newKey) && node[newKey].is_string()) {
            const auto value = node[newKey].get<std::string>();
            if (value == "modern") return Compatibility::Modern;
            if (value == "tinfoil") return Compatibility::Tinfoil;
            if (value == "auto") return Compatibility::Auto;
        }
        return node.contains(oldKey) && node[oldKey].is_boolean() && node[oldKey].get<bool>() ? Compatibility::Tinfoil : Compatibility::Auto;
    }
    Family DetectFamily(const std::string& body) {
        nlohmann::json node;
        if (!Parse(body, node)) return Family::Unknown;
        if (node.contains("sections") && node["sections"].is_array()) return Family::Modern;
        return IsIndex(node) ? Family::CustomIndex : Family::Unknown;
    }
    Negotiation Negotiate(const std::string& rawUrl, Compatibility mode, const Fetch& fetch, const Capabilities* cached) {
        Negotiation result;
        const std::string url = http::CanonicalUrl(rawUrl);
        if (url.empty()) { result.error = "Invalid Remote address."; return result; }
        auto tryEndpoint = [&](const std::string& endpoint, RequestProfile profile, Family expected, const std::string& prefix) {
            result.response = fetch(endpoint, profile, true);
            if (!result.response.ok()) { result.error = http::Describe(result.response); return false; }
            const auto detected = DetectFamily(result.response.body);
            if (detected == Family::Unknown || (expected != Family::Unknown && detected != expected)) {
                nlohmann::json node;
                result.error = Parse(result.response.body, node) ? "Unsupported Remote schema." : "Invalid JSON response.";
                return false;
            }
            result.capabilities = {detected, profile, prefix, endpoint, std::time(nullptr)};
            result.error.clear();
            return true;
        };
        if (cached && cached->family != Family::Unknown && std::time(nullptr) >= cached->negotiatedAt &&
            std::time(nullptr) - cached->negotiatedAt < 900 &&
            (mode == Compatibility::Auto || (mode == Compatibility::Modern && cached->family == Family::Modern) ||
             (mode == Compatibility::Tinfoil && cached->family == Family::CustomIndex)) &&
            http::Origin(cached->endpoint) == http::Origin(url) &&
            tryEndpoint(cached->endpoint, cached->profile, cached->family, cached->apiPrefix)) return result;
        if (mode == Compatibility::Tinfoil) {
            tryEndpoint(url, RequestProfile::Tinfoil, Family::Unknown, "/api/remote");
            return result;
        }
        // Explicit JSON feeds are indexes; avoid probing paths beneath a filename.
        const bool fileFeed = url.find(".json") != std::string::npos;
        if (mode == Compatibility::Auto && fileFeed && tryEndpoint(url, RequestProfile::Public, Family::Unknown, "/api/remote")) return result;
        for (const char* prefix : {"/api/remote", "/api/shop"}) {
            if (fileFeed && mode == Compatibility::Auto) break;
            const std::string endpoint = url + (url.back() == '/' ? "" : "/") + std::string(prefix + 1) + "/sections";
            if (tryEndpoint(endpoint, RequestProfile::Modern, Family::Modern, prefix)) return result;
        }
        if (mode == Compatibility::Modern) return result;
        if (tryEndpoint(url, RequestProfile::Public, Family::Unknown, "/api/remote")) return result;
        // Retry an opted-in source after an authentication/schema challenge, not every host.
        if (NeedsCompatibilityRetry(result.response)) tryEndpoint(url, RequestProfile::Tinfoil, Family::Unknown, "/api/remote");
        return result;
    }
    AggregateReport WalkIndex(const std::string& rawUrl, const http::Result& root, const Fetch& fetch, const Consume& consume, const Limits& limits) {
        AggregateReport report;
        std::unordered_set<std::string> seen;
        const auto rootUrl = http::CanonicalUrl(rawUrl);
        const auto credentialOrigin = http::Origin(rootUrl);
        std::size_t bytes = 0;
        const auto started = std::chrono::steady_clock::now();
        auto warn = [&](const std::string& url, const std::string& error) {
            if (report.warnings.size() < limits.sources + 1) report.warnings.push_back({url, error});
        };
        std::function<bool(const std::string&, const http::Result&, std::size_t, const IndexPolicy&)> visit;
        visit = [&](const std::string& url, const http::Result& response, std::size_t depth, const IndexPolicy& inherited) {
            std::string error;
            if (!response.ok()) error = http::Describe(response);
            else if (response.body.size() > limits.bytes - bytes) error = "Aggregate response exceeds the size limit.";
            bytes += std::min(response.body.size(), limits.bytes - bytes);
            nlohmann::json node;
            if (error.empty() && !Parse(response.body, node)) error = "Invalid JSON response.";
            if (!error.empty()) { if (depth == 0) report.error = error; else warn(url, error); return false; }
            IndexPolicy policy = inherited;
            if (node.contains("googleApiKey") && node["googleApiKey"].is_string()) policy.googleApiKey = node["googleApiKey"].get<std::string>();
            if (node.contains("headers")) {
                policy.headers.clear();
                if (!node["headers"].is_array() || node["headers"].size() > 32) error = "Invalid custom request headers.";
                else for (const auto& value : node["headers"]) {
                    if (!value.is_string()) { error = "Invalid custom request header."; break; }
                    const auto h = value.get<std::string>();
                    if (h.size() > 4096 || h.find_first_of("\r\n") != std::string::npos || h.find(':') == std::string::npos) { error = "Invalid custom request header."; break; }
                    policy.headers.push_back(h);
                }
            }
            if (node.contains("directories") && !node["directories"].is_array()) error = "Directories must be an array.";
            // Bound entry allocations before invoking the existing item parser.
            std::size_t entries = 0;
            for (const char* key : {"files", "paths", "titledb"}) if (node.contains(key)) entries += node[key].size();
            if (node.contains("sections") && node["sections"].is_array()) for (const auto& section : node["sections"])
                if (section.is_object() && section.contains("items")) entries += section["items"].size();
            if (entries > limits.items - report.items) error = "Remote item limit exceeded.";
            std::size_t count = 0;
            if (error.empty()) {
                try { count = consume(node, url, policy, error); } catch (...) { error = "Invalid index content."; }
            }
            if (!error.empty()) { if (depth == 0) report.error = error; else warn(url, error); return false; }
            report.items += count;
            report.sources.push_back(url);
            if (count > 0) ++report.usableSources;
            if (node.contains("directories") && node["directories"].size() > limits.sources) warn(url, "Extra child endpoints skipped at the source limit.");
            if (node.contains("directories")) for (std::size_t childIndex = 0; childIndex < node["directories"].size() && childIndex < limits.sources; ++childIndex) {
                const auto& child = node["directories"][childIndex];
                const auto childUrl = http::ResolveUrl(url, DirectoryUrl(child));
                if (childUrl.empty()) { warn(url, "Invalid child endpoint."); continue; }
                if (seen.count(childUrl)) continue;
                if (depth >= limits.depth || report.requested >= limits.sources || bytes >= limits.bytes || std::chrono::steady_clock::now() - started > std::chrono::seconds(90)) {
                    warn(childUrl, "Source traversal limit reached."); continue;
                }
                seen.insert(childUrl); ++report.requested;
                const bool same = http::Origin(childUrl) == credentialOrigin;
                auto childResponse = fetch(childUrl, RequestProfile::Public, same);
                if (NeedsCompatibilityRetry(childResponse))
                    childResponse = fetch(childUrl, RequestProfile::Tinfoil, same);
                const auto effective = childResponse.effectiveUrl.empty() ? childUrl : http::CanonicalUrl(childResponse.effectiveUrl);
                const bool inherit = http::Origin(effective.empty() ? childUrl : effective) == http::Origin(url);
                if (!effective.empty() && effective != childUrl && !seen.insert(effective).second) continue;
                visit(effective.empty() ? childUrl : effective, childResponse, depth + 1, inherit ? policy : IndexPolicy{});
            }
            return true;
        };
        if (rootUrl.empty()) { report.error = "Invalid root endpoint."; return report; }
        seen.insert(rootUrl); report.requested = 1;
        const auto effective = root.effectiveUrl.empty() ? rootUrl : http::CanonicalUrl(root.effectiveUrl);
        seen.insert(effective);
        visit(effective.empty() ? rootUrl : effective, root, 0, {});
        if (report.error.empty() && report.items == 0 && !report.warnings.empty()) report.error = "No usable child sources. Check source details.";
        return report;
    }
    void SetActiveCapabilities(const Capabilities& value) { std::lock_guard<std::mutex> lock(stateMutex); active = value; }
    Capabilities ActiveCapabilities() { std::lock_guard<std::mutex> lock(stateMutex); return active; }
    void SetLastReport(const AggregateReport& report) { std::lock_guard<std::mutex> lock(stateMutex); lastReport = report; }
    AggregateReport LastReport() { std::lock_guard<std::mutex> lock(stateMutex); return lastReport; }
}
