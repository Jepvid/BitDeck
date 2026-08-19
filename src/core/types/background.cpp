#include "background.h"

namespace bitdeck {

void Background::writeResourceData() {
    writeInt32(texDataSize_);
    appendData(texData_);
}

void Background::readResourceData() {
    texDataSize_ = readInt32();
    texData_ = readBytes(static_cast<size_t>(texDataSize_));
}

} // namespace bitdeck
