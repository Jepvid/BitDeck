#include "inspect_otr_page.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <fstream>

#include "../../app/background_worker.h"
#include "../../app/main_window.h"
#include "../../archive/arc.h"
#include "../../archive/extraction_safety.h"

namespace bitdeck {

namespace {

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

InspectOtrPage::InspectOtrPage(MainWindow& window) : window_(window) {
    setTitle(QStringLiteral("Inspect"));
    setBackButtonVisible(true);
    connect(this, &PageFrame::backRequested, &window_, &MainWindow::goBack);

    searchField_ = new QLineEdit();
    searchField_->setPlaceholderText(tr("Search"));
    connect(searchField_, &QLineEdit::textChanged, this, &InspectOtrPage::onSearchChanged);
    setTopRightWidget(searchField_);

    auto* content = new QWidget();
    auto* layout = new QVBoxLayout(content);

    auto* selectRow = new QHBoxLayout();
    pathLabel_ = new QLabel(tr("No archive selected"));
    pathLabel_->setStyleSheet(QStringLiteral("color: white;"));
    selectRow->addWidget(pathLabel_, 1);

    selectButton_ = new QPushButton(tr("Select"));
    connect(selectButton_, &QPushButton::clicked, this, &InspectOtrPage::onSelectOtr);
    selectRow->addWidget(selectButton_);

    extractButton_ = new QPushButton(tr("Extract All"));
    extractButton_->setEnabled(false);
    connect(extractButton_, &QPushButton::clicked, this, &InspectOtrPage::onExtractAll);
    selectRow->addWidget(extractButton_);
    layout->addLayout(selectRow);

    filesList_ = new QListWidget();
    layout->addWidget(filesList_, 1);

    statusLabel_ = new QLabel();
    statusLabel_->setStyleSheet(QStringLiteral("color: white;"));
    layout->addWidget(statusLabel_);

    setContentWidget(content);
}

void InspectOtrPage::onSelectOtr() {
    QString path = QFileDialog::getOpenFileName(this, tr("Select OTR/O2R"), QString(), tr("Archives (*.otr *.o2r)"));
    if (path.isEmpty()) {
        return;
    }
    selectedPath_ = path.toStdString();
    pathLabel_->setText(path);

    Arc arc(selectedPath_);
    filesInArchive_ = arc.listItems();
    arc.close();

    extractButton_->setEnabled(true);
    statusLabel_->setText(tr("%1 files").arg(filesInArchive_.size()));
    refreshFilteredList();
}

void InspectOtrPage::onSearchChanged(const QString& /*query*/) {
    refreshFilteredList();
}

void InspectOtrPage::refreshFilteredList() {
    filesList_->clear();
    QString query = searchField_->text().toLower();
    for (const auto& name : filesInArchive_) {
        QString qName = QString::fromStdString(name);
        if (query.isEmpty() || qName.toLower().contains(query)) {
            filesList_->addItem(qName);
        }
    }
}

void InspectOtrPage::onExtractAll() {
    if (selectedPath_.empty()) {
        return;
    }
    QString outputDir = QFileDialog::getExistingDirectory(this, tr("Extract To"));
    if (outputDir.isEmpty()) {
        return;
    }

    std::filesystem::path archiveBase = std::filesystem::path(selectedPath_).stem();
    std::filesystem::path targetDir = std::filesystem::path(outputDir.toStdString()) / archiveBase;

    selectButton_->setEnabled(false);
    extractButton_->setEnabled(false);
    statusLabel_->setText(tr("Extracting..."));

    std::string archivePath = selectedPath_;
    runInBackground(
        [archivePath, targetDir](TaskProgress& progress) { extractAllFiles(archivePath, targetDir, progress); },
        [this](int processed, int total) { statusLabel_->setText(tr("Extracting (%1/%2)").arg(processed).arg(total)); },
        [this, targetDir] {
            statusLabel_->setText(tr("Extracted to %1").arg(QString::fromStdString(targetDir.string())));
            selectButton_->setEnabled(true);
            extractButton_->setEnabled(true);
        },
        [this](QString error) {
            statusLabel_->setText(tr("Extraction failed: %1").arg(error));
            selectButton_->setEnabled(true);
            extractButton_->setEnabled(true);
        });
}

QWidget* makeInspectOtrPage(MainWindow& window) {
    return new InspectOtrPage(window);
}

} // namespace bitdeck
