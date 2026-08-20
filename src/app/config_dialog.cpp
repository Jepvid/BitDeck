#include "config_dialog.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

namespace bitdeck {

namespace {

constexpr int kDefaultFontPointSize = 14;

QString fontPointSizeKey() {
    return QStringLiteral("appearance/fontPointSize");
}

} // namespace

int loadConfiguredFontPointSize() {
    QSettings settings;
    return settings.value(fontPointSizeKey(), kDefaultFontPointSize).toInt();
}

ConfigDialog::ConfigDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Settings"));

    auto* layout = new QVBoxLayout(this);

    auto* form = new QFormLayout();
    fontSizeSpin_ = new QSpinBox(this);
    fontSizeSpin_->setRange(6, 32);
    fontSizeSpin_->setValue(loadConfiguredFontPointSize());
    connect(fontSizeSpin_, &QSpinBox::valueChanged, this, &ConfigDialog::onFontSizeChanged);
    form->addRow(tr("Font size"), fontSizeSpin_);
    layout->addLayout(form);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &ConfigDialog::accept);
    layout->addWidget(buttons);
}

void ConfigDialog::onFontSizeChanged(int pointSize) {
    QFont font = QApplication::font();
    font.setPointSize(pointSize);
    QApplication::setFont(font);

    QSettings settings;
    settings.setValue(fontPointSizeKey(), pointSize);
}

} // namespace bitdeck
