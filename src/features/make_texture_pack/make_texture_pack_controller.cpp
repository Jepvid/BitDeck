#include "make_texture_pack_controller.h"

#include <QCheckBox>
#include <QFile>
#include <QFileDialog>
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTreeView>

#include <algorithm>
#include <cctype>

#include "../../app/background_worker.h"
#include "../../app/file_preview.h"
#include "../../app/game_conventions_registry.h"
#include "../../app/main_window.h"
#include "../../app/texture_extraction.h"
#include "../../app/texture_manifest_json.h"
#include "../../core/path_utils.h"
#include "../../core/sha256.h"

namespace bitdeck {

namespace {

constexpr int kAbsolutePathRole = Qt::UserRole + 1;

QByteArray readQByteArray(const std::filesystem::path& path) {
    QFile file(QString::fromStdString(path.string()));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

bool isImageFile(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".png" || ext == ".jpeg" || ext == ".jpg";
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

MakeTexturePackController::MakeTexturePackController(MainWindow& window) : ModeController(window) {
    // Clears the dedup cache after a successful generate.
    connect(&window_.stagingModel(), &StagingModel::generationFinished, this, [this] { stagedHashes_.clear(); });
}

QWidget* MakeTexturePackController::treeWidget() {
    if (treeView_ != nullptr) {
        return treeView_;
    }
    treeView_ = new QTreeView();
    return treeView_;
}

QWidget* MakeTexturePackController::contentWidget() {
    if (contentStack_ != nullptr) {
        return contentStack_;
    }

    contentTable_ = new QTableWidget(0, 2);
    contentTable_->setHorizontalHeaderLabels({QObject::tr("Target"), QObject::tr("Status")});
    contentTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    contentTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    contentTable_->verticalHeader()->setVisible(false);
    contentTable_->setEditTriggers(QTableWidget::NoEditTriggers);
    contentTable_->setSelectionMode(QTableWidget::SingleSelection);
    contentTable_->setSelectionBehavior(QTableWidget::SelectRows);
    connect(contentTable_, &QTableWidget::itemSelectionChanged, this, &MakeTexturePackController::onContentRowSelected);

    preview_ = new FilePreview();
    connect(preview_, &FilePreview::closeRequested, this, [this] { contentStack_->setCurrentWidget(contentTable_); });

    contentStack_ = new QStackedWidget();
    contentStack_->addWidget(contentTable_);
    contentStack_->addWidget(preview_);
    return contentStack_;
}

QWidget* MakeTexturePackController::statusBarWidget() {
    if (statusBar_ != nullptr) {
        return statusBar_;
    }
    statusBar_ = new QWidget();
    auto* layout = new QHBoxLayout(statusBar_);
    layout->setContentsMargins(8, 4, 8, 4);

    fileCountLabel_ = new QLabel();
    layout->addWidget(fileCountLabel_);

    layout->addStretch(1);

    applyTlutCheckbox_ = new QCheckBox(QObject::tr("Apply TLUT to applicable images"));
    applyTlutCheckbox_->setChecked(true);
    layout->addWidget(applyTlutCheckbox_);

    extractButton_ = new QPushButton(QObject::tr("Extract textures from otr/o2r"));
    connect(extractButton_, &QPushButton::clicked, this, &MakeTexturePackController::onExtractTexturesClicked);
    layout->addWidget(extractButton_);

    return statusBar_;
}

void MakeTexturePackController::onOpenRequested() {
    QString directory = QFileDialog::getExistingDirectory(treeWidget(), QObject::tr("Select Folder"));
    if (directory.isEmpty()) {
        return;
    }

    std::filesystem::path folder(directory.toStdString());
    if (!std::filesystem::exists(folder / "manifest.json")) {
        QMessageBox::warning(
            treeWidget(), QObject::tr("Missing manifest.json"),
            QObject::tr("This folder has no manifest.json, so there's no way to know which images map to which "
                        "archive textures. Pick a folder produced by \"Extract textures from otr/o2r\"."));
        return;
    }

    openFolder(folder);
}

void MakeTexturePackController::openFolder(const std::filesystem::path& folder) {
    selectedFolder_ = folder;
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
    QString directory = QString::fromStdString(folder.string());
    fsModel_->setRootPath(directory);
    QModelIndex rootIndex = fsModel_->index(directory);
    treeView_->setRootIndex(rootIndex);
    treeView_->setCurrentIndex(rootIndex);

    rescanAndStage();
}

void MakeTexturePackController::rescanAndStage() {
    lastScan_.clear();

    if (selectedFolder_.empty() || !std::filesystem::exists(selectedFolder_)) {
        refreshContentForSelectedFolder();
        return;
    }

    TextureManifestMap manifest = parseManifestJson(readQByteArray(selectedFolder_ / "manifest.json"));
    std::map<std::string, std::vector<std::string>> aliases;
    std::filesystem::path aliasesPath = selectedFolder_ / "aliases.json";
    if (std::filesystem::exists(aliasesPath)) {
        aliases = parseAliasesJson(readQByteArray(aliasesPath));
    }

    // Opening a folder here means the user wants its resolved textures in
    // the finalized mod -- stage every changed one immediately, same as
    // Pack Mod, but a source image may fan out to several target archive
    // paths via aliases.json, so staging is grouped by each target's dir.
    std::map<std::string, std::vector<std::pair<std::filesystem::path, TextureManifestEntry>>> toStage;

    for (const auto& dirEntry : std::filesystem::recursive_directory_iterator(selectedFolder_)) {
        if (!dirEntry.is_regular_file() || !isImageFile(dirEntry.path())) {
            continue;
        }

        std::filesystem::path relative = std::filesystem::relative(dirEntry.path(), selectedFolder_);
        std::string relativeKey = normalizePath(relative.string());
        std::string relativeNoExt = normalizePath(std::filesystem::path(relative).replace_extension("").string());
        std::string sourceDirKey = normalizePath(relative.parent_path().string());
        if (sourceDirKey == ".") {
            sourceDirKey.clear();
        }

        auto aliasIt = aliases.find(relativeKey);
        std::vector<std::string> targets =
            (aliasIt != aliases.end()) ? aliasIt->second : std::vector<std::string>{relativeNoExt};

        std::string hash = sha256Hex(readFileBytes(dirEntry.path()));

        for (const auto& target : targets) {
            auto manifestEntry = resolveTextureEntry(manifest, target);
            if (!manifestEntry.has_value()) {
                continue; // not present in the manifest -- skip
            }

            // Status compares the current file hash against
            // manifestEntry->hash, the pristine baseline recorded at
            // extraction time (see texture_extraction.cpp), not against
            // whether it's been staged this session.
            FileStageStatus status;
            if (manifestEntry->hash.empty()) {
                status = FileStageStatus::New;
            } else if (manifestEntry->hash == hash) {
                status = FileStageStatus::UpToDate;
            } else {
                status = FileStageStatus::Modified;
            }

            lastScan_.push_back({dirEntry.path(), sourceDirKey, target, *manifestEntry, hash, status});
            if (status == FileStageStatus::UpToDate) {
                continue; // matches the manifest baseline -- nothing to stage
            }

            // Session-level dedup only, to avoid re-adding an unchanged
            // edit to the staging model on a repeat scan within the same
            // folder-open -- unrelated to the status shown above.
            auto knownIt = stagedHashes_.find(target);
            if (knownIt != stagedHashes_.end() && knownIt->second == hash) {
                continue;
            }

            manifestEntry->targetName = target;
            manifestEntry->sourceHash = hash;
            std::filesystem::path targetPath(target);
            std::string dirKey = normalizePath(targetPath.parent_path().string());
            toStage[dirKey].emplace_back(dirEntry.path(), *manifestEntry);
            stagedHashes_[target] = hash;
        }
    }

    if (!toStage.empty()) {
        window_.stagingModel().addCustomTextureEntry(toStage);
    }

    refreshContentForSelectedFolder();
}

void MakeTexturePackController::onTreeSelectionChanged(const QModelIndex& current) {
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

void MakeTexturePackController::onContentRowSelected() {
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

void MakeTexturePackController::refreshContentForSelectedFolder() {
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
    for (const auto& entry : lastScan_) {
        if (entry.sourceDirKey != dirKey) {
            continue;
        }
        contentTable_->insertRow(row);
        QString baseName = QString::fromStdString(std::filesystem::path(entry.targetName).filename().string());
        auto* nameItem = new QTableWidgetItem(baseName);
        nameItem->setData(kAbsolutePathRole, QString::fromStdString(entry.sourcePath.string()));
        contentTable_->setItem(row, 0, nameItem);
        contentTable_->setItem(row, 1, new QTableWidgetItem(statusText(entry.status)));
        ++row;
    }

    contentTable_->setSortingEnabled(true);
    fileCountLabel_->setText(QObject::tr("%1 texture(s) in this folder").arg(row));
}

void MakeTexturePackController::onExtractTexturesClicked() {
    QStringList paths = QFileDialog::getOpenFileNames(treeWidget(), QObject::tr("Select OTR/O2R"), QString(),
                                                        QObject::tr("Archives (*.otr *.o2r)"));
    if (paths.isEmpty()) {
        return;
    }
    QString outputDir = QFileDialog::getExistingDirectory(treeWidget(), QObject::tr("Extract To"));
    if (outputDir.isEmpty()) {
        return;
    }

    std::vector<std::string> archivePaths;
    for (const auto& path : paths) {
        archivePaths.push_back(path.toStdString());
    }
    std::filesystem::path firstPath(archivePaths.front());
    std::filesystem::path targetDir = std::filesystem::path(outputDir.toStdString()) / firstPath.stem();
    bool applyTlut = applyTlutCheckbox_->isChecked();

    extractButton_->setEnabled(false);
    fileCountLabel_->setText(QObject::tr("Extracting..."));

    runInBackground(
        [archivePaths, targetDir, applyTlut](TaskProgress& progress) {
            extractTexturesToFolder(archivePaths, targetDir, progress, applyTlut);
        },
        nullptr,
        [this, targetDir] {
            extractButton_->setEnabled(true);
            openFolder(targetDir);
        },
        [this](QString error) {
            extractButton_->setEnabled(true);
            fileCountLabel_->setText(QObject::tr("Extraction failed: %1").arg(error));
        });
}

} // namespace bitdeck
