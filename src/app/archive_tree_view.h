#pragma once

#include <QStandardItemModel>

#include "../core/path_tree.h"

namespace bitdeck {

// Set on every folder row, pointing back at that folder's own PathTreeNode
// -- reading it off a selected row gives a content pane that folder's file
// list (node->files) directly, with no path re-derivation needed. root must
// outlive model and stay at a fixed address (its own children are pointed
// to directly, not copied).
constexpr int kPathTreeNodeRole = Qt::UserRole + 1;

// Rebuilds model with one row per folder in root, nested to match the tree
// (root itself is not a row -- its direct children become the top-level
// rows).
void populateArchiveFolderTree(QStandardItemModel& model, PathTreeNode& root);

} // namespace bitdeck
