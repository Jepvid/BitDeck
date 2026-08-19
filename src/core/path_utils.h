#pragma once

#include <algorithm>
#include <string>

namespace bitdeck {

// Converts backslashes to forward slashes. No other normalization (no
// ../. collapsing, no case folding, no trailing-slash handling).
inline std::string normalizePath(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    return path;
}

} // namespace bitdeck
