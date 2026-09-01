#pragma once

#include <QWidget>

#include <memory>
#include <vector>

#include "mode_controller.h"

class QComboBox;
class QPushButton;
class QSplitter;
class QStackedWidget;

namespace bitdeck {

class MainWindow;

// Owns the shell chrome: top bar (mode selector, Open, per-mode action),
// the tree/content body, and the status-bar strip. Holds one ModeController
// per mode; switching modes swaps which controller's tree/content/status
// widgets are visible via three parallel QStackedWidgets kept in lockstep
// with the mode QComboBox. An in-progress scan in one mode survives
// switching to another and back: controllers are constructed once up
// front, never recreated. Each mode owns its own staging and its own
// top-bar action entirely -- there is no cross-mode combined output.
class ShellWindow : public QWidget {
    Q_OBJECT

public:
    explicit ShellWindow(MainWindow& window, QWidget* parent = nullptr);

private:
    void onModeChanged(int index);
    void onOpenClicked();
    void onPrimaryActionClicked();
    void onSettingsClicked();
    void onInstructionsClicked();
    ModeController& currentController();

    MainWindow& window_;
    std::vector<std::unique_ptr<ModeController>> controllers_;

    QComboBox* modeCombo_;
    QPushButton* openButton_;
    QPushButton* primaryActionButton_; // label/behavior fully owned by the active mode; hidden when its label is empty
    QPushButton* instructionsButton_;
    QPushButton* settingsButton_;
    QSplitter* bodySplitter_;
    QStackedWidget* treeStack_;
    QStackedWidget* contentStack_;
    QStackedWidget* statusBarStack_;
};

} // namespace bitdeck
