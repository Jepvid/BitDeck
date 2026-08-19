#include "page_frame.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

#include "theme.h"

namespace bitdeck {

PageFrame::PageFrame(QWidget* parent) : QWidget(parent) {
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(15, 15, 15, 0);
    outerLayout->setSpacing(0);

    auto* header = new QWidget(this);
    header->setFixedHeight(60);
    headerLayout_ = new QHBoxLayout(header);
    headerLayout_->setContentsMargins(0, 0, 0, 0);

    backButton_ = new QToolButton(header);
    backButton_->setText(QStringLiteral("‹")); // single left-pointing angle quotation mark
    backButton_->setAutoRaise(true);
    backButton_->setVisible(false);
    QFont backFont = backButton_->font();
    backFont.setPointSize(24);
    backButton_->setFont(backFont);
    connect(backButton_, &QToolButton::clicked, this, &PageFrame::backRequested);
    headerLayout_->addWidget(backButton_);

    auto* titleColumn = new QVBoxLayout();
    titleColumn->setSpacing(2);
    titleLabel_ = new QLabel(header);
    titleLabel_->setFont(theme::headlineSmallFont());
    titleLabel_->setStyleSheet(QStringLiteral("color: white;"));
    subtitleLabel_ = new QLabel(header);
    subtitleLabel_->setFont(theme::bodyMediumFont());
    subtitleLabel_->setStyleSheet(QStringLiteral("color: rgba(255, 255, 255, 128);"));
    subtitleLabel_->setVisible(false);
    titleColumn->addWidget(titleLabel_);
    titleColumn->addWidget(subtitleLabel_);
    headerLayout_->addLayout(titleColumn);

    headerLayout_->addStretch(1);

    topRightLayout_ = new QHBoxLayout();
    headerLayout_->addLayout(topRightLayout_);

    outerLayout->addWidget(header);

    contentLayout_ = new QVBoxLayout();
    contentLayout_->setContentsMargins(0, 0, 0, 0);
    outerLayout->addLayout(contentLayout_, 1);
}

void PageFrame::setTitle(const QString& title) {
    titleLabel_->setText(title);
}

void PageFrame::setSubtitle(const QString& subtitle) {
    subtitleLabel_->setText(subtitle);
    subtitleLabel_->setVisible(!subtitle.isEmpty());
}

void PageFrame::setBackButtonVisible(bool visible) {
    backButton_->setVisible(visible);
}

void PageFrame::setTopRightWidget(QWidget* widget) {
    if (topRightWidget_ != nullptr) {
        topRightLayout_->removeWidget(topRightWidget_);
        topRightWidget_->deleteLater();
    }
    topRightWidget_ = widget;
    if (widget != nullptr) {
        topRightLayout_->addWidget(widget);
    }
}

void PageFrame::setContentWidget(QWidget* widget) {
    if (contentWidget_ != nullptr) {
        contentLayout_->removeWidget(contentWidget_);
        contentWidget_->deleteLater();
    }
    contentWidget_ = widget;
    if (widget != nullptr) {
        contentLayout_->addWidget(widget);
    }
}

} // namespace bitdeck
