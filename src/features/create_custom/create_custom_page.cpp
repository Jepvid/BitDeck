#include "create_custom_page.h"

#include <QCheckBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include <fstream>

#include "../../app/main_window.h"
#include "../../core/path_utils.h"
#include "../../core/sha256.h"

namespace bitdeck {

namespace {

std::vector<uint8_t> readFileBytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

} // namespace

CreateCustomPage::CreateCustomPage(MainWindow& window) : window_(window) {
    setTitle(QStringLiteral("Pack a Mod"));
    setBackButtonVisible(true);
    connect(this, &PageFrame::backRequested, &window_, &MainWindow::goBack);

    auto* content = new QWidget();
    auto* layout = new QVBoxLayout(content);

    auto* selectRow = new QHBoxLayout();
    pathLabel_ = new QLabel(tr("No folder selected"));
    pathLabel_->setStyleSheet(QStringLiteral("color: white;"));
    selectRow->addWidget(pathLabel_, 1);
    selectButton_ = new QPushButton(tr("Select"));
    connect(selectButton_, &QPushButton::clicked, this, &CreateCustomPage::onSelectFolder);
    selectRow->addWidget(selectButton_);
    layout->addLayout(selectRow);

    fileCountLabel_ = new QLabel();
    fileCountLabel_->setStyleSheet(QStringLiteral("color: white;"));
    layout->addWidget(fileCountLabel_);

    filesList_ = new QListWidget();
    layout->addWidget(filesList_, 1);

    keepOpenCheckbox_ = new QCheckBox(tr("Keep folder open after staging"));
    connect(keepOpenCheckbox_, &QCheckBox::toggled, this, [this](bool checked) {
        window_.stagingModel().keepFolderOpenAfterStaging = checked;
        stageButton_->setText(checked ? tr("Scan and Stage Files") : tr("Stage Files"));
        updateStageButtonState();
    });
    layout->addWidget(keepOpenCheckbox_);

    stageButton_ = new QPushButton(tr("Stage Files"));
    connect(stageButton_, &QPushButton::clicked, this, &CreateCustomPage::onStage);
    layout->addWidget(stageButton_);

    setContentWidget(content);
    updateStageButtonState();
}

void CreateCustomPage::onSelectFolder() {
    QString directory = QFileDialog::getExistingDirectory(this, tr("Select Folder"));
    if (directory.isEmpty()) {
        return;
    }
    selectedFolder_ = std::filesystem::path(directory.toStdString());
    pathLabel_->setText(directory);
    stagedHashes_.clear();
    scanFolder();
}

void CreateCustomPage::scanFolder() {
    pendingFilesByDir_.clear();
    pendingHashes_.clear();
    filesList_->clear();

    if (selectedFolder_.empty() || !std::filesystem::exists(selectedFolder_)) {
        fileCountLabel_->setText(QString());
        updateStageButtonState();
        return;
    }

    bool keepOpen = keepOpenCheckbox_->isChecked();
    for (const auto& dirEntry : std::filesystem::recursive_directory_iterator(selectedFolder_)) {
        if (!dirEntry.is_regular_file()) {
            continue;
        }
        std::filesystem::path relative = std::filesystem::relative(dirEntry.path(), selectedFolder_);
        std::string relativeKey = normalizePath(relative.string());
        std::string dirKey = normalizePath(relative.parent_path().string());
        if (dirKey == ".") {
            dirKey.clear();
        }

        std::string hash = sha256Hex(readFileBytes(dirEntry.path()));

        if (keepOpen) {
            auto it = stagedHashes_.find(relativeKey);
            if (it != stagedHashes_.end() && it->second == hash) {
                continue; // unchanged since it was last staged
            }
        }

        pendingFilesByDir_[dirKey].push_back(dirEntry.path());
        pendingHashes_[relativeKey] = hash;
        filesList_->addItem(QString::fromStdString(relativeKey));
    }

    fileCountLabel_->setText(tr("Files to insert: %1").arg(filesList_->count()));
    updateStageButtonState();
}

void CreateCustomPage::onStage() {
    if (pendingFilesByDir_.empty() && !keepOpenCheckbox_->isChecked()) {
        return;
    }

    for (const auto& [dirKey, files] : pendingFilesByDir_) {
        window_.stagingModel().addCustomStageEntries(files, dirKey);
    }
    for (const auto& [relativeKey, hash] : pendingHashes_) {
        stagedHashes_[relativeKey] = hash;
    }

    if (keepOpenCheckbox_->isChecked()) {
        scanFolder(); // re-scan in place; already-staged files are now deduped out
    } else {
        resetToIdle();
        window_.popTo(QStringLiteral("create_selection"));
    }
}

void CreateCustomPage::resetToIdle() {
    selectedFolder_.clear();
    stagedHashes_.clear();
    pendingFilesByDir_.clear();
    pendingHashes_.clear();
    pathLabel_->setText(tr("No folder selected"));
    fileCountLabel_->setText(QString());
    filesList_->clear();
    updateStageButtonState();
}

void CreateCustomPage::updateStageButtonState() {
    bool hasFolder = !selectedFolder_.empty();
    bool keepOpen = keepOpenCheckbox_->isChecked();
    stageButton_->setEnabled(hasFolder && (keepOpen || !pendingFilesByDir_.empty()));
}

QWidget* makeCreateCustomPage(MainWindow& window) {
    return new CreateCustomPage(window);
}

} // namespace bitdeck
