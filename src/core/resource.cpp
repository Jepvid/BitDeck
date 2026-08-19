#include "resource.h"

#include <cstring>
#include <stdexcept>

namespace bitdeck {

std::vector<uint8_t> Resource::build() {
    writeBuffer_.clear();
    writeHeader();
    writeResourceData();
    return writeBuffer_;
}

void Resource::writeHeader() {
    writeBinary(static_cast<int32_t>(nativeEndianness()));
    writeInt32(static_cast<int32_t>(resourceType_));
    writeInt32(static_cast<int32_t>(gameVersion_));
    writeInt64(kMagicId);
    writeInt32(resourceVersion_);
    writeInt8(isCustom ? 1 : 0);
    writeInt8(0);
    writeInt8(0);
    writeInt8(0);
    writeInt32(0);
    writeInt32(0);

    while (writeBuffer_.size() < kHeaderSize) {
        writeInt32(0);
    }
}

void Resource::appendData(const std::vector<uint8_t>& data) {
    writeBuffer_.insert(writeBuffer_.end(), data.begin(), data.end());
}

void Resource::writeBinary(int32_t value) {
    writeBuffer_.push_back(static_cast<uint8_t>(value));
    writeBuffer_.push_back(0);
    writeBuffer_.push_back(0);
    writeBuffer_.push_back(0);
}

void Resource::writeInt8(uint8_t value) {
    writeBuffer_.push_back(value);
}

void Resource::writeInt32(int32_t value) {
    uint8_t bytes[4];
    std::memcpy(bytes, &value, sizeof(bytes));
    writeBuffer_.insert(writeBuffer_.end(), bytes, bytes + sizeof(bytes));
}

void Resource::writeInt64(int64_t value) {
    uint8_t bytes[8];
    std::memcpy(bytes, &value, sizeof(bytes));
    writeBuffer_.insert(writeBuffer_.end(), bytes, bytes + sizeof(bytes));
}

void Resource::writeFloat32(float value) {
    uint8_t bytes[4];
    std::memcpy(bytes, &value, sizeof(bytes));
    writeBuffer_.insert(writeBuffer_.end(), bytes, bytes + sizeof(bytes));
}

void Resource::writeString(const std::string& value) {
    // Raw bytes, not length-prefixed or null-terminated.
    writeBuffer_.insert(writeBuffer_.end(), value.begin(), value.end());
}

void Resource::open(const std::vector<uint8_t>& data) {
    readBuffer_ = data;
    readPos_ = 0;
    readHeader();
    if (isValid_ && !rawLoad) {
        readResourceData();
    }
}

void Resource::readHeader() {
    if (readBuffer_.size() < kHeaderSize) {
        isValid_ = false;
        return;
    }

    endianness_ = endiannessFromValue(static_cast<int32_t>(readBinary()));
    if (endianness_ == Endianness::Unknown) {
        isValid_ = false;
        return;
    }

    for (int i = 0; i < 3; ++i) {
        readInt8();
    }

    if (!rawLoad) {
        ResourceType read = resourceTypeFromValue(readInt32());
        isValid_ = (read == resourceType_);
        if (!isValid_) {
            return;
        }
    } else {
        resourceType_ = resourceTypeFromValue(readInt32());
        // isValid_ intentionally left as-is here -- see header comment.
    }

    gameVersion_ = versionFromValue(readInt32());
    magicId_ = readInt64();
    resourceVersion_ = readInt32();
    readInt64(); // ROM CRC, discarded
    readInt32(); // ROM enum, discarded

    seek(static_cast<int64_t>(kHeaderSize), /*fromStart=*/true);
}

void Resource::seek(int64_t offset, bool fromStart) {
    readPos_ = fromStart ? static_cast<size_t>(offset) : readPos_ + static_cast<size_t>(offset);
}

std::vector<uint8_t> Resource::readBytes(size_t length) {
    if (readPos_ + length > readBuffer_.size()) {
        throw std::out_of_range("Resource::readBytes: out of range");
    }
    std::vector<uint8_t> data(readBuffer_.begin() + static_cast<ptrdiff_t>(readPos_),
                               readBuffer_.begin() + static_cast<ptrdiff_t>(readPos_ + length));
    seek(static_cast<int64_t>(length));
    return data;
}

uint8_t Resource::readBinary() {
    return readInt8();
}

uint8_t Resource::readInt8() {
    uint8_t value = readBuffer_.at(readPos_);
    seek(1);
    return value;
}

int32_t Resource::readInt32() {
    uint8_t b0 = readBuffer_.at(readPos_ + 0);
    uint8_t b1 = readBuffer_.at(readPos_ + 1);
    uint8_t b2 = readBuffer_.at(readPos_ + 2);
    uint8_t b3 = readBuffer_.at(readPos_ + 3);
    uint32_t value = (endianness_ == Endianness::Little)
        ? (static_cast<uint32_t>(b0) | static_cast<uint32_t>(b1) << 8 |
           static_cast<uint32_t>(b2) << 16 | static_cast<uint32_t>(b3) << 24)
        : (static_cast<uint32_t>(b3) | static_cast<uint32_t>(b2) << 8 |
           static_cast<uint32_t>(b1) << 16 | static_cast<uint32_t>(b0) << 24);
    seek(4);
    return static_cast<int32_t>(value);
}

int64_t Resource::readInt64() {
    uint64_t value = 0;
    if (endianness_ == Endianness::Little) {
        for (int i = 7; i >= 0; --i) {
            value = (value << 8) | readBuffer_.at(readPos_ + static_cast<size_t>(i));
        }
    } else {
        for (int i = 0; i < 8; ++i) {
            value = (value << 8) | readBuffer_.at(readPos_ + static_cast<size_t>(i));
        }
    }
    seek(8);
    return static_cast<int64_t>(value);
}

float Resource::readFloat32() {
    // IEEE-754 bit-reinterpretation of the 4 bytes.
    uint8_t b0 = readBuffer_.at(readPos_ + 0);
    uint8_t b1 = readBuffer_.at(readPos_ + 1);
    uint8_t b2 = readBuffer_.at(readPos_ + 2);
    uint8_t b3 = readBuffer_.at(readPos_ + 3);
    uint32_t bits = (endianness_ == Endianness::Little)
        ? (static_cast<uint32_t>(b0) | static_cast<uint32_t>(b1) << 8 |
           static_cast<uint32_t>(b2) << 16 | static_cast<uint32_t>(b3) << 24)
        : (static_cast<uint32_t>(b3) | static_cast<uint32_t>(b2) << 8 |
           static_cast<uint32_t>(b1) << 16 | static_cast<uint32_t>(b0) << 24);
    seek(4);
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

} // namespace bitdeck
