#include "util/diagnostics.hpp"

#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>

#include <switch.h>

#include "identity/identity.hpp"
#include "util/config.hpp"
#include "util/update.hpp"

namespace inst::diagnostics {
    namespace {
        bool Fail(std::string* error, const std::string& message)
        {
            if (error != nullptr) *error = message;
            return false;
        }

        std::string AppletTypeName(AppletType type)
        {
            return type == AppletType_LibraryApplet ? "applet / limited-memory" : "application / full-memory compatible";
        }
    }

    bool ExportDiagnosticReport(std::string* outPath, std::string* error)
    {
        const std::string directory = inst::config::appDir + "/diagnostics";
        std::error_code ec;
        std::filesystem::create_directories(directory, ec);
        if (ec) return Fail(error, "Could not create the diagnostics directory.");

        std::time_t now = std::time(nullptr);
        std::tm utc{};
        gmtime_r(&now, &utc);
        char stamp[32]{};
        std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%SZ", &utc);
        const std::string path = directory + "/personafoil-" + stamp + ".txt";

        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if (!file) return Fail(error, "Could not create the diagnostic report.");

        const u32 hos = hosversionGet();
        const auto personas = inst::identity::ListPersonas();
        const char* loader = envGetLoaderInfo();

        file << "PersonaFoil Diagnostic Report\n";
        file << "Generated UTC: " << std::put_time(&utc, "%Y-%m-%d %H:%M:%S") << "\n\n";
        file << "Application version: " << inst::config::appVersionFull << "\n";
        file << "Build metadata: " << (inst::config::appGitMeta.empty() ? "release/none" : inst::config::appGitMeta) << "\n";
        file << "Horizon version: " << HOSVER_MAJOR(hos) << '.' << HOSVER_MINOR(hos) << '.' << HOSVER_MICRO(hos) << "\n";
        file << "Atmosphere environment: " << (hosversionIsAtmosphere() ? "yes" : "no/unknown") << "\n";
        file << "Loader: " << ((loader != nullptr && *loader != '\0') ? loader : "unknown") << "\n";
        file << "Executable type: " << (envIsNso() ? "NSO" : "NRO") << "\n";
        file << "Launch mode: " << AppletTypeName(appletGetAppletType()) << "\n";
        const std::string running = inst::update::GetRunningNroPath();
        file << "Running NRO path: " << (running.empty() ? "unknown" : running) << "\n\n";

        file << "Identity mode: " << (inst::identity::IsNativeMode() ? "Native" : "Persona") << "\n";
        file << "Active identity: " << inst::identity::GetActiveIdentityName() << "\n";
        file << "UID fingerprint: " << inst::identity::FormatUidFingerprint(inst::identity::GetActiveUid()) << "\n";
        file << "Persona count: " << personas.size() << "\n";
        file << "Identity schema: " << inst::identity::kIdentitySchemaVersion << "\n";
        file << "Identity configuration loaded: " << (inst::identity::ConfigurationLoadedSuccessfully() ? "yes" : "no") << "\n";
        file << "Identity status: " << inst::identity::GetConfigurationStatus() << "\n\n";

        file << "Auto update check: " << (inst::config::autoUpdate ? "enabled" : "disabled") << "\n";
        file << "Saved Remotes: " << inst::config::LoadRemotes().size() << "\n";
        file << "\nPrivacy note: this report intentionally excludes physical CID, full UID, persona seeds, Remote URLs/credentials, passwords, Authorization headers, and authentication tokens.\n";
        file.flush();
        if (!file.good()) return Fail(error, "Could not finish writing the diagnostic report.");

        if (outPath != nullptr) *outPath = path;
        return true;
    }
}
