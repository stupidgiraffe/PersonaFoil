#include "util/catalog.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <set>
#ifdef __SWITCH__
#include <switch.h>
#else
#include <openssl/sha.h>
#endif

namespace inst::catalog {
    namespace {
        bool Read(const std::string& path, std::string& text) {
            std::error_code ec;
            const auto size = std::filesystem::file_size(path, ec);
            if (ec || size > kMaxBytes) return false;
            std::ifstream file(path, std::ios::binary);
            text.assign(std::istreambuf_iterator<char>(file), {});
            return file.good() || file.eof();
        }
        bool Text(const nlohmann::json& node, const char* key, std::string& value, std::size_t max = 256) {
            if (!node.contains(key) || !node[key].is_string()) return false;
            value = node[key].get<std::string>();
            return value.size() <= max && std::none_of(value.begin(), value.end(), [](unsigned char c) { return c < 32 || c == 127; });
        }
    }
    std::string Entry::url() const {
        return protocol + "://" + host + ":" + std::to_string(port) + (path.empty() ? "/" : path);
    }
    std::string Sha256(const std::string& data) {
        std::array<unsigned char, 32> hash{};
#ifdef __SWITCH__
        sha256CalculateHash(hash.data(), data.data(), data.size());
#else
        SHA256(reinterpret_cast<const unsigned char*>(data.data()), data.size(), hash.data());
#endif
        static constexpr char hex[] = "0123456789abcdef";
        std::string out;
        for (auto b : hash) { out += hex[b >> 4]; out += hex[b & 15]; }
        return out;
    }
    bool Parse(const std::string& text, Catalog& out, std::string& error) {
        auto fail = [&](const std::string& reason) { error = reason; return false; };
        if (text.size() > kMaxBytes) return fail("Catalog exceeds the size limit.");
        try {
            const auto root = inst::remote::ParseDocument(text);
            if (!root.is_object() || root.value("schema_version", 0) != 1) return fail("Unsupported catalog version.");
            const std::set<std::string> rootKeys = {"schema_version", "generated_at", "entries", "sha256"};
            for (auto it = root.begin(); it != root.end(); ++it) if (!rootKeys.count(it.key())) return fail("Unknown catalog field.");
            if (!root.contains("entries") || !root["entries"].is_array() || root["entries"].empty() || root["entries"].size() > 256) return fail("Invalid catalog entries.");
            std::string checksum;
            if (!Text(root, "sha256", checksum, 64) || checksum != Sha256(root["entries"].dump())) return fail("Catalog checksum does not match.");
            Catalog parsed;
            if (!Text(root, "generated_at", parsed.generatedAt, 40)) return fail("Catalog generation date is missing.");
            std::set<std::string> ids, endpoints;
            for (const auto& node : root["entries"]) {
                Entry entry;
                const std::set<std::string> entryKeys = {"id", "title", "protocol", "host", "port", "path", "compatibility", "authentication", "source_url", "provenance", "discovered_at", "last_checked_at", "health", "http_result", "redirect_target", "tags"};
                if (node.is_object()) for (auto it = node.begin(); it != node.end(); ++it) if (!entryKeys.count(it.key())) return fail("Unknown or sensitive catalog field.");
                if (!node.is_object() || !Text(node, "id", entry.id, 64) || entry.id.empty() ||
                    !Text(node, "title", entry.title, 80) || entry.title.empty() ||
                    !Text(node, "protocol", entry.protocol, 5) || (entry.protocol != "https" && entry.protocol != "http") ||
                    !Text(node, "host", entry.host) || entry.host.empty() || entry.host.find_first_of("/@?# ") != std::string::npos ||
                    !Text(node, "path", entry.path, 1024) || entry.path.empty() || entry.path.front() != '/' || entry.path.find_first_of("?#") != std::string::npos ||
                    !node.contains("port") || !node["port"].is_number_integer()) return fail("Catalog endpoint is invalid.");
                entry.port = node["port"].get<int>();
                if (entry.port < 1 || entry.port > 65535) return fail("Catalog port is invalid.");
                if (node.contains("username") || node.contains("password") || node.contains("headers") || node.contains("token")) return fail("Catalog must not contain credentials.");
                std::string compatibility;
                if (!Text(node, "compatibility", compatibility, 16) || (compatibility != "auto" && compatibility != "modern" && compatibility != "tinfoil")) return fail("Catalog compatibility is invalid.");
                entry.compatibility = inst::remote::ReadCompatibility(node);
                if (!Text(node, "authentication", entry.authentication, 32) || !Text(node, "source_url", entry.sourceUrl, 2048) ||
                    !Text(node, "provenance", entry.provenance, 80) || !Text(node, "discovered_at", entry.discoveredAt, 40) ||
                    !Text(node, "last_checked_at", entry.checkedAt, 40) || !Text(node, "health", entry.health, 32)) return fail("Catalog provenance is incomplete.");
                const std::set<std::string> health = {"online", "authentication_required", "degraded", "offline", "invalid_response", "unknown"};
                if (!health.count(entry.health)) return fail("Catalog health state is invalid.");
                if (entry.authentication != "none" && entry.authentication != "required" && entry.authentication != "unknown") return fail("Catalog authentication state is invalid.");
                entry.httpStatus = node.value("http_result", 0);
                entry.redirect = node.value("redirect_target", "");
                if (!entry.redirect.empty() && (inst::http::CanonicalUrl(entry.redirect).empty() || entry.redirect.find_first_of("?#") != std::string::npos)) return fail("Catalog redirect is invalid.");
                if (inst::http::CanonicalUrl(entry.sourceUrl).empty() || entry.sourceUrl.find_first_of("?#") != std::string::npos) return fail("Catalog source URL is invalid.");
                if (node.contains("tags")) {
                    if (!node["tags"].is_array() || node["tags"].size() > 8) return fail("Catalog tags are invalid.");
                    for (const auto& tag : node["tags"]) {
                        if (!tag.is_string() || tag.get<std::string>().size() > 32) return fail("Catalog tag is invalid.");
                        entry.tags.push_back(tag.get<std::string>());
                    }
                }
                const auto canonical = inst::http::CanonicalUrl(entry.url());
                if (canonical.empty() || !ids.insert(entry.id).second || !endpoints.insert(canonical).second) return fail("Catalog has duplicate or invalid endpoints.");
                parsed.entries.push_back(std::move(entry));
            }
            error.clear(); out = std::move(parsed); return true;
        } catch (...) { return fail("Invalid catalog JSON or field types."); }
    }
    Catalog Load(const std::string& cachePath, const std::string& bundledPath) {
        Catalog catalog;
        std::string text, error;
        for (const auto& path : {cachePath, cachePath + ".bak", bundledPath}) {
            if (Read(path, text) && Parse(text, catalog, error)) {
                catalog.origin = path == bundledPath ? "Bundled catalog" : "Cached catalog";
                return catalog;
            }
        }
        catalog.error = "No valid cached or bundled catalog.";
        return catalog;
    }
    bool Store(const std::string& path, const std::string& text, std::string& error) {
        Catalog parsed;
        if (!Parse(text, parsed, error)) return false;
        const std::string staged = path + ".new", backup = path + ".bak";
        std::ofstream file(staged, std::ios::binary | std::ios::trunc);
        if (!file || !file.write(text.data(), text.size()) || !file.flush()) { error = "Could not stage the catalog."; return false; }
        file.close();
        if (file.fail()) { error = "Could not close the staged catalog."; return false; }
        std::string checked;
        if (!Read(staged, checked) || checked != text) { error = "Catalog write verification failed."; return false; }
        std::error_code ec;
        const bool exists = std::filesystem::exists(path, ec);
        std::string preserved;
        if (exists) {
            std::string previousText, previousError;
            Catalog previousCatalog;
            const bool validPrevious = Read(path, previousText) && Parse(previousText, previousCatalog, previousError);
            preserved = validPrevious ? backup : path + ".invalid";
            std::filesystem::remove(preserved, ec); ec.clear();
            std::filesystem::rename(path, preserved, ec);
            if (ec) { error = "Could not preserve the previous catalog."; return false; }
        }
        std::filesystem::rename(staged, path, ec);
        if (ec) {
            std::error_code rollback;
            if (exists) std::filesystem::rename(preserved, path, rollback);
            error = rollback ? "Catalog replacement and rollback failed; backup retained." : "Catalog replacement failed; previous copy retained.";
            return false;
        }
        return true;
    }
    bool Refresh(const std::string& path, Catalog& current, std::string& error) {
        inst::http::Request request; request.maxBytes = kMaxBytes;
        const auto response = inst::http::Get(kEndpoint, request);
        if (!response.ok()) { error = inst::http::Describe(response); return false; }
        Catalog parsed;
        if (!Parse(response.body, parsed, error) || !Store(path, response.body, error)) return false;
        parsed.origin = "Refreshed catalog"; current = std::move(parsed); return true;
    }
}
