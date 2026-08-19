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

// Owns the shell chrome: top bar (mode selector, Open, Finalize Mod), the
// tree/content body, and the status-bar strip. Holds one ModeController per
// mode; switching modes swaps which controller's tree/content/status
// widgets are visible via three parallel QStackedWidgets kept in lockstep
// with the mode QComboBox. Controllers are constructed once up front, so an
// in-progress scan in one mode survives switching to another and back.
class ShellWindow : public QWidget {
    Q_OBJECT

public:
    explicit ShellWindow(MainWindow& window, QWidget* parent = nullptr);

private:
    void onModeChanged(int index);
    void onOpenClicked();
    void onFinalizeClicked();
    ModeController& currentController();

    MainWindow& window_;
    std::vector<std::unique_ptr<ModeController>> controllers_;

    QComboBox* modeCombo_;
    QPushButton* openButton_;
    QPushButton* finalizeButton_;
    QSplitter* bodySplitter_;
    QStackedWidget* treeStack_;
    QStackedWidget* contentStack_;
    QStackedWidget* statusBarStack_;

    // An extra statusBarStack_ page (index == controllers_.size(), appended
    // after every mode's own page) shown in place of the active mode's
    // status bar while Finalize Mod's pre-scan runs -- a QProgressDialog
    // here was unreliable (a brand-new top-level window needs the window
    // manager to map/expose it before a single processEvents() call is
    // guaranteed to paint it); swapping a page already inside the visible
    // main window has no such dependency.
    int scanningStatusIndex_ = -1;
};

} // namespace bitdeck
