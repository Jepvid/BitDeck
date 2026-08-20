# Regenerates src/games/mk64_tlut_map.cpp from a local SpaghettiKart checkout.
# Usage: python3 gen_mk64_tlut_map.py <path-to-SpaghettiKart>/yamls/us ../src/games/mk64_tlut_map.cpp
import yaml, glob, os, sys

def build(yaml_root):
    files = glob.glob(f"{yaml_root}/textures/**/*.yml", recursive=True)
    symbol_to_path = {}
    key_to_path = {}
    ci_entries = []

    for f in files:
        rel = os.path.relpath(f, yaml_root)
        archive_dir = os.path.splitext(rel)[0]
        with open(f) as fh:
            try:
                doc = yaml.safe_load(fh)
            except Exception:
                continue
        if not isinstance(doc, dict):
            continue
        for k, v in doc.items():
            if k.startswith(":") or not isinstance(v, dict):
                continue
            archive_path = f"{archive_dir}/{k}"
            symbol = v.get("symbol", k)
            symbol_to_path[symbol] = archive_path
            key_to_path[k] = archive_path
            fmt = str(v.get("format", "")).lower()
            tlut_ref = v.get("tlut_symbol") or (v.get("tlut") if isinstance(v.get("tlut"), str) else None)
            if fmt in ("ci4", "ci8") and tlut_ref:
                ci_entries.append((archive_path, tlut_ref))

    pairs = {}
    for ci_path, tlut_ref in ci_entries:
        tlut_path = symbol_to_path.get(tlut_ref) or key_to_path.get(tlut_ref)
        if tlut_path:
            pairs[ci_path] = tlut_path
    return pairs


def emit(pairs, out_path):
    with open(out_path, "w") as out:
        out.write('#include "mk64_tlut_map.h"\n\n')
        out.write('#include <iterator>\n')
        out.write('#include <unordered_map>\n')
        out.write('#include <utility>\n\n')
        out.write('namespace bitdeck {\n\n')
        out.write('namespace {\n\n')
        out.write('// Ground-truth CI-texture -> TLUT archive path pairs, derived from\n')
        out.write('// SpaghettiKart\'s yamls/us/textures/**/*.yml (tlut/tlut_symbol fields).\n')
        out.write(f'const std::pair<const char*, const char*> kMk64TlutPairs[] = {{\n')
        for ci, tlut in sorted(pairs.items()):
            out.write(f'    {{"{ci}", "{tlut}"}},\n')
        out.write('};\n\n')
        out.write('} // namespace\n\n')
        out.write('std::optional<std::string> mk64TlutArchivePathFor(const std::string& archivePath) {\n')
        out.write('    static const std::unordered_map<std::string, std::string> kMap = [] {\n')
        out.write('        std::unordered_map<std::string, std::string> map;\n')
        out.write('        map.reserve(std::size(kMk64TlutPairs));\n')
        out.write('        for (const auto& [ci, tlut] : kMk64TlutPairs) {\n')
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
    yaml_root = sys.argv[1]
    out_path = sys.argv[2]
    pairs = build(yaml_root)
    emit(pairs, out_path)
