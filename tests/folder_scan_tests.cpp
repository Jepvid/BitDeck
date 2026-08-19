#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "core/folder_scan.h"
#include "core/sha256.h"

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

void writeFile(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << content;
}

const bitdeck::ScannedFile* findByKey(const std::vector<bitdeck::ScannedFile>& files, const std::string& key) {
    for (const auto& file : files) {
        if (file.relativeKey == key) {
            return &file;
        }
    }
    return nullptr;
}

void testNewFilesWithNoKnownHashes() {
    using namespace bitdeck;
    std::filesystem::path dir = std::filesystem::temp_directory_path() / "bitdeck_folder_scan_test_new";
    std::filesystem::remove_all(dir);
    writeFile(dir / "root.txt", "root");
    writeFile(dir / "sub" / "nested.txt", "nested");

    auto results = scanFolder(dir, {});

    check(results.size() == 2, "scans both the root-level and nested file");
    const ScannedFile* rootFile = findByKey(results, "root.txt");
    const ScannedFile* nestedFile = findByKey(results, "sub/nested.txt");
    check(rootFile != nullptr && rootFile->dirKey.empty(), "a root-level file has an empty dirKey");
    check(nestedFile != nullptr && nestedFile->dirKey == "sub", "a nested file's dirKey is its parent folder");
    check(rootFile != nullptr && rootFile->status == FileStageStatus::New,
          "every file is New when knownHashes is empty");

    std::filesystem::remove_all(dir);
}

void testModifiedVsUpToDate() {
    using namespace bitdeck;
    std::filesystem::path dir = std::filesystem::temp_directory_path() / "bitdeck_folder_scan_test_dedup";
    std::filesystem::remove_all(dir);
    writeFile(dir / "unchanged.txt", "same content");
    writeFile(dir / "changed.txt", "new content");

    std::map<std::string, std::string> knownHashes = {
        {"unchanged.txt", sha256Hex({'s', 'a', 'm', 'e', ' ', 'c', 'o', 'n', 't', 'e', 'n', 't'})},
        {"changed.txt", sha256Hex({'o', 'l', 'd'})},
    };

    auto results = scanFolder(dir, knownHashes);

    const ScannedFile* unchanged = findByKey(results, "unchanged.txt");
    const ScannedFile* changed = findByKey(results, "changed.txt");
    check(unchanged != nullptr && unchanged->status == FileStageStatus::UpToDate,
          "a file whose hash matches knownHashes is UpToDate");
    check(changed != nullptr && changed->status == FileStageStatus::Modified,
          "a file whose hash differs from knownHashes is Modified, not New");

    std::filesystem::remove_all(dir);
}

void testAcceptFilter() {
    using namespace bitdeck;
    std::filesystem::path dir = std::filesystem::temp_directory_path() / "bitdeck_folder_scan_test_filter";
    std::filesystem::remove_all(dir);
    writeFile(dir / "song.seq", "seq data");
    writeFile(dir / "notes.txt", "irrelevant");

    auto results = scanFolder(dir, {}, [](const std::filesystem::path& path) { return path.extension() == ".seq"; });

    check(results.size() == 1 && results[0].relativeKey == "song.seq",
          "accept filter excludes non-matching files entirely, not just marks them");

    std::filesystem::remove_all(dir);
}

void testMissingFolderReturnsEmpty() {
    using namespace bitdeck;
    auto results = scanFolder(std::filesystem::temp_directory_path() / "bitdeck_folder_scan_does_not_exist", {});
    check(results.empty(), "scanning a nonexistent folder returns an empty result, not an error");
}

} // namespace

int main() {
    testNewFilesWithNoKnownHashes();
    testModifiedVsUpToDate();
    testAcceptFilter();
    testMissingFolderReturnsEmpty();

    if (g_failures > 0) {
        std::printf("\n%d check(s) failed.\n", g_failures);
        return 1;
    }
    std::printf("\nAll checks passed.\n");
    return 0;
}
