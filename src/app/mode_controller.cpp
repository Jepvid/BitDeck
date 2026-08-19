#include "mode_controller.h"

#include <QWidget>

#include "main_window.h"

namespace bitdeck {

namespace {

// hasTreePane() == false, empty content/status placeholders: the wrapped
// page owns its own Open/select controls, so the shell's top-bar Open button
// and status-bar strip stay inert for this mode.
class EmbeddedPageModeController : public ModeController {
public:
    EmbeddedPageModeController(QString name, MainWindow& window, std::function<QWidget*(MainWindow&)> pageFactory)
        : ModeController(window), name_(std::move(name)), page_(pageFactory(window)) {}

    QString name() const override { return name_; }
    bool hasTreePane() const override { return false; }

    QWidget* treeWidget() override {
        if (treePlaceholder_ == nullptr) {
            treePlaceholder_ = new QWidget();
        }
        return treePlaceholder_;
    }
    QWidget* contentWidget() override { return page_; }
    QWidget* statusBarWidget() override {
        if (statusPlaceholder_ == nullptr) {
            statusPlaceholder_ = new QWidget();
        }
        return statusPlaceholder_;
    }

    void onOpenRequested() override {}

private:
    QString name_;
    QWidget* page_;
    QWidget* treePlaceholder_ = nullptr;
    QWidget* statusPlaceholder_ = nullptr;
};

} // namespace

std::unique_ptr<ModeController> makeEmbeddedPageModeController(
    const QString& name, MainWindow& window, std::function<QWidget*(MainWindow&)> pageFactory) {
    return std::make_unique<EmbeddedPageModeController>(name, window, std::move(pageFactory));
}

} // namespace bitdeck
