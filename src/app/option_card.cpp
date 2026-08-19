#include "option_card.h"

#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>

#include "theme.h"

namespace bitdeck {

OptionCard::OptionCard(QString glyph, QString label, QWidget* parent)
    : QWidget(parent), glyph_(std::move(glyph)) {
    setCursor(Qt::PointingHandCursor);
    setFixedWidth(kIconBoxSize);

    layout_ = new QVBoxLayout(this);
    layout_->setContentsMargins(0, 0, 0, 0);
    layout_->setSpacing(8);
    layout_->addSpacing(kIconBoxSize); // reserved for the painted icon box

    label_ = new QLabel(label, this);
    label_->setFont(theme::bodyLargeFont());
    label_->setAlignment(Qt::AlignCenter);
    label_->setStyleSheet(QStringLiteral("color: rgba(255, 255, 255, 128);"));
    label_->setWordWrap(true);
    layout_->addWidget(label_);
}

void OptionCard::setOverlayWidget(QWidget* overlay) {
    if (overlay_ != nullptr) {
        overlay_->deleteLater();
    }
    overlay_ = overlay;
    if (overlay_ != nullptr) {
        overlay_->setParent(this);
        overlay_->move(0, 0);
        overlay_->show();
    }
}

void OptionCard::paintEvent(QPaintEvent* /*event*/) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRect iconBox(0, 0, kIconBoxSize, kIconBoxSize);
    QPainterPath path;
    path.addRoundedRect(iconBox, 20, 20);
    painter.fillPath(path, hovered_ ? QColor(255, 255, 255, 61) /* white24 */
                                     : QColor(255, 255, 255, 31) /* white12 */);

    if (!glyph_.isEmpty()) {
        QFont glyphFont = theme::displaySmallFont();
        glyphFont.setPointSize(28);
        painter.setFont(glyphFont);
        painter.setPen(Qt::white);
        painter.drawText(iconBox, Qt::AlignCenter, glyph_);
    }
}

void OptionCard::enterEvent(QEnterEvent* /*event*/) {
    hovered_ = true;
    update();
    emit hoverEntered();
}

void OptionCard::leaveEvent(QEvent* /*event*/) {
    hovered_ = false;
    update();
    emit hoverLeft();
}

void OptionCard::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit clicked();
    }
}

} // namespace bitdeck
