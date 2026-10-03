#include "util/catalog.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char** argv) {
    assert(argc == 3 || argc == 4);
    const std::size_t expected = argc == 4 ? std::stoul(argv[3]) : 2;
    const std::string bundled = argv[1], cache = std::string(argv[2]) + "/catalog.json";
    std::ifstream file(bundled);
    const std::string text((std::istreambuf_iterator<char>(file)), {});
    inst::catalog::Catalog catalog;
    std::string error;
    assert(inst::catalog::Parse(text, catalog, error) && catalog.entries.size() == expected);
    assert(inst::catalog::Store(cache, text, error));
    auto loaded = inst::catalog::Load(cache, bundled);
    assert(loaded.entries.size() == expected && loaded.origin == "Cached catalog");
    assert(!inst::catalog::Store(cache, "corrupt", error));
    assert(inst::catalog::Load(cache, bundled).entries.size() == expected);
    auto altered = nlohmann::json::parse(text);
    altered["schema_version"] = 2;
    assert(!inst::catalog::Parse(altered.dump(), catalog, error));
    altered = nlohmann::json::parse(text);
    altered["entries"][0]["host"] = "untrusted.example";
    assert(!inst::catalog::Parse(altered.dump(), catalog, error));
    altered["sha256"] = inst::catalog::Sha256(altered["entries"].dump());
    altered["entries"][0]["password"] = "fixture";
    altered["sha256"] = inst::catalog::Sha256(altered["entries"].dump());
    assert(!inst::catalog::Parse(altered.dump(), catalog, error));
    assert(!inst::catalog::Parse(std::string(inst::catalog::kMaxBytes + 1, 'x'), catalog, error));
    assert(inst::catalog::Store(cache, text, error));
    std::ofstream(cache) << "corrupt";
    assert(inst::catalog::Load(cache, bundled).entries.size() == expected); // Backup recovery.
    std::cout << "Catalog schema, checksum, size, credential rejection and cache retention tests passed\n";
}
