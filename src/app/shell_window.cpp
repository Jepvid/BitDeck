#include "shell_window.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "finalize_dialog.h"
#include "main_window.h"
#include "../features/custom_sequences/custom_sequences_controller.h"
#include "../features/inspect_otr/inspect_otr_controller.h"
#include "../features/make_texture_pack/make_texture_pack_controller.h"
#include "../features/pack_mod/pack_mod_controller.h"

namespace bitdeck {

namespace {

// Stands in for a mode not yet wired into the shell. Phases 2-5 replace
// each of these with its real ModeController one at a time.
class PlaceholderModeController : public ModeController {
public:
    PlaceholderModeController(QString name, MainWindow& window) : ModeController(window), name_(std::move(name)) {}

    QString name() const override { return name_; }

    QWidget* treeWidget() override {
        if (tree_ == nullptr) {
            tree_ = new QWidget();
        }
        return tree_;
    }
    QWidget* contentWidget() override {
        if (content_ == nullptr) {
            content_ = new QLabel(QObject::tr("Coming soon"));
            content_->setAlignment(Qt::AlignCenter);
        }
        return content_;
    }
    QWidget* statusBarWidget() override {
        if (statusBar_ == nullptr) {
            statusBar_ = new QWidget();
        }
        return statusBar_;
    }
    void onOpenRequested() override {}

private:
    QString name_;
    QWidget* tree_ = nullptr;
    QLabel* content_ = nullptr;
    QWidget* statusBar_ = nullptr;
};

} // namespace

ShellWindow::ShellWindow(MainWindow& window, QWidget* parent) : QWidget(parent), window_(window) {
    controllers_.push_back(std::make_unique<PackModController>(window_));
    controllers_.push_back(std::make_unique<MakeTexturePackController>(window_));
    controllers_.push_back(std::make_unique<CustomSequencesController>(window_));
    controllers_.push_back(std::make_unique<InspectOtrController>(window_));
    controllers_.push_back(
        std::make_unique<PlaceholderModeController>(QStringLiteral("Debug: Convert Textures"), window_));
    controllers_.push_back(
        std::make_unique<PlaceholderModeController>(QStringLiteral("Debug: Font Generator"), window_));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* topBar = new QWidget();
    auto* topBarLayout = new QHBoxLayout(topBar);
    modeCombo_ = new QComboBox();
    for (const auto& controller : controllers_) {
        modeCombo_->addItem(controller->name());
    }
    connect(modeCombo_, &QComboBox::currentIndexChanged, this, &ShellWindow::onModeChanged);
    topBarLayout->addWidget(modeCombo_);

    openButton_ = new QPushButton();
    connect(openButton_, &QPushButton::clicked, this, &ShellWindow::onOpenClicked);
    topBarLayout->addWidget(openButton_);

    topBarLayout->addStretch(1);

    primaryActionButton_ = new QPushButton();
    connect(primaryActionButton_, &QPushButton::clicked, this, &ShellWindow::onPrimaryActionClicked);
    topBarLayout->addWidget(primaryActionButton_);

    layout->addWidget(topBar);

    bodySplitter_ = new QSplitter(Qt::Horizontal);
    treeStack_ = new QStackedWidget();
    contentStack_ = new QStackedWidget();
    for (const auto& controller : controllers_) {
        treeStack_->addWidget(controller->treeWidget());
        contentStack_->addWidget(controller->contentWidget());
    }
    bodySplitter_->addWidget(treeStack_);
    bodySplitter_->addWidget(contentStack_);
    bodySplitter_->setStretchFactor(1, 1);
    layout->addWidget(bodySplitter_, 1);

    statusBarStack_ = new QStackedWidget();
    for (const auto& controller : controllers_) {
        statusBarStack_->addWidget(controller->statusBarWidget());
    }

    auto* scanningStatusWidget = new QWidget();
    auto* scanningLayout = new QHBoxLayout(scanningStatusWidget);
    scanningLayout->setContentsMargins(8, 4, 8, 4);
    scanningLayout->addWidget(new QLabel(tr("Scanning for new or changed files...")));
    auto* scanningBar = new QProgressBar();
    scanningBar->setRange(0, 0); // indeterminate -- duration is unknown up front
    scanningBar->setMaximumWidth(150);
    scanningLayout->addWidget(scanningBar);
    scanningLayout->addStretch(1);
    scanningStatusIndex_ = statusBarStack_->addWidget(scanningStatusWidget);

    layout->addWidget(statusBarStack_);

    onModeChanged(0);
}

void ShellWindow::onModeChanged(int index) {
    treeStack_->setCurrentIndex(index);
    contentStack_->setCurrentIndex(index);
    statusBarStack_->setCurrentIndex(index);

    ModeController& controller = *controllers_[index];
    openButton_->setText(controller.openButtonLabel());
    primaryActionButton_->setText(controller.primaryActionLabel());
    treeStack_->setVisible(controller.hasTreePane());
}

void ShellWindow::onOpenClicked() {
    currentController().onOpenRequested();
}

void ShellWindow::onPrimaryActionClicked() {
    ModeController& controller = currentController();
    if (!controller.usesFinalizeFlow()) {
        controller.onPrimaryActionRequested();
        return;
    }

    // rescanBeforeFinalize() hashes every file in the open folder
    // synchronously (on the GUI thread) -- for a large folder that can take
    // a few seconds with no visual feedback otherwise, which reads as a
    // frozen window. Swapping the status bar to a progress meter (rather
    // than popping up a QProgressDialog) only needs a repaint of a widget
    // already inside the visible main window, not a new top-level window
    // waiting on the window manager to map/expose it.
    int previousStatusIndex = statusBarStack_->currentIndex();
    statusBarStack_->setCurrentIndex(scanningStatusIndex_);
    QCoreApplication::processEvents();

    controller.rescanBeforeFinalize();

    statusBarStack_->setCurrentIndex(previousStatusIndex);

    FinalizeDialog dialog(window_, this);
    dialog.exec();
}

ModeController& ShellWindow::currentController() {
    return *controllers_[modeCombo_->currentIndex()];
}

} // namespace bitdeck
