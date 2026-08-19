#include "finalize_dialog.h"

#include <QVBoxLayout>

#include "../features/finish/finish_panel.h"

namespace bitdeck {

FinalizeDialog::FinalizeDialog(MainWindow& window, QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Finalize Mod"));
    resize(480, 400);

    auto* layout = new QVBoxLayout(this);
    panel_ = new FinishPanel(window, this);
    layout->addWidget(panel_);

    connect(panel_, &FinishPanel::archiveGenerated, this, &QDialog::accept);
}

} // namespace bitdeck
