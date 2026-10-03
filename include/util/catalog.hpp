#pragma once
#include "util/remote_core.hpp"
#include <string>
#include <vector>

namespace inst::catalog {
    constexpr std::size_t kMaxBytes = 256 * 1024;
    constexpr const char* kEndpoint = "https://raw.githubusercontent.com/stupidgiraffe/PersonaFoil/catalog-data/catalog-v1.json";
    struct Entry {
        std::string id, title, protocol, host, path;
        int port = 443;
        inst::remote::Compatibility compatibility = inst::remote::Compatibility::Auto;
        std::string authentication, sourceUrl, provenance, discoveredAt, checkedAt, health, redirect;
        int httpStatus = 0;
        std::vector<std::string> tags;
        std::string url() const;
    };
    struct Catalog {
        std::string generatedAt;
        std::vector<Entry> entries;
        std::string origin;
        std::string error;
    };
    std::string Sha256(const std::string& data);
    bool Parse(const std::string& text, Catalog& out, std::string& error);
    Catalog Load(const std::string& cachePath, const std::string& bundledPath);
    bool Store(const std::string& cachePath, const std::string& text, std::string& error);
    bool Refresh(const std::string& cachePath, Catalog& current, std::string& error);
}
