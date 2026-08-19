#pragma once

#include <cstdint>
#include <vector>

#include "../resource.h"

namespace bitdeck {

// OBGI resource: a raw pre-encoded (JPEG) background image blob.
class Background : public Resource {
public:
    Background() : Resource(ResourceType::SohBackground, 0, Version::Version0) {}
    explicit Background(std::vector<uint8_t> texData)
        : Resource(ResourceType::SohBackground, 0, Version::Version0),
          texDataSize_(static_cast<int32_t>(texData.size())),
          texData_(std::move(texData)) {}

    static Background empty() { return Background(); }

    void fromBytes(std::vector<uint8_t> bytes) {
        texDataSize_ = static_cast<int32_t>(bytes.size());
        texData_ = std::move(bytes);
    }

    const std::vector<uint8_t>& texData() const { return texData_; }
    int32_t texDataSize() const { return texDataSize_; }

protected:
    void writeResourceData() override;
    void readResourceData() override;

private:
    int32_t texDataSize_ = 0;
    std::vector<uint8_t> texData_;
};

} // namespace bitdeck
