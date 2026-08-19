#include "main_window.h"

#include <QStackedWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <stdexcept>

#include "ephemeral_bar.h"

namespace bitdeck {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    pageStack_ = new QStackedWidget(central);
    layout->addWidget(pageStack_, 1);

    ephemeralBar_ = new EphemeralBar(&stagingModel_, central);
    layout->addWidget(ephemeralBar_);

    setCentralWidget(central);
}

void MainWindow::registerPage(const QString& name, std::function<QWidget*(MainWindow&)> factory) {
    pageFactories_[name] = std::move(factory);
}

void MainWindow::navigateTo(const QString& name) {
    auto it = pageFactories_.find(name);
    if (it == pageFactories_.end()) {
        throw std::runtime_error("MainWindow::navigateTo: no page registered for " + name.toStdString());
    }
    QWidget* page = it->second(*this);
    pageStack_->addWidget(page);
    pageStack_->setCurrentWidget(page);
    widgetStack_.push_back(page);
    nameStack_.push_back(name);
}

void MainWindow::goBack() {
    if (widgetStack_.size() <= 1) {
        return; // never pop the root page
    }
    QWidget* top = widgetStack_.back();
    widgetStack_.pop_back();
    nameStack_.pop_back();
    pageStack_->removeWidget(top);
    top->deleteLater();
    pageStack_->setCurrentWidget(widgetStack_.back());
}

void MainWindow::popTo(const QString& name) {
    while (widgetStack_.size() > 1 && nameStack_.back() != name) {
        goBack();
    }
}

} // namespace bitdeck
