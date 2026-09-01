#include "shell_window.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "config_dialog.h"
#include "instructions_dialog.h"
#include "main_window.h"
#include "../features/common/not_implemented_page.h"
#include "../features/custom_sequences/custom_sequences_controller.h"
#include "../features/debug_convert_textures/debug_convert_textures_page.h"
#include "../features/inspect_otr/inspect_otr_controller.h"
#include "../features/make_texture_pack/make_texture_pack_controller.h"
#include "../features/pack_mod/pack_mod_controller.h"

namespace bitdeck {

ShellWindow::ShellWindow(MainWindow& window, QWidget* parent) : QWidget(parent), window_(window) {
    controllers_.push_back(std::make_unique<PackModController>(window_));
    controllers_.push_back(std::make_unique<MakeTexturePackController>(window_));
    controllers_.push_back(std::make_unique<CustomSequencesController>(window_));
    controllers_.push_back(std::make_unique<InspectOtrController>(window_));
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
    const QStringList modeTooltips = {
        tr("Pack folder content as is."),
        tr("Match replacement images against a manifest, or extract textures to start editing."),
        tr("Stage .seq audio files paired with .meta sidecars."),
        tr("Browse an OTR/O2R archive's contents, read-only."),
        tr("Round-trip PNG and N64 texture formats for testing."),
        tr("Not yet implemented."),
    };

    modeCombo_ = new QComboBox();
    for (size_t i = 0; i < controllers_.size(); ++i) {
        modeCombo_->addItem(controllers_[i]->name());
        if (i < static_cast<size_t>(modeTooltips.size())) {
            modeCombo_->setItemData(static_cast<int>(i), modeTooltips[static_cast<int>(i)], Qt::ToolTipRole);
        }
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

    instructionsButton_ = new QPushButton(tr("?"));
    instructionsButton_->setToolTip(tr("Instructions: what each mode is for and how to build a mod."));
    instructionsButton_->setFixedWidth(28);
    connect(instructionsButton_, &QPushButton::clicked, this, &ShellWindow::onInstructionsClicked);
    topBarLayout->addWidget(instructionsButton_);

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

    layout->addWidget(statusBarStack_);

    onModeChanged(0);
}

void ShellWindow::onModeChanged(int index) {
    treeStack_->setCurrentIndex(index);
    contentStack_->setCurrentIndex(index);
    statusBarStack_->setCurrentIndex(index);

    ModeController& controller = *controllers_[index];
    openButton_->setText(controller.openButtonLabel());
    QString primaryLabel = controller.primaryActionLabel();
    primaryActionButton_->setText(primaryLabel);
    primaryActionButton_->setVisible(!primaryLabel.isEmpty());
    treeStack_->setVisible(controller.hasTreePane());
}

void ShellWindow::onOpenClicked() {
    currentController().onOpenRequested();
}

void ShellWindow::onPrimaryActionClicked() {
    currentController().onPrimaryActionRequested();
}

void ShellWindow::onSettingsClicked() {
    ConfigDialog dialog(this);
    dialog.exec();
}

void ShellWindow::onInstructionsClicked() {
    InstructionsDialog dialog(this);
    dialog.exec();
}

ModeController& ShellWindow::currentController() {
    return *controllers_[modeCombo_->currentIndex()];
}

} // namespace bitdeck
