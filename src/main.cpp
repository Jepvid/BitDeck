#include <QApplication>

#include "app/main_window.h"
#include "app/theme.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    bitdeck::theme::applyDarkTheme(app);

    bitdeck::MainWindow window;
    window.setWindowTitle(QStringLiteral("BitDeck"));
    window.resize(900, 700);
    window.setMinimumSize(700, 700);

    window.show();
    return app.exec();
}
