#include "ephemeral_bar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QVBoxLayout>

#include <bitdeck/version.h>

#include "staging_model.h"
#include "theme.h"

namespace bitdeck {

EphemeralBar::EphemeralBar(StagingModel* stagingModel, QWidget* parent)
    : QWidget(parent), stagingModel_(stagingModel) {
    setAutoFillBackground(true);
    setFixedHeight(kCollapsedHeight);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    collapsedRow_ = new QWidget(this);
    collapsedRow_->setFixedHeight(kCollapsedHeight);
    auto* collapsedLayout = new QHBoxLayout(collapsedRow_);
    collapsedLayout->setContentsMargins(8, 0, 8, 0);

    stateLabel_ = new QLabel(collapsedRow_);
    stateLabel_->setFont(theme::labelSmallFont());
    stateLabel_->setStyleSheet(QStringLiteral("color: white;"));
    collapsedLayout->addWidget(stateLabel_);

    collapsedLayout->addStretch(1);

    finalizeButton_ = new QPushButton(collapsedRow_);
    finalizeButton_->setFlat(true);
    connect(finalizeButton_, &QPushButton::clicked, this, [this] {
        setExpanded(true);
        emit generateRequested();
    });
    collapsedLayout->addWidget(finalizeButton_);

    collapsedLayout->addStretch(1);

    versionLabel_ = new QLabel(tr("BitDeck: %1").arg(kVersionName), collapsedRow_);
    versionLabel_->setFont(theme::labelSmallFont());
    versionLabel_->setStyleSheet(QStringLiteral("color: white;"));
    collapsedLayout->addWidget(versionLabel_);

    linkButton_ = new QPushButton(QStringLiteral("↗"), collapsedRow_); // north-east arrow
    linkButton_->setFlat(true);
    connect(linkButton_, &QPushButton::clicked, this, &EphemeralBar::linkRequested);
    collapsedLayout->addWidget(linkButton_);

    outer->addWidget(collapsedRow_);

    expandedContainer_ = new QWidget(this);
    expandedLayout_ = new QVBoxLayout(expandedContainer_);
    expandedLayout_->setContentsMargins(0, 0, 0, 0);
    expandedContainer_->setVisible(false);
    outer->addWidget(expandedContainer_, 1);

    animation_ = new QPropertyAnimation(this, "barHeight", this);
    animation_->setDuration(500);
    animation_->setEasingCurve(QEasingCurve::OutCubic);
    connect(animation_, &QPropertyAnimation::finished, this, [this] {
        if (!expanded_) {
            expandedContainer_->setVisible(false);
        }
    });

    connect(stagingModel_, &StagingModel::changed, this, &EphemeralBar::refresh);
    refresh();
}

void EphemeralBar::setBarHeight(int height) {
    setFixedHeight(height);
}

void EphemeralBar::setExpandedContentWidget(QWidget* widget) {
    if (expandedContentWidget_ != nullptr) {
        expandedLayout_->removeWidget(expandedContentWidget_);
        expandedContentWidget_->deleteLater();
    }
    expandedContentWidget_ = widget;
    if (widget != nullptr) {
        expandedLayout_->addWidget(widget);
    }
}

void EphemeralBar::setExpanded(bool expanded) {
    if (expanded_ == expanded) {
        return;
    }
    expanded_ = expanded;
    stagingModel_->ephemeralBarExpanded = expanded;

    if (expanded) {
        expandedContainer_->setVisible(true);
    }
    finalizeButton_->setVisible(!expanded);
    versionLabel_->setVisible(!expanded);
    linkButton_->setVisible(!expanded);

    animation_->stop();
    animation_->setStartValue(height());
    animation_->setEndValue(expanded ? kExpandedHeight : kCollapsedHeight);
    animation_->start();

    updateBackground();
}

void EphemeralBar::toggleExpanded() {
    setExpanded(!expanded_);
}

void EphemeralBar::refresh() {
    stateLabel_->setText(stagingModel_->state() == AppState::ChangesStaged ? tr("state/changesStaged")
                                                                            : tr("state/none"));
    bool hasStaged = stagingModel_->state() == AppState::ChangesStaged;
    finalizeButton_->setText(tr("Finalize .%1").arg(stagingModel_->outputExtension));
    finalizeButton_->setVisible(!expanded_ && hasStaged);
    updateBackground();
}

void EphemeralBar::updateBackground() {
    QColor background = expanded_ ? theme::kBlueBayoux
                                   : (stagingModel_->state() == AppState::ChangesStaged ? theme::kYellowGreen
                                                                                         : theme::kTelegramBlue);
    QPalette palette = this->palette();
    palette.setColor(QPalette::Window, background);
    setPalette(palette);
}

} // namespace bitdeck
