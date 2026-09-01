#pragma once

#include <QWidget>

class QListWidget;
class QComboBox;
class QCheckBox;
class QPushButton;
class QLabel;

namespace bitdeck {

class StagingModel;

// Content shown inside FinalizeDialog: the staged-entries list,
// output-format picker, and the Generate button that actually calls
// generateArchive() in the background. Operates on whichever StagingModel
// it's given -- each mode passes its own. showPrependAlt hides the
// "Prepend alt/" toggle for modes it has no effect on (only
// addCustomTexturesEntry in archive_generator.cpp reads it).
class FinishPanel : public QWidget {
    Q_OBJECT

public:
    explicit FinishPanel(StagingModel& stagingModel, bool showPrependAlt, QWidget* parent = nullptr);

signals:
    void archiveGenerated();

private:
    void refresh();
    void onGenerate();

    StagingModel& stagingModel_;
    QListWidget* entriesList_;
    QComboBox* extensionCombo_;
    QCheckBox* prependAltCheckbox_ = nullptr;
    QCheckBox* compressCheckbox_;
    QPushButton* generateButton_;
    QLabel* statusLabel_;
};

} // namespace bitdeck
