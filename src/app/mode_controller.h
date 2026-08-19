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

protected:
    MainWindow& window_;
};

// Wraps an existing PageFrame-returning factory (makeDebugConvertTexturesPage,
// makeNotImplementedPage) as a tree-less, status-bar-less ModeController, for
// screens this redesign leaves embedded as-is.
std::unique_ptr<ModeController> makeEmbeddedPageModeController(
    const QString& name, MainWindow& window, std::function<QWidget*(MainWindow&)> pageFactory);

} // namespace bitdeck
