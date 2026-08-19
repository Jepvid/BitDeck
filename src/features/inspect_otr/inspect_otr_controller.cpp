#include "inspect_otr_controller.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QTableWidget>
#include <QTreeView>

#include <fstream>

#include "../../app/background_worker.h"
#include "../../app/file_preview.h"
#include "../../app/main_window.h"
#include "../../archive/arc.h"
#include "../../archive/extraction_safety.h"

namespace bitdeck {

namespace {

constexpr int kEntryNameRole = Qt::UserRole + 1;

// Byte-for-byte dump of every archive entry, matching RetroPlus's "Extract
// All" -- unlike texture_extraction.cpp, nothing here is decoded/converted.
void extractAllFiles(const std::string& archivePath, const std::filesystem::path& targetDir, TaskProgress& progress) {
    std::filesystem::create_directories(targetDir);
    Arc arc(archivePath);
    int total = static_cast<int>(arc.listItems().size());
    int processed = 0;

    arc.listItems([&](const std::string& fileName, const std::vector<uint8_t>& data) {
        auto safePath = safeExtractionPath(targetDir, fileName);
        if (safePath.has_value()) {
            std::filesystem::create_directories(safePath->parent_path());
            std::ofstream file(*safePath, std::ios::binary);
            file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        }
        // entries that fail the safety check are silently skipped, matching
        // RetroPlus's "skip and keep going" extraction behavior
        progress.reportProgress(++processed, total);
    });
    arc.close();
}

} // namespace

InspectOtrController::InspectOtrController(MainWindow& window) : ModeController(window) {}

QWidget* InspectOtrController::treeWidget() {
    if (treeView_ != nullptr) {
        return treeView_;
    }
    treeView_ = new QTreeView();
    treeView_->setHeaderHidden(true);
    return treeView_;
}

QWidget* InspectOtrController::contentWidget() {
    if (contentStack_ != nullptr) {
        return contentStack_;
    }

    contentTable_ = new QTableWidget(0, 1);
    contentTable_->setHorizontalHeaderLabels({QObject::tr("File")});
    contentTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    contentTable_->verticalHeader()->setVisible(false);
    contentTable_->setEditTriggers(QTableWidget::NoEditTriggers);
    contentTable_->setSelectionMode(QTableWidget::SingleSelection);
    contentTable_->setSelectionBehavior(QTableWidget::SelectRows);
    connect(contentTable_, &QTableWidget::itemSelectionChanged, this, &InspectOtrController::onContentRowSelected);

    preview_ = new FilePreview();
    connect(preview_, &FilePreview::closeRequested, this, [this] { contentStack_->setCurrentWidget(contentTable_); });

    contentStack_ = new QStackedWidget();
    contentStack_->addWidget(contentTable_);
    contentStack_->addWidget(preview_);
    return contentStack_;
}

QWidget* InspectOtrController::statusBarWidget() {
    if (statusBar_ != nullptr) {
        return statusBar_;
    }
    statusBar_ = new QWidget();
    auto* layout = new QHBoxLayout(statusBar_);
    layout->setContentsMargins(8, 4, 8, 4);

    fileCountLabel_ = new QLabel();
    layout->addWidget(fileCountLabel_);

    layout->addStretch(1);

    extractStatusLabel_ = new QLabel();
    layout->addWidget(extractStatusLabel_);

    return statusBar_;
}

void InspectOtrController::onOpenRequested() {
    QString path = QFileDialog::getOpenFileName(treeWidget(), QObject::tr("Select OTR/O2R"), QString(),
                                                  QObject::tr("Archives (*.otr *.o2r)"));
    if (path.isEmpty()) {
        return;
    }
    selectedArchivePath_ = path.toStdString();
    extractStatusLabel_->setText(QString());

    Arc arc(selectedArchivePath_);
    std::vector<std::string> items = arc.listItems();
    arc.close();

    archiveTree_ = buildPathTree(items);
    selectedNode_ = &archiveTree_;

    if (treeModel_ == nullptr) {
        treeModel_ = new QStandardItemModel(treeView_);
        treeView_->setModel(treeModel_);
        connect(treeView_->selectionModel(), &QItemSelectionModel::currentChanged, this,
                [this](const QModelIndex& current, const QModelIndex&) { onTreeSelectionChanged(current); });
    }
    populateArchiveFolderTree(*treeModel_, archiveTree_);
    treeView_->expandToDepth(0);
    treeView_->clearSelection();
    treeView_->setCurrentIndex(QModelIndex());

    refreshContentForSelectedFolder();
}

void InspectOtrController::onTreeSelectionChanged(const QModelIndex& current) {
    if (!current.isValid()) {
        selectedNode_ = &archiveTree_;
    } else {
        auto ptr = current.data(kPathTreeNodeRole).value<quintptr>();
        selectedNode_ = reinterpret_cast<const PathTreeNode*>(ptr);
    }
    refreshContentForSelectedFolder();
}

void InspectOtrController::onContentRowSelected() {
    auto selected = contentTable_->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        return;
    }
    QTableWidgetItem* item = contentTable_->item(selected.first().row(), 0);
    if (item == nullptr) {
        return;
    }
    previewArchiveEntry(item->data(kEntryNameRole).toString().toStdString());
}

void InspectOtrController::refreshContentForSelectedFolder() {
    contentStack_->setCurrentWidget(contentTable_);
    contentTable_->setSortingEnabled(false); // avoid resorting mid-population, corrupting row indices below
    contentTable_->setRowCount(0);

    int row = 0;
    if (selectedNode_ != nullptr) {
        for (const auto& entryName : selectedNode_->files) {
            contentTable_->insertRow(row);
            QString baseName = QString::fromStdString(std::filesystem::path(entryName).filename().string());
            auto* item = new QTableWidgetItem(baseName);
            item->setData(kEntryNameRole, QString::fromStdString(entryName));
            contentTable_->setItem(row, 0, item);
            ++row;
        }
    }

    contentTable_->setSortingEnabled(true);
    fileCountLabel_->setText(QObject::tr("%1 file(s) in this folder").arg(row));
}

void InspectOtrController::previewArchiveEntry(const std::string& entryName) {
    Arc arc(selectedArchivePath_);
    std::optional<std::vector<uint8_t>> data = arc.readFile(entryName);
    arc.close();
    if (!data.has_value()) {
        return;
    }

    // FilePreview reads a real filesystem path -- an archive entry isn't
    // one, so its bytes are dumped to a reused scratch file (named after
    // the entry so FilePreview's extension-based dispatch still works)
    // rather than teaching FilePreview to accept raw bytes for this one
    // caller.
    std::filesystem::path scratchDir = std::filesystem::temp_directory_path() / "bitdeck_inspect_preview";
    std::filesystem::create_directories(scratchDir);
    std::filesystem::path scratchPath = scratchDir / std::filesystem::path(entryName).filename();

    std::ofstream file(scratchPath, std::ios::binary);
    file.write(reinterpret_cast<const char*>(data->data()), static_cast<std::streamsize>(data->size()));
    file.close();

    preview_->showFile(scratchPath);
    contentStack_->setCurrentWidget(preview_);
}

void InspectOtrController::onPrimaryActionRequested() {
    if (selectedArchivePath_.empty()) {
        return;
    }
    QString outputDir = QFileDialog::getExistingDirectory(treeWidget(), QObject::tr("Extract To"));
    if (outputDir.isEmpty()) {
        return;
    }

    std::filesystem::path archiveBase = std::filesystem::path(selectedArchivePath_).stem();
    std::filesystem::path targetDir = std::filesystem::path(outputDir.toStdString()) / archiveBase;
    std::string archivePath = selectedArchivePath_;

    extractStatusLabel_->setText(QObject::tr("Extracting..."));

    runInBackground(
        [archivePath, targetDir](TaskProgress& progress) { extractAllFiles(archivePath, targetDir, progress); },
        [this](int processed, int total) {
            extractStatusLabel_->setText(QObject::tr("Extracting... (%1/%2)").arg(processed).arg(total));
        },
        [this, targetDir] {
            extractStatusLabel_->setText(QObject::tr("Extracted to %1").arg(QString::fromStdString(targetDir.string())));
        },
        [this](QString error) { extractStatusLabel_->setText(QObject::tr("Extraction failed: %1").arg(error)); });
}

} // namespace bitdeck
