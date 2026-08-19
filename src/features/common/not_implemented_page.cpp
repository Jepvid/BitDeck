#include "not_implemented_page.h"

#include <QLabel>

#include "../../app/main_window.h"
#include "../../app/page_frame.h"

namespace bitdeck {

QWidget* makeNotImplementedPage(MainWindow& window, const QString& title) {
    auto* frame = new PageFrame();
    frame->setTitle(title);
    frame->setSubtitle(QObject::tr("Not built yet"));
    frame->setBackButtonVisible(true);
    QObject::connect(frame, &PageFrame::backRequested, &window, &MainWindow::goBack);

    auto* label = new QLabel(QObject::tr("This screen hasn't been ported yet."));
    label->setStyleSheet(QStringLiteral("color: white;"));
    label->setAlignment(Qt::AlignCenter);
    frame->setContentWidget(label);
    return frame;
}

} // namespace bitdeck
