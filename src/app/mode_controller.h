#pragma once

#include <QObject>
#include <QString>

#include <functional>
#include <memory>

class QWidget;

namespace bitdeck {

class MainWindow;

// One instance per shell mode (Pack Mod, Make Texture Pack, ...). Owns that
// mode's tree pane, content pane, and status-bar strip, plus the
// select/scan/stage logic behind them. ShellWindow constructs one of each up
// front and swaps which one is visible on mode selection -- a controller is
// never recreated, so in-progress scan state survives a mode switch.
class ModeController : public QObject {
    Q_OBJECT

public:
    explicit ModeController(MainWindow& window) : window_(window) {}
    ~ModeController() override = default;

    virtual QString name() const = 0;
    virtual QString openButtonLabel() const { return QObject::tr("Open Folder..."); }
    virtual bool hasTreePane() const { return true; }

    virtual QWidget* treeWidget() = 0;
    virtual QWidget* contentWidget() = 0;
    virtual QWidget* statusBarWidget() = 0;

    // Routed here from the top bar's "Open file/folder" button when this
    // mode is active.
    virtual void onOpenRequested() = 0;

    // Called on the active mode just before Finalize Mod opens its dialog.
    // No-op by default -- folder-backed modes override it to rerun their
    // rescan-and-stage logic.
    virtual void rescanBeforeFinalize() {}

    // Label and behavior for the top bar's right-side action button.
    // Default: "Finalize Mod", handled by ShellWindow via
    // rescanBeforeFinalize() + FinalizeDialog. A mode can replace both --
    // return false from usesFinalizeFlow() and override primaryActionLabel()
    // and onPrimaryActionRequested() (e.g. Inspect OTR's "Extract OTR/O2R").
    virtual QString primaryActionLabel() const { return QObject::tr("Finalize Mod"); }
    virtual bool usesFinalizeFlow() const { return true; }
    virtual void onPrimaryActionRequested() {}

    // False hides the top bar's right-side action button entirely, for a
    // mode with no use for it at all (e.g. SoH Audio Tool, which writes
    // loose files directly and has its own Convert button).
    virtual bool showsPrimaryAction() const { return true; }

protected:
    MainWindow& window_;
};

// Wraps an existing PageFrame-returning factory (makeDebugConvertTexturesPage,
// makeNotImplementedPage) as a tree-less, status-bar-less ModeController, for
// screens this redesign leaves embedded as-is. showPrimaryAction hides the
// top bar's right-side action button when false.
std::unique_ptr<ModeController> makeEmbeddedPageModeController(
    const QString& name, MainWindow& window, std::function<QWidget*(MainWindow&)> pageFactory,
    bool showPrimaryAction = true);

} // namespace bitdeck
