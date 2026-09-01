#pragma once

#include <QObject>
#include <QString>

#include <functional>
#include <memory>

class QWidget;

namespace bitdeck {

class MainWindow;

// One instance per shell mode (Custom Files, Make Texture Pack, ...). Owns that
// mode's tree pane, content pane, and status-bar strip, plus the
// select/scan/stage logic behind them. In-progress scan state survives a
// mode switch: ShellWindow constructs one of each up front and swaps which
// one is visible, never recreating a controller.
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

    // Label for the top bar's right-side action button; empty hides it.
    // Each mode owns its own staging and its own action entirely (e.g. a
    // folder-backed mode's own "Export Mod" opens a FinalizeDialog over its
    // own private StagingModel, Inspect OTR's is "Extract OTR/O2R") --
    // there is no cross-mode combined output.
    virtual QString primaryActionLabel() const { return QString(); }
    virtual void onPrimaryActionRequested() {}

protected:
    MainWindow& window_;
};

// Wraps an existing PageFrame-returning factory (makeDebugConvertTexturesPage,
// makeNotImplementedPage) as a tree-less, status-bar-less ModeController, for
// screens this redesign leaves embedded as-is.
std::unique_ptr<ModeController> makeEmbeddedPageModeController(
    const QString& name, MainWindow& window, std::function<QWidget*(MainWindow&)> pageFactory);

} // namespace bitdeck
