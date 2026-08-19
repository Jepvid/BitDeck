#pragma once

#include <QWidget>

class QListWidget;
class QComboBox;
class QPushButton;
class QLabel;

namespace bitdeck {

class MainWindow;

// Content shown inside EphemeralBar's expanded state: the staged-entries
// list, output-format picker, and the Generate button that actually calls
// generateArchive() in the background.
class FinishPanel : public QWidget {
    Q_OBJECT

public:
    explicit FinishPanel(MainWindow& window, QWidget* parent = nullptr);

signals:
    void archiveGenerated();

private:
    void refresh();
    void onGenerate();

    MainWindow& window_;
    QListWidget* entriesList_;
    QComboBox* extensionCombo_;
    QPushButton* generateButton_;
    QLabel* statusLabel_;
};

} // namespace bitdeck
