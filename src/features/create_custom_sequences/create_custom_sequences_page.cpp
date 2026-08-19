#include "create_custom_sequences_page.h"

#include <QCheckBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cctype>
#include <fstream>

#include "../../app/main_window.h"
#include "../../core/path_utils.h"
#include "../../core/sha256.h"

namespace bitdeck {

namespace {

bool isSeqFile(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".seq";
}

} // namespace

CreateCustomSequencesPage::CreateCustomSequencesPage(MainWindow& window) : window_(window) {
    setTitle(QStringLiteral("Custom Sequences"));
    setBackButtonVisible(true);
    connect(this, &PageFrame::backRequested, &window_, &MainWindow::goBack);

    auto* content = new QWidget();
    auto* layout = new QVBoxLayout(content);

    auto* selectRow = new QHBoxLayout();
    pathLabel_ = new QLabel(tr("No folder selected"));
    pathLabel_->setStyleSheet(QStringLiteral("color: white;"));
    selectRow->addWidget(pathLabel_, 1);
    selectButton_ = new QPushButton(tr("Select"));
    connect(selectButton_, &QPushButton::clicked, this, &CreateCustomSequencesPage::onSelectFolder);
    selectRow->addWidget(selectButton_);
    layout->addLayout(selectRow);

    countLabel_ = new QLabel();
    countLabel_->setStyleSheet(QStringLiteral("color: white;"));
    layout->addWidget(countLabel_);

    sequencesList_ = new QListWidget();
    layout->addWidget(sequencesList_, 1);

    keepOpenCheckbox_ = new QCheckBox(tr("Keep folder open after staging"));
    connect(keepOpenCheckbox_, &QCheckBox::toggled, this, [this](bool checked) {
        window_.stagingModel().keepFolderOpenAfterStaging = checked;
        stageButton_->setText(checked ? tr("Scan and Stage Files") : tr("Stage Files"));
        updateStageButtonState();
    });
    layout->addWidget(keepOpenCheckbox_);

    stageButton_ = new QPushButton(tr("Stage Files"));
    connect(stageButton_, &QPushButton::clicked, this, &CreateCustomSequencesPage::onStage);
    layout->addWidget(stageButton_);

    setContentWidget(content);
    updateStageButtonState();
}

void CreateCustomSequencesPage::onSelectFolder() {
    QString directory = QFileDialog::getExistingDirectory(this, tr("Select Folder"));
    if (directory.isEmpty()) {
        return;
    }
    selectedFolder_ = std::filesystem::path(directory.toStdString());
    pathLabel_->setText(directory);
    stagedHashes_.clear();
    scanFolder();
}

void CreateCustomSequencesPage::scanFolder() {
    pendingPairs_.clear();
    pendingHashes_.clear();
    sequencesList_->clear();

    if (selectedFolder_.empty() || !std::filesystem::exists(selectedFolder_)) {
        countLabel_->setText(QString());
        updateStageButtonState();
        return;
    }

    bool keepOpen = keepOpenCheckbox_->isChecked();
    for (const auto& dirEntry : std::filesystem::recursive_directory_iterator(selectedFolder_)) {
        if (!dirEntry.is_regular_file() || !isSeqFile(dirEntry.path())) {
            continue;
        }

        std::filesystem::path metaPath = dirEntry.path();
        metaPath.replace_extension(".meta");
        if (!std::filesystem::exists(metaPath)) {
            continue; // no matching .meta -- skip, matching Retro
        }

        std::filesystem::path relative = std::filesystem::relative(dirEntry.path(), selectedFolder_);
        std::string relativeKey = normalizePath(relative.string());
        std::string hash = sha256Hex(readFileBytes(dirEntry.path()));

        if (keepOpen) {
            auto it = stagedHashes_.find(relativeKey);
            if (it != stagedHashes_.end() && it->second == hash) {
                continue; // unchanged since it was last staged
            }
        }

        pendingPairs_.emplace_back(dirEntry.path(), metaPath);
        pendingHashes_[relativeKey] = hash;
        sequencesList_->addItem(QString::fromStdString(dirEntry.path().stem().string()));
    }

    countLabel_->setText(tr("Sequences to insert: %1").arg(sequencesList_->count()));
    updateStageButtonState();
}

void CreateCustomSequencesPage::onStage() {
    if (pendingPairs_.empty() && !keepOpenCheckbox_->isChecked()) {
        return;
    }

    if (!pendingPairs_.empty()) {
        window_.stagingModel().addCustomSequenceEntry(pendingPairs_, kArchiveKey);
    }
    for (const auto& [relativeKey, hash] : pendingHashes_) {
        stagedHashes_[relativeKey] = hash;
    }

    if (keepOpenCheckbox_->isChecked()) {
        scanFolder();
    } else {
        resetToIdle();
        window_.popTo(QStringLiteral("create_selection"));
    }
}

void CreateCustomSequencesPage::resetToIdle() {
    selectedFolder_.clear();
    stagedHashes_.clear();
    pendingPairs_.clear();
    pendingHashes_.clear();
    pathLabel_->setText(tr("No folder selected"));
    countLabel_->setText(QString());
    sequencesList_->clear();
    updateStageButtonState();
}

void CreateCustomSequencesPage::updateStageButtonState() {
    bool hasFolder = !selectedFolder_.empty();
    bool keepOpen = keepOpenCheckbox_->isChecked();
    stageButton_->setEnabled(hasFolder && (keepOpen || !pendingPairs_.empty()));
}

QWidget* makeCreateCustomSequencesPage(MainWindow& window) {
    return new CreateCustomSequencesPage(window);
}

} // namespace bitdeck
