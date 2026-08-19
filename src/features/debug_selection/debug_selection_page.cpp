#include "debug_selection_page.h"

#include <QHBoxLayout>

#include "../../app/main_window.h"
#include "../../app/option_card.h"
#include "../../app/page_frame.h"

namespace bitdeck {

QWidget* makeDebugSelectionPage(MainWindow& window) {
    auto* frame = new PageFrame();
    frame->setTitle(QObject::tr("Debug"));
    frame->setSubtitle(QObject::tr("Experimental -- may not work"));
    frame->setBackButtonVisible(true);
    QObject::connect(frame, &PageFrame::backRequested, &window, &MainWindow::goBack);

    auto* content = new QWidget();
    auto* layout = new QHBoxLayout(content);
    layout->setAlignment(Qt::AlignCenter);

    auto* texturesCard = new OptionCard(QStringLiteral("T"), QObject::tr("Textures"));
    QObject::connect(texturesCard, &OptionCard::clicked, &window,
                      [&window] { window.navigateTo(QStringLiteral("debug_convert_textures")); });
    layout->addWidget(texturesCard);

    frame->setContentWidget(content);
    return frame;
}

} // namespace bitdeck
