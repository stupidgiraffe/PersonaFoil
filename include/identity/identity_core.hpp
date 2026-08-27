#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace inst::identity {
    inline constexpr int kIdentitySchemaVersion = 1;
    inline constexpr const char* kNativeIdentityId = "native";

    using PersonaSeed = std::array<std::uint8_t, 16>;

    struct Persona {
        std::string id;
        std::string name;
        PersonaSeed seed{};
        std::int64_t createdAt = 0;
    };

    struct IdentityState {
        int schemaVersion = kIdentitySchemaVersion;
        std::string activePersonaId = kNativeIdentityId;
        std::vector<Persona> personas;
    };

    struct ParseResult {
        bool success = false;
        bool usedNativeFallback = false;
        IdentityState state;
        std::string error;
    };

    std::string ComputeUidFromIdentityBytes(const PersonaSeed& bytes);
    std::string FormatUidFingerprint(const std::string& uid);
    std::string EncodeHexUpper(const PersonaSeed& bytes);
    bool DecodeSeedHex(const std::string& hex, PersonaSeed& out);

    std::string SerializeIdentityState(const IdentityState& state);
    ParseResult ParseIdentityState(const std::string& serialized);

    const Persona* FindPersona(const IdentityState& state, const std::string& id);
    bool ActivateIdentity(IdentityState& state, const std::string& id, std::string* error = nullptr);
    bool AddPersona(IdentityState& state, const Persona& persona, bool activate, std::string* error = nullptr);
    bool RenamePersona(IdentityState& state, const std::string& id, const std::string& name, std::string* error = nullptr);
    bool DeletePersona(IdentityState& state, const std::string& id, std::string* error = nullptr);
}
