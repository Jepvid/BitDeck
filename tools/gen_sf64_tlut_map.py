# Regenerates src/games/sf64_tlut_map.cpp from a local SF64 decomp checkout
# and a real compiled sf64.o2r (used to verify each candidate path exists).
# SF64's yaml pairs a CI texture to its TLUT by numeric address: the
# texture's own "tlut:" field is the paired TLUT entry's "offset:" value,
# matched within the same yaml file.
# Usage: python3 gen_sf64_tlut_map.py <sf64-decomp>/assets/yaml <path-to-sf64.o2r> ../src/games
import yaml, glob, os, zipfile, sys

def parse_flow_entries(path):
    # SF64's yaml uses flow-style single-line mappings per symbol
    # ("aOptInvoiceTex: { type: TEXTURE, ... }"), safe_load handles this
    # like any other block-mapping-of-flow-mappings document.
    with open(path) as f:
        try:
            doc = yaml.safe_load(f)
        except yaml.YAMLError:
            return {}
    if not isinstance(doc, dict):
        return {}
    return {k: v for k, v in doc.items() if isinstance(v, dict) and not k.startswith(":")}


def build(yaml_root, archive_path):
    z = zipfile.ZipFile(archive_path)
    archive_names = set(z.namelist())

    files = glob.glob(f"{yaml_root}/**/*.yaml", recursive=True) + glob.glob(f"{yaml_root}/**/*.yml", recursive=True)
    pairs = {}
    unresolved = []

    for f in files:
        stem = os.path.splitext(os.path.basename(f))[0]
        entries = parse_flow_entries(f)
        if not entries:
            continue

        offset_to_symbol = {}
        for key, entry in entries.items():
            offset = entry.get("offset")
            symbol = entry.get("symbol", key)
            if offset is not None:
                offset_to_symbol[offset] = symbol

        for key, entry in entries.items():
            fmt = str(entry.get("format", "")).lower()
            tlut_offset = entry.get("tlut")
            symbol = entry.get("symbol", key)
            if fmt not in ("ci4", "ci8") or tlut_offset is None:
                continue
            tlut_symbol = offset_to_symbol.get(tlut_offset)
            if not tlut_symbol:
                unresolved.append((stem, symbol, "no offset match"))
                continue
            ci_path = f"{stem}/{symbol}"
            tlut_path = f"{stem}/{tlut_symbol}"
            if ci_path in archive_names and tlut_path in archive_names:
                pairs[ci_path] = tlut_path
            else:
                unresolved.append((stem, symbol, f"ci_in_archive={ci_path in archive_names} tlut_in_archive={tlut_path in archive_names}"))

    return pairs, unresolved


def emit(pairs, out_dir):
    cpp_path = os.path.join(out_dir, "sf64_tlut_map.cpp")
    with open(cpp_path, "w") as out:
        out.write('#include "sf64_tlut_map.h"\n\n')
        out.write('#include <iterator>\n')
        out.write('#include <unordered_map>\n')
        out.write('#include <utility>\n\n')
        out.write('namespace bitdeck {\n\n')
        out.write('namespace {\n\n')
        out.write('// Ground-truth CI-texture -> TLUT archive path pairs, derived from\n')
        out.write('// the SF64 decomp\'s assets/yaml/**/*.yaml ("tlut:" numeric offset fields).\n')
        out.write('const std::pair<const char*, const char*> kSf64TlutPairs[] = {\n')
        for ci, tlut in sorted(pairs.items()):
            out.write(f'    {{"{ci}", "{tlut}"}},\n')
        out.write('};\n\n')
        out.write('} // namespace\n\n')
        out.write('std::optional<std::string> sf64TlutArchivePathFor(const std::string& archivePath) {\n')
        out.write('    static const std::unordered_map<std::string, std::string> kMap = [] {\n')
        out.write('        std::unordered_map<std::string, std::string> map;\n')
        out.write('        map.reserve(std::size(kSf64TlutPairs));\n')
        out.write('        for (const auto& [ci, tlut] : kSf64TlutPairs) {\n')
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
    yaml_root = sys.argv[1]
    archive = sys.argv[2]
    out_dir = sys.argv[3]
    pairs, unresolved = build(yaml_root, archive)
    print(f"resolved {len(pairs)} pairs, {len(unresolved)} unresolved")
    for u in unresolved[:20]:
        print("  unresolved:", u)
    emit(pairs, out_dir)
