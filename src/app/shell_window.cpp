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

#include "config_dialog.h"
#include "finalize_dialog.h"
#include "main_window.h"
#include "../features/common/not_implemented_page.h"
#include "../features/custom_sequences/custom_sequences_controller.h"
#include "../features/debug_convert_textures/debug_convert_textures_page.h"
#include "../features/inspect_otr/inspect_otr_controller.h"
#include "../features/make_texture_pack/make_texture_pack_controller.h"
#include "../features/pack_mod/pack_mod_controller.h"
#include "../features/soh_audiotool/audio_convert_page.h"

namespace bitdeck {

ShellWindow::ShellWindow(MainWindow& window, QWidget* parent) : QWidget(parent), window_(window) {
    controllers_.push_back(std::make_unique<PackModController>(window_));
    controllers_.push_back(std::make_unique<MakeTexturePackController>(window_));
    controllers_.push_back(std::make_unique<CustomSequencesController>(window_));
    controllers_.push_back(std::make_unique<InspectOtrController>(window_));
    controllers_.push_back(makeEmbeddedPageModeController(QStringLiteral("SoH Audio Tool"), window_,
                                                            &makeAudioConvertPage, /*showPrimaryAction=*/false));
    controllers_.push_back(makeEmbeddedPageModeController(QStringLiteral("Debug: Convert Textures"), window_,
                                                            &makeDebugConvertTexturesPage));
    controllers_.push_back(makeEmbeddedPageModeController(
        QStringLiteral("Debug: Font Generator"), window_,
        [](MainWindow& w) { return makeNotImplementedPage(w, QStringLiteral("Font Generator")); }));

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

    settingsButton_ = new QPushButton(tr("Settings"));
    connect(settingsButton_, &QPushButton::clicked, this, &ShellWindow::onSettingsClicked);
    topBarLayout->addWidget(settingsButton_);

    layout->addWidget(topBar);

    bodySplitter_ = new QSplitter(Qt::Horizontal);
    treeStack_ = new QStackedWidget();
    contentStack_ = new QStackedWidget();
    for (const auto& controller : controllers_) {
        treeStack_->addWidget(controller->treeWidget());
        contentStack_->addWidget(controller->contentWidget());
    }
    // Lets the splitter be dragged down to zero width on either side.
    treeStack_->setMinimumWidth(0);
    contentStack_->setMinimumWidth(0);
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
    primaryActionButton_->setVisible(controller.showsPrimaryAction());
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

    // Swaps the status bar to a progress meter for the duration of the
    // synchronous rescan, then restores it.
    int previousStatusIndex = statusBarStack_->currentIndex();
    statusBarStack_->setCurrentIndex(scanningStatusIndex_);
    QCoreApplication::processEvents();

    controller.rescanBeforeFinalize();

    statusBarStack_->setCurrentIndex(previousStatusIndex);

    FinalizeDialog dialog(window_, this);
    dialog.exec();
}

void ShellWindow::onSettingsClicked() {
    ConfigDialog dialog(this);
    dialog.exec();
}

ModeController& ShellWindow::currentController() {
    return *controllers_[modeCombo_->currentIndex()];
}

} // namespace bitdeck
