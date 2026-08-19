#pragma once

#include <QColor>
#include <QFont>
#include <QString>

class QApplication;

namespace bitdeck::theme {

inline const QColor kTelegramBlue{0x30, 0xA3, 0xE6};   // primary / accent
inline const QColor kBigStone{0x0D, 0x1D, 0x32};        // background
inline const QColor kElephant{0x0D, 0x28, 0x41};        // surface
inline const QColor kWildWatermelon{0xFF, 0x53, 0x70};  // error
inline const QColor kGoldenrod{0xFF, 0xCB, 0x6B};
inline const QColor kYellowGreen{0xC3, 0xE8, 0x8D};
inline const QColor kJordyBlue{0x82, 0xB1, 0xFF};
inline const QColor kBlueBayoux{0x54, 0x6E, 0x7A};      // ephemeral bar, expanded

// Falls back to the system sans-serif if not installed.
inline const QString kFontFamily = QStringLiteral("Google Sans");

QFont displayLargeFont();   // 95pt, normal
QFont displayMediumFont();  // 59pt, normal
QFont displaySmallFont();   // 48pt, medium
QFont headlineMediumFont(); // 34pt, medium
QFont headlineSmallFont();  // 24pt, medium
QFont titleLargeFont();     // 20pt, bold
QFont titleMediumFont();    // 16pt, medium
QFont titleSmallFont();     // 14pt, bold
QFont bodyLargeFont();      // 18pt, normal
QFont bodyMediumFont();     // 14pt, normal
QFont bodySmallFont();      // 12pt, medium
QFont labelLargeFont();     // 14pt, bold
QFont labelSmallFont();     // 10pt, medium

void applyDarkTheme(QApplication& app);

} // namespace bitdeck::theme
