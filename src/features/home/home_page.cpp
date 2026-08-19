#include "home_page.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

#include <bitdeck/version.h>

#include "../../app/main_window.h"
#include "../../app/option_card.h"
#include "../../app/page_frame.h"

namespace bitdeck {

QWidget* makeHomePage(MainWindow& window) {
    auto* frame = new PageFrame();
    frame->setTitle(QStringLiteral("BitDeck"));

    auto* content = new QWidget();
    auto* layout = new QVBoxLayout(content);
    layout->addStretch(1);

    auto* cardsRow = new QWidget();
    auto* cardsLayout = new QHBoxLayout(cardsRow);
    cardsLayout->setAlignment(Qt::AlignCenter);
    cardsLayout->setSpacing(24);

    auto* createCard = new OptionCard(QStringLiteral("+"), QObject::tr("Create"));
    QObject::connect(createCard, &OptionCard::clicked, &window,
                      [&window] { window.navigateTo(QStringLiteral("create_selection")); });
    cardsLayout->addWidget(createCard);

    auto* inspectCard = new OptionCard(QStringLiteral("?"), QObject::tr("Inspect"));
    QObject::connect(inspectCard, &OptionCard::clicked, &window,
                      [&window] { window.navigateTo(QStringLiteral("inspect_otr")); });
    cardsLayout->addWidget(inspectCard);

    auto* debugCard = new OptionCard(QStringLiteral("!"), QObject::tr("Debug"));
    QObject::connect(debugCard, &OptionCard::clicked, &window,
                      [&window] { window.navigateTo(QStringLiteral("debug_selection")); });
    cardsLayout->addWidget(debugCard);

    layout->addWidget(cardsRow);
    layout->addStretch(1);

    auto* footer = new QLabel(QStringLiteral("%1 / %2 (%3)").arg(kGitBranch, kGitCommitHash, kGitCommitDate));
    footer->setAlignment(Qt::AlignCenter);
    footer->setStyleSheet(QStringLiteral("color: rgba(255, 255, 255, 96);"));
    layout->addWidget(footer);

    frame->setContentWidget(content);
    return frame;
}

} // namespace bitdeck
