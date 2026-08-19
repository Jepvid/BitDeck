#include "theme.h"

#include <QApplication>
#include <QPalette>

namespace bitdeck::theme {

namespace {

QFont makeFont(int pointSize, QFont::Weight weight) {
    QFont font(kFontFamily);
    font.setPointSize(pointSize);
    font.setWeight(weight);
    return font;
}

} // namespace

QFont displayLargeFont() { return makeFont(95, QFont::Normal); }
QFont displayMediumFont() { return makeFont(59, QFont::Normal); }
QFont displaySmallFont() { return makeFont(48, QFont::Medium); }
QFont headlineMediumFont() { return makeFont(34, QFont::Medium); }
QFont headlineSmallFont() { return makeFont(24, QFont::Medium); }
QFont titleLargeFont() { return makeFont(20, QFont::Bold); }
QFont titleMediumFont() { return makeFont(16, QFont::Medium); }
QFont titleSmallFont() { return makeFont(14, QFont::Bold); }
QFont bodyLargeFont() { return makeFont(18, QFont::Normal); }
QFont bodyMediumFont() { return makeFont(14, QFont::Normal); }
QFont bodySmallFont() { return makeFont(12, QFont::Medium); }
QFont labelLargeFont() { return makeFont(14, QFont::Bold); }
QFont labelSmallFont() { return makeFont(10, QFont::Medium); }

void applyDarkTheme(QApplication& app) {
    QPalette palette;
    palette.setColor(QPalette::Window, kBigStone);
    palette.setColor(QPalette::Base, kBigStone);
    palette.setColor(QPalette::AlternateBase, kElephant);
    palette.setColor(QPalette::WindowText, Qt::white);
    palette.setColor(QPalette::Text, Qt::white);
    palette.setColor(QPalette::Button, kElephant);
    palette.setColor(QPalette::ButtonText, Qt::white);
    palette.setColor(QPalette::Highlight, kTelegramBlue);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::ToolTipBase, kElephant);
    palette.setColor(QPalette::ToolTipText, Qt::white);
    app.setPalette(palette);
    app.setFont(bodyMediumFont());
}

} // namespace bitdeck::theme
