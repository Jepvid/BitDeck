#include "audio_convert_page.h"

#include <QCheckBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMimeData>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <array>
#include <fstream>

#include "../../app/main_window.h"
#include "../../core/audio/vadpcm_codec.h"
#include "../../core/audio/wav_file.h"
#include "../../core/types/audio_sample.h"

namespace bitdeck {

namespace {

constexpr int kPredictorCount = 4;
constexpr int kColInput = 0;
constexpr int kColOutputName = 1;
constexpr int kColLoop = 2;
constexpr int kColStart = 3;
constexpr int kColEnd = 4;
constexpr int kColCount = 5;
constexpr int kColRate = 6;
constexpr int kColStatus = 7;

std::vector<uint8_t> readFileBytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

// Attempts to read path as a WAV, filling in the fields SampleItem needs for
// display; returns nullopt (with statusOut set) if it isn't a valid mono
// 16-bit PCM WAV.
std::optional<AudioSampleItem> makeItemFromWav(const std::filesystem::path& path, QString& statusOut) {
    std::string error;
    WavData wav;
    if (!readWavFile(readFileBytes(path), wav, error)) {
        statusOut = QStringLiteral("WAV error: %1").arg(QString::fromStdString(error));
        return std::nullopt;
    }

    AudioSampleItem item;
    item.inputPath = path;
    item.outputName = QString::fromStdString(path.stem().string());
    item.sampleRate = wav.sampleRate;
    item.sampleCount = static_cast<uint32_t>(wav.samples.size());
    item.tuning = static_cast<double>(wav.sampleRate) / 32000.0;
    item.status = QStringLiteral("Ready");
    return item;
}

bool convertItem(const AudioSampleItem& item, const std::filesystem::path& outputDir, QString& status) {
    std::string error;
    WavData wav;
    if (!readWavFile(readFileBytes(item.inputPath), wav, error)) {
        status = QStringLiteral("WAV error: %1").arg(QString::fromStdString(error));
        return false;
    }

    if (item.outputName.isEmpty()) {
        status = QStringLiteral("Output name is empty.");
        return false;
    }
    if (outputDir.empty()) {
        status = QStringLiteral("Output folder is empty.");
        return false;
    }

    VadpcmEncoded encoded;
    if (!encodeVadpcm(wav.samples, kPredictorCount, encoded, error)) {
        status = QStringLiteral("VADPCM encode failed: %1").arg(QString::fromStdString(error));
        return false;
    }

    std::vector<int16_t> decodedSamples;
    if (!decodeVadpcm(encoded, decodedSamples, error)) {
        status = QStringLiteral("VADPCM decode failed: %1").arg(QString::fromStdString(error));
        return false;
    }
    int maxAbs = 0;
    for (int16_t sample : decodedSamples) {
        int value = sample < 0 ? -static_cast<int>(sample) : static_cast<int>(sample);
        maxAbs = std::max(maxAbs, value);
    }
    if (maxAbs == 0) {
        status = QStringLiteral("Encoded audio is silent.");
        return false;
    }

    AudioSample sample = AudioSample::fromVadpcm(encoded.adpcmData, static_cast<uint32_t>(wav.samples.size()),
                                                  encoded.order, encoded.predictors, encoded.book);

    if (item.loopEnabled) {
        if (decodedSamples.empty()) {
            status = QStringLiteral("Decoded audio is empty.");
            return false;
        }
        uint32_t maxIndex = static_cast<uint32_t>(decodedSamples.size() - 1);
        uint32_t loopEnd = item.loopEnd == 0 ? maxIndex : item.loopEnd;
        if (item.loopStart > loopEnd || loopEnd > maxIndex) {
            status = QStringLiteral("Invalid loop range. Max index = %1.").arg(maxIndex);
            return false;
        }
        sample.setLoop(item.loopStart, loopEnd, item.loopCount, buildLoopState(decodedSamples, item.loopStart));
    }

    std::filesystem::create_directories(outputDir);
    std::filesystem::path outPath = outputDir / item.outputName.toStdString();
    std::vector<uint8_t> bytes = sample.build();
    std::ofstream out(outPath, std::ios::binary);
    if (!out) {
        status = QStringLiteral("Write error: could not open output file.");
        return false;
    }
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!out) {
        status = QStringLiteral("Write error: failed writing output file.");
        return false;
    }

    status = QStringLiteral("OK");
    return true;
}

} // namespace

AudioConvertPage::AudioConvertPage(MainWindow& window) : window_(window) {
    setTitle(QStringLiteral("SoH Audio Tool"));
    setSubtitle(tr("Convert WAV files into SoH/2Ship audio samples"));
    setAcceptDrops(true);

    auto* content = new QWidget();
    auto* layout = new QVBoxLayout(content);

    auto* outputRow = new QHBoxLayout();
    outputRow->addWidget(new QLabel(tr("Output folder:")));
    outputFolderEdit_ = new QLineEdit();
    outputRow->addWidget(outputFolderEdit_, 1);
    auto* browseButton = new QPushButton(tr("Browse"));
    connect(browseButton, &QPushButton::clicked, this, &AudioConvertPage::onBrowseOutputFolder);
    outputRow->addWidget(browseButton);
    layout->addLayout(outputRow);

    auto* hint = new QLabel(tr("Loop End = 0 uses last sample. Count = -1 means infinite. "
                                "You can also drag and drop WAV files onto this page."));
    hint->setEnabled(false);
    layout->addWidget(hint);

    auto* buttonRow = new QHBoxLayout();
    auto* addButton = new QPushButton(tr("Add WAVs"));
    connect(addButton, &QPushButton::clicked, this, &AudioConvertPage::onAddWavs);
    buttonRow->addWidget(addButton);
    auto* clearButton = new QPushButton(tr("Clear List"));
    connect(clearButton, &QPushButton::clicked, this, &AudioConvertPage::onClearList);
    buttonRow->addWidget(clearButton);
    auto* convertButton = new QPushButton(tr("Convert"));
    connect(convertButton, &QPushButton::clicked, this, &AudioConvertPage::onConvert);
    buttonRow->addWidget(convertButton);
    buttonRow->addStretch();
    layout->addLayout(buttonRow);

    table_ = new QTableWidget(0, 8);
    table_->setHorizontalHeaderLabels(
        {tr("Input"), tr("Output Name"), tr("Loop"), tr("Start"), tr("End"), tr("Count"), tr("Rate"), tr("Status")});
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->verticalHeader()->setVisible(false);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(table_, 1);

    setContentWidget(content);
}

void AudioConvertPage::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void AudioConvertPage::dropEvent(QDropEvent* event) {
    for (const QUrl& url : event->mimeData()->urls()) {
        if (!url.isLocalFile()) {
            continue;
        }
        QString path = url.toLocalFile();
        if (path.endsWith(QStringLiteral(".wav"), Qt::CaseInsensitive)) {
            addWavFile(path);
        }
    }
    rebuildTable();
}

void AudioConvertPage::onBrowseOutputFolder() {
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Output Folder"), outputFolderEdit_->text());
    if (!dir.isEmpty()) {
        outputFolderEdit_->setText(dir);
    }
}

void AudioConvertPage::onAddWavs() {
    QStringList paths = QFileDialog::getOpenFileNames(this, tr("Add WAVs"), QString(), tr("WAV Files (*.wav)"));
    for (const QString& path : paths) {
        addWavFile(path);
    }
    rebuildTable();
}

void AudioConvertPage::addWavFile(const QString& path) {
    QString status;
    auto item = makeItemFromWav(std::filesystem::path(path.toStdString()), status);
    if (item) {
        items_.push_back(std::move(*item));
    } else {
        AudioSampleItem failed;
        failed.inputPath = path.toStdString();
        failed.outputName = QString::fromStdString(failed.inputPath.stem().string());
        failed.status = status;
        items_.push_back(std::move(failed));
    }
}

void AudioConvertPage::onClearList() {
    items_.clear();
    rebuildTable();
}

void AudioConvertPage::onConvert() {
    std::filesystem::path outputDir(outputFolderEdit_->text().toStdString());
    for (size_t i = 0; i < items_.size(); ++i) {
        QString status;
        convertItem(items_[i], outputDir, status);
        items_[i].status = status;
        setStatusAt(static_cast<int>(i), status);
    }
}

void AudioConvertPage::rebuildTable() {
    table_->setRowCount(static_cast<int>(items_.size()));
    for (int row = 0; row < static_cast<int>(items_.size()); ++row) {
        AudioSampleItem& item = items_[static_cast<size_t>(row)];

        table_->setItem(row, kColInput, new QTableWidgetItem(QString::fromStdString(item.inputPath.string())));

        auto* nameEdit = new QLineEdit(item.outputName);
        connect(nameEdit, &QLineEdit::textChanged, this, [this, row](const QString& text) {
            items_[static_cast<size_t>(row)].outputName = text;
        });
        table_->setCellWidget(row, kColOutputName, nameEdit);

        auto* loopCheck = new QCheckBox();
        loopCheck->setChecked(item.loopEnabled);
        connect(loopCheck, &QCheckBox::toggled, this,
                [this, row](bool checked) { items_[static_cast<size_t>(row)].loopEnabled = checked; });
        table_->setCellWidget(row, kColLoop, loopCheck);

        auto* startSpin = new QSpinBox();
        startSpin->setRange(0, 999999999);
        startSpin->setValue(static_cast<int>(item.loopStart));
        connect(startSpin, &QSpinBox::valueChanged, this,
                [this, row](int value) { items_[static_cast<size_t>(row)].loopStart = static_cast<uint32_t>(value); });
        table_->setCellWidget(row, kColStart, startSpin);

        auto* endSpin = new QSpinBox();
        endSpin->setRange(0, 999999999);
        endSpin->setValue(static_cast<int>(item.loopEnd));
        connect(endSpin, &QSpinBox::valueChanged, this,
                [this, row](int value) { items_[static_cast<size_t>(row)].loopEnd = static_cast<uint32_t>(value); });
        table_->setCellWidget(row, kColEnd, endSpin);

        auto* countSpin = new QSpinBox();
        countSpin->setRange(-1, 999999999);
        countSpin->setValue(item.loopCount);
        connect(countSpin, &QSpinBox::valueChanged, this,
                [this, row](int value) { items_[static_cast<size_t>(row)].loopCount = value; });
        table_->setCellWidget(row, kColCount, countSpin);

        QString rateText = QStringLiteral("%1 (%2) / %3")
                                .arg(item.sampleRate)
                                .arg(item.tuning, 0, 'f', 4)
                                .arg(item.sampleCount);
        table_->setItem(row, kColRate, new QTableWidgetItem(rateText));

        table_->setItem(row, kColStatus, new QTableWidgetItem(item.status));
    }
}

void AudioConvertPage::setStatusAt(int row, const QString& status) {
    if (auto* cell = table_->item(row, kColStatus)) {
        cell->setText(status);
    } else {
        table_->setItem(row, kColStatus, new QTableWidgetItem(status));
    }
}

QWidget* makeAudioConvertPage(MainWindow& window) {
    return new AudioConvertPage(window);
}

} // namespace bitdeck
