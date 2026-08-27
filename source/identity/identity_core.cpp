#include "identity/identity_core.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <set>
#include <sstream>

#ifdef __SWITCH__
#include <switch.h>
#else
#include <openssl/sha.h>
#endif

#include "util/json.hpp"

namespace inst::identity {
    namespace {
        std::string Trim(const std::string& value)
        {
            const std::size_t first = value.find_first_not_of(" \t\r\n");
            if (first == std::string::npos)
                return "";
            const std::size_t last = value.find_last_not_of(" \t\r\n");
            return value.substr(first, (last - first) + 1);
        }

        std::string Uppercase(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
                return static_cast<char>(std::toupper(c));
            });
            return value;
        }

        bool IsHexIdentifier(const std::string& value)
        {
            if (value.size() != 32)
                return false;
            return std::all_of(value.begin(), value.end(), [](unsigned char c) {
                return std::isxdigit(c) != 0;
            });
        }

        bool Fail(std::string* error, const std::string& message)
        {
            if (error != nullptr)
                *error = message;
            return false;
        }
    }

    std::string ComputeUidFromIdentityBytes(const PersonaSeed& bytes)
    {
        std::array<unsigned char, 32> hash{};
#ifdef __SWITCH__
        sha256CalculateHash(hash.data(), bytes.data(), bytes.size());
#else
        SHA256(bytes.data(), bytes.size(), hash.data());
#endif

        static constexpr char kHex[] = "0123456789ABCDEF";
        std::string uid;
        uid.reserve(hash.size() * 2);
        for (const unsigned char byte : hash) {
            uid.push_back(kHex[(byte >> 4) & 0xF]);
            uid.push_back(kHex[byte & 0xF]);
        }
        std::fill(hash.begin(), hash.end(), 0);
        return uid;
    }

    std::string FormatUidFingerprint(const std::string& uid)
    {
        if (uid.size() <= 12)
            return uid;
        return uid.substr(0, 6) + "\xE2\x80\xA6" + uid.substr(uid.size() - 6);
    }

    std::string EncodeHexUpper(const PersonaSeed& bytes)
    {
        static constexpr char kHex[] = "0123456789ABCDEF";
        std::string out;
        out.reserve(bytes.size() * 2);
        for (const std::uint8_t byte : bytes) {
            out.push_back(kHex[(byte >> 4) & 0xF]);
            out.push_back(kHex[byte & 0xF]);
        }
        return out;
    }

    bool DecodeSeedHex(const std::string& hex, PersonaSeed& out)
    {
        if (hex.size() != out.size() * 2)
            return false;

        auto nibble = [](char c) -> int {
            if (c >= '0' && c <= '9')
                return c - '0';
            if (c >= 'A' && c <= 'F')
                return 10 + (c - 'A');
            if (c >= 'a' && c <= 'f')
                return 10 + (c - 'a');
            return -1;
        };

        PersonaSeed decoded{};
        for (std::size_t i = 0; i < decoded.size(); i++) {
            const int high = nibble(hex[i * 2]);
            const int low = nibble(hex[(i * 2) + 1]);
            if (high < 0 || low < 0)
                return false;
            decoded[i] = static_cast<std::uint8_t>((high << 4) | low);
        }
        out = decoded;
        return true;
    }

    std::string SerializeIdentityState(const IdentityState& state)
    {
        nlohmann::json personas = nlohmann::json::array();
        for (const auto& persona : state.personas) {
            personas.push_back({
                {"schemaVersion", kIdentitySchemaVersion},
                {"id", Uppercase(persona.id)},
                {"name", persona.name},
                {"seed", EncodeHexUpper(persona.seed)},
                {"createdAt", persona.createdAt}
            });
        }

        nlohmann::json root = {
            {"schemaVersion", kIdentitySchemaVersion},
            {"activePersona", state.activePersonaId == kNativeIdentityId ? kNativeIdentityId : Uppercase(state.activePersonaId)},
            {"personas", personas}
        };

        return root.dump(4) + "\n";
    }

    ParseResult ParseIdentityState(const std::string& serialized)
    {
        ParseResult result;
        result.state = IdentityState{};

        try {
            const nlohmann::json root = nlohmann::json::parse(serialized);
            if (!root.is_object()) {
                result.error = "Identity configuration root must be an object.";
                return result;
            }
            if (!root.contains("schemaVersion") || !root["schemaVersion"].is_number_integer()) {
                result.error = "Identity configuration is missing an integer schemaVersion.";
                return result;
            }

            const int schemaVersion = root["schemaVersion"].get<int>();
            if (schemaVersion != kIdentitySchemaVersion) {
                result.error = "Unsupported identity schema version " + std::to_string(schemaVersion) + ".";
                return result;
            }
            if (!root.contains("personas") || !root["personas"].is_array()) {
                result.error = "Identity configuration personas must be an array.";
                return result;
            }

            IdentityState parsed;
            std::set<std::string> seenIds;
            for (const auto& entry : root["personas"]) {
                if (!entry.is_object() ||
                    !entry.contains("id") || !entry["id"].is_string() ||
                    !entry.contains("name") || !entry["name"].is_string() ||
                    !entry.contains("seed") || !entry["seed"].is_string()) {
                    result.error = "A persona entry is missing a required string field.";
                    return result;
                }

                Persona persona;
                persona.id = Uppercase(Trim(entry["id"].get<std::string>()));
                persona.name = Trim(entry["name"].get<std::string>());
                if (!IsHexIdentifier(persona.id) || persona.id == Uppercase(kNativeIdentityId)) {
                    result.error = "A persona has an invalid identifier.";
                    return result;
                }
                if (persona.name.empty() || persona.name.size() > 64) {
                    result.error = "A persona has an invalid name.";
                    return result;
                }
                if (!DecodeSeedHex(entry["seed"].get<std::string>(), persona.seed)) {
                    result.error = "A persona has an invalid 16-byte seed.";
                    return result;
                }
                if (entry.contains("createdAt")) {
                    if (!entry["createdAt"].is_number_integer()) {
                        result.error = "A persona has an invalid creation timestamp.";
                        return result;
                    }
                    persona.createdAt = entry["createdAt"].get<std::int64_t>();
                }
                if (!seenIds.insert(persona.id).second) {
                    result.error = "Identity configuration contains a duplicate persona identifier.";
                    return result;
                }
                parsed.personas.push_back(std::move(persona));
            }

            parsed.activePersonaId = kNativeIdentityId;
            if (root.contains("activePersona")) {
                if (!root["activePersona"].is_string()) {
                    result.error = "activePersona must be a string.";
                    return result;
                }
                const std::string rawActive = Trim(root["activePersona"].get<std::string>());
                parsed.activePersonaId = Uppercase(rawActive) == Uppercase(kNativeIdentityId)
                    ? kNativeIdentityId
                    : Uppercase(rawActive);
            }

            if (parsed.activePersonaId != kNativeIdentityId && FindPersona(parsed, parsed.activePersonaId) == nullptr) {
                parsed.activePersonaId = kNativeIdentityId;
                result.usedNativeFallback = true;
                result.error = "The active persona was missing; Native Switch is active in memory.";
            }

            result.state = std::move(parsed);
            result.success = true;
            return result;
        }
        catch (const std::exception& e) {
            result.error = std::string("Could not parse identity configuration: ") + e.what();
            return result;
        }
    }

    const Persona* FindPersona(const IdentityState& state, const std::string& id)
    {
        const std::string normalized = Uppercase(Trim(id));
        const auto it = std::find_if(state.personas.begin(), state.personas.end(), [&](const Persona& persona) {
            return persona.id == normalized;
        });
        return it == state.personas.end() ? nullptr : &(*it);
    }

    bool ActivateIdentity(IdentityState& state, const std::string& id, std::string* error)
    {
        const std::string normalized = Uppercase(Trim(id));
        if (normalized == Uppercase(kNativeIdentityId)) {
            state.activePersonaId = kNativeIdentityId;
            return true;
        }
        if (FindPersona(state, normalized) == nullptr)
            return Fail(error, "Persona not found.");
        state.activePersonaId = normalized;
        return true;
    }

    bool AddPersona(IdentityState& state, const Persona& persona, bool activate, std::string* error)
    {
        Persona normalized = persona;
        normalized.id = Uppercase(Trim(normalized.id));
        normalized.name = Trim(normalized.name);
        if (!IsHexIdentifier(normalized.id) || normalized.id == Uppercase(kNativeIdentityId))
            return Fail(error, "Persona identifier must be 32 hexadecimal characters.");
        if (normalized.name.empty() || normalized.name.size() > 64)
            return Fail(error, "Persona name must contain 1 to 64 characters.");
        if (FindPersona(state, normalized.id) != nullptr)
            return Fail(error, "Persona identifier already exists.");
        state.personas.push_back(std::move(normalized));
        if (activate)
            state.activePersonaId = state.personas.back().id;
        return true;
    }

    bool RenamePersona(IdentityState& state, const std::string& id, const std::string& name, std::string* error)
    {
        const std::string normalizedId = Uppercase(Trim(id));
        const std::string normalizedName = Trim(name);
        if (normalizedName.empty() || normalizedName.size() > 64)
            return Fail(error, "Persona name must contain 1 to 64 characters.");
        auto it = std::find_if(state.personas.begin(), state.personas.end(), [&](const Persona& persona) {
            return persona.id == normalizedId;
        });
        if (it == state.personas.end())
            return Fail(error, "Persona not found.");
        it->name = normalizedName;
        return true;
    }

    bool DeletePersona(IdentityState& state, const std::string& id, std::string* error)
    {
        const std::string normalized = Uppercase(Trim(id));
        auto it = std::find_if(state.personas.begin(), state.personas.end(), [&](const Persona& persona) {
            return persona.id == normalized;
        });
        if (it == state.personas.end())
            return Fail(error, "Persona not found.");
        if (state.activePersonaId == normalized)
            state.activePersonaId = kNativeIdentityId;
        state.personas.erase(it);
        return true;
    }
}
