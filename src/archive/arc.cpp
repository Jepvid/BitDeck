#include "arc.h"

#include <ctime>
#include <filesystem>
#include <stdexcept>

namespace bitdeck {

namespace {

constexpr const char* kSignatureName = "(signature)";
constexpr const char* kListfileName = "(listfile)";
constexpr const char* kAttributesName = "(attributes)";

bool isMetaEntry(const std::string& name) {
    return name == kSignatureName || name == kListfileName || name == kAttributesName;
}

} // namespace

Arc::Arc(std::string path) : path_(std::move(path)) {
    mode_ = path_.size() >= 4 && path_.compare(path_.size() - 4, 4, ".otr") == 0 ? ArcMode::Otr : ArcMode::O2r;
    bool exists = std::filesystem::exists(path_);

    if (mode_ == ArcMode::Otr) {
        bool ok = exists ? SFileOpenArchive(path_.c_str(), 0, 0, &mpqHandle_)
                          : SFileCreateArchive(path_.c_str(), MPQ_CREATE_SIGNATURE | MPQ_CREATE_ARCHIVE_V2, 65535,
                                               &mpqHandle_);
        if (!ok) {
            throw std::runtime_error("Arc: failed to open/create MPQ archive: " + path_);
        }
    } else {
        int err = 0;
        zipHandle_ = exists ? zip_open(path_.c_str(), ZIP_RDONLY, &err)
                             : zip_open(path_.c_str(), ZIP_CREATE, &err);
        if (zipHandle_ == nullptr) {
            throw std::runtime_error("Arc: failed to open/create zip archive: " + path_);
        }
    }
}

Arc::~Arc() {
    if (!closed_) {
        close();
    }
}

std::vector<std::string> Arc::listItems(const ArchiveFileVisitor& onFile) {
    return mode_ == ArcMode::Otr ? listMpqFiles(onFile) : listZipFiles(onFile);
}

std::vector<std::string> Arc::listMpqFiles(const ArchiveFileVisitor& onFile) {
    std::vector<std::string> files;

    SFILE_FIND_DATA findData;
    HANDLE hFind = SFileFindFirstFile(mpqHandle_, "*", &findData, nullptr);
    if (hFind == nullptr) {
        return files;
    }

    do {
        std::string fileName = findData.cFileName;
        if (isMetaEntry(fileName)) {
            continue;
        }
        files.push_back(fileName);

        if (!onFile) {
            continue;
        }

        HANDLE hFile = nullptr;
        if (!SFileOpenFileEx(mpqHandle_, fileName.c_str(), 0, &hFile)) {
            continue; // skip unreadable entry, keep listing
        }
        DWORD size = SFileGetFileSize(hFile, nullptr);
        std::vector<uint8_t> data(size);
        DWORD bytesRead = 0;
        bool readOk = SFileReadFile(hFile, data.data(), size, &bytesRead, nullptr);
        SFileCloseFile(hFile);
        if (!readOk) {
            continue; // skip unreadable entry, keep listing
        }
        onFile(fileName, data);
    } while (SFileFindNextFile(hFind, &findData));

    SFileFindClose(hFind);
    return files;
}

std::vector<std::string> Arc::listZipFiles(const ArchiveFileVisitor& onFile) {
    std::vector<std::string> files;

    zip_int64_t numEntries = zip_get_num_entries(zipHandle_, 0);
    for (zip_int64_t i = 0; i < numEntries; ++i) {
        const char* name = zip_get_name(zipHandle_, static_cast<zip_uint64_t>(i), 0);
        if (name == nullptr) {
            continue;
        }
        files.emplace_back(name);

        if (!onFile) {
            continue;
        }

        zip_stat_t stat;
        if (zip_stat_index(zipHandle_, static_cast<zip_uint64_t>(i), 0, &stat) != 0) {
            continue;
        }
        zip_file_t* zf = zip_fopen_index(zipHandle_, static_cast<zip_uint64_t>(i), 0);
        if (zf == nullptr) {
            continue;
        }
        std::vector<uint8_t> data(stat.size);
        zip_int64_t readBytes = zip_fread(zf, data.data(), stat.size);
        zip_fclose(zf);
        if (readBytes < 0 || static_cast<zip_uint64_t>(readBytes) != stat.size) {
            continue;
        }
        onFile(files.back(), data);
    }

    return files;
}

std::optional<std::vector<uint8_t>> Arc::readFile(const std::string& path) {
    return mode_ == ArcMode::Otr ? readMpqFile(path) : readZipFile(path);
}

std::optional<std::vector<uint8_t>> Arc::readMpqFile(const std::string& path) {
    HANDLE hFile = nullptr;
    if (!SFileOpenFileEx(mpqHandle_, path.c_str(), 0, &hFile)) {
        return std::nullopt;
    }
    DWORD size = SFileGetFileSize(hFile, nullptr);
    std::vector<uint8_t> data(size);
    DWORD bytesRead = 0;
    bool readOk = SFileReadFile(hFile, data.data(), size, &bytesRead, nullptr);
    SFileCloseFile(hFile);
    if (!readOk) {
        return std::nullopt;
    }
    return data;
}

std::optional<std::vector<uint8_t>> Arc::readZipFile(const std::string& path) {
    zip_stat_t stat;
    if (zip_stat(zipHandle_, path.c_str(), 0, &stat) != 0) {
        return std::nullopt;
    }
    zip_file_t* zf = zip_fopen(zipHandle_, path.c_str(), 0);
    if (zf == nullptr) {
        return std::nullopt;
    }
    std::vector<uint8_t> data(stat.size);
    zip_int64_t readBytes = zip_fread(zf, data.data(), stat.size);
    zip_fclose(zf);
    if (readBytes < 0 || static_cast<zip_uint64_t>(readBytes) != stat.size) {
        return std::nullopt;
    }
    return data;
}

void Arc::addFile(const std::string& path, const std::vector<uint8_t>& data, bool compress) {
    if (mode_ == ArcMode::Otr) {
        addMpqFile(path, data, compress);
    } else {
        addZipFile(path, data, compress);
    }
}

void Arc::addMpqFile(const std::string& path, const std::vector<uint8_t>& data, bool compress) {
    HANDLE hFile = nullptr;
    uint64_t timestamp = static_cast<uint64_t>(std::time(nullptr));
    if (!SFileCreateFile(mpqHandle_, path.c_str(), timestamp, static_cast<DWORD>(data.size()), 0,
                          compress ? MPQ_FILE_COMPRESS : 0, &hFile)) {
        throw std::runtime_error("Arc: SFileCreateFile failed for " + path);
    }
    SFileWriteFile(hFile, data.data(), static_cast<DWORD>(data.size()), compress ? MPQ_COMPRESSION_ZLIB : 0);
    SFileFinishFile(hFile);
}

void Arc::addZipFile(const std::string& path, const std::vector<uint8_t>& data, bool compress) {
    pendingZipBuffers_.push_back(data);
    std::vector<uint8_t>& buffer = pendingZipBuffers_.back();

    zip_source_t* source = zip_source_buffer(zipHandle_, buffer.data(), buffer.size(), 0);
    zip_int64_t index = zip_file_add(zipHandle_, path.c_str(), source, ZIP_FL_OVERWRITE);
    if (index < 0) {
        zip_source_free(source);
        throw std::runtime_error("Arc: zip_file_add failed for " + path);
    }
    zip_set_file_compression(zipHandle_, static_cast<zip_uint64_t>(index), compress ? ZIP_CM_DEFLATE : ZIP_CM_STORE,
                              0);
}

void Arc::close() {
    if (mode_ == ArcMode::Otr) {
        closeMpq();
    } else {
        closeZip();
    }
    closed_ = true;
}

void Arc::closeMpq() {
    if (mpqHandle_ != nullptr) {
        SFileCloseArchive(mpqHandle_);
        mpqHandle_ = nullptr;
    }
}

void Arc::closeZip() {
    if (zipHandle_ != nullptr) {
        zip_close(zipHandle_);
        zipHandle_ = nullptr;
    }
    pendingZipBuffers_.clear();
}

} // namespace bitdeck
