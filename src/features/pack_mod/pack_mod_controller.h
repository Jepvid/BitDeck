#pragma once

#include <QObject>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "../../app/mode_controller.h"
#include "../../core/folder_scan.h"

class QFileSystemModel;
class QLabel;
class QModelIndex;
class QStackedWidget;
class QTableWidget;
class QTreeView;

namespace bitdeck {

class FilePreview;

// "Pack Mod" mode: open a folder, browse its subfolder tree; every file
// found is staged into the shared StagingModel immediately (under its
// directory-relative archive path), no separate stage step. Preserves
// RetroPlus's SHA-256 dedup: reopening the same folder later only
// (re-)stages files whose content changed since the last scan.
class PackModController : public ModeController {
    Q_OBJECT

public:
    explicit PackModController(MainWindow& window);

    QString name() const override { return QStringLiteral("Pack Mod"); }
    QWidget* treeWidget() override;
    QWidget* contentWidget() override;
    QWidget* statusBarWidget() override;
    void onOpenRequested() override;
    void rescanBeforeFinalize() override { rescanAndStage(); }

private:
    void rescanAndStage();
    void refreshContentForSelectedFolder();
    void onTreeSelectionChanged(const QModelIndex& current);
    void onContentRowSelected();

    std::filesystem::path selectedFolder_;
    std::map<std::string, std::string> stagedHashes_; // relativeKey -> sha256
    std::vector<ScannedFile> lastScan_;

    QFileSystemModel* fsModel_ = nullptr;
    QTreeView* treeView_ = nullptr;

    QStackedWidget* contentStack_ = nullptr;
    QTableWidget* contentTable_ = nullptr;
    FilePreview* preview_ = nullptr;

    QWidget* statusBar_ = nullptr;
    QLabel* fileCountLabel_ = nullptr;
};

} // namespace bitdeck
