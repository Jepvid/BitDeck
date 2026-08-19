#pragma once

#include <QWidget>

class QLabel;

namespace bitdeck {

// A "- value +" stepper with an optional title above it.
class Numpad : public QWidget {
    Q_OBJECT

public:
    Numpad(const QString& title, double initialValue, double step, QWidget* parent = nullptr);

    double value() const { return value_; }
    void setValue(double value);

signals:
    void valueChanged(double value);

private:
    QLabel* valueLabel_;
    double value_;
    double step_;
};

} // namespace bitdeck
