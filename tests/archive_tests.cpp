#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "archive/arc.h"
#include "archive/extraction_safety.h"

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

std::vector<uint8_t> toBytes(const std::string& s) {
    return std::vector<uint8_t>(s.begin(), s.end());
}

void testArcRoundTrip(const std::string& extension) {
    using namespace bitdeck;

    std::filesystem::path dir = std::filesystem::temp_directory_path() / "bitdeck_arc_test";
    std::filesystem::create_directories(dir);
    std::filesystem::path archivePath = dir / ("test" + extension);
    std::filesystem::remove(archivePath);

    {
        bitdeck::Arc arc(archivePath.string());
        arc.addFile("textures/foo.otex", toBytes("hello world"), true);
        arc.addFile("custom/music/bar.oseq", toBytes("sequence data"), false);
        arc.close();
    }

    check(std::filesystem::exists(archivePath), (extension + ": archive file created").c_str());

    bitdeck::Arc readBack(archivePath.string());
    std::map<std::string, std::vector<uint8_t>> found;
    std::vector<std::string> names = readBack.listItems([&](const std::string& name, const std::vector<uint8_t>& data) {
        found[name] = data;
    });
    readBack.close();

    check(names.size() == 2, (extension + ": lists exactly the 2 added files").c_str());
    check(found.count("textures/foo.otex") == 1 && found["textures/foo.otex"] == toBytes("hello world"),
          (extension + ": first file content round-trips").c_str());
    check(found.count("custom/music/bar.oseq") == 1 && found["custom/music/bar.oseq"] == toBytes("sequence data"),
          (extension + ": second file content round-trips").c_str());
}

void testExtractionSafety() {
    using namespace bitdeck;

    std::filesystem::path dir = std::filesystem::temp_directory_path() / "bitdeck_extract_safety_test";
    std::filesystem::create_directories(dir);

    auto safe = safeExtractionPath(dir, "textures/foo.png");
    check(safe.has_value(), "safeExtractionPath: normal nested entry is allowed");

    auto traversal = safeExtractionPath(dir, "../../../etc/passwd");
    check(!traversal.has_value(), "safeExtractionPath: path traversal is rejected");

    auto backslashTraversal = safeExtractionPath(dir, "..\\..\\evil.txt");
    check(!backslashTraversal.has_value(), "safeExtractionPath: backslash path traversal is rejected");
}

} // namespace

int main() {
    testArcRoundTrip(".otr");
    testArcRoundTrip(".o2r");
    testExtractionSafety();

    if (g_failures > 0) {
        std::printf("\n%d check(s) FAILED\n", g_failures);
        return EXIT_FAILURE;
    }
    std::printf("\nAll checks passed\n");
    return EXIT_SUCCESS;
}
