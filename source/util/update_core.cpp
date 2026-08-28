#include "util/update_core.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <sstream>

namespace inst::update {
    namespace {
        bool Fail(std::string* error, const std::string& message)
        {
            if (error != nullptr) *error = message;
            return false;
        }

        std::string Trim(const std::string& value)
        {
            const std::size_t first = value.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) return "";
            const std::size_t last = value.find_last_not_of(" \t\r\n");
            return value.substr(first, (last - first) + 1);
        }

        bool ParseComponent(const std::string& value, std::uint64_t& out)
        {
            if (value.empty()) return false;
            if (value.size() > 1 && value.front() == '0') return false;
            std::uint64_t result = 0;
            for (unsigned char c : value) {
                if (!std::isdigit(c)) return false;
                const std::uint64_t digit = static_cast<std::uint64_t>(c - '0');
                if (result > (std::numeric_limits<std::uint64_t>::max() - digit) / 10) return false;
                result = (result * 10) + digit;
            }
            out = result;
            return true;
        }

        bool IsSha256(const std::string& value)
        {
            return value.size() == 64 && std::all_of(value.begin(), value.end(), [](unsigned char c) { return std::isxdigit(c) != 0; });
        }

        std::string Lower(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return value;
        }
    }

    bool ParseStableSemver(const std::string& value, SemanticVersion& out, std::string* error)
    {
        std::string normalized = Trim(value);
        if (!normalized.empty() && (normalized.front() == 'v' || normalized.front() == 'V')) normalized.erase(normalized.begin());
        const std::size_t firstDot = normalized.find('.');
        const std::size_t secondDot = firstDot == std::string::npos ? std::string::npos : normalized.find('.', firstDot + 1);
        if (firstDot == std::string::npos || secondDot == std::string::npos || normalized.find('.', secondDot + 1) != std::string::npos)
            return Fail(error, "Version must be stable major.minor.patch semantic versioning.");

        SemanticVersion parsed;
        if (!ParseComponent(normalized.substr(0, firstDot), parsed.major) ||
            !ParseComponent(normalized.substr(firstDot + 1, secondDot - firstDot - 1), parsed.minor) ||
            !ParseComponent(normalized.substr(secondDot + 1), parsed.patch))
            return Fail(error, "Version contains an invalid semantic-version component.");
        out = parsed;
        return true;
    }

    int CompareSemanticVersions(const SemanticVersion& lhs, const SemanticVersion& rhs)
    {
        if (lhs.major != rhs.major) return lhs.major < rhs.major ? -1 : 1;
        if (lhs.minor != rhs.minor) return lhs.minor < rhs.minor ? -1 : 1;
        if (lhs.patch != rhs.patch) return lhs.patch < rhs.patch ? -1 : 1;
        return 0;
    }

    bool IsOfficialReleaseAssetUrl(const std::string& url)
    {
        static constexpr const char* kPrefix = "https://github.com/stupidgiraffe/PersonaFoil/releases/download/";
        return url.rfind(kPrefix, 0) == 0 && url.find("..") == std::string::npos;
    }

    bool SelectRequiredReleaseAssets(const std::vector<ReleaseAsset>& assets, std::string& nroUrl, std::string& checksumsUrl, std::string* error)
    {
        nroUrl.clear();
        checksumsUrl.clear();
        for (const auto& asset : assets) {
            if (asset.name != "personafoil.nro" && asset.name != "SHA256SUMS.txt") continue;
            if (!IsOfficialReleaseAssetUrl(asset.url)) return Fail(error, "Release contains a required asset with an untrusted download URL.");
            std::string* destination = asset.name == "personafoil.nro" ? &nroUrl : &checksumsUrl;
            if (!destination->empty()) return Fail(error, "Release contains a duplicate required asset.");
            *destination = asset.url;
        }
        if (nroUrl.empty()) return Fail(error, "Release is missing personafoil.nro.");
        if (checksumsUrl.empty()) return Fail(error, "Release is missing SHA256SUMS.txt.");
        return true;
    }

    bool ParseSha256Sums(const std::string& text, const std::string& targetFilename, std::string& expectedHash, std::string* error)
    {
        expectedHash.clear();
        std::istringstream stream(text);
        std::string line;
        while (std::getline(stream, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (Trim(line).empty()) continue;
            if (line.size() < 67 || !IsSha256(line.substr(0, 64)) || !std::isspace(static_cast<unsigned char>(line[64])))
                return Fail(error, "SHA256SUMS.txt contains a malformed checksum line.");
            std::size_t filenameStart = line.find_first_not_of(" \t", 64);
            if (filenameStart == std::string::npos) return Fail(error, "SHA256SUMS.txt contains a checksum without a filename.");
            if (line[filenameStart] == '*') filenameStart++;
            const std::string filename = Trim(line.substr(filenameStart));
            if (filename.empty()) return Fail(error, "SHA256SUMS.txt contains a checksum without a filename.");
            if (filename != targetFilename) continue;
            if (!expectedHash.empty()) return Fail(error, "SHA256SUMS.txt contains duplicate entries for personafoil.nro.");
            expectedHash = Lower(line.substr(0, 64));
        }
        if (expectedHash.empty()) return Fail(error, "SHA256SUMS.txt does not contain personafoil.nro.");
        return true;
    }

    bool Sha256HexEqual(const std::string& lhs, const std::string& rhs)
    {
        return IsSha256(lhs) && IsSha256(rhs) && Lower(lhs) == Lower(rhs);
    }
}
