#pragma once

#include <QByteArray>

#include <map>
#include <string>
#include <vector>

#include "../games/game_texture_conventions.h"

namespace bitdeck {

TextureManifestMap parseManifestJson(const QByteArray& data);
QByteArray writeManifestJson(const TextureManifestMap& manifest);

// aliases.json: source image path (relative to a folder, extension
// included) -> one or more target archive texture paths.
std::map<std::string, std::vector<std::string>> parseAliasesJson(const QByteArray& data);

} // namespace bitdeck
