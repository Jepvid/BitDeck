#include "pack_mod_controller.h"

#include <QFileDialog>
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTreeView>

#include "../../app/file_preview.h"
#include "../../app/main_window.h"
#include "../../core/path_utils.h"

namespace bitdeck {

namespace {

constexpr int kAbsolutePathRole = Qt::UserRole + 1;

QString statusText(FileStageStatus status) {
    switch (status) {
        case FileStageStatus::New: return QObject::tr("New");
        case FileStageStatus::Modified: return QObject::tr("Modified");
        case FileStageStatus::UpToDate: return QObject::tr("Up to date");
    }
    return QString();
}

} // namespace

PackModController::PackModController(MainWindow& window) : ModeController(window) {}

QWidget* PackModController::treeWidget() {
    if (treeView_ != nullptr) {
        return treeView_;
    }
    treeView_ = new QTreeView();
    return treeView_;
}

QWidget* PackModController::contentWidget() {
    if (contentStack_ != nullptr) {
        return contentStack_;
    }

    contentTable_ = new QTableWidget(0, 2);
    contentTable_->setHorizontalHeaderLabels({QObject::tr("File"), QObject::tr("Status")});
    contentTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    contentTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    contentTable_->verticalHeader()->setVisible(false);
    contentTable_->setEditTriggers(QTableWidget::NoEditTriggers);
    contentTable_->setSelectionMode(QTableWidget::SingleSelection);
    contentTable_->setSelectionBehavior(QTableWidget::SelectRows);
    connect(contentTable_, &QTableWidget::itemSelectionChanged, this, &PackModController::onContentRowSelected);

    preview_ = new FilePreview();

    contentStack_ = new QStackedWidget();
    contentStack_->addWidget(contentTable_);
    contentStack_->addWidget(preview_);
    return contentStack_;
}

QWidget* PackModController::statusBarWidget() {
    if (statusBar_ != nullptr) {
        return statusBar_;
    }
    statusBar_ = new QWidget();
    auto* layout = new QHBoxLayout(statusBar_);
    layout->setContentsMargins(8, 4, 8, 4);

    fileCountLabel_ = new QLabel();
    layout->addWidget(fileCountLabel_);

    layout->addStretch(1);

    return statusBar_;
}

void PackModController::onOpenRequested() {
    QString directory = QFileDialog::getExistingDirectory(treeWidget(), QObject::tr("Select Folder"));
    if (directory.isEmpty()) {
        return;
    }
    selectedFolder_ = std::filesystem::path(directory.toStdString());
    stagedHashes_.clear();

    if (fsModel_ == nullptr) {
        fsModel_ = new QFileSystemModel(treeView_);
        treeView_->setModel(fsModel_);
        connect(treeView_->selectionModel(), &QItemSelectionModel::currentChanged, this,
                [this](const QModelIndex& current, const QModelIndex&) { onTreeSelectionChanged(current); });

        // Only the folder name matters for browsing here -- QFileSystemModel's
        // Size/Type/Date columns just crowd out the narrow sidebar. Column
        // count only exists once a model is attached, so this must happen
        // here, not in treeWidget().
        treeView_->hideColumn(1);
        treeView_->hideColumn(2);
        treeView_->hideColumn(3);
        treeView_->header()->setStretchLastSection(false);
        treeView_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    }
    fsModel_->setRootPath(directory);
    QModelIndex rootIndex = fsModel_->index(directory);
    treeView_->setRootIndex(rootIndex);
    treeView_->setCurrentIndex(rootIndex);

    rescanAndStage();
}

void PackModController::rescanAndStage() {
    lastScan_ = scanFolder(selectedFolder_, stagedHashes_);

    // Opening a folder here means the user wants it in the finalized mod --
    // stage every changed file immediately, no separate confirmation step.
    std::map<std::string, std::vector<std::filesystem::path>> filesByDir;
    for (const auto& file : lastScan_) {
        if (file.status == FileStageStatus::UpToDate) {
            continue;
        }
        filesByDir[file.dirKey].push_back(file.absolutePath);
        stagedHashes_[file.relativeKey] = file.contentHash;
    }
    for (const auto& [dirKey, files] : filesByDir) {
        window_.stagingModel().addCustomStageEntries(files, dirKey);
    }

    // Every staged file now matches its own hash -- rescan so the content
    // pane reflects that (everything shows Up to date until the folder's
    // contents actually change).
    if (!filesByDir.empty()) {
        lastScan_ = scanFolder(selectedFolder_, stagedHashes_);
    }
    refreshContentForSelectedFolder();
}

void PackModController::onTreeSelectionChanged(const QModelIndex& current) {
    if (!current.isValid()) {
        return;
    }
    if (fsModel_->isDir(current)) {
        refreshContentForSelectedFolder();
        return;
    }
    preview_->showFile(std::filesystem::path(fsModel_->filePath(current).toStdString()));
    contentStack_->setCurrentWidget(preview_);
}

void PackModController::onContentRowSelected() {
    auto selected = contentTable_->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        return;
    }
    QTableWidgetItem* item = contentTable_->item(selected.first().row(), 0);
    if (item == nullptr) {
        return;
    }
    preview_->showFile(std::filesystem::path(item->data(kAbsolutePathRole).toString().toStdString()));
    contentStack_->setCurrentWidget(preview_);
}

void PackModController::refreshContentForSelectedFolder() {
    contentStack_->setCurrentWidget(contentTable_);
    contentTable_->setRowCount(0);

    std::string dirKey;
    QModelIndex current = treeView_->currentIndex();
    if (current.isValid()) {
        std::filesystem::path absolute(fsModel_->filePath(current).toStdString());
        std::filesystem::path relative = std::filesystem::relative(absolute, selectedFolder_);
        dirKey = normalizePath(relative.string());
        if (dirKey == ".") {
            dirKey.clear();
        }
    }

    int row = 0;
    for (const auto& file : lastScan_) {
        if (file.dirKey != dirKey) {
            continue;
        }
        contentTable_->insertRow(row);
        QString baseName = QString::fromStdString(std::filesystem::path(file.relativeKey).filename().string());
        auto* nameItem = new QTableWidgetItem(baseName);
        nameItem->setData(kAbsolutePathRole, QString::fromStdString(file.absolutePath.string()));
        contentTable_->setItem(row, 0, nameItem);
        contentTable_->setItem(row, 1, new QTableWidgetItem(statusText(file.status)));
        ++row;
    }

    fileCountLabel_->setText(QObject::tr("%1 file(s) in this folder").arg(row));
}

} // namespace bitdeck
