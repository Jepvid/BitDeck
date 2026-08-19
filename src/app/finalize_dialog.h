#pragma once

#include <QDialog>

namespace bitdeck {

class MainWindow;
class FinishPanel;

// Hosts the existing FinishPanel (staged-entries list, extension picker,
// Generate button) as a modal dialog, opened by the shell's top-bar
// "Finalize Mod" button. Closes itself once generation succeeds.
class FinalizeDialog : public QDialog {
    Q_OBJECT

public:
    explicit FinalizeDialog(MainWindow& window, QWidget* parent = nullptr);

private:
    FinishPanel* panel_;
};

} // namespace bitdeck
