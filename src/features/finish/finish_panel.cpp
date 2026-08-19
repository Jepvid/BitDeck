#include "finish_panel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

#include "../../app/archive_generator.h"
#include "../../app/background_worker.h"
#include "../../app/main_window.h"

namespace bitdeck {

FinishPanel::FinishPanel(MainWindow& window, QWidget* parent) : QWidget(parent), window_(window) {
    auto* layout = new QVBoxLayout(this);

    entriesList_ = new QListWidget(this);
    layout->addWidget(entriesList_, 1);

    auto* controlsRow = new QHBoxLayout();
    extensionCombo_ = new QComboBox(this);
    extensionCombo_->addItems({QStringLiteral("o2r"), QStringLiteral("otr")});
    extensionCombo_->setCurrentText(window_.stagingModel().outputExtension());
    connect(extensionCombo_, &QComboBox::currentTextChanged, this,
            [this](const QString& ext) { window_.stagingModel().setOutputExtension(ext); });
    controlsRow->addWidget(extensionCombo_);

    prependAltCheckbox_ = new QCheckBox(tr("Prepend alt/"), this);
    prependAltCheckbox_->setChecked(window_.stagingModel().prependAlt);
    connect(prependAltCheckbox_, &QCheckBox::toggled, this,
            [this](bool checked) { window_.stagingModel().prependAlt = checked; });
    controlsRow->addWidget(prependAltCheckbox_);

    compressCheckbox_ = new QCheckBox(tr("Compress files"), this);
    compressCheckbox_->setChecked(window_.stagingModel().compressFiles);
    connect(compressCheckbox_, &QCheckBox::toggled, this,
            [this](bool checked) { window_.stagingModel().compressFiles = checked; });
    controlsRow->addWidget(compressCheckbox_);

    generateButton_ = new QPushButton(tr("Generate"), this);
    connect(generateButton_, &QPushButton::clicked, this, &FinishPanel::onGenerate);
    controlsRow->addWidget(generateButton_);
    layout->addLayout(controlsRow);

    statusLabel_ = new QLabel(this);
    statusLabel_->setStyleSheet(QStringLiteral("color: white;"));
    layout->addWidget(statusLabel_);

    connect(&window_.stagingModel(), &StagingModel::changed, this, &FinishPanel::refresh);
    refresh();
}

void FinishPanel::refresh() {
    entriesList_->clear();
    for (const auto& [key, entry] : window_.stagingModel().entries()) {
        auto label = QStringLiteral("%1 (%2 files)")
                         .arg(QString::fromStdString(key.empty() ? "(root)" : key))
                         .arg(stageEntryFileCount(entry));
        entriesList_->addItem(label);
    }
    bool hasEntries = !window_.stagingModel().entries().empty();
    generateButton_->setEnabled(hasEntries && !window_.stagingModel().isGenerating());
}

void FinishPanel::onGenerate() {
    auto& stagingModel = window_.stagingModel();
    if (stagingModel.entries().empty()) {
        return;
    }

    QString extension = extensionCombo_->currentText();

    // Pre-fills the save dialog with the last folder and filename used,
    // persisted across app restarts, with the extension swapped to match
    // the current picker.
    QSettings settings;
    QString lastPath = settings.value(QStringLiteral("finalize/lastOutputPath")).toString();
    QString defaultPath;
    if (!lastPath.isEmpty()) {
        QFileInfo lastInfo(lastPath);
        defaultPath = lastInfo.path() + QStringLiteral("/") + lastInfo.completeBaseName() + QStringLiteral(".") + extension;
    } else {
        defaultPath = QStringLiteral("generated.%1").arg(extension);
    }

    QString path = QFileDialog::getSaveFileName(this, tr("Generate Archive"), defaultPath,
                                                 QStringLiteral("*.%1").arg(extension));
    if (path.isEmpty()) {
        return;
    }
    if (!path.endsWith(QStringLiteral(".%1").arg(extension))) {
        path += QStringLiteral(".%1").arg(extension);
    }
    settings.setValue(QStringLiteral("finalize/lastOutputPath"), path);

    int total = 0;
    for (const auto& [key, entry] : stagingModel.entries()) {
        total += static_cast<int>(stageEntryFileCount(entry));
    }
    stagingModel.beginGeneration(total);
    generateButton_->setEnabled(false);
    statusLabel_->setText(tr("Generating..."));

    auto entriesSnapshot = stagingModel.entries();
    bool compress = stagingModel.compressFiles;
    bool prependAlt = stagingModel.prependAlt;
    std::string outputPath = path.toStdString();

    runInBackground(
        [entriesSnapshot, outputPath, compress, prependAlt](TaskProgress& progress) {
            generateArchive(entriesSnapshot, outputPath, compress, prependAlt, progress);
        },
        [this](int processed, int total) {
            window_.stagingModel().reportGenerationProgress(processed);
            statusLabel_->setText(tr("Generating... (%1/%2)").arg(processed).arg(total));
        },
        [this] {
            statusLabel_->setText(tr("Done"));
            window_.stagingModel().finishGeneration();
            window_.stagingModel().clear();
            emit archiveGenerated();
        },
        [this](QString error) {
            statusLabel_->setText(tr("Failed: %1").arg(error));
            window_.stagingModel().failGeneration(error);
            generateButton_->setEnabled(true);
        });
}

} // namespace bitdeck
