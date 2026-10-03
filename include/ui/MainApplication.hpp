#pragma once
#include <pu/Plutonium>
#include <switch.h>
#include "ui/input_gate.hpp"
#include "ui/mainPage.hpp"
#include "ui/netInstPage.hpp"
#include "ui/remoteInstPage.hpp"
#include "ui/sdInstPage.hpp"
#include "ui/usbInstPage.hpp"
#include "ui/hddInstPage.hpp"
#include "ui/instPage.hpp"
#include "ui/optionsPage.hpp"

namespace inst::ui {
    class MainApplication;
    extern MainApplication *mainApp;

    class MainApplication : public pu::ui::Application {
        public:
            using Application::Application;
            PU_SMART_CTOR(MainApplication)
            void Close();
            void ConfirmExit();
            void SuppressInput();
            void LoadLayout(std::shared_ptr<pu::ui::Layout> layout);
            int CreateShowDialog(const std::string& title, const std::string& content, std::vector<std::string> options, bool lastCancel, const std::string& icon = "");
            void BindInput(pu::ui::Layout::Ref layout, std::function<void(u64, u64, u64, pu::ui::Touch)> callback);
            void OnLoad() override;
            void RefreshInputDevice(bool force = false);
            pu::ui::Layout::Ref GetCurrentLayout() const { return this->lyt; }
            MainPage::Ref mainPage;
            netInstPage::Ref netinstPage;
            remoteInstPage::Ref remoteinstPage;
            sdInstPage::Ref sdinstPage;
            usbInstPage::Ref usbinstPage;
            hddInstPage::Ref hddinstPage;
            instPage::Ref instpage;
            optionsPage::Ref optionspage;
        private:
            InputGate inputGate;
            bool exitDialogOpen = false;
            AppletFocusState lastFocusState = AppletFocusState_InFocus;
    };
}
