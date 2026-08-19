#include "numpad.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "theme.h"

namespace bitdeck {

Numpad::Numpad(const QString& title, double initialValue, double step, QWidget* parent)
    : QWidget(parent), value_(initialValue), step_(step) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(4);

    if (!title.isEmpty()) {
        auto* titleLabel = new QLabel(title, this);
        titleLabel->setFont(theme::labelLargeFont());
        titleLabel->setStyleSheet(QStringLiteral("color: white;"));
        outer->addWidget(titleLabel);
    }

    auto* row = new QHBoxLayout();
    row->setSpacing(8);

    auto* minusButton = new QPushButton(QStringLiteral("-"), this);
    connect(minusButton, &QPushButton::clicked, this, [this] { setValue(value_ - step_); });
    row->addWidget(minusButton);

    valueLabel_ = new QLabel(QString::number(value_), this);
    valueLabel_->setAlignment(Qt::AlignCenter);
    valueLabel_->setFont(theme::bodyMediumFont());
    valueLabel_->setStyleSheet(QStringLiteral("color: white;"));
    valueLabel_->setMinimumWidth(48);
    row->addWidget(valueLabel_);

    auto* plusButton = new QPushButton(QStringLiteral("+"), this);
    connect(plusButton, &QPushButton::clicked, this, [this] { setValue(value_ + step_); });
    row->addWidget(plusButton);

    outer->addLayout(row);
}

void Numpad::setValue(double value) {
    value_ = value;
    valueLabel_->setText(QString::number(value_));
    emit valueChanged(value_);
}

} // namespace bitdeck
