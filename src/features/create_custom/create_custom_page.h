#pragma once

#include <QWidget>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "../../app/page_frame.h"

class QLabel;
class QListWidget;
class QCheckBox;
class QPushButton;

namespace bitdeck {

class MainWindow;

// Folder -> stage files, port of Retro's Create Custom screen. Includes
// RetroPlus's "keep folder open" rescan-in-place flow: re-scanning the same
// folder skips files whose SHA-256 content hash matches what was already
// staged, so only new/changed files get added on subsequent scans.
class CreateCustomPage : public PageFrame {
    Q_OBJECT

public:
    explicit CreateCustomPage(MainWindow& window);

private:
    void onSelectFolder();
    void scanFolder();
    void onStage();
    void updateStageButtonState();
    void resetToIdle();

    MainWindow& window_;
    std::filesystem::path selectedFolder_;

    // Populated by scanFolder(): files not yet staged, grouped by their
    // directory relative to selectedFolder_ (the StagingModel entry key).
    std::map<std::string, std::vector<std::filesystem::path>> pendingFilesByDir_;
    // relative "dir/filename" -> sha256 hex, for files found in the current
    // scan; committed into stagedHashes_ once actually staged.
    std::map<std::string, std::string> pendingHashes_;
    // relative "dir/filename" -> sha256 hex, for files already staged in a
    // prior scan+stage cycle (only used while "keep folder open" is on).
    std::map<std::string, std::string> stagedHashes_;

    QLabel* pathLabel_;
    QLabel* fileCountLabel_;
    QListWidget* filesList_;
    QCheckBox* keepOpenCheckbox_;
    QPushButton* selectButton_;
    QPushButton* stageButton_;
};

QWidget* makeCreateCustomPage(MainWindow& window);

} // namespace bitdeck
