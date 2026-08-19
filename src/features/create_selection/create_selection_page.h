#pragma once

class QWidget;

namespace bitdeck {

class MainWindow;

QWidget* makeCreateSelectionPage(MainWindow& window);
QWidget* makeGameSelectionPage(MainWindow& window);
QWidget* makeSohPage(MainWindow& window);

} // namespace bitdeck
