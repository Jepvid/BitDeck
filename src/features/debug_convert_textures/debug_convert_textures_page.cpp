#include "debug_convert_textures_page.h"

#include <QComboBox>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

#include <fstream>

#include "../../app/main_window.h"
#include "../../core/image_codec.h"
#include "../../core/resource.h"
#include "../../core/resource_type.h"
#include "../../core/types/background.h"

namespace bitdeck {

namespace {

std::vector<uint8_t> readFileBytes(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

void writeFileBytes(const std::filesystem::path& path, const std::vector<uint8_t>& data) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

struct TextureTypeOption {
    TextureType type;
    const char* label;
};

// All types except Error, matching Retro's type dropdown.
constexpr TextureTypeOption kTextureTypeOptions[] = {
    {TextureType::RGBA32bpp, "RGBA32bpp"},
    {TextureType::RGBA16bpp, "RGBA16bpp"},
    {TextureType::Palette4bpp, "Palette4bpp"},
    {TextureType::Palette8bpp, "Palette8bpp"},
    {TextureType::Grayscale4bpp, "Grayscale4bpp"},
    {TextureType::Grayscale8bpp, "Grayscale8bpp"},
    {TextureType::GrayscaleAlpha4bpp, "GrayscaleAlpha4bpp"},
    {TextureType::GrayscaleAlpha8bpp, "GrayscaleAlpha8bpp"},
    {TextureType::GrayscaleAlpha16bpp, "GrayscaleAlpha16bpp"},
};

} // namespace

DebugConvertTexturesPage::DebugConvertTexturesPage(MainWindow& window) : window_(window) {
    setTitle(QStringLiteral("Convert Textures"));
    setSubtitle(tr("Experimental -- may not work"));

    auto* content = new QWidget();
    auto* mainLayout = new QHBoxLayout(content);

    // -- Left: PNG -> N64 --
    auto* pngPanel = new QVBoxLayout();
    pngPanel->addWidget(new QLabel(tr("PNG to N64")));

    textureTypeCombo_ = new QComboBox();
    for (const auto& option : kTextureTypeOptions) {
        textureTypeCombo_->addItem(QString::fromLatin1(option.label), static_cast<int>(option.type));
    }
    pngPanel->addWidget(textureTypeCombo_);

    pngPathLabel_ = new QLabel(tr("No file selected"));
    pngPathLabel_->setStyleSheet(QStringLiteral("color: white;"));
    pngPanel->addWidget(pngPathLabel_);

    auto* selectPngButton = new QPushButton(tr("Select Texture File"));
    connect(selectPngButton, &QPushButton::clicked, this, &DebugConvertTexturesPage::onSelectPngSource);
    pngPanel->addWidget(selectPngButton);

    convertButton_ = new QPushButton(tr("Convert Texture"));
    convertButton_->setEnabled(false);
    connect(convertButton_, &QPushButton::clicked, this, &DebugConvertTexturesPage::onConvert);
    pngPanel->addWidget(convertButton_);

    loadTlutButton_ = new QPushButton(tr("Load TLUT"));
    loadTlutButton_->setVisible(false);
    connect(loadTlutButton_, &QPushButton::clicked, this, &DebugConvertTexturesPage::onLoadTlut);
    pngPanel->addWidget(loadTlutButton_);

    saveAsButton_ = new QPushButton(tr("Save As"));
    saveAsButton_->setEnabled(false);
    connect(saveAsButton_, &QPushButton::clicked, this, &DebugConvertTexturesPage::onSaveAs);
    pngPanel->addWidget(saveAsButton_);

    pngPanel->addStretch(1);
    mainLayout->addLayout(pngPanel);

    // -- Middle: raw OTR resource viewer --
    auto* rawPanel = new QVBoxLayout();
    rawPanel->addWidget(new QLabel(tr("OTR N64 Viewer")));

    rawPathLabel_ = new QLabel(tr("No file selected"));
    rawPathLabel_->setStyleSheet(QStringLiteral("color: white;"));
    rawPanel->addWidget(rawPathLabel_);

    auto* selectRawButton = new QPushButton(tr("Select Texture File"));
    connect(selectRawButton, &QPushButton::clicked, this, &DebugConvertTexturesPage::onSelectRawFile);
    rawPanel->addWidget(selectRawButton);

    loadRawButton_ = new QPushButton(tr("Load Texture"));
    loadRawButton_->setEnabled(false);
    connect(loadRawButton_, &QPushButton::clicked, this, &DebugConvertTexturesPage::onLoadRaw);
    rawPanel->addWidget(loadRawButton_);

    rawPanel->addStretch(1);
    mainLayout->addLayout(rawPanel);

    // -- Right: preview --
    auto* previewPanel = new QVBoxLayout();
    previewLabel_ = new QLabel();
    previewLabel_->setMinimumSize(256, 256);
    previewLabel_->setAlignment(Qt::AlignCenter);
    previewLabel_->setStyleSheet(QStringLiteral("background-color: #000; color: white;"));
    previewLabel_->setText(tr("No preview"));
    previewPanel->addWidget(previewLabel_, 1);

    statusLabel_ = new QLabel();
    statusLabel_->setStyleSheet(QStringLiteral("color: white;"));
    previewPanel->addWidget(statusLabel_);

    mainLayout->addLayout(previewPanel, 1);

    setContentWidget(content);
}

void DebugConvertTexturesPage::setStatus(const QString& text) {
    statusLabel_->setText(text);
}

void DebugConvertTexturesPage::onSelectPngSource() {
    QString path = QFileDialog::getOpenFileName(this, tr("Select Texture File"), QString(), tr("Images (*.png)"));
    if (path.isEmpty()) {
        return;
    }
    selectedPngPath_ = std::filesystem::path(path.toStdString());
    pngPathLabel_->setText(path);
    convertButton_->setEnabled(true);
}

void DebugConvertTexturesPage::onConvert() {
    if (selectedPngPath_.empty()) {
        return;
    }

    RgbaImage source;
    try {
        source = decodePng(readFileBytes(selectedPngPath_));
    } catch (const std::exception& e) {
        setStatus(tr("Failed to decode PNG: %1").arg(QString::fromStdString(e.what())));
        return;
    }

    auto type = static_cast<TextureType>(textureTypeCombo_->currentData().toInt());
    bool typeNeedsPalette = (type == TextureType::Palette4bpp || type == TextureType::Palette8bpp);
    if (typeNeedsPalette && !source.withPalette) {
        setStatus(tr("Selected type requires an indexed (palette) PNG"));
        return;
    }

    try {
        std::vector<uint8_t> texData = encodeN64Texture(source, type);
        currentDecoded_ = decodeN64Texture(texData, type, source.width, source.height);
        resultIsPalette_ = typeNeedsPalette;
        currentPreviewPng_ = encodePng(currentDecoded_);
        hasResult_ = true;

        loadTlutButton_->setVisible(resultIsPalette_);
        saveAsButton_->setEnabled(true);
        updatePreview();
        setStatus(tr("Converted as %1").arg(textureTypeCombo_->currentText()));
    } catch (const std::exception& e) {
        setStatus(tr("Conversion failed: %1").arg(QString::fromStdString(e.what())));
    }
}

void DebugConvertTexturesPage::onLoadTlut() {
    if (!hasResult_ || !resultIsPalette_) {
        return;
    }
    QString path = QFileDialog::getOpenFileName(this, tr("Load TLUT"), QString(), tr("Images (*.png)"));
    if (path.isEmpty()) {
        return;
    }
    try {
        RgbaImage tlut = decodePng(readFileBytes(std::filesystem::path(path.toStdString())));
        applyTlutPalette(currentDecoded_, tlut);
        currentPreviewPng_ = encodePng(currentDecoded_);
        updatePreview();
        setStatus(tr("TLUT applied"));
    } catch (const std::exception& e) {
        setStatus(tr("Failed to load TLUT: %1").arg(QString::fromStdString(e.what())));
    }
}

void DebugConvertTexturesPage::onSaveAs() {
    if (!hasResult_) {
        return;
    }
    QString path = QFileDialog::getSaveFileName(this, tr("Save As"), QStringLiteral("converted.png"), tr("PNG (*.png)"));
    if (path.isEmpty()) {
        return;
    }
    writeFileBytes(std::filesystem::path(path.toStdString()), currentPreviewPng_);
    setStatus(tr("Saved to %1").arg(path));
}

void DebugConvertTexturesPage::onSelectRawFile() {
    QString path = QFileDialog::getOpenFileName(this, tr("Select Texture File"));
    if (path.isEmpty()) {
        return;
    }
    selectedRawPath_ = std::filesystem::path(path.toStdString());
    rawPathLabel_->setText(path);
    loadRawButton_->setEnabled(true);
}

void DebugConvertTexturesPage::onLoadRaw() {
    if (selectedRawPath_.empty()) {
        return;
    }

    std::vector<uint8_t> data = readFileBytes(selectedRawPath_);
    Resource sniffer;
    sniffer.rawLoad = true;
    sniffer.open(data);

    if (sniffer.resourceType() == ResourceType::Texture) {
        Texture texture;
        texture.open(data);
        if (!texture.isValid()) {
            setStatus(tr("Not a valid Texture resource"));
            return;
        }
        currentDecoded_ =
            decodeN64Texture(texture.texData(), texture.textureType(), texture.width(), texture.height());
        resultIsPalette_ = texture.isPalette();
        currentPreviewPng_ = encodePng(currentDecoded_);
        hasResult_ = true;
        loadTlutButton_->setVisible(resultIsPalette_);
        saveAsButton_->setEnabled(true);
        updatePreview();
        setStatus(tr("Loaded texture: %1x%2").arg(texture.width()).arg(texture.height()));
    } else if (sniffer.resourceType() == ResourceType::SohBackground) {
        Background background;
        background.open(data);
        if (!background.isValid()) {
            setStatus(tr("Not a valid Background resource"));
            return;
        }
        currentDecoded_ = decodeJpeg(background.texData());
        resultIsPalette_ = false;
        currentPreviewPng_ = encodePng(currentDecoded_);
        hasResult_ = true;
        loadTlutButton_->setVisible(false);
        saveAsButton_->setEnabled(true);
        updatePreview();
        setStatus(tr("Loaded background: %1x%2").arg(currentDecoded_.width).arg(currentDecoded_.height));
    } else {
        setStatus(tr("Unsupported resource type"));
    }
}

void DebugConvertTexturesPage::updatePreview() {
    QPixmap pixmap;
    pixmap.loadFromData(currentPreviewPng_.data(), static_cast<uint>(currentPreviewPng_.size()), "PNG");
    previewLabel_->setPixmap(
        pixmap.scaled(previewLabel_->size(), Qt::KeepAspectRatio, Qt::FastTransformation));
}

QWidget* makeDebugConvertTexturesPage(MainWindow& window) {
    return new DebugConvertTexturesPage(window);
}

} // namespace bitdeck
