#include <filesystem>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>
#include "ui/MainApplication.hpp"
#include "ui/instPage.hpp"
#include "util/util.hpp"
#include "util/config.hpp"
#include "mtp_install.hpp"
#include "mtp_server.hpp"
#include "ui/bottomHint.hpp"
#include <switch.h>

#define COLOR(hex) pu::ui::Color::FromHex(hex)

namespace inst::ui {
    extern MainApplication *mainApp;
    static pu::ui::Layout::Ref lastLayoutBeforeInstall;
    static std::atomic<bool> g_installCancelRequested{false};
    static std::atomic<bool> g_installDimSessionActive{false};
    static std::atomic<bool> g_installDimStopRequested{false};
    static std::thread g_installDimThread;
    static std::mutex g_installDimMutex;
    static bool g_installDimLblReady = false;
    static bool g_installDimSavedBrightnessValid = false;
    static bool g_installDimIsDimmed = false;
    static float g_installDimSavedBrightness = 0.0f;
    static std::chrono::steady_clock::time_point g_installDimLastTouchAt;

    constexpr int kInstallIconSize = 256;
    constexpr int kInstallIconX = (1280 - kInstallIconSize) / 2;
    constexpr int kInstallIconY = 220;
    constexpr auto kInstallDimDelay = std::chrono::seconds(60);
    constexpr auto kInstallDimPollInterval = std::chrono::milliseconds(250);
    constexpr float kInstallDimBrightness = 0.2f;

    static void RestoreInstallBrightnessLocked()
    {
        if (!g_installDimLblReady || !g_installDimSavedBrightnessValid || !g_installDimIsDimmed)
            return;

        lblSetCurrentBrightnessSetting(g_installDimSavedBrightness);
        lblApplyCurrentBrightnessSettingToBacklight();
        g_installDimIsDimmed = false;
    }

    static void EnsureInstallDimmedLocked()
    {
        if (!g_installDimLblReady || !g_installDimSavedBrightnessValid || g_installDimIsDimmed)
            return;

        const float targetBrightness = std::min(g_installDimSavedBrightness, kInstallDimBrightness);
        if (targetBrightness >= g_installDimSavedBrightness)
            return;

        if (R_SUCCEEDED(lblSetCurrentBrightnessSetting(targetBrightness)) &&
            R_SUCCEEDED(lblApplyCurrentBrightnessSettingToBacklight())) {
            g_installDimIsDimmed = true;
        }
    }

    static void StartInstallDimSession()
    {
        std::lock_guard<std::mutex> lock(g_installDimMutex);
        g_installDimLastTouchAt = std::chrono::steady_clock::now();

        if (g_installDimSessionActive)
            return;

        g_installDimStopRequested = false;
        g_installDimLblReady = R_SUCCEEDED(lblInitialize());
        g_installDimSavedBrightnessValid = false;
        g_installDimIsDimmed = false;
        g_installDimSavedBrightness = 0.0f;

        if (g_installDimLblReady) {
            float currentBrightness = 0.0f;
            if (R_SUCCEEDED(lblGetCurrentBrightnessSetting(&currentBrightness))) {
                g_installDimSavedBrightness = currentBrightness;
                g_installDimSavedBrightnessValid = true;
            }
        }

        g_installDimSessionActive = true;
        g_installDimThread = std::thread([]() {
            while (!g_installDimStopRequested) {
                {
                    std::lock_guard<std::mutex> lock(g_installDimMutex);
                    if (!g_installDimSessionActive)
                        break;
                    if (std::chrono::steady_clock::now() - g_installDimLastTouchAt >= kInstallDimDelay)
                        EnsureInstallDimmedLocked();
                }
                std::this_thread::sleep_for(kInstallDimPollInterval);
            }
        });
    }

    static void StopInstallDimSession()
    {
        g_installDimStopRequested = true;
        if (g_installDimThread.joinable())
            g_installDimThread.join();

        std::lock_guard<std::mutex> lock(g_installDimMutex);
        RestoreInstallBrightnessLocked();
        if (g_installDimLblReady)
            lblExit();
        g_installDimLblReady = false;
        g_installDimSavedBrightnessValid = false;
        g_installDimSessionActive = false;
    }

    static void NotifyInstallTouchActivity()
    {
        std::lock_guard<std::mutex> lock(g_installDimMutex);
        if (!g_installDimSessionActive)
            return;

        g_installDimLastTouchAt = std::chrono::steady_clock::now();
        RestoreInstallBrightnessLocked();
    }

    instPage::instPage() : Layout::Layout() {
        if (inst::config::oledMode) {
            this->SetBackgroundColor(COLOR("#000000FF"));
        } else {
            this->SetBackgroundColor(COLOR("#670000FF"));
            if (std::filesystem::exists(inst::config::appDir + "/background.png")) this->SetBackgroundImage(inst::config::appDir + "/background.png");
            else this->SetBackgroundImage("romfs:/images/background.jpg");
        }
        const auto topColor = inst::config::oledMode ? COLOR("#000000FF") : COLOR("#170909FF");
        const auto infoColor = inst::config::oledMode ? COLOR("#000000FF") : COLOR("#17090980");
        const auto botColor = inst::config::oledMode ? COLOR("#000000FF") : COLOR("#17090980");
        this->topRect = Rectangle::New(0, 0, 1280, 74, topColor);
        this->infoRect = Rectangle::New(0, 75, 1280, 60, infoColor);
        this->botRect = Rectangle::New(0, 660, 1280, 60, botColor);
        if (inst::config::gayMode) {
            this->titleImage = Image::New(-113, -8, "romfs:/images/personafoil-logo.png");
            this->appVersionText = TextBlock::New(367, 29, "v" + inst::config::appVersion + (inst::config::appGitMeta.empty() ? "" : ("\n" + inst::config::appGitMeta)), 22);
        }
        else {
            this->titleImage = Image::New(0, -8, "romfs:/images/personafoil-logo.png");
            this->appVersionText = TextBlock::New(480, 29, "v" + inst::config::appVersion + (inst::config::appGitMeta.empty() ? "" : ("\n" + inst::config::appGitMeta)), 22);
        }
        this->appVersionText->SetColor(COLOR("#FFFFFFFF"));
        this->timeText = TextBlock::New(0, 18, "--:--", 22);
        this->timeText->SetColor(COLOR("#FFFFFFFF"));
        this->ipText = TextBlock::New(0, 26, "IP: --", 16);
        this->ipText->SetColor(COLOR("#FFFFFFFF"));
        this->sysLabelText = TextBlock::New(0, 6, "System Memory", 16);
        this->sysLabelText->SetColor(COLOR("#FFFFFFFF"));
        this->sysFreeText = TextBlock::New(0, 42, "Free --", 16);
        this->sysFreeText->SetColor(COLOR("#FFFFFFFF"));
        this->sdLabelText = TextBlock::New(0, 6, "microSD Card", 16);
        this->sdLabelText->SetColor(COLOR("#FFFFFFFF"));
        this->sdFreeText = TextBlock::New(0, 42, "Free --", 16);
        this->sdFreeText->SetColor(COLOR("#FFFFFFFF"));
        this->sysBarBack = Rectangle::New(0, 30, 180, 6, COLOR("#FFFFFF33"));
        this->sysBarFill = Rectangle::New(0, 30, 0, 6, COLOR("#FF4D4DFF"));
        this->sdBarBack = Rectangle::New(0, 30, 180, 6, COLOR("#FFFFFF33"));
        this->sdBarFill = Rectangle::New(0, 30, 0, 6, COLOR("#FF4D4DFF"));
        this->netIndicator = Rectangle::New(0, 0, 6, 6, COLOR("#FF3B30FF"), 3);
        this->wifiBar1 = Rectangle::New(0, 0, 4, 4, COLOR("#FFFFFF55"));
        this->wifiBar2 = Rectangle::New(0, 0, 4, 7, COLOR("#FFFFFF55"));
        this->wifiBar3 = Rectangle::New(0, 0, 4, 10, COLOR("#FFFFFF55"));
        this->batteryOutline = Rectangle::New(0, 0, 24, 12, COLOR("#FFFFFF66"));
        this->batteryFill = Rectangle::New(0, 0, 0, 10, COLOR("#4CD964FF"));
        this->batteryCap = Rectangle::New(0, 0, 3, 6, COLOR("#FFFFFF66"));
        this->pageInfoText = TextBlock::New(10, 89, "", 30);
        this->pageInfoText->SetColor(COLOR("#FFFFFFFF"));
        this->installInfoText = TextBlock::New(15, 568, "", 22);
        this->installInfoText->SetColor(COLOR("#FFFFFFFF"));
        this->installBar = pu::ui::elm::ProgressBar::New(10, 600, 850, 40, 100.0f);
        this->installBar->SetColor(COLOR("#222222FF"));
        this->installBar->SetProgressColor(COLOR("#FF4D4DFF"));
        this->hintText = TextBlock::New(0, 678, " Back", 20);
        this->hintText->SetColor(COLOR("#FFFFFFFF"));
        this->hintText->SetX(1280 - 10 - this->hintText->GetTextWidth());
        this->hintText->SetVisible(false);
        this->bottomHintSegments = BuildBottomHintSegments(this->hintText->GetText(), this->hintText->GetX(), 20);
        this->progressText = TextBlock::New(0, 340, "", 30);
        this->progressText->SetColor(COLOR("#FFFFFFFF"));
        this->progressText->SetVisible(false);
        this->progressDetailText = TextBlock::New(0, 646, "", 22);
        this->progressDetailText->SetColor(COLOR("#FFFFFFFF"));
        this->progressDetailText->SetVisible(false);
        if (std::filesystem::exists(inst::config::appDir + "/awoo_inst.png")) this->awooImage = Image::New(410, 190, inst::config::appDir + "/awoo_inst.png");
        else this->awooImage = Image::New(510, 166, "romfs:/images/awoos/7d8a05cddfef6da4901b20d2698d5a71.png");
        this->installIconImage = Image::New(kInstallIconX, kInstallIconY, "romfs:/images/awoos/7d8a05cddfef6da4901b20d2698d5a71.png");
        this->installIconImage->SetWidth(kInstallIconSize);
        this->installIconImage->SetHeight(kInstallIconSize);
        this->installIconImage->SetVisible(false);
        this->Add(this->topRect);
        this->Add(this->infoRect);
        this->Add(this->botRect);
        this->Add(this->titleImage);
        this->Add(this->appVersionText);
        this->Add(this->sysBarBack);
        this->Add(this->sysBarFill);
        this->Add(this->sdBarBack);
        this->Add(this->sdBarFill);
        this->Add(this->sysLabelText);
        this->Add(this->sysFreeText);
        this->Add(this->sdLabelText);
        this->Add(this->sdFreeText);
        this->Add(this->netIndicator);
        this->Add(this->wifiBar1);
        this->Add(this->wifiBar2);
        this->Add(this->wifiBar3);
        this->Add(this->batteryOutline);
        this->Add(this->batteryFill);
        this->Add(this->batteryCap);
        this->Add(this->timeText);
        this->Add(this->ipText);
        this->Add(this->pageInfoText);
        this->Add(this->installInfoText);
        this->Add(this->installBar);
        this->Add(this->progressText);
        this->Add(this->progressDetailText);
        this->Add(this->hintText);
        this->Add(this->awooImage);
        this->Add(this->installIconImage);
        if (inst::config::gayMode) this->awooImage->SetVisible(false);
    }

    void instPage::setTopInstInfoText(std::string ourText){
        mainApp->instpage->pageInfoText->SetText(ourText);
        mainApp->CallForRender();
    }

    void instPage::setInstInfoText(std::string ourText){
        mainApp->instpage->installInfoText->SetText(ourText);
        mainApp->CallForRender();
    }

    void instPage::setInstBarPerc(double ourPercent){
        mainApp->instpage->installBar->SetVisible(true);
        mainApp->instpage->installBar->SetProgress(ourPercent);
        mainApp->CallForRender();
    }

    void instPage::setProgressDetailText(const std::string& ourText){
        mainApp->instpage->progressDetailText->SetText(ourText);
        mainApp->instpage->progressDetailText->SetX((1280 - mainApp->instpage->progressDetailText->GetTextWidth()) / 2);
        mainApp->instpage->progressDetailText->SetVisible(true);
        mainApp->CallForRender();
    }

    void instPage::clearProgressDetailText(){
        mainApp->instpage->progressDetailText->SetVisible(false);
        mainApp->CallForRender();
    }

    void instPage::setInstallIconFromTitleId(u64 titleId){
        if (titleId == 0) {
            return;
        }

        NsApplicationControlData appControlData{};
        size_t sizeRead = 0;
        Result rc = nsGetApplicationControlData(NsApplicationControlSource_Storage, titleId, &appControlData, sizeof(NsApplicationControlData), &sizeRead);
        if (R_FAILED(rc) || sizeRead <= sizeof(appControlData.nacp)) {
            return;
        }

        const size_t iconSize = sizeRead - sizeof(appControlData.nacp);
        if (iconSize == 0) {
            return;
        }

        mainApp->instpage->installIconImage->SetJpegImage(appControlData.icon, iconSize);
        mainApp->instpage->installIconImage->SetVisible(true);
        mainApp->instpage->awooImage->SetVisible(false);
        mainApp->CallForRender();
    }

    void instPage::setInstallIcon(const std::string& imagePath){
        if (imagePath.empty()) {
            clearInstallIcon();
            return;
        }
        mainApp->instpage->installIconImage->SetImage(imagePath);
        mainApp->instpage->installIconImage->SetX(kInstallIconX);
        mainApp->instpage->installIconImage->SetY(kInstallIconY);
        mainApp->instpage->installIconImage->SetWidth(kInstallIconSize);
        mainApp->instpage->installIconImage->SetHeight(kInstallIconSize);
        mainApp->instpage->installIconImage->SetVisible(true);
        mainApp->instpage->awooImage->SetVisible(false);
        mainApp->CallForRender();
    }

    void instPage::setInstallIconData(const void* imageData, std::uint32_t imageSize){
        if (imageData == nullptr || imageSize == 0) {
            clearInstallIcon();
            return;
        }
        mainApp->instpage->installIconImage->SetJpegImage(const_cast<void*>(imageData), static_cast<s32>(imageSize));
        mainApp->instpage->installIconImage->SetX(kInstallIconX);
        mainApp->instpage->installIconImage->SetY(kInstallIconY);
        mainApp->instpage->installIconImage->SetWidth(kInstallIconSize);
        mainApp->instpage->installIconImage->SetHeight(kInstallIconSize);
        mainApp->instpage->installIconImage->SetVisible(true);
        mainApp->instpage->awooImage->SetVisible(false);
        mainApp->CallForRender();
    }

    void instPage::clearInstallIcon(){
        mainApp->instpage->installIconImage->SetVisible(false);
        if (!inst::config::gayMode)
            mainApp->instpage->awooImage->SetVisible(true);
        mainApp->CallForRender();
    }

    void instPage::loadMainMenu(){
        StopInstallDimSession();
        if (lastLayoutBeforeInstall != nullptr && lastLayoutBeforeInstall != mainApp->instpage)
            mainApp->LoadLayout(lastLayoutBeforeInstall);
        else
            mainApp->LoadLayout(mainApp->mainPage);
    }

    void instPage::loadInstallScreen(){
        auto currentLayout = mainApp->GetCurrentLayout();
        if (currentLayout != nullptr && currentLayout != mainApp->instpage)
            lastLayoutBeforeInstall = currentLayout;
        mainApp->instpage->pageInfoText->SetText("");
        mainApp->instpage->installInfoText->SetText("");
        mainApp->instpage->installBar->SetProgress(0);
        mainApp->instpage->installBar->SetVisible(false);
        mainApp->instpage->hintText->SetText(" Cancel");
        mainApp->instpage->hintText->SetX(1280 - 10 - mainApp->instpage->hintText->GetTextWidth());
        mainApp->instpage->hintText->SetVisible(true);
        mainApp->instpage->bottomHintSegments = BuildBottomHintSegments(mainApp->instpage->hintText->GetText(), mainApp->instpage->hintText->GetX(), 20);
        mainApp->instpage->progressText->SetVisible(false);
        mainApp->instpage->progressDetailText->SetVisible(false);
        mainApp->instpage->installIconImage->SetVisible(false);
        mainApp->instpage->awooImage->SetVisible(!inst::config::gayMode);
        mainApp->instpage->touchWakeActive = false;
        g_installCancelRequested.store(false);
        StartInstallDimSession();
        mainApp->LoadLayout(mainApp->instpage);
        mainApp->CallForRender();
    }

    void instPage::requestInstallCancel(){
        g_installCancelRequested.store(true);
    }

    bool instPage::isInstallCancelRequested(){
        return g_installCancelRequested.load();
    }

    void instPage::clearInstallCancel(){
        g_installCancelRequested.store(false);
    }

    void instPage::onInput(u64 Down, u64 Up, u64 Held, pu::ui::Touch Pos) {
        if (!Pos.IsEmpty()) {
            if (!this->touchWakeActive) {
                this->touchWakeActive = true;
                NotifyInstallTouchActivity();
            }
        } else {
            this->touchWakeActive = false;
        }

        int bottomTapX = 0;
        if (DetectBottomHintTap(Pos, this->bottomHintTouch, 668, 52, bottomTapX)) {
            Down |= FindBottomHintButton(this->bottomHintSegments, bottomTapX);
        }
        inst::util::playNavigationClickIfNeeded(Down);
        if (Down & HidNpadButton_B) {
            const bool mtpWaitingAfterComplete =
                inst::mtp::IsInstallServerRunning() && !inst::mtp::IsStreamInstallActive() && this->hintText->IsVisible();

            if (this->installBar->IsVisible() && !mtpWaitingAfterComplete) {
                if (isInstallCancelRequested()) {
                    setInstInfoText("Cancelling install...");
                    return;
                }
                const int choice = mainApp->CreateShowDialog(
                    "Cancel install?",
                    "Stop the current install and clean up partial data?",
                    {"Cancel Install", "Go Back"},
                    false
                );
                if (choice == 0) {
                    requestInstallCancel();
                    setInstInfoText("Cancelling install...");
                }
                return;
            }
            if (!this->hintText->IsVisible()) {
                requestInstallCancel();
                setInstInfoText("Cancelling install...");
                return;
            }
            if (inst::mtp::IsInstallServerRunning()) {
                inst::mtp::StopInstallServer();
            }
            if (this->hintText->IsVisible()) {
                loadMainMenu();
            }
        }
    }
}

