#include "instructions_dialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

namespace bitdeck {

namespace {

QString instructionsHtml() {
    return QObject::tr(
        "<p>Each mode works independently -- pick the one that matches what "
        "you're adding.</p>"
        "<p>1. Pick the mode for what you're adding, below.<br>"
        "2. Click Open and choose a folder -- files are staged "
        "automatically.<br>"
        "3. Need resources from an existing .otr/.o2r first (e.g. textures "
        "to edit)? That's a separate step: use \"Extract textures from "
        "otr/o2r\" (Make Texture Pack) or Inspect OTR to unpack what you "
        "need into a folder, then open that folder as in step 2.<br>"
        "4. Click <b>Export Mod</b> to build this mode's own .otr/.o2r from "
        "what it has staged.</p>"
        "<h3>Modes</h3>"
        "<p><b>Custom Files</b> -- pack folder content as is. Use this for "
        "anything that isn't a texture or a music sequence.</p>"
        "<p><b>Make Texture Pack</b> -- match a folder of replacement images "
        "against a manifest.json, or extract textures from an OTR/O2R to "
        "start editing. Extracting has two toggles: <i>Apply TLUT</i> colors "
        "CI4/CI8 textures with their real palette instead of a flat "
        "grayscale ramp, and <i>Apply IA Transparency</i> shows the real "
        "translucent shape of applicable IA4/IA8 glow/spark/dust textures "
        "instead of a flat opaque square.</p>"
        "<p><b>Custom Sequences</b> -- stage .seq audio files paired with "
        ".meta sidecars.</p>"
        "<p><b>Inspect OTR</b> -- browse an OTR/O2R archive's contents, "
        "read-only.</p>"
        "<p><b>Debug: Convert Textures</b> -- round-trip PNG and N64 texture "
        "formats for testing.</p>"
        "<p><b>Debug: Font Generator</b> -- not yet implemented.</p>");
}

} // namespace

InstructionsDialog::InstructionsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Instructions"));
    resize(480, 520);

    auto* layout = new QVBoxLayout(this);

    auto* label = new QLabel(instructionsHtml());
    label->setWordWrap(true);
    label->setTextFormat(Qt::RichText);

    auto* scrollArea = new QScrollArea();
    scrollArea->setWidget(label);
    scrollArea->setWidgetResizable(true);
    layout->addWidget(scrollArea);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &InstructionsDialog::accept);
    layout->addWidget(buttons);
}

} // namespace bitdeck
