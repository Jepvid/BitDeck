#pragma once

#include <map>
#include <string>
#include <vector>

namespace bitdeck {

// A folder node built from a flat list of '/'-separated paths (as returned
// by Arc::listItems()). files holds this folder's direct-child leaf paths,
// each in their original unmodified (full, not just basename) form.
struct PathTreeNode {
    std::map<std::string, PathTreeNode> children;
    std::vector<std::string> files;
};

// Splits every path in paths on '/' and inserts it into the tree at the
// matching folder chain, creating intermediate folders as needed. A path
// with no '/' becomes a direct child of the root. Empty path segments
// (leading/trailing/doubled slashes) are skipped.
PathTreeNode buildPathTree(const std::vector<std::string>& paths);

} // namespace bitdeck
