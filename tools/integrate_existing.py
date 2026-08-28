from pathlib import Path
import re

ROOT = Path.cwd()

def load(p): return (ROOT/p).read_text(encoding='utf-8')
def save(p,s): (ROOT/p).write_text(s,encoding='utf-8')
def one(s,a,b,label):
    n=s.count(a)
    if n!=1: raise RuntimeError(f'{label}: expected 1, found {n}')
    return s.replace(a,b,1)
def rx(s,p,r,label):
    o,n=re.subn(p,r,s,count=1,flags=re.S)
    if n!=1: raise RuntimeError(f'{label}: expected 1, found {n}')
    return o

p='source/ui/optionsPage.cpp'; s=load(p)
s=one(s,'#include "util/unzip.hpp"\n','#include "util/update.hpp"\n#include "util/diagnostics.hpp"\n','options includes')
ask='''    void optionsPage::askToUpdate(std::vector<std::string> updateInfo) {
        if (updateInfo.size() < 4) {
            mainApp->CreateShowDialog("Update unavailable", "Update metadata is incomplete. Check again later.", {"common.ok"_lang}, true);
            return;
        }
        inst::update::ReleaseInfo release;
        release.version = updateInfo[0];
        release.nroUrl = updateInfo[1];
        release.notes = updateInfo[2].empty() ? "No changelog available for this release." : updateInfo[2];
        release.checksumsUrl = updateInfo[3];
        while (true) {
            const std::string body = "Current: v" + inst::config::appVersion + "\\nLatest: " + release.version +
                "\\n\\nA verified update is available from the official PersonaFoil GitHub Release.";
            const int choice = mainApp->CreateShowDialog("PersonaFoil update", body,
                {"Download & Install", "View Changelog", "common.cancel"_lang}, false);
            if (choice == 1) {
                ShowPagedTextDialog("Changelog " + release.version, release.notes);
                continue;
            }
            if (choice != 0) break;
            inst::ui::instPage::loadInstallScreen();
            inst::ui::instPage::setTopInstInfoText("Updating PersonaFoil to " + release.version);
            inst::ui::instPage::setInstBarPerc(0);
            inst::ui::instPage::setInstInfoText("Preparing verified update...");
            const auto install = inst::update::InstallUpdate(release,
                [](const std::string& stage, double percent) {
                    inst::ui::instPage::setInstInfoText(stage);
                    inst::ui::instPage::setInstBarPerc(percent);
                });
            mainApp->LoadLayout(mainApp->optionspage);
            if (!install.success) {
                mainApp->CreateShowDialog("Update failed", install.error, {"common.ok"_lang}, true);
                break;
            }
            std::string done = "PersonaFoil " + release.version + " installed successfully.\\n\\nExit and relaunch PersonaFoil to use the new version.";
            if (!install.backupPath.empty()) done += "\\n\\nPrevious NRO backup: " + install.backupPath;
            mainApp->CreateShowDialog("Update complete", done, {"common.ok"_lang}, false);
            mainApp->FadeOut();
            mainApp->Close();
            break;
        }
    }

'''
s=rx(s,r'    void optionsPage::askToUpdate\(std::vector<std::string> updateInfo\) \{.*?\n    \}\n\n    std::string optionsPage::getMenuOptionIcon',ask+'    std::string optionsPage::getMenuOptionIcon','askToUpdate')
s=one(s,'addItem("Native Switch" + std::string(activeId == inst::identity::kNativeIdentityId ? "  (Active)" : ""), false, false);','addItem(activeId == inst::identity::kNativeIdentityId ? "Native Switch  (Active)" : "Use Native Switch", false, false);','native label')
s=one(s,'            addItem("Create new persona", false, false);\n            addItem("Diagnostics (" + std::to_string(personas.size()) + " personas)", false, false);','            addItem("New Identity", false, false);\n            addItem("Export Diagnostic Report", false, false);\n            addItem("Diagnostics (" + std::to_string(personas.size()) + " personas)", false, false);','identity actions')
create='''    void optionsPage::createPersona() {
        const std::string defaultName = inst::identity::NextDefaultPersonaName();
        const int confirm = mainApp->CreateShowDialog(
            "New Identity",
            "Create and activate a persistent local identity named " + defaultName + "?\\n\\nIts random seed stays on this SD card.",
            {"Create & Activate", "common.cancel"_lang}, false);
        if (confirm != 0) return;
        std::string error;
        if (!inst::identity::CreatePersona(defaultName, true, nullptr, &error)) {
            mainApp->CreateShowDialog("Could not create identity", error, {"common.ok"_lang}, true);
            return;
        }
        this->refreshOptions();
        const std::string fingerprint = inst::identity::FormatUidFingerprint(inst::identity::GetActiveUid());
        mainApp->CreateShowDialog("Identity activated", defaultName + " is now active.\\nUID: " + fingerprint,
            {"common.ok"_lang}, false);
    }

    void optionsPage::exportDiagnosticReport() {
        std::string path;
        std::string error;
        if (!inst::diagnostics::ExportDiagnosticReport(&path, &error)) {
            mainApp->CreateShowDialog("Diagnostic export failed", error, {"common.ok"_lang}, true);
            return;
        }
        mainApp->CreateShowDialog("Diagnostic report exported", path, {"common.ok"_lang}, false);
    }

'''
s=rx(s,r'    void optionsPage::createPersona\(\) \{.*?\n    \}\n\n    void optionsPage::managePersona',create+'    void optionsPage::managePersona','create persona')
s=one(s,'                if (selectedIndex == static_cast<int>(personas.size()) + 4) {\n                    this->showIdentityDiagnostics();\n                    return;\n                }','                if (selectedIndex == static_cast<int>(personas.size()) + 4) {\n                    this->exportDiagnosticReport();\n                    return;\n                }\n                if (selectedIndex == static_cast<int>(personas.size()) + 5) {\n                    this->showIdentityDiagnostics();\n                    return;\n                }','identity indexes')
s=one(s,'            std::vector<std::string> downloadUrl;\n','', 'remove legacy downloadUrl')
old='''                case 17:
                    if (inst::util::getIPAddress() == "1.0.0.127") {
                        inst::ui::mainApp->CreateShowDialog("main.net.title"_lang, "main.net.desc"_lang, {"common.ok"_lang}, true);
                        break;
                    }
                    downloadUrl = inst::util::checkForAppUpdate();
                    if (!downloadUrl.size()) {
                        mainApp->CreateShowDialog("options.update.title_check_fail"_lang, "options.update.desc_check_fail"_lang, {"common.ok"_lang}, false);
                        break;
                    }
                    this->askToUpdate(downloadUrl);
                    break;'''
new='''                case 17: {
                    if (inst::util::getIPAddress() == "1.0.0.127") {
                        inst::ui::mainApp->CreateShowDialog("main.net.title"_lang, "main.net.desc"_lang, {"common.ok"_lang}, true);
                        break;
                    }
                    const auto check = inst::update::CheckForUpdate(inst::config::appVersion);
                    if (check.status == inst::update::CheckStatus::Error) {
                        mainApp->CreateShowDialog("Update check failed", check.error.empty() ? "Could not check PersonaFoil releases." : check.error, {"common.ok"_lang}, true);
                        break;
                    }
                    if (check.status == inst::update::CheckStatus::UpToDate) {
                        mainApp->CreateShowDialog("PersonaFoil is up to date", "Current version: v" + inst::config::appVersion, {"common.ok"_lang}, false);
                        break;
                    }
                    this->askToUpdate({check.release.version, check.release.nroUrl, check.release.notes, check.release.checksumsUrl});
                    break;
                }'''
s=one(s,old,new,'manual check update')
save(p,s)

p='include/ui/optionsPage.hpp'; s=load(p)
s=one(s,'            void createPersona();\n            void showIdentityDiagnostics();','            void createPersona();\n            void exportDiagnosticReport();\n            void showIdentityDiagnostics();','options header')
save(p,s)

p='source/util/util.cpp'; s=load(p)
s=one(s,'#include "identity/identity.hpp"\n','#include "identity/identity.hpp"\n#include "util/update.hpp"\n','util updater include')
s=rx(s,r'\n        std::string NormalizeReleaseNotes\(std::string text\) \{.*?            return out;\n        \}\n','\n','remove legacy release notes')
wrapper='''   std::vector<std::string> checkForAppUpdate () {
        inst::config::updateInfo.clear();
        const auto check = inst::update::CheckForUpdate(inst::config::appVersion);
        if (check.status != inst::update::CheckStatus::UpdateAvailable) return {};
        std::vector<std::string> updateInfo = {
            check.release.version,
            check.release.nroUrl,
            check.release.notes,
            check.release.checksumsUrl
        };
        inst::config::updateInfo = updateInfo;
        return updateInfo;
    }
'''
s=rx(s,r'   std::vector<std::string> checkForAppUpdate \(\) \{.*?\n    \}\n\}',wrapper+'}','legacy updater')
save(p,s)

p='source/main.cpp'; s=load(p)
s=one(s,'#include "util/offline_db_update.hpp"\n','#include "util/offline_db_update.hpp"\n#include "util/update.hpp"\n','main include')
s=one(s,'    bool appInitialized = false;\n    try {\n        debugLogReset();','    bool appInitialized = false;\n    try {\n        inst::update::SetRunningNroPath((argc > 0 && argv != nullptr && argv[0] != nullptr) ? argv[0] : "");\n        debugLogReset();','main path')
save(p,s)

p='source/util/config.cpp'; s=load(p)
s=one(s,'        autoUpdate = true;\n','        autoUpdate = false;\n','auto update default')
save(p,s)

p='Makefile'; s=load(p)
old='''HOST_CXX ?= g++
HOST_TEST_BIN := build-host/identity_tests

host-test:
\t@mkdir -p build-host
\t$(HOST_CXX) -std=c++20 -Wall -Wextra -Werror -Iinclude tests/identity_tests.cpp source/identity/identity_core.cpp -lcrypto -o $(HOST_TEST_BIN)
\t./$(HOST_TEST_BIN)'''
new='''HOST_CXX ?= g++
HOST_TEST_BIN := build-host/identity_tests
HOST_UPDATE_TEST_BIN := build-host/update_core_tests

host-test:
\t@mkdir -p build-host
\t$(HOST_CXX) -std=c++20 -Wall -Wextra -Werror -Iinclude tests/identity_tests.cpp source/identity/identity_core.cpp -lcrypto -o $(HOST_TEST_BIN)
\t./$(HOST_TEST_BIN)
\t$(HOST_CXX) -std=c++20 -Wall -Wextra -Werror -Iinclude tests/update_core_tests.cpp source/util/update_core.cpp -o $(HOST_UPDATE_TEST_BIN)
\t./$(HOST_UPDATE_TEST_BIN)'''
s=one(s,old,new,'Makefile host tests'); save(p,s)

p='.github/workflows/personafoil-ci.yml'; s=load(p)
s=one(s,'          python3 -m zipfile --list personafoil.zip\n','          python3 -m zipfile --list personafoil.zip\n          sha256sum personafoil.nro personafoil.zip > SHA256SUMS.txt\n          grep -Eq "^[0-9a-f]{64}  personafoil\\.nro$" SHA256SUMS.txt\n          grep -Eq "^[0-9a-f]{64}  personafoil\\.zip$" SHA256SUMS.txt\n','CI hashes')
s=one(s,'            personafoil.zip\n          if-no-files-found: error','            personafoil.zip\n            SHA256SUMS.txt\n          if-no-files-found: error','CI artifact hashes'); save(p,s)

p='.github/workflows/personafoil-release.yml'; s=load(p)
s=one(s,'          PY\n\n      - name: Publish tagged release','          PY\n          sha256sum personafoil.nro personafoil.zip > SHA256SUMS.txt\n          grep -Eq "^[0-9a-f]{64}  personafoil\\.nro$" SHA256SUMS.txt\n          grep -Eq "^[0-9a-f]{64}  personafoil\\.zip$" SHA256SUMS.txt\n\n      - name: Publish tagged release','release hashes')
s=one(s,'            personafoil.zip\n          generate_release_notes: true','            personafoil.zip\n            SHA256SUMS.txt\n          generate_release_notes: true','release publish hash'); save(p,s)

p='docs/TESTING.md'; s=load(p)
if '## Current real-hardware status' not in s:
    s=s.rstrip()+'''\n\n## Current real-hardware status\n\nObserved: PersonaFoil launches; persona creation/derivation executes; activating a persona changes the displayed UID fingerprint.\n\nStill required before outgoing identity is called hardware validated:\n\n```text\nNative -> controlled endpoint -> UID A\nPersona 1 -> controlled endpoint -> UID B\nrestart -> controlled endpoint -> UID B\nPersona 2 -> controlled endpoint -> UID C\nNative -> controlled endpoint -> UID A\n```\n\nRequire `A != B`, `A != C`, `B != C`, Persona 1 stability across restart, and exact Native equality before/after.\n\nThe in-app updater is not hardware validated until an actual older stable release successfully verifies/installs a newer stable release and preserves PersonaFoil user state.\n'''
save(p,s)

p='docs/ARCHITECTURE.md'; s=load(p)
if '## Verified updater boundary' not in s:
    s=s.rstrip()+'''\n\n## Verified updater boundary\n\nUpdate discovery and parsing are separated from Switch filesystem installation. Portable `update_core` logic owns stable semantic-version parsing, exact asset selection, trusted release-URL checks, and strict `SHA256SUMS.txt` parsing. The Switch-specific updater obtains the launched NRO path from process argv, downloads only official stable GitHub Release assets, verifies SHA-256 before touching the installed executable, stages `.new`, preserves `.bak`, and attempts rollback on final replacement failure.\n\nPersona/config/offline-database/Remote data are outside the updater transaction.\n\n## Diagnostic reports\n\nDiagnostic export is privacy-minimizing: it records build/environment/state fingerprints useful for support but excludes physical CID, full UID, persona seed material, passwords, Remote credentials/URLs, Authorization headers, and authentication tokens.\n'''
save(p,s)

print('integration transform complete')
