# Regenerates src/games/mm_tlut_map.cpp from a local MM decomp checkout and
# a real compiled mm.o2r (used to verify each candidate path actually exists).
# Usage: python3 gen_mm_tlut_map.py <mm-decomp>/assets/xml <path-to-mm.o2r> ../src/games/mm_tlut_map.cpp
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


def emit(pairs, out_path):
    with open(out_path, "w") as out:
        out.write('#include "mm_tlut_map.h"\n\n')
        out.write('#include <iterator>\n')
        out.write('#include <unordered_map>\n')
        out.write('#include <utility>\n\n')
        out.write('namespace bitdeck {\n\n')
        out.write('namespace {\n\n')
        out.write('// Ground-truth CI-texture -> TLUT archive path pairs, derived from\n')
        out.write('// the MM decomp\'s assets/xml/**/*.xml (Texture TlutOffset attributes).\n')
        out.write('const std::pair<const char*, const char*> kMmTlutPairs[] = {\n')
        for ci, tlut in sorted(pairs.items()):
            out.write(f'    {{"{ci}", "{tlut}"}},\n')
        out.write('};\n\n')
        out.write('} // namespace\n\n')
        out.write('std::optional<std::string> mmTlutArchivePathFor(const std::string& archivePath) {\n')
        out.write('    static const std::unordered_map<std::string, std::string> kMap = [] {\n')
        out.write('        std::unordered_map<std::string, std::string> map;\n')
        out.write('        map.reserve(std::size(kMmTlutPairs));\n')
        out.write('        for (const auto& [ci, tlut] : kMmTlutPairs) {\n')
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
    print(f"wrote {len(pairs)} pairs to {out_path}")


if __name__ == "__main__":
    xml_root = sys.argv[1]
    archive = sys.argv[2]
    out_path = sys.argv[3]
    pairs, unresolved = build(xml_root, archive)
    print(f"resolved {len(pairs)} pairs, {len(unresolved)} unresolved")
    for u in unresolved[:20]:
        print("  unresolved:", u)
    emit(pairs, out_path)
