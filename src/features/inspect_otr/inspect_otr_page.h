#pragma once

#include <string>
#include <vector>

#include "../../app/page_frame.h"

class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace bitdeck {

class MainWindow;

// Read-only archive browser + "Extract All", independent of the staging
// flow. Search filters the listed file names live.
class InspectOtrPage : public PageFrame {
    Q_OBJECT

public:
    explicit InspectOtrPage(MainWindow& window);

private:
    void onSelectOtr();
    void onExtractAll();
    void onSearchChanged(const QString& query);
    void refreshFilteredList();

    MainWindow& window_;
    std::string selectedPath_;
    std::vector<std::string> filesInArchive_;

    QLabel* pathLabel_;
    QLineEdit* searchField_;
    QPushButton* selectButton_;
    QPushButton* extractButton_;
    QListWidget* filesList_;
    QLabel* statusLabel_;
};

QWidget* makeInspectOtrPage(MainWindow& window);

} // namespace bitdeck
