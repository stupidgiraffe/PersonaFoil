#pragma once

#include <string>
#include <vector>

#include "identity/identity_core.hpp"

namespace inst::identity {
    bool Initialize(std::string* error = nullptr);

    std::string GetActiveUid();
    std::string GetActiveIdentityName();
    std::string GetActivePersonaId();
    bool IsNativeMode();
    std::vector<Persona> ListPersonas();

    bool ConfigurationLoadedSuccessfully();
    std::string GetConfigurationStatus();

    std::string NextDefaultPersonaName();
    bool CreatePersona(const std::string& name, bool activate, std::string* createdId = nullptr, std::string* error = nullptr);
    bool ActivatePersona(const std::string& id, std::string* error = nullptr);
    bool ActivateNative(std::string* error = nullptr);
    bool RenamePersona(const std::string& id, const std::string& name, std::string* error = nullptr);
    bool DeletePersona(const std::string& id, std::string* error = nullptr);
}
