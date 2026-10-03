/*
Copyright (c) 2017-2018 Adubbz

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <string>
#include <thread>
#include <malloc.h>
#include <algorithm>
#include "usbInstall.hpp"
#include "install/usb_nsp.hpp"
#include "install/install_nsp.hpp"
#include "install/usb_xci.hpp"
#include "install/install_xci.hpp"
#include "util/error.hpp"
#include "util/usb_util.hpp"
#include "util/util.hpp"
#include "util/config.hpp"
#include "util/install_diagnostics.hpp"
#include "util/lang.hpp"
#include "ui/MainApplication.hpp"
#include "ui/usbInstPage.hpp"
#include "ui/instPage.hpp"

namespace inst::ui {
    extern MainApplication *mainApp;
}

namespace usbInstStuff {
    struct TUSHeader
    {
        u32 magic; // TUL0 (Tinfoil Usb List 0)
        u32 titleListSize;
        u64 padding;
    } NX_PACKED;

    int bufferData(void* buf, size_t size, u64 timeout = 5000000000)
    {
        u8* tempBuffer = (u8*)memalign(0x1000, size);
        if (tin::util::USBRead(tempBuffer, size, timeout) == 0) return 0;
        memcpy(buf, tempBuffer, size);
        free(tempBuffer);
        return size;
    }

    std::vector<std::string> OnSelected() {
        TUSHeader header;
        while(true) {
            if (bufferData(&header, sizeof(TUSHeader), 500000000) != 0) break;
            inst::ui::mainApp->RefreshInputDevice();
            inst::ui::mainApp->UpdateButtons();
            u64 kDown = inst::ui::mainApp->GetButtonsDown();
            if (kDown & HidNpadButton_B) return {};
            if (kDown & HidNpadButton_X) inst::ui::mainApp->CreateShowDialog("inst.usb.help.title"_lang, "inst.usb.help.desc"_lang, {"common.ok"_lang}, true);
            if (!inst::util::usbIsConnected()) return {};
        }

        if (header.magic != 0x304C5554) return {};

        std::vector<std::string> titleNames;
        char* titleNameBuffer = (char*)memalign(0x1000, header.titleListSize + 1);
        memset(titleNameBuffer, 0, header.titleListSize + 1);

        tin::util::USBRead(titleNameBuffer, header.titleListSize, 10000000000);

        // Split the string up into individual title names
        std::stringstream titleNamesStream(titleNameBuffer);
        std::string segment;
        while (std::getline(titleNamesStream, segment, '\n')) titleNames.push_back(segment);
        free(titleNameBuffer);
        std::sort(titleNames.begin(), titleNames.end(), inst::util::ignoreCaseCompare);

        return titleNames;
    }

    void installTitleUsb(std::vector<std::string> ourTitleList, int ourStorage)
    {
        inst::util::initInstallServices();
        inst::ui::instPage::loadInstallScreen();
        bool nspInstalled = true;
        NcmStorageId m_destStorageId = NcmStorageId_SdCard;
        std::string currentName;

        if (ourStorage) m_destStorageId = NcmStorageId_BuiltInUser;
        unsigned int fileItr;
        inst::diag::StartSession("usb", ourTitleList.size());

        std::vector<std::string> fileNames;
        for (long unsigned int i = 0; i < ourTitleList.size(); i++) {
            fileNames.push_back(inst::util::shortenString(inst::util::formatUrlString(ourTitleList[i]), 40, true));
        }

        std::vector<int> previousClockValues;
        if (inst::config::overClock) {
            previousClockValues.push_back(inst::util::setClockSpeed(0, 1785000000)[0]);
            previousClockValues.push_back(inst::util::setClockSpeed(1, 76800000)[0]);
            previousClockValues.push_back(inst::util::setClockSpeed(2, 1600000000)[0]);
        }

        try {
            for (fileItr = 0; fileItr < ourTitleList.size(); fileItr++) {
                currentName = fileNames[fileItr];
                inst::diag::NoteTransferReceived(currentName);
                inst::ui::instPage::setTopInstInfoText("inst.info_page.top_info0"_lang + currentName + "inst.usb.source_string"_lang);
                std::unique_ptr<tin::install::Install> installTask;

                if (ourTitleList[fileItr].compare(ourTitleList[fileItr].size() - 3, 2, "xc") == 0) {
                    auto usbXCI = std::make_shared<tin::install::xci::USBXCI>(ourTitleList[fileItr]);
                    installTask = std::make_unique<tin::install::xci::XCIInstallTask>(m_destStorageId, inst::config::ignoreReqVers, usbXCI);
                } else {
                    auto usbNSP = std::make_shared<tin::install::nsp::USBNSP>(ourTitleList[fileItr]);
                    installTask = std::make_unique<tin::install::nsp::NSPInstall>(m_destStorageId, inst::config::ignoreReqVers, usbNSP);
                }

                LOG_DEBUG("%s\n", "Preparing installation");
                inst::ui::instPage::setInstInfoText("Transfer received. Install started...");
                inst::ui::instPage::setInstBarPerc(0);
                inst::diag::NoteInstallStarted(currentName);
                installTask->Prepare();

                installTask->Begin();
                inst::diag::RecordSuccess(currentName);
                inst::ui::instPage::setInstInfoText("Install succeeded: " + currentName);
            }
        }
        catch (std::exception& e) {
            LOG_DEBUG("Failed to install");
            LOG_DEBUG("%s", e.what());
            fprintf(stdout, "%s", e.what());
            const std::string failedName = currentName.empty()
                ? (fileNames.empty() ? std::string("unknown item") : fileNames.front())
                : currentName;
            const auto failure = inst::diag::ClassifyFailure(e.what());
            inst::diag::RecordFailure(failedName, failure);
            inst::ui::instPage::setInstInfoText(failure.canceled ? "Installation canceled." : ("inst.info_page.failed"_lang + failedName));
            inst::ui::instPage::setInstBarPerc(0);
            if (!failure.canceled) {
                inst::ui::mainApp->CreateShowDialog("inst.info_page.failed"_lang + failedName + "!", inst::diag::BuildUserMessage(failure), {"common.ok"_lang}, true);

            } else {
                inst::ui::mainApp->CreateShowDialog("Canceled", inst::diag::BuildUserMessage(failure), {"common.ok"_lang}, true);
            }
            nspInstalled = false;
        }

        if (previousClockValues.size() > 0) {
            inst::util::setClockSpeed(0, previousClockValues[0]);
            inst::util::setClockSpeed(1, previousClockValues[1]);
            inst::util::setClockSpeed(2, previousClockValues[2]);
        }

        if(nspInstalled) {
            tin::util::USBCmdManager::SendExitCmd();
            inst::ui::instPage::setInstInfoText("inst.info_page.complete"_lang);
            inst::ui::instPage::setInstBarPerc(100);
            std::string audioPath = "romfs:/audio/success.wav";
            if (!inst::config::soundEnabled) audioPath = "";
            else if (std::filesystem::exists(inst::config::appDir + "/success.wav")) audioPath = inst::config::appDir + "/success.wav";
            std::thread audioThread(inst::util::playAudio,audioPath);
            if (ourTitleList.size() > 1) inst::ui::mainApp->CreateShowDialog(std::to_string(ourTitleList.size()) + "inst.info_page.desc0"_lang, "inst.info_page.complete"_lang, {"common.ok"_lang}, true);
            else inst::ui::mainApp->CreateShowDialog(fileNames[0] + "inst.info_page.desc1"_lang, "inst.info_page.complete"_lang, {"common.ok"_lang}, true);
            audioThread.join();
        }
        
        LOG_DEBUG("Done");
        inst::ui::instPage::loadMainMenu();
        inst::util::deinitInstallServices();
        return;
    }
}
