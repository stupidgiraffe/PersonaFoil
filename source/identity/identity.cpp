#include "identity/identity.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <system_error>

#include <switch.h>

#include "util/config.hpp"
#include "util/uid.hpp"

namespace inst::identity {
    namespace {
        std::mutex gMutex;
        IdentityState gState;
        bool gInitialized = false;
        bool gConfigurationLoaded = true;
        std::string gConfigurationStatus = "Not initialized.";

        bool Fail(std::string* error, const std::string& message)
        {
            if (error != nullptr)
                *error = message;
            return false;
        }

        void SecureWipe(void* ptr, std::size_t size)
        {
            volatile std::uint8_t* bytes = static_cast<volatile std::uint8_t*>(ptr);
            for (std::size_t i = 0; i < size; i++)
                bytes[i] = 0;
        }

        bool ReadTextFile(const std::filesystem::path& path, std::string& out)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file)
                return false;
            std::ostringstream stream;
            stream << file.rdbuf();
            if (!file.good() && !file.eof())
                return false;
            out = stream.str();
            return true;
        }

        bool WriteStateLocked(const IdentityState& state, std::string* error)
        {
            const std::filesystem::path target(inst::config::identityConfigPath);
            const std::filesystem::path temporary = target.string() + ".tmp";
            const std::filesystem::path backup = target.string() + ".bak";
            std::error_code ec;
            std::filesystem::create_directories(target.parent_path(), ec);
            if (ec)
                return Fail(error, "Could not create the PersonaFoil configuration directory.");

            std::filesystem::remove(temporary, ec);
            ec.clear();
            {
                std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
                if (!file)
                    return Fail(error, "Could not open the temporary identity configuration.");
                file << SerializeIdentityState(state);
                file.flush();
                if (!file.good()) {
                    file.close();
                    std::filesystem::remove(temporary, ec);
                    return Fail(error, "Could not write the temporary identity configuration.");
                }
            }

            const bool hadTarget = std::filesystem::exists(target, ec);
            ec.clear();
            if (hadTarget) {
                std::filesystem::remove(backup, ec);
                ec.clear();
                std::filesystem::rename(target, backup, ec);
                if (ec) {
                    std::filesystem::remove(temporary, ec);
                    return Fail(error, "Could not preserve the previous identity configuration.");
                }
            }

            std::filesystem::rename(temporary, target, ec);
            if (ec) {
                std::error_code restoreEc;
                if (hadTarget)
                    std::filesystem::rename(backup, target, restoreEc);
                std::filesystem::remove(temporary, restoreEc);
                return Fail(error, "Could not replace the identity configuration safely.");
            }
            return true;
        }

        bool SaveCandidateLocked(const IdentityState& candidate, std::string* error)
        {
            if (!gConfigurationLoaded)
                return Fail(error, "Identity configuration is malformed or unsupported. The existing file was preserved; repair or move it before changing personas.");
            if (!WriteStateLocked(candidate, error))
                return false;
            gState = candidate;
            gConfigurationStatus = "Loaded successfully.";
            return true;
        }

        std::string GenerateIdentifier(const std::array<std::uint8_t, 16>& bytes)
        {
            PersonaSeed value{};
            std::copy(bytes.begin(), bytes.end(), value.begin());
            return EncodeHexUpper(value);
        }
    }

    bool Initialize(std::string* error)
    {
        std::lock_guard<std::mutex> lock(gMutex);
        gState = IdentityState{};
        gInitialized = true;
        gConfigurationLoaded = true;
        gConfigurationStatus = "No identity configuration found; using Native Switch.";

        const std::filesystem::path path(inst::config::identityConfigPath);
        std::error_code ec;
        if (!std::filesystem::exists(path, ec)) {
            if (ec) {
                gConfigurationLoaded = false;
                gConfigurationStatus = "Could not inspect the identity configuration path.";
                return Fail(error, gConfigurationStatus);
            }
            return true;
        }

        std::string serialized;
        if (!ReadTextFile(path, serialized)) {
            gConfigurationLoaded = false;
            gConfigurationStatus = "Could not read identity.json; using Native Switch without modifying the file.";
            return Fail(error, gConfigurationStatus);
        }

        ParseResult parsed = ParseIdentityState(serialized);
        if (!parsed.success) {
            gConfigurationLoaded = false;
            gConfigurationStatus = parsed.error + " Existing identity.json was preserved; using Native Switch in memory.";
            return Fail(error, gConfigurationStatus);
        }

        gState = std::move(parsed.state);
        if (parsed.usedNativeFallback)
            gConfigurationStatus = parsed.error;
        else
            gConfigurationStatus = "Loaded successfully.";
        return true;
    }

    std::string GetActiveUid()
    {
        PersonaSeed seed{};
        bool personaMode = false;
        {
            std::lock_guard<std::mutex> lock(gMutex);
            if (gInitialized && gState.activePersonaId != kNativeIdentityId) {
                const Persona* persona = FindPersona(gState, gState.activePersonaId);
                if (persona != nullptr) {
                    seed = persona->seed;
                    personaMode = true;
                }
            }
        }
        if (personaMode) {
            const std::string uid = ComputeUidFromIdentityBytes(seed);
            SecureWipe(seed.data(), seed.size());
            return uid;
        }
        return inst::util::ComputeUidFromMmcCid();
    }

    std::string GetActiveIdentityName()
    {
        std::lock_guard<std::mutex> lock(gMutex);
        if (gState.activePersonaId == kNativeIdentityId)
            return "Native Switch";
        const Persona* persona = FindPersona(gState, gState.activePersonaId);
        return persona == nullptr ? "Native Switch" : persona->name;
    }

    std::string GetActivePersonaId()
    {
        std::lock_guard<std::mutex> lock(gMutex);
        return gState.activePersonaId;
    }

    bool IsNativeMode()
    {
        std::lock_guard<std::mutex> lock(gMutex);
        return gState.activePersonaId == kNativeIdentityId;
    }

    std::vector<Persona> ListPersonas()
    {
        std::lock_guard<std::mutex> lock(gMutex);
        return gState.personas;
    }

    bool ConfigurationLoadedSuccessfully()
    {
        std::lock_guard<std::mutex> lock(gMutex);
        return gConfigurationLoaded;
    }

    std::string GetConfigurationStatus()
    {
        std::lock_guard<std::mutex> lock(gMutex);
        return gConfigurationStatus;
    }

    std::string NextDefaultPersonaName()
    {
        std::lock_guard<std::mutex> lock(gMutex);
        int suffix = 1;
        while (true) {
            const std::string candidate = "Persona " + std::to_string(suffix);
            const bool exists = std::any_of(gState.personas.begin(), gState.personas.end(), [&](const Persona& persona) {
                return persona.name == candidate;
            });
            if (!exists)
                return candidate;
            suffix++;
        }
    }

    bool CreatePersona(const std::string& name, bool activate, std::string* createdId, std::string* error)
    {
        std::array<std::uint8_t, 32> randomMaterial{};
        randomGet(randomMaterial.data(), randomMaterial.size());

        Persona persona;
        std::array<std::uint8_t, 16> idBytes{};
        std::copy_n(randomMaterial.begin(), idBytes.size(), idBytes.begin());
        std::copy_n(randomMaterial.begin() + idBytes.size(), persona.seed.size(), persona.seed.begin());
        persona.id = GenerateIdentifier(idBytes);
        persona.name = name;
        persona.createdAt = static_cast<std::int64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
        SecureWipe(randomMaterial.data(), randomMaterial.size());
        SecureWipe(idBytes.data(), idBytes.size());

        std::lock_guard<std::mutex> lock(gMutex);
        IdentityState candidate = gState;
        if (!AddPersona(candidate, persona, activate, error)) {
            SecureWipe(persona.seed.data(), persona.seed.size());
            return false;
        }
        if (!SaveCandidateLocked(candidate, error)) {
            SecureWipe(persona.seed.data(), persona.seed.size());
            return false;
        }
        if (createdId != nullptr)
            *createdId = persona.id;
        SecureWipe(persona.seed.data(), persona.seed.size());
        return true;
    }

    bool ActivatePersona(const std::string& id, std::string* error)
    {
        std::lock_guard<std::mutex> lock(gMutex);
        IdentityState candidate = gState;
        if (!ActivateIdentity(candidate, id, error))
            return false;
        return SaveCandidateLocked(candidate, error);
    }

    bool ActivateNative(std::string* error)
    {
        return ActivatePersona(kNativeIdentityId, error);
    }

    bool RenamePersona(const std::string& id, const std::string& name, std::string* error)
    {
        std::lock_guard<std::mutex> lock(gMutex);
        IdentityState candidate = gState;
        if (!inst::identity::RenamePersona(candidate, id, name, error))
            return false;
        return SaveCandidateLocked(candidate, error);
    }

    bool DeletePersona(const std::string& id, std::string* error)
    {
        std::lock_guard<std::mutex> lock(gMutex);
        IdentityState candidate = gState;
        if (!inst::identity::DeletePersona(candidate, id, error))
            return false;
        return SaveCandidateLocked(candidate, error);
    }
}
