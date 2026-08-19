#include "path_tree.h"

namespace bitdeck {

namespace {

std::vector<std::string> splitPathSegments(const std::string& path) {
    std::vector<std::string> segments;
    std::string current;
    for (char c : path) {
        if (c == '/') {
            if (!current.empty()) {
                segments.push_back(current);
                current.clear();
            }
        } else {
            current += c;
        }
    }
    if (!current.empty()) {
        segments.push_back(current);
    }
    return segments;
}

} // namespace

PathTreeNode buildPathTree(const std::vector<std::string>& paths) {
    PathTreeNode root;
    for (const auto& path : paths) {
        std::vector<std::string> segments = splitPathSegments(path);
        if (segments.empty()) {
            continue;
        }
        PathTreeNode* node = &root;
        for (size_t i = 0; i + 1 < segments.size(); ++i) {
            node = &node->children[segments[i]];
        }
        node->files.push_back(path);
    }
    return root;
}

} // namespace bitdeck
