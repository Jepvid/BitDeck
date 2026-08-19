#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "../../app/page_frame.h"
#include "../../app/staging_model.h"

class QStackedWidget;
class QLabel;
class QListWidget;
class QCheckBox;
class QPushButton;

namespace bitdeck {

class MainWindow;

enum class ReplaceTexturesStep {
    Question,
    SelectFolder,
    SelectOtr,
};

// 3-step wizard: a folder of pre-organized texture replacements (matched
// against manifest.json + optional aliases.json) can be staged directly, or
// an existing .otr/.o2r can be extracted into a fresh manifest+folder pair.
class CreateReplaceTexturesPage : public PageFrame {
    Q_OBJECT

public:
    explicit CreateReplaceTexturesPage(MainWindow& window);

private:
    void showStep(ReplaceTexturesStep step);
    QWidget* buildQuestionStep();
    QWidget* buildFolderStep();
    QWidget* buildOtrStep();

    void onSelectFolder();
    void scanFolder();
    void onStageTextures();

    void onSelectOtr();
    void onProcessOtr();

    MainWindow& window_;
    QStackedWidget* stepStack_;

    // Folder step state
    std::filesystem::path selectedFolder_;
    std::map<std::string, std::vector<std::pair<std::filesystem::path, TextureManifestEntry>>> pendingTextures_;
    std::map<std::string, std::string> stagedHashes_; // target archive path -> sha256 hex

    QLabel* folderPathLabel_;
    QListWidget* folderFilesList_;
    QCheckBox* prependAltCheckbox_;
    QCheckBox* compressCheckbox_;
    QCheckBox* keepOpenCheckbox_;
    QPushButton* folderStageButton_;

    // OTR step state
    std::vector<std::string> selectedOtrPaths_;

    QLabel* otrPathLabel_;
    QListWidget* otrResultsList_;
    QPushButton* otrProcessButton_;
};

QWidget* makeCreateReplaceTexturesPage(MainWindow& window);

} // namespace bitdeck
