#pragma once
#include <pu/Plutonium>
#include "ui/bottomHint.hpp"
#include "util/config.hpp"
#include "identity/identity.hpp"

using namespace pu::ui::elm;
namespace inst::ui {
    class optionsPage : public pu::ui::Layout
    {
        public:
            optionsPage();
            PU_SMART_CTOR(optionsPage)
            void onInput(u64 Down, u64 Up, u64 Held, pu::ui::Touch Pos);
            static void askToUpdate(std::vector<std::string> updateInfo);
            Image::Ref titleImage;
            TextBlock::Ref appVersionText;
            TextBlock::Ref timeText;
            TextBlock::Ref ipText;
            TextBlock::Ref sysLabelText;
            TextBlock::Ref sysFreeText;
            TextBlock::Ref sdLabelText;
            TextBlock::Ref sdFreeText;
            Rectangle::Ref sysBarBack;
            Rectangle::Ref sysBarFill;
            Rectangle::Ref sdBarBack;
            Rectangle::Ref sdBarFill;
            Rectangle::Ref netIndicator;
            Rectangle::Ref wifiBar1;
            Rectangle::Ref wifiBar2;
            Rectangle::Ref wifiBar3;
            Rectangle::Ref batteryOutline;
            Rectangle::Ref batteryFill;
            Rectangle::Ref batteryCap;
        private:
            BottomHintTouchState bottomHintTouch;
            std::vector<BottomHintSegment> bottomHintSegments;
            TextBlock::Ref butText;
            Rectangle::Ref topRect;
            Rectangle::Ref infoRect;
            Rectangle::Ref botRect;
            Rectangle::Ref sideNavRect;
            TextBlock::Ref pageInfoText;
            pu::ui::elm::Menu::Ref menu;
            bool touchActive = false;
            bool touchMoved = false;
            int touchStartX = 0;
            int touchStartY = 0;
            int touchRegion = 0;
            int selectedSection = 0;
            bool tabsFocused = false;
            bool remoteListVisible = false;
            bool remoteFormVisible = false;
            bool remoteProtocolDropdownVisible = false;
            bool remoteModeDropdownVisible = false;
            bool remoteFormWasActive = false;
            bool remoteFormReturnToList = false;
            int lockedMenuIndex = 0;
            int remoteListIndex = 0;
            int remoteFormReturnIndex = 0;
            inst::config::RemoteProfile remoteFormProfile;
            std::vector<int> sectionMenuIndices;
            std::vector<inst::config::RemoteProfile> remoteListProfiles;
            std::vector<TextBlock::Ref> sectionTexts;
            std::vector<Rectangle::Ref> sectionHighlights;
            Rectangle::Ref remoteFormShade;
            Rectangle::Ref remoteFormBorder;
            Rectangle::Ref remoteFormPanel;
            Rectangle::Ref remoteProtocolDropdownPanel;
            TextBlock::Ref remoteFormTitle;
            TextBlock::Ref remoteFormHint;
            pu::ui::elm::Menu::Ref remoteFormMenu;
            pu::ui::elm::Menu::Ref remoteProtocolDropdownMenu;
            void setSectionNavText();
            void setSettingsMenuText();
            void refreshOptions(bool resetSelection = false);
            void openRemoteList(int selectedIndex = 0);
            void closeRemoteList();
            void manageRemote(const inst::config::RemoteProfile& remote);
            void openRemoteForm(const inst::config::RemoteProfile& remote, bool wasActive, bool returnToList, int returnIndex = 0);
            void closeRemoteForm();
            void refreshRemoteForm();
            void editRemoteFormField();
            void saveRemoteForm();
            void managePersona(const inst::identity::Persona& persona);
            void createPersona();
            void exportDiagnosticReport();
            void showIdentityDiagnostics();
            void rememberCurrentSectionMenuIndex();
            void restoreSelectedSectionMenuIndex();
            void setSelectedSectionAndRefresh(int newSection);
            int getSectionFromTouch(int x, int y) const;
            std::string getMenuOptionIcon(bool ourBool);
            std::string getMenuLanguage(int ourLangCode);
    };
}
