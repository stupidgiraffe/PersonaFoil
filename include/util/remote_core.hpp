#pragma once
#include "util/http.hpp"
#include "util/json.hpp"
#include <ctime>
#include <functional>
#include <string>
#include <vector>

namespace inst::remote {
    enum class Compatibility { Auto, Modern, Tinfoil };
    enum class Family { Unknown, Modern, CustomIndex };
    enum class RequestProfile { Public, Modern, Tinfoil };
    struct Capabilities {
        Family family = Family::Unknown;
        RequestProfile profile = RequestProfile::Public;
        std::string apiPrefix = "/api/remote";
        std::string endpoint;
        std::time_t negotiatedAt = 0;
        bool customIndex() const { return family == Family::CustomIndex; }
        bool saveSync() const { return family == Family::Modern; }
    };
    struct Warning { std::string endpoint; std::string message; };
    struct AggregateReport {
        std::vector<Warning> warnings;
        std::vector<std::string> sources;
        std::size_t requested = 0;
        std::size_t usableSources = 0;
        std::size_t items = 0;
        std::string error;
    };
    struct IndexPolicy { std::string googleApiKey; std::vector<std::string> headers; };
    struct Limits {
        std::size_t depth = 6;
        std::size_t sources = 32;
        std::size_t items = 100000;
        std::size_t bytes = 64 * 1024 * 1024;
    };
    using Fetch = std::function<http::Result(const std::string&, RequestProfile, bool useCredentials)>;
    using Consume = std::function<std::size_t(const nlohmann::json&, const std::string&, const IndexPolicy&, std::string&)>;
    struct Negotiation { Capabilities capabilities; http::Result response; std::string error; };
    const char* CompatibilityName(Compatibility mode);
    Compatibility ReadCompatibility(const nlohmann::json& node, const char* newKey = "compatibility", const char* oldKey = "legacyMode");
    nlohmann::json ParseDocument(const std::string& body);
    Family DetectFamily(const std::string& body);
    Negotiation Negotiate(const std::string& url, Compatibility mode, const Fetch& fetch, const Capabilities* cached = nullptr);
    AggregateReport WalkIndex(const std::string& url, const http::Result& root, const Fetch& fetch, const Consume& consume, const Limits& limits = {});
    void SetActiveCapabilities(const Capabilities& capabilities);
    Capabilities ActiveCapabilities();
    void SetLastReport(const AggregateReport& report);
    AggregateReport LastReport();
}
