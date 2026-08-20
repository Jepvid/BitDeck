#pragma once

#include <QDialog>

class QSpinBox;

namespace bitdeck {

// Reads the persisted UI font point size (QSettings key
// "appearance/fontPointSize"), or a sensible default if never set. Called at
// startup (main.cpp) and by ConfigDialog itself.
int loadConfiguredFontPointSize();

// Settings window, opened from the shell's top bar. Currently just the UI
// font size; changes apply immediately (QApplication::setFont) and persist
// via QSettings.
class ConfigDialog : public QDialog {
    Q_OBJECT

public:
    explicit ConfigDialog(QWidget* parent = nullptr);

private:
    void onFontSizeChanged(int pointSize);

    QSpinBox* fontSizeSpin_;
};

} // namespace bitdeck
