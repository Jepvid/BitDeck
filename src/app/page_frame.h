#pragma once

#include <QWidget>

class QLabel;
class QToolButton;
class QVBoxLayout;
class QHBoxLayout;

namespace bitdeck {

// Page header (back button + title/subtitle + optional right-side widget)
// over a content area. Used as the frame for every routed page.
class PageFrame : public QWidget {
    Q_OBJECT

public:
    explicit PageFrame(QWidget* parent = nullptr);

    void setTitle(const QString& title);
    void setSubtitle(const QString& subtitle);
    void setBackButtonVisible(bool visible);
    void setTopRightWidget(QWidget* widget);
    void setContentWidget(QWidget* widget);

signals:
    void backRequested();

private:
    QLabel* titleLabel_;
    QLabel* subtitleLabel_;
    QToolButton* backButton_;
    QHBoxLayout* headerLayout_;
    QHBoxLayout* topRightLayout_;
    QVBoxLayout* contentLayout_;
    QWidget* topRightWidget_ = nullptr;
    QWidget* contentWidget_ = nullptr;
};

} // namespace bitdeck
