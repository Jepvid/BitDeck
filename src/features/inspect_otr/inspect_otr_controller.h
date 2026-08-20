#pragma once

#include <QObject>

#include <string>

#include "../../app/archive_tree_view.h"
#include "../../app/mode_controller.h"

class QLabel;
class QModelIndex;
class QStackedWidget;
class QStandardItemModel;
class QTableWidget;
class QTreeView;

namespace bitdeck {

class FilePreview;

// "Inspect OTR" mode: read-only browsing of a single .otr/.o2r archive.
// Open a file; tree = the archive's folder structure (via
// path_tree/archive_tree_view); content = the selected folder's files, with
// a FilePreview for a selected entry (via a scratch temp file). The top
// bar's primary action is "Extract OTR/O2R" (RetroPlus's "Extract All",
// zip-slip-guarded via safeExtractionPath) in place of Finalize Mod.
class InspectOtrController : public ModeController {
    Q_OBJECT

public:
    explicit InspectOtrController(MainWindow& window);

    QString name() const override { return QStringLiteral("Inspect OTR"); }
    QString openButtonLabel() const override { return QObject::tr("Open OTR/O2R..."); }
    QString primaryActionLabel() const override { return QObject::tr("Extract OTR/O2R"); }
    bool usesFinalizeFlow() const override { return false; }
    QWidget* treeWidget() override;
    QWidget* contentWidget() override;
    QWidget* statusBarWidget() override;
    void onOpenRequested() override;
    void onPrimaryActionRequested() override;

private:
    void onTreeSelectionChanged(const QModelIndex& current);
    void onContentRowSelected();
    void navigateToChildFolder(const QString& name);
    void refreshContentForSelectedFolder();
    void previewArchiveEntry(const std::string& entryName);

    std::string selectedArchivePath_;
    PathTreeNode archiveTree_;
    const PathTreeNode* selectedNode_ = nullptr;

    QStandardItemModel* treeModel_ = nullptr;
    QTreeView* treeView_ = nullptr;

    QStackedWidget* contentStack_ = nullptr;
    QTableWidget* contentTable_ = nullptr;
    FilePreview* preview_ = nullptr;

    QWidget* statusBar_ = nullptr;
    QLabel* fileCountLabel_ = nullptr;
    QLabel* extractStatusLabel_ = nullptr;
};

} // namespace bitdeck
