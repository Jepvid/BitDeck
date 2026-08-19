#pragma once

#include <QObject>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "../../app/mode_controller.h"
#include "../../core/folder_scan.h"
#include "../../games/game_texture_conventions.h"

class QCheckBox;
class QFileSystemModel;
class QLabel;
class QModelIndex;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QTreeView;

namespace bitdeck {

class FilePreview;

// "Make Texture Pack" mode: a single folder-backed tree/content pane, same
// auto-stage-on-open/SHA-256-dedup shape as Pack Mod.
//  - Open Folder...: a folder of replacement images matched against
//    manifest.json (+ optional aliases.json), staged as a
//    CustomTexturesEntry. One source image can fan out to several target
//    archive paths via aliases.json, so content rows are keyed by the
//    resolved target rather than 1:1 with source files. Every matched
//    texture is shown, with status comparing its current hash against
//    manifest.json's recorded baseline hash; only edited images get staged.
//  - "Extract textures from otr/o2r" (status bar button): picks one or more
//    .otr/.o2r files and an output folder, runs extractTexturesToFolder()
//    (decodes every Texture/Background resource to PNG/JPG + a fresh
//    manifest.json, with Apply TLUT recovering real palette colors for
//    CI4/CI8 textures), then opens that folder into the same tree/content
//    pane as "Open Folder...".
class MakeTexturePackController : public ModeController {
    Q_OBJECT

public:
    explicit MakeTexturePackController(MainWindow& window);

    QString name() const override { return QStringLiteral("Make Texture Pack"); }
    QWidget* treeWidget() override;
    QWidget* contentWidget() override;
    QWidget* statusBarWidget() override;
    void onOpenRequested() override;
    void rescanBeforeFinalize() override { rescanAndStage(); }

private:
    struct PendingTexture {
        std::filesystem::path sourcePath;
        std::string sourceDirKey; // source-folder-relative parent dir, for tree scoping
        std::string targetName;   // resolved target archive path
        TextureManifestEntry manifestEntry;
        std::string sourceHash;
        FileStageStatus status;
    };

    void openFolder(const std::filesystem::path& folder);
    void rescanAndStage();
    void refreshContentForSelectedFolder();
    void onTreeSelectionChanged(const QModelIndex& current);
    void onContentRowSelected();
    void onExtractTexturesClicked();

    std::filesystem::path selectedFolder_;
    std::map<std::string, std::string> stagedHashes_; // target archive path -> sha256
    std::vector<PendingTexture> lastScan_;

    QFileSystemModel* fsModel_ = nullptr;
    QTreeView* treeView_ = nullptr;

    QStackedWidget* contentStack_ = nullptr;
    QTableWidget* contentTable_ = nullptr;
    FilePreview* preview_ = nullptr;

    QWidget* statusBar_ = nullptr;
    QLabel* fileCountLabel_ = nullptr;
    QCheckBox* applyTlutCheckbox_ = nullptr;
    QPushButton* extractButton_ = nullptr;
};

} // namespace bitdeck
