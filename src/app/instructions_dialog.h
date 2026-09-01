#pragma once

#include <QDialog>

namespace bitdeck {

// Static help dialog opened from the shell's top bar "?" button: a rundown
// of each mode plus the common open -> stage -> Finalize Mod workflow.
class InstructionsDialog : public QDialog {
    Q_OBJECT

public:
    explicit InstructionsDialog(QWidget* parent = nullptr);
};

} // namespace bitdeck
