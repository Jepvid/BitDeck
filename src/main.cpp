#include <QApplication>

#include "app/config_dialog.h"
#include "app/main_window.h"
#include "app/theme.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("BitDeck"));
    QCoreApplication::setApplicationName(QStringLiteral("BitDeck"));
    bitdeck::theme::applyDarkTheme(app);

    QFont font = app.font();
    font.setPointSize(bitdeck::loadConfiguredFontPointSize());
    app.setFont(font);

    bitdeck::MainWindow window;
    window.setWindowTitle(QStringLiteral("BitDeck"));
    window.resize(900, 700);
    window.setMinimumSize(700, 700);

    window.show();
    return app.exec();
}
