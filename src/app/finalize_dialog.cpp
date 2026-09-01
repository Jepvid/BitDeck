#include "finalize_dialog.h"

#include <QVBoxLayout>

#include "../features/finish/finish_panel.h"

namespace bitdeck {

FinalizeDialog::FinalizeDialog(StagingModel& stagingModel, bool showPrependAlt, QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Export Mod"));
    resize(480, 400);

    auto* layout = new QVBoxLayout(this);
    panel_ = new FinishPanel(stagingModel, showPrependAlt, this);
    layout->addWidget(panel_);

    connect(panel_, &FinishPanel::archiveGenerated, this, &QDialog::accept);
}

} // namespace bitdeck
