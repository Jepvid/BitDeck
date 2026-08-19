#include "texture_manifest_json.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "../core/path_utils.h"

namespace bitdeck {

TextureManifestMap parseManifestJson(const QByteArray& data) {
    TextureManifestMap manifest;
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        return manifest;
    }

    QJsonObject root = doc.object();
    for (auto it = root.begin(); it != root.end(); ++it) {
        if (!it.value().isObject()) {
            continue;
        }
        QJsonObject obj = it.value().toObject();
        try {
            TextureManifestEntry entry;
            entry.hash = obj.value(QStringLiteral("hash")).toString().toStdString();
            entry.textureType = textureTypeFromValue(obj.value(QStringLiteral("textureType")).toInt());
            entry.textureWidth = obj.value(QStringLiteral("textureWidth")).toInt();
            entry.textureHeight = obj.value(QStringLiteral("textureHeight")).toInt();
            if (obj.contains(QStringLiteral("tileWidth"))) {
                entry.tileWidth = obj.value(QStringLiteral("tileWidth")).toInt();
            }
            if (obj.contains(QStringLiteral("tileHeight"))) {
                entry.tileHeight = obj.value(QStringLiteral("tileHeight")).toInt();
            }
            manifest[it.key().toStdString()] = entry;
        } catch (...) {
            continue; // skip malformed entries, keep parsing the rest
        }
    }
    return manifest;
}

QByteArray writeManifestJson(const TextureManifestMap& manifest) {
    QJsonObject root;
    for (const auto& [key, entry] : manifest) {
        QJsonObject obj;
        obj[QStringLiteral("hash")] = QString::fromStdString(entry.hash);
        obj[QStringLiteral("textureType")] = static_cast<int>(entry.textureType);
        obj[QStringLiteral("textureWidth")] = entry.textureWidth;
        obj[QStringLiteral("textureHeight")] = entry.textureHeight;
        if (entry.tileWidth.has_value()) {
            obj[QStringLiteral("tileWidth")] = *entry.tileWidth;
        }
        if (entry.tileHeight.has_value()) {
            obj[QStringLiteral("tileHeight")] = *entry.tileHeight;
        }
        root[QString::fromStdString(key)] = obj;
    }
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

std::map<std::string, std::vector<std::string>> parseAliasesJson(const QByteArray& data) {
    std::map<std::string, std::vector<std::string>> aliases;
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        return aliases;
    }

    QJsonObject root = doc.object();
    for (auto it = root.begin(); it != root.end(); ++it) {
        std::vector<std::string> targets;
        if (it.value().isArray()) {
            for (const auto& v : it.value().toArray()) {
                targets.push_back(v.toString().toStdString());
            }
        } else if (it.value().isString()) {
            targets.push_back(it.value().toString().toStdString());
        } else {
            continue;
        }

        for (auto& target : targets) {
            target = normalizePath(target);
            if (target.rfind("alt/", 0) == 0) {
                target = target.substr(4);
            }
        }
        aliases[normalizePath(it.key().toStdString())] = std::move(targets);
    }
    return aliases;
}

} // namespace bitdeck
