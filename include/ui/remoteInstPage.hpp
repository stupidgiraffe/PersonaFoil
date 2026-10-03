#pragma once

#include <pu/Plutonium>
#include "remoteInstall.hpp"
#include "ui/bottomHint.hpp"
#include "util/save_sync.hpp"
#include <atomic>
#include <cstddef>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace pu::ui::elm;
namespace inst::ui {
    class remoteInstPage : public pu::ui::Layout
    {
        public:
            remoteInstPage();
            ~remoteInstPage();
            void stopIconDownloadWorker();
            PU_SMART_CTOR(remoteInstPage)
            void startRemote(bool forceRefresh = false);
            void startInstall();
            void onInput(u64 Down, u64 Up, u64 Held, pu::ui::Touch Pos);
            TextBlock::Ref pageInfoText;
            TextBlock::Ref loadingProgressText;
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
            enum class BrowseSortMode {
                Default,
                DateDesc,
                NameAsc
            };
            std::vector<remoteInstStuff::RemoteSection> remoteSections;
            std::vector<remoteInstStuff::RemoteItem> selectedItems;
            std::vector<remoteInstStuff::RemoteItem> visibleItems;
            std::vector<remoteInstStuff::RemoteItem> availableUpdates;
            std::vector<inst::save_sync::SaveSyncEntry> saveSyncEntries;
            struct InstalledSnapshot {
                bool ready = false;
                bool installedSectionBuilt = false;
                std::unordered_map<std::uint64_t, bool> baseInstalled;
                std::unordered_map<std::uint64_t, std::uint32_t> installedUpdateVersion;
                std::unordered_set<std::uint64_t> installedDlcIds;
                std::vector<std::uint64_t> installedBaseIds;
            };
            InstalledSnapshot installedSnapshot;
            bool nativeUpdatesSectionPresent = false;
            bool nativeDlcSectionPresent = false;
            bool saveSyncEnabled = false;
            bool saveSyncLoaded = false;
            bool pendingMotdFetch = false;
            bool suppressBottomHints = false;
            bool cheatInstallProgressVisible = false;
            std::string activeRemoteUrl;
            inst::remote::AggregateReport catalogCacheReport;
            inst::remote::Capabilities catalogCacheCapabilities;
            bool catalogCacheValid = false;
            bool catalogCacheUsedLegacyFallback = false;
            std::string catalogCacheKey;
            std::vector<remoteInstStuff::RemoteSection> catalogCacheSections;
            BrowseSortMode browseSortMode = BrowseSortMode::Default;
            BottomHintTouchState bottomHintTouch;
            std::vector<BottomHintSegment> bottomHintSegments;
            int selectedSectionIndex = 0;
            std::string searchQuery;
            std::string previewKey;
            bool debugVisible = false;
            int allSortMode = 0;
            bool descriptionVisible = false;
            bool descriptionOverlayVisible = false;
            std::vector<std::string> descriptionOverlayLines;
            int descriptionOverlayOffset = 0;
            int descriptionOverlayVisibleLines = 16;
            struct IconDownloadRequest {
                std::uint64_t generation = 0;
                std::string key;
                std::string iconUrl;
                std::string filePath;
            };
            std::thread iconDownloadThread;
            std::mutex iconDownloadMutex;
            std::condition_variable iconDownloadCv;
            std::deque<IconDownloadRequest> iconDownloadQueue;
            std::unordered_set<std::string> iconDownloadQueuedKeys;
            std::atomic<bool> iconDownloadStopRequested{false};
            std::uint64_t iconDownloadGeneration = 0;
            std::size_t iconDownloadTotal = 0;
            std::size_t iconDownloadCompleted = 0;
            std::atomic<bool> iconDownloadUiDirty{false};
            bool saveVersionSelectorVisible = false;
            std::uint64_t saveVersionSelectorTitleId = 0;
            bool saveVersionSelectorLocalAvailable = false;
            bool saveVersionSelectorDeleteMode = false;
            int saveVersionSelectorPreviousSectionIndex = 0;
            std::string saveVersionSelectorTitleName;
            std::vector<inst::save_sync::SaveSyncRemoteVersion> saveVersionSelectorVersions;
            int gridSelectedIndex = 0;
            int gridPage = -1;
            bool remoteGridMode = false;
            int remoteGridIndex = 0;
            int remoteGridPage = -1;
            int gridHoldDirX = 0;
            int gridHoldDirY = 0;
            u64 gridHoldStartTick = 0;
            u64 gridHoldLastTick = 0;
            int holdDirection = 0;
            u64 holdStartTick = 0;
            u64 lastHoldTick = 0;
            int listMarqueeIndex = -1;
            int listVisibleTopIndex = 0;
            int listPrevSelectedIndex = -1;
            int listMarqueeOffset = 0;
            int listMarqueeMaxOffset = 0;
            std::string listMarqueeFullLabel;
            u64 listMarqueeLastTick = 0;
            u64 listMarqueePauseUntilTick = 0;
            u64 listMarqueeEndPauseUntilTick = 0;
            u64 listMarqueeSpeedRemainder = 0;
            u64 listMarqueeFadeStartTick = 0;
            int listMarqueePhase = 0;
            int listMarqueeFadeAlpha = 0;
            bool listMarqueeClipEnabled = false;
            int listMarqueeClipX = 0;
            int listMarqueeClipY = 0;
            int listMarqueeClipW = 0;
            int listMarqueeClipH = 0;
            bool touchActive = false;
            bool touchMoved = false;
            u64 imageLoadingUntilTick = 0;
            TextBlock::Ref butText;
            Rectangle::Ref loadingBarBack;
            Rectangle::Ref loadingBarFill;
            Rectangle::Ref loadingStagesBack;
            TextBlock::Ref loadingStagesText;
            Rectangle::Ref topRect;
            Rectangle::Ref infoRect;
            Rectangle::Ref botRect;
            pu::ui::elm::Menu::Ref menu;
            Image::Ref infoImage;
            Image::Ref previewImage;
            Rectangle::Ref gridHighlight;
            std::vector<Image::Ref> gridImages;
            std::vector<Rectangle::Ref> remoteGridSelectHighlights;
            std::vector<Image::Ref> remoteGridSelectIcons;
            TextBlock::Ref gridTitleText;
            TextBlock::Ref imageLoadingText;
            Rectangle::Ref listMarqueeMaskRect;
            Rectangle::Ref listMarqueeTintRect;
            pu::ui::elm::Element::Ref listMarqueeClipBegin;
            pu::ui::elm::Element::Ref listMarqueeClipEnd;
            Rectangle::Ref listMarqueeFadeRect;
            TextBlock::Ref debugText;
            TextBlock::Ref emptySectionText;
            TextBlock::Ref listMarqueeOverlayText;
            TextBlock::Ref searchInfoText;
            Rectangle::Ref descriptionRect;
            TextBlock::Ref descriptionText;
            Rectangle::Ref descriptionOverlayRect;
            TextBlock::Ref descriptionOverlayTitleText;
            TextBlock::Ref descriptionOverlayBodyText;
            TextBlock::Ref descriptionOverlayHintText;
            Rectangle::Ref saveVersionSelectorRect;
            TextBlock::Ref saveVersionSelectorTitleText;
            TextBlock::Ref saveVersionSelectorDetailText;
            TextBlock::Ref saveVersionSelectorHintText;
            pu::ui::elm::Menu::Ref saveVersionSelectorMenu;
            std::string loadingStageLabel;
            int loadingStageIndex = -1;
            int loadingSpinnerFrame = 0;
            u64 loadingSpinnerLastTick = 0;
            void centerPageInfoText();
            void refreshLoadingStagesText();
            int mapLoadingStageToIndex(const std::string& stage) const;
            void setLoadingProgressStage(const std::string& stage);
            void setLoadingProgress(int percent, bool visible);
            void applyAllSectionSort();
            std::string getAllSortModeLabel() const;
            const char* getBrowseSortLabel() const;
            void applyBrowseSort();
            void openSearchDialog();
            void openSortDialog();
            void drawMenuItems(bool clearItems);
            void refreshListSelectionIcons();
            void selectTitle(int selectedIndex);
            void updateRememberedSelection();
            void updateSectionText();
            void updateButtonsText();
            void setButtonsText(const std::string& text);
            void getListTextBounds(int& textX, int& textWidth) const;
            std::string buildListMenuLabel(const remoteInstStuff::RemoteItem& item);
            void updateListMarquee(bool force);
            void buildInstalledSection();
            void buildLegacyOwnedSections();
            void cacheAvailableUpdates();
            void filterOwnedSections();
            void updatePreview();
            void updateInstalledGrid();
            void updateRemoteGrid();
            void updateDebug();
            const std::vector<remoteInstStuff::RemoteItem>& getCurrentItems() const;
            bool isAllSection() const;
            bool isInstalledSection() const;
            bool isSaveSyncSection() const;
            bool isCheatsSection() const;
            bool isCheatCompatible(const remoteInstStuff::RemoteItem& item) const;
            void ensureSaveSyncSectionLoaded();
            void showInstalledDetails();
            void buildSaveSyncSection(const std::string& remoteUrl);
            void refreshSaveSyncSection(std::uint64_t selectedTitleId, int previousSectionIndex);
            bool openSaveVersionSelector(const inst::save_sync::SaveSyncEntry& entry, int previousSectionIndex, bool deleteMode = false);
            void closeSaveVersionSelector(bool refreshList);
            void refreshSaveVersionSelectorDetailText();
            bool handleSaveVersionSelectorInput(u64 Down, u64 Up, u64 Held, pu::ui::Touch Pos);
            void handleSaveSyncAction(int selectedIndex);
            void handleCheatAction(int selectedIndex);
            void installAllCheats();
            void showCheatInstallProgress();
            void manageLocalCheats(const remoteInstStuff::RemoteItem& item);
            void showCurrentDescriptionDialog();
            bool tryGetCurrentDescription(std::string& outTitle, std::string& outDescription) const;
            void openDescriptionOverlay();
            void closeDescriptionOverlay();
            void scrollDescriptionOverlay(int delta);
            void refreshDescriptionOverlayBody();
            void updateDescriptionPanel();
            void refreshAfterInstall();
            void resetIconDownloadState();
            void queueIconDownload(const remoteInstStuff::RemoteItem& item, const std::string& filePath);
            void refreshImageLoadingText(bool showCompleted = false);
            void iconDownloadThreadMain();
            bool buildInstalledSnapshot();
            void ensureInstalledSectionPlaceholder();
            bool ensureInstalledSectionBuilt();
    };
}
