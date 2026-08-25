#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "endianness.h"
#include "resource_type.h"
#include "version.h"

namespace bitdeck {

// Base OTR resource: a fixed 64-byte header followed by type-specific
// payload data written/read by writeResourceData()/readResourceData()
// overrides.
class Resource {
public:
    static constexpr int64_t kMagicId = static_cast<int64_t>(0xDEADBEEFDEADBEEFULL);
    static constexpr size_t kHeaderSize = 0x40;

    Resource() : Resource(ResourceType::Unknown, 0, Version::Unknown) {}
    Resource(ResourceType resourceType, int32_t resourceVersion, Version gameVersion)
        : resourceType_(resourceType), resourceVersion_(resourceVersion), gameVersion_(gameVersion) {}
    virtual ~Resource() = default;

    // When true, open() stores whatever resourceType it finds instead of
    // validating against the constructed resourceType; isValid() is not set
    // on this path.
    bool rawLoad = false;
    bool isCustom = true;

    // Serializes the header + writeResourceData() payload.
    std::vector<uint8_t> build();

    // Parses data as this resource's header + (unless rawLoad) payload.
    void open(const std::vector<uint8_t>& data);

    bool isValid() const { return isValid_; }
    ResourceType resourceType() const { return resourceType_; }
    Version gameVersion() const { return gameVersion_; }
    int32_t resourceVersion() const { return resourceVersion_; }
    int64_t magicId() const { return magicId_; }
    Endianness endianness() const { return endianness_; }

protected:
    virtual void writeResourceData() {}
    virtual void readResourceData() {}

    // Write primitives: always host-native byte order.
    void appendData(const std::vector<uint8_t>& data);
    void writeBinary(int32_t value);
    void writeInt8(uint8_t value);
    void writeInt16(int16_t value);
    void writeInt32(int32_t value);
    void writeInt64(int64_t value);
    void writeFloat32(float value);
    void writeString(const std::string& value);

    // Read primitives: endianness-aware per the header's endianness byte.
    uint8_t readBinary();
    uint8_t readInt8();
    int32_t readInt32();
    int64_t readInt64();
    float readFloat32();
    std::vector<uint8_t> readBytes(size_t length);
    void seek(int64_t offset, bool fromStart = false);

    ResourceType resourceType_;
    int32_t resourceVersion_;
    Version gameVersion_;

private:
    void writeHeader();
    void readHeader();

    std::vector<uint8_t> writeBuffer_;
    std::vector<uint8_t> readBuffer_;
    size_t readPos_ = 0;
    Endianness endianness_ = nativeEndianness();
    int64_t magicId_ = kMagicId;
    bool isValid_ = false;
};

} // namespace bitdeck
