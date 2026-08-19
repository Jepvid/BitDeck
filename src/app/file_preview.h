#pragma once

#include <QWidget>

#include <filesystem>

class QLabel;
class QPlainTextEdit;
class QStackedWidget;

namespace bitdeck {

// Shows a single file's contents in the shell's content pane when a file
// (rather than a folder) is selected: an image preview for image files, raw
// text for text-like files, or a "no preview" placeholder otherwise.
class FilePreview : public QWidget {
    Q_OBJECT

public:
    explicit FilePreview(QWidget* parent = nullptr);

    void showFile(const std::filesystem::path& path);

signals:
    // "< Back to folder" clicked -- the owning controller switches its
    // content pane back to the folder/table view.
    void closeRequested();

private:
    QStackedWidget* stack_;
    QLabel* imageLabel_;
    QPlainTextEdit* textEdit_;
    QLabel* unsupportedLabel_;
};

} // namespace bitdeck
