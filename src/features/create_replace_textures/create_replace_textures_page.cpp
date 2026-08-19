#include "create_replace_textures_page.h"

#include <QCheckBox>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <cctype>
#include <fstream>

#include "../../app/background_worker.h"
#include "../../app/game_conventions_registry.h"
#include "../../app/main_window.h"
#include "../../app/texture_extraction.h"
#include "../../app/texture_manifest_json.h"
#include "../../core/path_utils.h"
#include "../../core/sha256.h"

namespace bitdeck {

namespace {

std::vector<uint8_t> readFileBytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

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

} // namespace

CreateReplaceTexturesPage::CreateReplaceTexturesPage(MainWindow& window) : window_(window) {
    setTitle(QStringLiteral("Make Texture Pack"));
    setBackButtonVisible(true);
    connect(this, &PageFrame::backRequested, &window_, &MainWindow::goBack);

    stepStack_ = new QStackedWidget();
    stepStack_->addWidget(buildQuestionStep());
    stepStack_->addWidget(buildFolderStep());
    stepStack_->addWidget(buildOtrStep());
    setContentWidget(stepStack_);

    showStep(ReplaceTexturesStep::Question);
}

void CreateReplaceTexturesPage::showStep(ReplaceTexturesStep step) {
    stepStack_->setCurrentIndex(static_cast<int>(step));
}

QWidget* CreateReplaceTexturesPage::buildQuestionStep() {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);
    layout->setAlignment(Qt::AlignCenter);

    auto* question = new QLabel(tr("Do you already have a folder of texture replacements,\n"
                                     "organized against a manifest.json?"));
    question->setAlignment(Qt::AlignCenter);
    question->setStyleSheet(QStringLiteral("color: white;"));
    layout->addWidget(question);

    auto* buttonsRow = new QHBoxLayout();
    auto* yesButton = new QPushButton(tr("Yes"));
    connect(yesButton, &QPushButton::clicked, this, [this] { showStep(ReplaceTexturesStep::SelectFolder); });
    buttonsRow->addWidget(yesButton);

    auto* noButton = new QPushButton(tr("No"));
    connect(noButton, &QPushButton::clicked, this, [this] { showStep(ReplaceTexturesStep::SelectOtr); });
    buttonsRow->addWidget(noButton);
    layout->addLayout(buttonsRow);

    return widget;
}

QWidget* CreateReplaceTexturesPage::buildFolderStep() {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);

    auto* selectRow = new QHBoxLayout();
    folderPathLabel_ = new QLabel(tr("No folder selected"));
    folderPathLabel_->setStyleSheet(QStringLiteral("color: white;"));
    selectRow->addWidget(folderPathLabel_, 1);
    auto* selectButton = new QPushButton(tr("Select"));
    connect(selectButton, &QPushButton::clicked, this, &CreateReplaceTexturesPage::onSelectFolder);
    selectRow->addWidget(selectButton);
    layout->addLayout(selectRow);

    folderFilesList_ = new QListWidget();
    layout->addWidget(folderFilesList_, 1);

    prependAltCheckbox_ = new QCheckBox(tr("Prepend alt/"));
    connect(prependAltCheckbox_, &QCheckBox::toggled, this,
            [this](bool checked) { window_.stagingModel().prependAlt = checked; });
    layout->addWidget(prependAltCheckbox_);

    compressCheckbox_ = new QCheckBox(tr("Compress files"));
    connect(compressCheckbox_, &QCheckBox::toggled, this,
            [this](bool checked) { window_.stagingModel().compressFiles = checked; });
    layout->addWidget(compressCheckbox_);

    keepOpenCheckbox_ = new QCheckBox(tr("Keep folder open after staging"));
    connect(keepOpenCheckbox_, &QCheckBox::toggled, this, [this](bool checked) {
        window_.stagingModel().keepFolderOpenAfterStaging = checked;
        folderStageButton_->setText(checked ? tr("Scan and Stage Textures") : tr("Stage Textures"));
    });
    layout->addWidget(keepOpenCheckbox_);

    folderStageButton_ = new QPushButton(tr("Stage Textures"));
    folderStageButton_->setEnabled(false);
    connect(folderStageButton_, &QPushButton::clicked, this, &CreateReplaceTexturesPage::onStageTextures);
    layout->addWidget(folderStageButton_);

    return widget;
}

QWidget* CreateReplaceTexturesPage::buildOtrStep() {
    auto* widget = new QWidget();
    auto* layout = new QVBoxLayout(widget);

    auto* selectRow = new QHBoxLayout();
    otrPathLabel_ = new QLabel(tr("No OTR/O2R selected"));
    otrPathLabel_->setStyleSheet(QStringLiteral("color: white;"));
    selectRow->addWidget(otrPathLabel_, 1);
    auto* selectButton = new QPushButton(tr("Select"));
    connect(selectButton, &QPushButton::clicked, this, &CreateReplaceTexturesPage::onSelectOtr);
    selectRow->addWidget(selectButton);
    layout->addLayout(selectRow);

    otrResultsList_ = new QListWidget();
    layout->addWidget(otrResultsList_, 1);

    otrProcessButton_ = new QPushButton(tr("Process"));
    otrProcessButton_->setEnabled(false);
    connect(otrProcessButton_, &QPushButton::clicked, this, &CreateReplaceTexturesPage::onProcessOtr);
    layout->addWidget(otrProcessButton_);

    return widget;
}

void CreateReplaceTexturesPage::onSelectFolder() {
    QString directory = QFileDialog::getExistingDirectory(this, tr("Select Folder"));
    if (directory.isEmpty()) {
        return;
    }
    selectedFolder_ = std::filesystem::path(directory.toStdString());
    folderPathLabel_->setText(directory);
    stagedHashes_.clear();
    scanFolder();
}

void CreateReplaceTexturesPage::scanFolder() {
    pendingTextures_.clear();
    folderFilesList_->clear();

    if (selectedFolder_.empty() || !std::filesystem::exists(selectedFolder_)) {
        folderStageButton_->setEnabled(false);
        return;
    }

    TextureManifestMap manifest = parseManifestJson(readQByteArray(selectedFolder_ / "manifest.json"));
    std::map<std::string, std::vector<std::string>> aliases;
    std::filesystem::path aliasesPath = selectedFolder_ / "aliases.json";
    if (std::filesystem::exists(aliasesPath)) {
        aliases = parseAliasesJson(readQByteArray(aliasesPath));
    }

    bool keepOpen = keepOpenCheckbox_->isChecked();
    int stagedCount = 0;

    for (const auto& dirEntry : std::filesystem::recursive_directory_iterator(selectedFolder_)) {
        if (!dirEntry.is_regular_file() || !isImageFile(dirEntry.path())) {
            continue;
        }

        std::filesystem::path relative = std::filesystem::relative(dirEntry.path(), selectedFolder_);
        std::string relativeKey = normalizePath(relative.string());
        std::string relativeNoExt = normalizePath(std::filesystem::path(relative).replace_extension("").string());

        std::vector<std::string> targets;
        auto aliasIt = aliases.find(relativeKey);
        targets = (aliasIt != aliases.end()) ? aliasIt->second : std::vector<std::string>{relativeNoExt};

        for (const auto& target : targets) {
            auto manifestEntry = resolveTextureEntry(manifest, target);
            if (!manifestEntry.has_value()) {
                continue; // not present in the manifest -- skip
            }

            std::string hash = sha256Hex(readFileBytes(dirEntry.path()));
            if (stagedHashes_.count(target) != 0 && stagedHashes_[target] == hash) {
                continue; // unchanged since it was last staged
            }

            manifestEntry->targetName = target;
            manifestEntry->sourceHash = hash;

            std::filesystem::path targetPath(target);
            std::string dirKey = normalizePath(targetPath.parent_path().string());
            pendingTextures_[dirKey].emplace_back(dirEntry.path(), *manifestEntry);
            folderFilesList_->addItem(QString::fromStdString(target));
            ++stagedCount;
        }
    }

    folderStageButton_->setEnabled(!selectedFolder_.empty() && (keepOpen || stagedCount > 0));
}

void CreateReplaceTexturesPage::onStageTextures() {
    if (pendingTextures_.empty() && !keepOpenCheckbox_->isChecked()) {
        return;
    }

    if (!pendingTextures_.empty()) {
        window_.stagingModel().addCustomTextureEntry(pendingTextures_);
        for (const auto& [dirKey, files] : pendingTextures_) {
            for (const auto& [path, entry] : files) {
                if (entry.targetName.has_value() && entry.sourceHash.has_value()) {
                    stagedHashes_[*entry.targetName] = *entry.sourceHash;
                }
            }
        }
    }

    if (keepOpenCheckbox_->isChecked()) {
        scanFolder();
    } else {
        selectedFolder_.clear();
        stagedHashes_.clear();
        pendingTextures_.clear();
        folderPathLabel_->setText(tr("No folder selected"));
        folderFilesList_->clear();
        folderStageButton_->setEnabled(false);
        window_.popTo(QStringLiteral("create_selection"));
    }
}

void CreateReplaceTexturesPage::onSelectOtr() {
    QStringList paths = QFileDialog::getOpenFileNames(this, tr("Select OTR/O2R"), QString(),
                                                        tr("Archives (*.otr *.o2r)"));
    if (paths.isEmpty()) {
        return;
    }
    selectedOtrPaths_.clear();
    for (const auto& path : paths) {
        selectedOtrPaths_.push_back(path.toStdString());
    }
    otrPathLabel_->setText(paths.join(QStringLiteral(", ")));
    otrProcessButton_->setEnabled(true);
    otrProcessButton_->setText(tr("Process"));
    otrResultsList_->clear();
}

void CreateReplaceTexturesPage::onProcessOtr() {
    if (selectedOtrPaths_.empty()) {
        return;
    }
    QString outputDir = QFileDialog::getExistingDirectory(this, tr("Extract To"));
    if (outputDir.isEmpty()) {
        return;
    }

    std::filesystem::path firstPath(selectedOtrPaths_[0]);
    std::filesystem::path targetDir = std::filesystem::path(outputDir.toStdString()) / firstPath.stem();

    otrProcessButton_->setEnabled(false);
    otrProcessButton_->setText(tr("Processing..."));

    auto archivePaths = selectedOtrPaths_;
    runInBackground(
        [archivePaths, targetDir](TaskProgress& progress) { extractTexturesToFolder(archivePaths, targetDir, progress); },
        nullptr,
        [this, targetDir] {
            otrProcessButton_->setText(tr("Extracted to %1").arg(QString::fromStdString(targetDir.string())));
            otrResultsList_->addItem(tr("Done -- open %1 in the folder step to stage it")
                                          .arg(QString::fromStdString(targetDir.string())));
        },
        [this](QString error) {
            otrProcessButton_->setEnabled(true);
            otrProcessButton_->setText(tr("Process"));
            otrResultsList_->addItem(tr("Failed: %1").arg(error));
        });
}

QWidget* makeCreateReplaceTexturesPage(MainWindow& window) {
    return new CreateReplaceTexturesPage(window);
}

} // namespace bitdeck
