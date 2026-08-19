#include <cstdio>
#include <string>
#include <vector>

#include "core/path_tree.h"

namespace {

int g_failures = 0;

void check(bool condition, const char* description) {
    if (condition) {
        std::printf("[PASS] %s\n", description);
    } else {
        std::printf("[FAIL] %s\n", description);
        ++g_failures;
    }
}

void testFlatRootFiles() {
    using namespace bitdeck;
    PathTreeNode root = buildPathTree({"a.txt", "b.txt"});

    check(root.children.empty(), "no subfolders in a flat root file list");
    check(root.files.size() == 2 && root.files[0] == "a.txt" && root.files[1] == "b.txt",
          "root-level files listed in input order");
}

void testNestedFolders() {
    using namespace bitdeck;
    PathTreeNode root = buildPathTree({"objects/object_du/gDaruniaNoseSeriousTex",
                                        "objects/object_du/gDaruniaNoseHappyTex",
                                        "objects/object_link_boy/gLinkAdultHandTex"});

    check(root.children.count("objects") == 1, "top-level folder created from a multi-segment path");
    const PathTreeNode& objects = root.children.at("objects");
    check(objects.children.size() == 2, "two distinct subfolders under objects/");
    check(objects.children.at("object_du").files.size() == 2, "both Darunia files grouped under object_du/");
    check(objects.children.at("object_link_boy").files.size() == 1, "Link's file in its own sibling folder");
    check(objects.children.at("object_du").files[0] == "objects/object_du/gDaruniaNoseSeriousTex",
          "leaf entries keep their full original path, not just the basename");
}

void testMixedRootAndNestedFiles() {
    using namespace bitdeck;
    PathTreeNode root = buildPathTree({"manifest.json", "textures/vr_cloud0_static/gSkybox1Tex"});

    check(root.files.size() == 1 && root.files[0] == "manifest.json",
          "a root-level file alongside a nested one stays at the root");
    check(root.children.count("textures") == 1, "the nested file still creates its folder chain");
}

void testEmptyInput() {
    using namespace bitdeck;
    PathTreeNode root = buildPathTree({});
    check(root.children.empty() && root.files.empty(), "an empty path list produces an empty tree");
}

} // namespace

int main() {
    testFlatRootFiles();
    testNestedFolders();
    testMixedRootAndNestedFiles();
    testEmptyInput();

    if (g_failures > 0) {
        std::printf("\n%d check(s) failed.\n", g_failures);
        return 1;
    }
    std::printf("\nAll checks passed.\n");
    return 0;
}
