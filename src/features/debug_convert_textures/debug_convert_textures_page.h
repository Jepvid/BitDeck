#pragma once

#include <filesystem>
#include <vector>

#include "../../app/page_frame.h"
#include "../../core/types/texture.h"

class QComboBox;
class QLabel;
class QPushButton;

namespace bitdeck {

class MainWindow;

// Raw PNG<->N64 texture round-trip viewer: convert a PNG into a chosen
// TextureType and back (proving the codec), or load a raw OTR resource file
// directly and preview it as decoded pixels.
class DebugConvertTexturesPage : public PageFrame {
    Q_OBJECT

public:
    explicit DebugConvertTexturesPage(MainWindow& window);

private:
    void onSelectPngSource();
    void onConvert();
    void onLoadTlut();
    void onSaveAs();
    void onSelectRawFile();
    void onLoadRaw();
    void updatePreview();
    void setStatus(const QString& text);

    MainWindow& window_;

    QComboBox* textureTypeCombo_;
    QLabel* pngPathLabel_;
    QPushButton* convertButton_;
    QPushButton* loadTlutButton_;
    QPushButton* saveAsButton_;

    QLabel* rawPathLabel_;
    QPushButton* loadRawButton_;

    QLabel* previewLabel_;
    QLabel* statusLabel_;

    std::filesystem::path selectedPngPath_;
    std::filesystem::path selectedRawPath_;

    bool hasResult_ = false;
    bool resultIsPalette_ = false;
    RgbaImage currentDecoded_;
    std::vector<uint8_t> currentPreviewPng_;
};

QWidget* makeDebugConvertTexturesPage(MainWindow& window);

} // namespace bitdeck
