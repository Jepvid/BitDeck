#include "create_selection_page.h"

#include <QHBoxLayout>
#include <functional>
#include <vector>

#include "../../app/main_window.h"
#include "../../app/option_card.h"
#include "../../app/page_frame.h"
#include "../common/not_implemented_page.h"

namespace bitdeck {

namespace {

struct MenuEntry {
    QString glyph;
    QString label;
    std::function<void()> onClick;
};

QWidget* makeMenuPage(const QString& title, const std::vector<MenuEntry>& entries, MainWindow& window) {
    auto* frame = new PageFrame();
    frame->setTitle(title);
    frame->setBackButtonVisible(true);
    QObject::connect(frame, &PageFrame::backRequested, &window, &MainWindow::goBack);

    auto* content = new QWidget();
    auto* layout = new QHBoxLayout(content);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(24);

    for (const auto& entry : entries) {
        auto* card = new OptionCard(entry.glyph, entry.label);
        QObject::connect(card, &OptionCard::clicked, &window, entry.onClick);
        layout->addWidget(card);
    }

    frame->setContentWidget(content);
    return frame;
}

} // namespace

QWidget* makeCreateSelectionPage(MainWindow& window) {
    return makeMenuPage(
        QObject::tr("Create"),
        {
            {QStringLiteral("T"), QObject::tr("Make Texture Pack"),
             [&window] { window.navigateTo(QStringLiteral("create_replace_textures")); }},
            {QStringLiteral("G"), QObject::tr("Games"), [&window] { window.navigateTo(QStringLiteral("game_selection")); }},
            {QStringLiteral("C"), QObject::tr("Custom"), [&window] { window.navigateTo(QStringLiteral("create_custom")); }},
        },
        window);
}

QWidget* makeGameSelectionPage(MainWindow& window) {
    // Retro's "2Ship" card is dead/miswired in the source app. Routes to a
    // placeholder pending real 2Ship support.
    return makeMenuPage(
        QObject::tr("Games"),
        {
            {QStringLiteral("S"), QObject::tr("SOH"), [&window] { window.navigateTo(QStringLiteral("game_selection_soh")); }},
            {QStringLiteral("2"), QObject::tr("2Ship"), [&window] { window.navigateTo(QStringLiteral("game_selection_2ship")); }},
        },
        window);
}

QWidget* makeSohPage(MainWindow& window) {
    return makeMenuPage(
        QObject::tr("SOH"),
        {
            {QStringLiteral("M"), QObject::tr("Custom Sequences"),
             [&window] { window.navigateTo(QStringLiteral("create_custom_sequences")); }},
            {QStringLiteral("F"), QObject::tr("Font Generator"),
             [&window] { window.navigateTo(QStringLiteral("debug_generate_font")); }},
        },
        window);
}

} // namespace bitdeck
