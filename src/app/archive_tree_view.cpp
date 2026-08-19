#include "archive_tree_view.h"

#include <QStandardItem>

namespace bitdeck {

namespace {

void addChildren(QStandardItem* parentItem, PathTreeNode& node) {
    for (auto& [name, child] : node.children) {
        auto* item = new QStandardItem(QString::fromStdString(name));
        item->setEditable(false);
        item->setData(QVariant::fromValue(reinterpret_cast<quintptr>(&child)), kPathTreeNodeRole);
        parentItem->appendRow(item);
        addChildren(item, child);
    }
}

} // namespace

void populateArchiveFolderTree(QStandardItemModel& model, PathTreeNode& root) {
    model.clear();
    addChildren(model.invisibleRootItem(), root);
}

} // namespace bitdeck
