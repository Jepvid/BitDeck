#pragma once

#include <QMainWindow>

namespace bitdeck {

class ShellWindow;

// Top-level window: hosts the ShellWindow that provides all actual UI. Each
// mode owns its own staging (see StagingModel) independently -- there is no
// app-wide shared staging basket here.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    ShellWindow* shell_;
};

} // namespace bitdeck
