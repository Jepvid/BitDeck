#pragma once

#include <QString>

class QWidget;

namespace bitdeck {

class MainWindow;

// Placeholder for a route whose real screen hasn't been built yet (per the
// port plan's phase order). Shows a back button and a plain message.
QWidget* makeNotImplementedPage(MainWindow& window, const QString& title);

} // namespace bitdeck
