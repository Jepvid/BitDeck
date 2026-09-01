#pragma once

#include <QObject>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "../../app/mode_controller.h"
#include "../../app/staging_model.h"
#include "../../core/folder_scan.h"

class QFileSystemModel;
class QLabel;
class QModelIndex;
class QStackedWidget;
class QTableWidget;
class QTreeView;

namespace bitdeck {

class FilePreview;

// "Custom Files" mode: open a folder, browse its subfolder tree; every file
// found is staged into this mode's own StagingModel immediately (under its
// directory-relative archive path), no separate stage step, no
// texture-specific matching -- files are staged as-is. Preserves
// RetroPlus's SHA-256 dedup: reopening the same folder later only
// (re-)stages files whose content changed since the last scan.
class PackModController : public ModeController {
    Q_OBJECT

public:
    explicit PackModController(MainWindow& window);

    QString name() const override { return QStringLiteral("Custom Files"); }
    QWidget* treeWidget() override;
    QWidget* contentWidget() override;
    QWidget* statusBarWidget() override;
    void onOpenRequested() override;
    QString primaryActionLabel() const override { return QObject::tr("Export Mod"); }
    void onPrimaryActionRequested() override;

private:
    void rescanAndStage();
    void refreshContentForSelectedFolder();
    void onTreeSelectionChanged(const QModelIndex& current);
    void onContentRowSelected();

    StagingModel stagingModel_;
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
