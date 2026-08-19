#pragma once

#include <QMainWindow>

#include <functional>
#include <map>
#include <vector>

#include "staging_model.h"

class QStackedWidget;

namespace bitdeck {

class EphemeralBar;

// Owns the page stack and the app-wide StagingModel. Pages are registered
// by name (mirroring Retro's named-route table) and pushed/popped as plain
// widget instances -- each navigateTo() call creates a fresh widget via its
// factory, matching Retro's Navigator.push semantics. A page wanting a back
// button wires it up itself (e.g. connect PageFrame::backRequested to
// mainWindow->goBack()) inside its factory lambda.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    void registerPage(const QString& name, std::function<QWidget*(MainWindow&)> factory);

    void navigateTo(const QString& name);
    void goBack();
    void popTo(const QString& name);

    StagingModel& stagingModel() { return stagingModel_; }
    EphemeralBar& ephemeralBar() { return *ephemeralBar_; }

private:
    QStackedWidget* pageStack_;
    EphemeralBar* ephemeralBar_;
    StagingModel stagingModel_;
    std::map<QString, std::function<QWidget*(MainWindow&)>> pageFactories_;
    std::vector<QWidget*> widgetStack_;
    std::vector<QString> nameStack_;
};

} // namespace bitdeck
