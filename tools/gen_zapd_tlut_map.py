# Regenerates a src/games/<game>_tlut_map.cpp baked TLUT-pairing table from
# a local ZAPD-schema decomp checkout (MM, OOT) and a real compiled archive
# (used to verify each candidate path actually exists).
# Usage: python3 gen_zapd_tlut_map.py <game> <decomp>/assets/xml <path-to-archive.o2r> ../src/games
import xml.etree.ElementTree as ET
import glob, os, zipfile, sys

def build(xml_root, archive_path):
    z = zipfile.ZipFile(archive_path)
    archive_names = set(z.namelist())

    files = glob.glob(f"{xml_root}/**/*.xml", recursive=True)
    pairs = {}
    unresolved = []

    for f in files:
        category = os.path.relpath(f, xml_root).split(os.sep)[0]
        try:
            tree = ET.parse(f)
        except ET.ParseError:
            continue
        root = tree.getroot()
        for file_el in root.findall("File"):
            file_name = file_el.get("Name")
            if not file_name:
                continue
            offset_to_name = {}
            for child in file_el:
                off = child.get("Offset")
                name = child.get("Name")
                if off is not None and name:
                    offset_to_name[off.lower()] = name

            def resolve_path(entry_name):
                for candidate in (f"{category}/{file_name}/{entry_name}", f"{file_name}/{entry_name}"):
                    if candidate in archive_names:
                        return candidate
                return None

            for tex in file_el.findall("Texture"):
                fmt = (tex.get("Format") or "").lower()
                tlut_offset = tex.get("TlutOffset")
                name = tex.get("Name")
                if fmt not in ("ci4", "ci8") or not tlut_offset or not name:
                    continue
                tlut_name = offset_to_name.get(tlut_offset.lower())
                if not tlut_name:
                    unresolved.append((file_name, name, "no offset match"))
                    continue
                ci_path = resolve_path(name)
                tlut_path = resolve_path(tlut_name)
                if ci_path and tlut_path:
                    pairs[ci_path] = tlut_path
                else:
                    unresolved.append((file_name, name, f"ci={ci_path} tlut={tlut_path}"))

    return pairs, unresolved


def emit(pairs, game, out_dir):
    header_name = f"{game}_tlut_map.h"
    cpp_path = os.path.join(out_dir, f"{game}_tlut_map.cpp")
    table_name = f"k{game.capitalize()}TlutPairs"
    func_name = f"{game}TlutArchivePathFor"
    with open(cpp_path, "w") as out:
        out.write(f'#include "{header_name}"\n\n')
        out.write('#include <iterator>\n')
        out.write('#include <unordered_map>\n')
        out.write('#include <utility>\n\n')
        out.write('namespace bitdeck {\n\n')
        out.write('namespace {\n\n')
        out.write('// Ground-truth CI-texture -> TLUT archive path pairs, derived from\n')
        out.write(f'// the {game.upper()} decomp\'s assets/xml/**/*.xml (Texture TlutOffset attributes).\n')
        out.write(f'const std::pair<const char*, const char*> {table_name}[] = {{\n')
        for ci, tlut in sorted(pairs.items()):
            out.write(f'    {{"{ci}", "{tlut}"}},\n')
        out.write('};\n\n')
        out.write('} // namespace\n\n')
        out.write(f'std::optional<std::string> {func_name}(const std::string& archivePath) {{\n')
        out.write('    static const std::unordered_map<std::string, std::string> kMap = [] {\n')
        out.write('        std::unordered_map<std::string, std::string> map;\n')
        out.write(f'        map.reserve(std::size({table_name}));\n')
        out.write(f'        for (const auto& [ci, tlut] : {table_name}) {{\n')
        out.write('            map.emplace(ci, tlut);\n')
        out.write('        }\n')
        out.write('        return map;\n')
        out.write('    }();\n')
        out.write('    auto it = kMap.find(archivePath);\n')
        out.write('    if (it == kMap.end()) {\n')
        out.write('        return std::nullopt;\n')
        out.write('    }\n')
        out.write('    return it->second;\n')
        out.write('}\n\n')
        out.write('} // namespace bitdeck\n')
    print(f"wrote {len(pairs)} pairs to {cpp_path}")


if __name__ == "__main__":
    game = sys.argv[1]
    xml_root = sys.argv[2]
    archive = sys.argv[3]
    out_dir = sys.argv[4]
    pairs, unresolved = build(xml_root, archive)
    print(f"resolved {len(pairs)} pairs, {len(unresolved)} unresolved")
    for u in unresolved[:20]:
        print("  unresolved:", u)
    emit(pairs, game, out_dir)
