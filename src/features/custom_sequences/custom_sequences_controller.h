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

// "Custom Sequences" mode: open a folder, browse its subfolder tree; every
// .seq file with a matching same-stem .meta sibling is staged immediately
// under the fixed "custom/music" archive path (a .seq missing its .meta is
// skipped, matching Retro -- no per-directory grouping here, unlike Pack
// Mod). Preserves SHA-256 dedup: reopening the same folder only (re-)stages
// sequences whose content actually changed.
class CustomSequencesController : public ModeController {
    Q_OBJECT

public:
    explicit CustomSequencesController(MainWindow& window);

    QString name() const override { return QStringLiteral("Custom Sequences"); }
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

    static constexpr const char* kArchiveKey = "custom/music";

    std::filesystem::path selectedFolder_;
    std::map<std::string, std::string> stagedHashes_; // relativeKey (.seq) -> sha256
    std::vector<ScannedFile> lastScan_;               // .seq files with a matching .meta only

    QFileSystemModel* fsModel_ = nullptr;
    QTreeView* treeView_ = nullptr;

    QStackedWidget* contentStack_ = nullptr;
    QTableWidget* contentTable_ = nullptr;
    FilePreview* preview_ = nullptr;

    QWidget* statusBar_ = nullptr;
    QLabel* fileCountLabel_ = nullptr;
};

} // namespace bitdeck
