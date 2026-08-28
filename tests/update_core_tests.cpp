#include "util/update_core.hpp"

#include <cassert>
#include <iostream>
#include <string>

using inst::update::CompareSemanticVersions;
using inst::update::ParseSha256Sums;
using inst::update::ParseStableSemver;
using inst::update::SemanticVersion;
using inst::update::SelectRequiredReleaseAssets;
using inst::update::Sha256HexEqual;

static SemanticVersion Version(const std::string& text)
{
    SemanticVersion out;
    std::string error;
    assert(ParseStableSemver(text, out, &error));
    return out;
}

int main()
{
    assert(CompareSemanticVersions(Version("0.1.1"), Version("0.1.0")) > 0);
    assert(CompareSemanticVersions(Version("0.1.10"), Version("0.1.9")) > 0);
    assert(CompareSemanticVersions(Version("v0.2.0"), Version("0.2.0")) == 0);
    assert(CompareSemanticVersions(Version("1.0.0"), Version("0.99.99")) > 0);
    assert(CompareSemanticVersions(Version("0.1.0"), Version("0.1.0")) == 0);

    SemanticVersion malformed;
    assert(!ParseStableSemver("0.1", malformed));
    assert(!ParseStableSemver("0.1.0-beta", malformed));
    assert(!ParseStableSemver("0.1.0+build", malformed));
    assert(!ParseStableSemver("01.1.0", malformed));

    const std::string base = "https://github.com/stupidgiraffe/PersonaFoil/releases/download/v0.1.1/";
    std::string nro;
    std::string sums;
    std::string error;
    assert(SelectRequiredReleaseAssets({
        {"notes.txt", base + "notes.txt"},
        {"SHA256SUMS.txt", base + "SHA256SUMS.txt"},
        {"personafoil.nro", base + "personafoil.nro"}
    }, nro, sums, &error));
    assert(nro == base + "personafoil.nro");
    assert(sums == base + "SHA256SUMS.txt");
    assert(!SelectRequiredReleaseAssets({{"personafoil.nro", base + "personafoil.nro"}}, nro, sums, &error));
    assert(!SelectRequiredReleaseAssets({
        {"personafoil.nro", base + "personafoil.nro"},
        {"personafoil.nro", base + "personafoil.nro"},
        {"SHA256SUMS.txt", base + "SHA256SUMS.txt"}
    }, nro, sums, &error));
    assert(!SelectRequiredReleaseAssets({
        {"personafoil.nro", "https://example.com/personafoil.nro"},
        {"SHA256SUMS.txt", base + "SHA256SUMS.txt"}
    }, nro, sums, &error));

    const std::string hash = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    std::string parsedHash;
    assert(ParseSha256Sums(hash + "  personafoil.nro\n" + hash + "  personafoil.zip\n", "personafoil.nro", parsedHash, &error));
    assert(parsedHash == hash);
    assert(Sha256HexEqual(parsedHash, hash));
    assert(Sha256HexEqual(parsedHash, "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF"));
    assert(!ParseSha256Sums(hash + "  personafoil.zip\n", "personafoil.nro", parsedHash, &error));
    assert(!ParseSha256Sums("not-a-hash  personafoil.nro\n", "personafoil.nro", parsedHash, &error));
    assert(!ParseSha256Sums(hash + "  personafoil.nro\n" + hash + "  personafoil.nro\n", "personafoil.nro", parsedHash, &error));

    std::cout << "update_core_tests: PASS\n";
    return 0;
}
