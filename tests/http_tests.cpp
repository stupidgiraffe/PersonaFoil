#include "util/http.hpp"
#include "util/update.hpp"
#include <cassert>
#include <iostream>

int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--github") {
        inst::http::Request request;
        request.timeoutMs = 8000;
        request.maxBytes = 1024 * 1024;
        const auto response = inst::http::Get("https://api.github.com/repos/stupidgiraffe/PersonaFoil/releases/latest", request);
        const auto release = inst::update::ParseReleaseResponse(response, "0.1.2");
        assert(response.ok() && release.status == inst::update::CheckStatus::UpToDate);
        std::cout << "Live official GitHub release check: up to date (" << release.release.version << ")\n";
        return 0;
    }
    assert(argc == 3);
    using namespace inst::http;
    const std::string base = argv[1];
    Request request;
    request.timeoutMs = 500;
    assert(Get(base + "/range", request).body == "none");
    request.range = ByteRange{0, 3};
    auto ranged = Get(base + "/range", request);
    assert(ranged.ok() && ranged.body == "bytes=0-3");
    request.range = ByteRange{3, 0};
    assert(Get(base + "/range", request).error == Error::InvalidRequest);
    request.range.reset();
    assert(Get(base + "/empty", request).ok());
    auto missing = Get(base + "/missing", request);
    assert(missing.error == Error::Http && missing.status == 404);
    assert(Get(base + "/slow", request).error == Error::Timeout);
    request.maxBytes = 8;
    assert(Get(base + "/large", request).error == Error::TooLarge);
    request.maxBytes = 1024;
    std::string streamed;
    request.writer = [&](const char* data, std::size_t count) { streamed.append(data, count); return count; };
    assert(Get(base + "/payload", request).ok() && streamed == "test");
    streamed.clear();
    assert(Get(base + "/missing", request).error == Error::Http && streamed.empty());
    request.range = ByteRange{0, 3};
    assert(Get(base + "/payload", request).error == Error::Http && streamed.empty());
    request.range.reset();
    request.writer = [](const char*, std::size_t) { return std::size_t(0); };
    assert(Get(base + "/payload", request).error == Error::Io);
    request.writer = {};
    request.headers = {"HAUTH: fixture", "UAUTH: fixture", "UID: fixture"};
    request.username = "fixture"; request.password = "fixture";
    auto same = Get(base + "/same", request);
    assert(same.body == "present" && same.effectiveUrl.find("fixture") == std::string::npos);
    assert(Get(base + "/cross", request).body == "absent");
    request.head = true;
    const auto header = Get(base + "/cross", request);
    assert(header.ok() && header.body.empty() && header.headers.at("x-credentials") == "absent");
    request.head = false;
    assert(Get(base + "/cycle", request).error == Error::Redirect);
    assert(!Get(argv[2], request).ok()); // Self-signed certificate rejected.
    request.verifyTls = false;
    assert(Get(argv[2], request).ok());
    assert(Origin("https://EXAMPLE.com/a") == Origin("https://example.com:443/b"));
    assert(Origin("https://example.com/a") != Origin("http://example.com/a"));
    assert(CanonicalUrl("https://user:pass@example.com/").empty());
    assert(ResolveUrl("https://example.com/a/index.json", "../child") == "https://example.com/child");
    using namespace inst::update;
    Result response; response.status = 200;
    response.body = R"({"tag_name":"v0.1.1","assets":[]})";
    assert(ParseReleaseResponse(response, "0.1.1").status == CheckStatus::UpToDate);
    assert(ParseReleaseResponse(response, "0.1.2").status == CheckStatus::UpToDate);
    assert(ParseReleaseResponse(response, "0.1.0").status == CheckStatus::Error);
    response.body = R"({"tag_name":"v0.1.2","assets":[{"name":"personafoil.nro","browser_download_url":"https://github.com/stupidgiraffe/PersonaFoil/releases/download/v0.1.2/personafoil.nro"},{"name":"SHA256SUMS.txt","browser_download_url":"https://github.com/stupidgiraffe/PersonaFoil/releases/download/v0.1.2/SHA256SUMS.txt"}]})";
    assert(ParseReleaseResponse(response, "0.1.1").status == CheckStatus::UpdateAvailable);
    assert(ParseReleaseResponse(response, "0.1.1-dev").status == CheckStatus::Error);
    response.body = "invalid";
    assert(ParseReleaseResponse(response, "0.1.1").status == CheckStatus::Error);
    response.body = R"({"tag_name":"v0.1.3-rc.1"})";
    assert(ParseReleaseResponse(response, "0.1.1").status == CheckStatus::Error);
    response.status = 404; response.error = Error::Http;
    assert(ParseReleaseResponse(response, "0.1.1").status == CheckStatus::NoRelease);
    response.status = 429;
    assert(ParseReleaseResponse(response, "0.1.1").error.find("rate limited") != std::string::npos);
    std::cout << "HTTP transport and release response tests passed\n";
}
