#pragma once

#include <QDialog>

namespace bitdeck {

class StagingModel;
class FinishPanel;

// Hosts the existing FinishPanel (staged-entries list, extension picker,
// Generate button) as a modal dialog, scoped to whichever StagingModel it's
// given -- each mode opens its own. Closes itself once generation succeeds.
class FinalizeDialog : public QDialog {
    Q_OBJECT

public:
    explicit FinalizeDialog(StagingModel& stagingModel, bool showPrependAlt = false, QWidget* parent = nullptr);

private:
    FinishPanel* panel_;
};

} // namespace bitdeck
