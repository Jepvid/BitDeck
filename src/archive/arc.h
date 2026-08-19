#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include <StormLib.h>
#include <zip.h>

#include "../games/game_texture_conventions.h" // ArchiveFileVisitor

namespace bitdeck {

enum class ArcMode {
    Otr,
    O2r,
};

// .otr (MPQ, via StormLib) / .o2r (zip, via libzip) archive reader/writer.
// Mode is chosen by file extension. If the file already exists it's opened
// for reading; otherwise a new archive is created for writing.
class Arc {
public:
    explicit Arc(std::string path);
    ~Arc();

    Arc(const Arc&) = delete;
    Arc& operator=(const Arc&) = delete;

    ArcMode mode() const { return mode_; }

    // Lists every file, invoking onFile(name, data) for each if given.
    // Returns the file names. A single unreadable entry is skipped rather
    // than aborting the rest of the listing.
    std::vector<std::string> listItems(const ArchiveFileVisitor& onFile = nullptr);

    void addFile(const std::string& path, const std::vector<uint8_t>& data, bool compress = false);

    void close();

private:
    std::vector<std::string> listMpqFiles(const ArchiveFileVisitor& onFile);
    std::vector<std::string> listZipFiles(const ArchiveFileVisitor& onFile);
    void addMpqFile(const std::string& path, const std::vector<uint8_t>& data, bool compress);
    void addZipFile(const std::string& path, const std::vector<uint8_t>& data, bool compress);
    void closeMpq();
    void closeZip();

    ArcMode mode_;
    std::string path_;
    bool closed_ = false;

    HANDLE mpqHandle_ = nullptr;
    zip_t* zipHandle_ = nullptr;
    std::deque<std::vector<uint8_t>> pendingZipBuffers_; // kept alive until zip_close()
};

} // namespace bitdeck
