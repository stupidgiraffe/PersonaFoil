#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <string>

#include "identity/identity_core.hpp"

namespace {
    int failures = 0;

    void Expect(bool condition, const std::string& message)
    {
        if (condition)
            return;
        std::cerr << "FAIL: " << message << '\n';
        failures++;
    }

    inst::identity::Persona MakePersona(const std::string& id, const std::string& name, std::uint8_t firstByte)
    {
        inst::identity::Persona persona;
        persona.id = id;
        persona.name = name;
        persona.seed.fill(0);
        persona.seed[0] = firstByte;
        persona.createdAt = 123456789;
        return persona;
    }
}

int main()
{
    using namespace inst::identity;

    PersonaSeed nativeVector{};
    for (std::size_t i = 0; i < nativeVector.size(); i++)
        nativeVector[i] = static_cast<std::uint8_t>(i);
    const std::string nativeUid = ComputeUidFromIdentityBytes(nativeVector);
    Expect(nativeUid == "BE45CB2605BF36BEBDE684841A28F0FD43C69850A3DCE5FEDBA69928EE3A8991",
        "known 16-byte native derivation vector remains SHA-256 compatible");
    Expect(nativeUid.size() == 64, "UID contains 64 characters");
    Expect(std::all_of(nativeUid.begin(), nativeUid.end(), [](unsigned char c) {
        return std::isdigit(c) || (c >= 'A' && c <= 'F');
    }), "UID uses uppercase hexadecimal");

    PersonaSeed secondSeed{};
    secondSeed.fill(0x5A);
    const std::string firstPersonaUid = ComputeUidFromIdentityBytes(nativeVector);
    const std::string repeatedPersonaUid = ComputeUidFromIdentityBytes(nativeVector);
    const std::string secondPersonaUid = ComputeUidFromIdentityBytes(secondSeed);
    Expect(firstPersonaUid == repeatedPersonaUid, "the same persona seed is deterministic");
    Expect(firstPersonaUid != secondPersonaUid, "different persona seeds produce different UIDs");
    Expect(FormatUidFingerprint(nativeUid) == "BE45CB\xE2\x80\xA6" "3A8991", "UID fingerprint is shortened safely");

    IdentityState state;
    const Persona alpha = MakePersona("00112233445566778899AABBCCDDEEFF", "Persona A", 0x11);
    const Persona beta = MakePersona("FFEEDDCCBBAA99887766554433221100", "Persona B", 0x22);
    std::string error;
    Expect(AddPersona(state, alpha, true, &error), "Persona A can be added and activated");
    Expect(AddPersona(state, beta, false, &error), "Persona B can be added");

    const std::string serialized = SerializeIdentityState(state);
    const ParseResult parsed = ParseIdentityState(serialized);
    Expect(parsed.success, "identity configuration round-trips");
    Expect(parsed.state.personas.size() == 2, "round-trip preserves persona count");
    Expect(parsed.state.activePersonaId == alpha.id, "round-trip preserves active persona");
    Expect(ComputeUidFromIdentityBytes(parsed.state.personas[0].seed) == ComputeUidFromIdentityBytes(alpha.seed),
        "round-trip preserves persona seed and UID");

    const ParseResult malformed = ParseIdentityState("{ not-json");
    Expect(!malformed.success, "malformed configuration is rejected without a fake state");

    const std::string missingActive =
        "{\"schemaVersion\":1,\"activePersona\":\"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\",\"personas\":[]}";
    const ParseResult fallback = ParseIdentityState(missingActive);
    Expect(fallback.success, "missing active persona is handled gracefully");
    Expect(fallback.usedNativeFallback, "missing active persona records fallback");
    Expect(fallback.state.activePersonaId == kNativeIdentityId, "missing active persona falls back to Native Switch");

    IdentityState deletionState = state;
    Expect(DeletePersona(deletionState, alpha.id, &error), "active persona can be deleted");
    Expect(deletionState.activePersonaId == kNativeIdentityId, "deleting active persona returns to Native Switch");
    Expect(deletionState.personas.size() == 1, "deletion removes exactly one persona");

    const std::string duplicateIds =
        "{\"schemaVersion\":1,\"activePersona\":\"native\",\"personas\":["
        "{\"id\":\"00112233445566778899AABBCCDDEEFF\",\"name\":\"A\",\"seed\":\"00000000000000000000000000000000\"},"
        "{\"id\":\"00112233445566778899AABBCCDDEEFF\",\"name\":\"B\",\"seed\":\"11111111111111111111111111111111\"}]}";
    Expect(!ParseIdentityState(duplicateIds).success, "duplicate persona identifiers are rejected");

    if (failures != 0) {
        std::cerr << failures << " identity test(s) failed\n";
        return EXIT_FAILURE;
    }
    std::cout << "All identity tests passed\n";
    return EXIT_SUCCESS;
}
