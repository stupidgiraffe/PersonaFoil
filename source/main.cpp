#include <thread>
#include "switch.h"
#include "util/debug.h"
#include "util/error.hpp"
#include "ui/MainApplication.hpp"
#include "util/util.hpp"
#include "util/config.hpp"
#include "util/offline_db_update.hpp"
#include "util/update.hpp"

using namespace pu::ui::render;
int main(int argc, char* argv[])
{
    bool appInitialized = false;
    try {
        inst::update::SetRunningNroPath((argc > 0 && argv != nullptr && argv[0] != nullptr) ? argv[0] : "");
        debugLogReset();
        inst::util::initApp();
        appInitialized = true;
        auto renderer = Renderer::New(SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC | SDL_INIT_GAMECONTROLLER,
            RendererInitOptions::RendererNoSound, RendererHardwareFlags);
        auto main = inst::ui::MainApplication::New(renderer);
        std::thread updateThread;
        std::thread offlineDbUpdateCheckThread;
        if (inst::config::autoUpdate && inst::util::getIPAddress() != "1.0.0.127") updateThread = std::thread(inst::util::checkForAppUpdate);
        inst::offline::dbupdate::ResetStartupCheckState();
        if (inst::config::offlineDbAutoCheckOnStartup && inst::util::getIPAddress() != "1.0.0.127") {
            offlineDbUpdateCheckThread = std::thread([]() {
                const auto result = inst::offline::dbupdate::CheckForUpdate(inst::config::offlineDbManifestUrl);
                inst::offline::dbupdate::SetStartupCheckResult(result);
            });
        }
        main->Prepare();
        main->ShowWithFadeIn();
        if (updateThread.joinable()) {
            updateThread.join();
        }
        if (offlineDbUpdateCheckThread.joinable()) {
            offlineDbUpdateCheckThread.join();
        }
    } catch (std::exception& e) {
        LOG_DEBUG("An error occurred:\n%s", e.what());
    } catch (...) {
        LOG_DEBUG("An unknown error occurred during startup.");
    }
    if (appInitialized) {
        inst::util::deinitApp();
    }
    return 0;
}
