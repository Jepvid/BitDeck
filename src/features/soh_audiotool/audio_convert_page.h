#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

#include "../../app/page_frame.h"

class QLineEdit;
class QTableWidget;

namespace bitdeck {

class MainWindow;

// One loaded WAV, matching SoH-AudioTool's own SampleItem: an input file
// plus the output name and loop settings the user edits before conversion.
struct AudioSampleItem {
    std::filesystem::path inputPath;
    QString outputName;
    bool loopEnabled = false;
    uint32_t loopStart = 0;
    uint32_t loopEnd = 0;
    int32_t loopCount = -1;
    uint32_t sampleRate = 0;
    uint32_t sampleCount = 0;
    double tuning = 0.0;
    QString status;
};

// Port of SoH-AudioTool: convert WAV files into SoH/2Ship audio sample
// resources. Self-contained -- picks its own output folder and writes loose
// files directly on Convert, not through BitDeck's staging/Finalize flow.
class AudioConvertPage : public PageFrame {
    Q_OBJECT

public:
    explicit AudioConvertPage(MainWindow& window);

protected:
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private:
    void onBrowseOutputFolder();
    void onAddWavs();
    void onClearList();
    void onConvert();

    void addWavFile(const QString& path);
    void rebuildTable();
    void setStatusAt(int row, const QString& status);

    MainWindow& window_;
    QLineEdit* outputFolderEdit_;
    QTableWidget* table_;
    std::vector<AudioSampleItem> items_;
};

QWidget* makeAudioConvertPage(MainWindow& window);

} // namespace bitdeck
