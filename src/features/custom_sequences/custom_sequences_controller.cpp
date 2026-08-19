#include "custom_sequences_controller.h"

#include <QFileDialog>
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTreeView>

#include <algorithm>
#include <cctype>

#include "../../app/file_preview.h"
#include "../../app/main_window.h"
#include "../../core/path_utils.h"

namespace bitdeck {

namespace {

constexpr int kAbsolutePathRole = Qt::UserRole + 1;

bool isSeqFile(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".seq";
}

std::filesystem::path metaPathFor(const std::filesystem::path& seqPath) {
    std::filesystem::path metaPath = seqPath;
    metaPath.replace_extension(".meta");
    return metaPath;
}

// scanFolder() only knows about individual files -- the .seq/.meta pairing
// requirement (skip a .seq with no matching .meta, matching Retro) is
// applied here, on top of its generic scan+hash+dedup result.
std::vector<ScannedFile> scanPairedSequences(const std::filesystem::path& folder,
                                              const std::map<std::string, std::string>& knownHashes) {
    std::vector<ScannedFile> paired;
    for (auto& file : scanFolder(folder, knownHashes, isSeqFile)) {
        if (std::filesystem::exists(metaPathFor(file.absolutePath))) {
            paired.push_back(std::move(file));
        }
    }
    return paired;
}

QString statusText(FileStageStatus status) {
    switch (status) {
        case FileStageStatus::New: return QObject::tr("New");
        case FileStageStatus::Modified: return QObject::tr("Modified");
        case FileStageStatus::UpToDate: return QObject::tr("Up to date");
    }
    return QString();
}

} // namespace

CustomSequencesController::CustomSequencesController(MainWindow& window) : ModeController(window) {
    // A successful generate clears the shared StagingModel, but this
    // controller's own dedup cache doesn't know that -- without this, an
    // unchanged-since-last-stage sequence would look "already staged" on
    // the next rescan and never get re-added, even though the staging
    // model is actually empty again.
    connect(&window_.stagingModel(), &StagingModel::generationFinished, this, [this] { stagedHashes_.clear(); });
}

QWidget* CustomSequencesController::treeWidget() {
    if (treeView_ != nullptr) {
        return treeView_;
    }
    treeView_ = new QTreeView();
    return treeView_;
}

QWidget* CustomSequencesController::contentWidget() {
    if (contentStack_ != nullptr) {
        return contentStack_;
    }

    contentTable_ = new QTableWidget(0, 2);
    contentTable_->setHorizontalHeaderLabels({QObject::tr("Sequence"), QObject::tr("Status")});
    contentTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    contentTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    contentTable_->verticalHeader()->setVisible(false);
    contentTable_->setEditTriggers(QTableWidget::NoEditTriggers);
    contentTable_->setSelectionMode(QTableWidget::SingleSelection);
    contentTable_->setSelectionBehavior(QTableWidget::SelectRows);
    connect(contentTable_, &QTableWidget::itemSelectionChanged, this, &CustomSequencesController::onContentRowSelected);

    preview_ = new FilePreview();
    connect(preview_, &FilePreview::closeRequested, this,
            [this] { contentStack_->setCurrentWidget(contentTable_); });

    contentStack_ = new QStackedWidget();
    contentStack_->addWidget(contentTable_);
    contentStack_->addWidget(preview_);
    return contentStack_;
}

QWidget* CustomSequencesController::statusBarWidget() {
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

void CustomSequencesController::onOpenRequested() {
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

void CustomSequencesController::rescanAndStage() {
    lastScan_ = scanPairedSequences(selectedFolder_, stagedHashes_);

    std::vector<std::pair<std::filesystem::path, std::filesystem::path>> pendingPairs;
    for (const auto& file : lastScan_) {
        if (file.status == FileStageStatus::UpToDate) {
            continue;
        }
        pendingPairs.emplace_back(file.absolutePath, metaPathFor(file.absolutePath));
        stagedHashes_[file.relativeKey] = file.contentHash;
    }
    if (!pendingPairs.empty()) {
        window_.stagingModel().addCustomSequenceEntry(pendingPairs, kArchiveKey);
        lastScan_ = scanPairedSequences(selectedFolder_, stagedHashes_);
    }
    refreshContentForSelectedFolder();
}

void CustomSequencesController::onTreeSelectionChanged(const QModelIndex& current) {
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

void CustomSequencesController::onContentRowSelected() {
    auto selected = contentTable_->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        return;
    }
    QTableWidgetItem* item = contentTable_->item(selected.first().row(), 0);
    if (item == nullptr) {
        return;
    }
    // Previews the .meta sidecar rather than the (binary, unpreviewable)
    // .seq itself -- that's the human-readable half of the pair.
    preview_->showFile(std::filesystem::path(item->data(kAbsolutePathRole).toString().toStdString()));
    contentStack_->setCurrentWidget(preview_);
}

void CustomSequencesController::refreshContentForSelectedFolder() {
    contentStack_->setCurrentWidget(contentTable_);
    contentTable_->setSortingEnabled(false); // avoid resorting mid-population, corrupting row indices below
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
        QString stem = QString::fromStdString(std::filesystem::path(file.relativeKey).stem().string());
        auto* nameItem = new QTableWidgetItem(stem);
        nameItem->setData(kAbsolutePathRole, QString::fromStdString(metaPathFor(file.absolutePath).string()));
        contentTable_->setItem(row, 0, nameItem);
        contentTable_->setItem(row, 1, new QTableWidgetItem(statusText(file.status)));
        ++row;
    }

    contentTable_->setSortingEnabled(true);
    fileCountLabel_->setText(QObject::tr("%1 sequence(s) in this folder").arg(row));
}

} // namespace bitdeck
