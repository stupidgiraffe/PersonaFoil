#include "util/remote_core.hpp"
#include <cassert>
#include <iostream>
#include <map>

using namespace inst;
http::Result response(std::string body, long status = 200) {
    http::Result result; result.body = std::move(body); result.status = status;
    if (status >= 400) result.error = http::Error::Http;
    return result;
}
int main() {
    const std::string root = "https://root.example/index.json";
    std::map<std::string, http::Result> fixtures;
    std::vector<std::string> fetched;
    remote::Fetch fetch = [&](const std::string& url, remote::RequestProfile, bool credentials) {
        assert(credentials == (http::Origin(url) == http::Origin(root)));
        fetched.push_back(url);
        return fixtures.count(url) ? fixtures.at(url) : response("", 404);
    };
    remote::Consume consume = [&](const nlohmann::json& node, const std::string& url, const remote::IndexPolicy& policy, std::string& error) {
        if (url.find("other.example") != std::string::npos) assert(policy.headers.empty() && policy.googleApiKey.empty());
        if (node.contains("error")) { error = "Source error"; return std::size_t(0); }
        if (!node.contains("files") && !node.contains("directories")) { error = "Unsupported schema"; return std::size_t(0); }
        return node.contains("files") ? node["files"].size() : std::size_t(0);
    };
    fixtures["https://root.example/good"] = response(R"({"files":["a"]})");
    fixtures["https://other.example/good"] = response(R"({"files":["b"]})");
    fixtures["https://root.example/dead"] = response("", 403);
    fixtures["https://root.example/bad"] = response("bad");
    const auto aggregate = response(R"({"headers":["Authorization: fixture"],"googleApiKey":"fixture","directories":["/good","/dead","/bad","https://other.example/good","/good","/index.json"]})");
    auto report = remote::WalkIndex(root, aggregate, fetch, consume);
    assert(report.error.empty() && report.items == 2 && report.usableSources == 2 && report.warnings.size() == 2);
    assert(report.requested == 5);
    fixtures["https://root.example/redirect"] = response(R"({"files":["b"]})");
    fixtures["https://root.example/redirect"].effectiveUrl = "https://other.example/redirected";
    report = remote::WalkIndex(root, response(R"({"headers":["Authorization: fixture"],"googleApiKey":"fixture","directories":["/redirect"]})"), fetch, consume);
    assert(report.error.empty() && report.items == 1);
    report = remote::WalkIndex(root, response(R"({"directories":["/dead","/bad"]})"), fetch, consume);
    assert(report.items == 0 && !report.error.empty());
    report = remote::WalkIndex(root, response("invalid"), fetch, consume);
    assert(!report.error.empty() && report.requested == 1);
    fixtures["https://root.example/cycle"] = response(R"({"files":["a"],"directories":["/cycle","/index.json"]})");
    report = remote::WalkIndex(root, response(R"({"directories":["/cycle"]})"), fetch, consume);
    assert(report.items == 1 && report.requested == 2);
    remote::Limits limits; limits.depth = 0;
    report = remote::WalkIndex(root, aggregate, fetch, consume, limits);
    assert(!report.error.empty() && report.requested == 1);
    limits = {}; limits.sources = 2;
    report = remote::WalkIndex(root, aggregate, fetch, consume, limits);
    assert(report.items == 1 && report.requested == 2);
    limits = {}; limits.items = 1;
    report = remote::WalkIndex(root, response(R"({"files":["a","b"]})"), fetch, consume, limits);
    assert(!report.error.empty());
    auto modern = response(R"({"sections":[{"id":"all","items":[]}]})");
    int calls = 0;
    remote::Fetch modernFetch = [&](const std::string& url, remote::RequestProfile profile, bool) {
        ++calls; assert(profile == remote::RequestProfile::Modern);
        return url.find("/api/remote/sections") != std::string::npos ? modern : response("", 404);
    };
    auto negotiation = remote::Negotiate("https://root.example", remote::Compatibility::Auto, modernFetch);
    assert(negotiation.error.empty() && negotiation.capabilities.family == remote::Family::Modern && calls == 1);
    const auto cached = negotiation.capabilities;
    calls = 0;
    negotiation = remote::Negotiate("https://root.example", remote::Compatibility::Auto, modernFetch, &cached);
    assert(negotiation.error.empty() && calls == 1);
    remote::Fetch indexFetch = [&](const std::string&, remote::RequestProfile profile, bool) {
        return profile == remote::RequestProfile::Modern ? response("", 404) : response(R"({"files":["a"]})");
    };
    assert(remote::Negotiate("https://root.example", remote::Compatibility::Auto, indexFetch).capabilities.customIndex());
    assert(!remote::Negotiate("https://root.example", remote::Compatibility::Modern, indexFetch).error.empty());
    negotiation = remote::Negotiate(root, remote::Compatibility::Tinfoil, indexFetch);
    assert(negotiation.error.empty() && negotiation.capabilities.profile == remote::RequestProfile::Tinfoil);
    remote::Fetch challenged = [&](const std::string&, remote::RequestProfile profile, bool) {
        return profile == remote::RequestProfile::Tinfoil ? response(R"({"files":["a"]})") : response("", 401);
    };
    negotiation = remote::Negotiate(root, remote::Compatibility::Auto, challenged);
    assert(negotiation.error.empty() && negotiation.capabilities.profile == remote::RequestProfile::Tinfoil);
    assert(remote::ReadCompatibility(nlohmann::json{{"legacyMode", true}}) == remote::Compatibility::Tinfoil);
    assert(remote::ReadCompatibility(nlohmann::json{{"legacyMode", false}}) == remote::Compatibility::Auto);
    assert(remote::ReadCompatibility(nlohmann::json{{"legacyMode", true}, {"compatibility", "modern"}}) == remote::Compatibility::Modern);
    std::cout << "Remote aggregation, negotiation, credential policy and migration tests passed\n";
}
