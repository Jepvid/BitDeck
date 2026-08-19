#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QVBoxLayout;
class QPropertyAnimation;

namespace bitdeck {

class StagingModel;

// Persistent bar pinned to the bottom of the main window: 24px collapsed,
// animates to kExpandedHeight to reveal the finalize/staging panel.
class EphemeralBar : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int barHeight READ barHeight WRITE setBarHeight)

public:
    static constexpr int kCollapsedHeight = 24;
    static constexpr int kExpandedHeight = 480;

    explicit EphemeralBar(StagingModel* stagingModel, QWidget* parent = nullptr);

    int barHeight() const { return height(); }
    void setBarHeight(int barHeight);

    // Panel shown while expanded (e.g. Phase 4's staging list + generate
    // button). Ownership passes to EphemeralBar.
    void setExpandedContentWidget(QWidget* widget);

    bool isExpanded() const { return expanded_; }

signals:
    void generateRequested();
    void linkRequested();

public slots:
    void setExpanded(bool expanded);
    void toggleExpanded();

private:
    void refresh();
    void updateBackground();

    StagingModel* stagingModel_;
    bool expanded_ = false;

    QWidget* collapsedRow_;
    QLabel* stateLabel_;
    QPushButton* finalizeButton_;
    QLabel* versionLabel_;
    QPushButton* linkButton_;

    QWidget* expandedContainer_;
    QVBoxLayout* expandedLayout_;
    QWidget* expandedContentWidget_ = nullptr;

    QPropertyAnimation* animation_;
};

} // namespace bitdeck
