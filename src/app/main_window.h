#pragma once

#include <QMainWindow>

#include "staging_model.h"

namespace bitdeck {

class ShellWindow;

// Top-level window: owns the app-wide StagingModel (every mode's staged
// entries accumulate here, combined into one archive by Finalize Mod) and
// hosts the ShellWindow that provides all actual UI.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    StagingModel& stagingModel() { return stagingModel_; }

private:
    StagingModel stagingModel_;
    ShellWindow* shell_;
};

} // namespace bitdeck
