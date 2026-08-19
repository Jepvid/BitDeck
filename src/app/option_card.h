#pragma once

#include <QWidget>

class QLabel;
class QVBoxLayout;

namespace bitdeck {

// A clickable rounded-rect tile with a centered glyph and a label below it.
// Used for the menu screens (Home, Create selection, game selection, ...).
class OptionCard : public QWidget {
    Q_OBJECT

public:
    static constexpr int kIconBoxSize = 210; // 50px glyph + 80px padding each side

    OptionCard(QString glyph, QString label, QWidget* parent = nullptr);

    // Positioned at the icon box's top-left corner, e.g. a warning badge.
    void setOverlayWidget(QWidget* overlay);

signals:
    void clicked();
    void hoverEntered();
    void hoverLeft();

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    QString glyph_;
    bool hovered_ = false;
    QLabel* label_;
    QVBoxLayout* layout_;
    QWidget* overlay_ = nullptr;
};

} // namespace bitdeck
