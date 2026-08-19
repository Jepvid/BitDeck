#include <QApplication>

#include "app/ephemeral_bar.h"
#include "app/main_window.h"
#include "app/theme.h"
#include "features/common/not_implemented_page.h"
#include "features/create_custom/create_custom_page.h"
#include "features/create_custom_sequences/create_custom_sequences_page.h"
#include "features/create_replace_textures/create_replace_textures_page.h"
#include "features/create_selection/create_selection_page.h"
#include "features/debug_convert_textures/debug_convert_textures_page.h"
#include "features/debug_selection/debug_selection_page.h"
#include "features/finish/finish_panel.h"
#include "features/home/home_page.h"
#include "features/inspect_otr/inspect_otr_page.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    bitdeck::theme::applyDarkTheme(app);

    bitdeck::MainWindow window;
    window.setWindowTitle(QStringLiteral("BitDeck"));
    window.resize(900, 700);
    window.setMinimumSize(700, 700);

    window.ephemeralBar().setExpandedContentWidget(new bitdeck::FinishPanel(window));

    window.registerPage(QStringLiteral("home"), bitdeck::makeHomePage);
    window.registerPage(QStringLiteral("create_selection"), bitdeck::makeCreateSelectionPage);
    window.registerPage(QStringLiteral("game_selection"), bitdeck::makeGameSelectionPage);
    window.registerPage(QStringLiteral("game_selection_soh"), bitdeck::makeSohPage);
    window.registerPage(QStringLiteral("game_selection_2ship"), [](bitdeck::MainWindow& w) {
        return bitdeck::makeNotImplementedPage(w, QStringLiteral("2Ship"));
    });
    window.registerPage(QStringLiteral("create_custom"), bitdeck::makeCreateCustomPage);
    window.registerPage(QStringLiteral("create_custom_sequences"), bitdeck::makeCreateCustomSequencesPage);
    window.registerPage(QStringLiteral("create_replace_textures"), bitdeck::makeCreateReplaceTexturesPage);
    window.registerPage(QStringLiteral("inspect_otr"), bitdeck::makeInspectOtrPage);
    window.registerPage(QStringLiteral("debug_selection"), bitdeck::makeDebugSelectionPage);
    window.registerPage(QStringLiteral("debug_convert_textures"), bitdeck::makeDebugConvertTexturesPage);

    // Font Generator: needs Retro's ~256-entry SoH font character table (not yet ported).
    window.registerPage(QStringLiteral("debug_generate_font"), [](bitdeck::MainWindow& w) {
        return bitdeck::makeNotImplementedPage(w, QStringLiteral("Font Generator"));
    });

    window.navigateTo(QStringLiteral("home"));

    window.show();
    return app.exec();
}
