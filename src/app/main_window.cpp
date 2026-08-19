#include "main_window.h"

#include "shell_window.h"

namespace bitdeck {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    shell_ = new ShellWindow(*this, this);
    setCentralWidget(shell_);
}

} // namespace bitdeck
