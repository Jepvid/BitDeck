#include "sequence.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace bitdeck {

namespace {

std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

} // namespace

Sequence Sequence::fromSeqFile(std::vector<uint8_t> rawBinary, const std::string& metaText) {
    std::vector<std::string> lines;
    std::istringstream iss(metaText);
    std::string line;
    while (std::getline(iss, line)) {
        lines.push_back(line);
    }

    std::string metaName = !lines.empty() ? trim(lines[0]) : "";
    std::replace(metaName.begin(), metaName.end(), '/', '|');

    std::string fontIdxStr = lines.size() > 1 ? toLower(trim(lines[1])) : "0";
    if (fontIdxStr.rfind("0x", 0) == 0) {
        fontIdxStr = fontIdxStr.substr(2);
    }
    uint8_t fontIdx = fontIdxStr.empty() ? 0 : static_cast<uint8_t>(std::stoul(fontIdxStr, nullptr, 16));

    std::string type = lines.size() > 2 ? toLower(trim(lines[2])) : "";
    if (type.empty()) {
        type = "bgm";
    }

    Sequence sequence;
    sequence.size_ = static_cast<int32_t>(rawBinary.size());
    sequence.rawBinary_ = std::move(rawBinary);
    sequence.sequenceNum_ = 0;
    sequence.medium_ = 2;
    sequence.cachePolicy_ = (type == "bgm") ? 2 : 1;
    sequence.numFonts_ = 1;
    sequence.fontIndices_ = {fontIdx};
    sequence.path_ = metaName + "_" + type;
    return sequence;
}

void Sequence::writeResourceData() {
    writeInt32(size_);
    appendData(rawBinary_);
    writeInt8(static_cast<uint8_t>(sequenceNum_));
    writeInt8(static_cast<uint8_t>(medium_));
    writeInt8(static_cast<uint8_t>(cachePolicy_));
    writeInt32(numFonts_);
    for (int32_t i = 0; i < numFonts_; ++i) {
        writeInt8(fontIndices_[static_cast<size_t>(i)]);
    }
}

} // namespace bitdeck
