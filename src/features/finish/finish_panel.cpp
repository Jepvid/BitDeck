#include "finish_panel.h"

#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include "../../app/archive_generator.h"
#include "../../app/background_worker.h"
#include "../../app/ephemeral_bar.h"
#include "../../app/main_window.h"

namespace bitdeck {

FinishPanel::FinishPanel(MainWindow& window, QWidget* parent) : QWidget(parent), window_(window) {
    auto* layout = new QVBoxLayout(this);

    entriesList_ = new QListWidget(this);
    layout->addWidget(entriesList_, 1);

    auto* controlsRow = new QHBoxLayout();
    extensionCombo_ = new QComboBox(this);
    extensionCombo_->addItems({QStringLiteral("o2r"), QStringLiteral("otr")});
    extensionCombo_->setCurrentText(window_.stagingModel().outputExtension);
    connect(extensionCombo_, &QComboBox::currentTextChanged, this,
            [this](const QString& ext) { window_.stagingModel().outputExtension = ext; });
    controlsRow->addWidget(extensionCombo_);

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
    QString path = QFileDialog::getSaveFileName(this, tr("Generate Archive"),
                                                 QStringLiteral("generated.%1").arg(extension),
                                                 QStringLiteral("*.%1").arg(extension));
    if (path.isEmpty()) {
        return;
    }
    if (!path.endsWith(QStringLiteral(".%1").arg(extension))) {
        path += QStringLiteral(".%1").arg(extension);
    }

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
            window_.ephemeralBar().setExpanded(false);
        },
        [this](QString error) {
            statusLabel_->setText(tr("Failed: %1").arg(error));
            window_.stagingModel().failGeneration(error);
            generateButton_->setEnabled(true);
        });
}

} // namespace bitdeck
