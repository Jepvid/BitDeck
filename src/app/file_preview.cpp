#include "file_preview.h"

#include <QFile>
#include <QLabel>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QStackedWidget>
#include <QTextStream>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cctype>

namespace bitdeck {

namespace {

constexpr std::array<const char*, 5> kImageExtensions = {"png", "jpg", "jpeg", "bmp", "gif"};
constexpr std::array<const char*, 9> kTextExtensions = {"json", "txt", "meta", "xml", "ini",
                                                         "cfg",  "md",  "csv",  "yaml"};

std::string lowerExtension(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    if (!ext.empty() && ext.front() == '.') {
        ext.erase(0, 1);
    }
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

template <size_t N>
bool matchesExtension(const std::string& ext, const std::array<const char*, N>& list) {
    return std::find(list.begin(), list.end(), ext) != list.end();
}

} // namespace

FilePreview::FilePreview(QWidget* parent) : QWidget(parent) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    stack_ = new QStackedWidget();
    layout->addWidget(stack_);

    imageLabel_ = new QLabel();
    imageLabel_->setAlignment(Qt::AlignCenter);
    stack_->addWidget(imageLabel_);

    textEdit_ = new QPlainTextEdit();
    textEdit_->setReadOnly(true);
    stack_->addWidget(textEdit_);

    unsupportedLabel_ = new QLabel(tr("No preview available for this file type."));
    unsupportedLabel_->setAlignment(Qt::AlignCenter);
    stack_->addWidget(unsupportedLabel_);
}

void FilePreview::showFile(const std::filesystem::path& path) {
    std::string ext = lowerExtension(path);

    if (matchesExtension(ext, kImageExtensions)) {
        QPixmap pixmap(QString::fromStdString(path.string()));
        if (!pixmap.isNull()) {
            imageLabel_->setPixmap(pixmap.scaled(stack_->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            stack_->setCurrentWidget(imageLabel_);
            return;
        }
    } else if (matchesExtension(ext, kTextExtensions)) {
        QFile file(QString::fromStdString(path.string()));
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            textEdit_->setPlainText(stream.readAll());
            stack_->setCurrentWidget(textEdit_);
            return;
        }
    }

    stack_->setCurrentWidget(unsupportedLabel_);
}

} // namespace bitdeck
