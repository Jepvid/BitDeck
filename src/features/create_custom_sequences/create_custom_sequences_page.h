#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "../../app/page_frame.h"

class QLabel;
class QListWidget;
class QCheckBox;
class QPushButton;

namespace bitdeck {

class MainWindow;

// Folder -> .seq+.meta pairs, staged under the fixed "custom/music" archive
// path. Same rescan-in-place SHA-256 dedup pattern as Create Custom.
class CreateCustomSequencesPage : public PageFrame {
    Q_OBJECT

public:
    explicit CreateCustomSequencesPage(MainWindow& window);

private:
    void onSelectFolder();
    void scanFolder();
    void onStage();
    void resetToIdle();
    void updateStageButtonState();

    static constexpr const char* kArchiveKey = "custom/music";

    MainWindow& window_;
    std::filesystem::path selectedFolder_;

    std::vector<std::pair<std::filesystem::path, std::filesystem::path>> pendingPairs_; // (seq, meta)
    std::map<std::string, std::string> pendingHashes_; // relative .seq path -> sha256 hex
    std::map<std::string, std::string> stagedHashes_;

    QLabel* pathLabel_;
    QLabel* countLabel_;
    QListWidget* sequencesList_;
    QCheckBox* keepOpenCheckbox_;
    QPushButton* selectButton_;
    QPushButton* stageButton_;
};

QWidget* makeCreateCustomSequencesPage(MainWindow& window);

} // namespace bitdeck
