# Regenerates src/games/bk64_i8_transparency_map.cpp, the baked table of
# BK64 I8 textures confirmed safe for the "Preview I8 transparency from
# brightness" toggle -- same purpose as gen_transparency_map.py (OOT/MM),
# adapted to BK64's very different decomp conventions:
#
#  - BK64's decomp doesn't have readable per-texture C symbols at all
#    (textures load at runtime from a numeric ROM asset table, not named
#    arrays) -- but game code DOES reference assets through meaningful
#    constants like ASSET_70E_SPRITE_SMOKE_2, and the archive's own
#    resource paths embed that same hex ID (assets/sprite/ASSET_70E_
#    SMOKE_2_<frame>_<variant>). The correlation key here is the hex ID,
#    not a full symbol name; one ID maps to MANY archive paths (every
#    frame/variant of that sprite).
#  - BK64 doesn't use the POLY_XLU_DISP/POLY_OPA_DISP queue-macro
#    convention OOT/MM do; render mode is set via literal
#    gsDPSetRenderMode(...)/gDPSetRenderMode(...) calls, either with the
#    FORCE_BL flag spelled out directly or via a canned macro name
#    containing XLU (translucent) or OPA (opaque).
#
# Usage: python3 gen_bk64_transparency_map.py <decomp_root> <path-to-archive.o2r> ../src/games
import glob, os, re, sys, zipfile

RENDER_MODE_CALL_RE = re.compile(r"gs?DPSetRenderMode\s*\(([^;]*?)\)", re.DOTALL)
ASSET_ID_RE = re.compile(r"\bASSET_([0-9A-Fa-f]+)_")
FUNCTION_CLOSE_RE = re.compile(r"\n\}(?!;)")
FORWARD_WINDOW_CHARS = 2000
# Canned mode names (e.g. G_RM_XLU_SURF, G_RM_AA_ZB_OPA_SURF2) -- NOT a bare
# "OPA"/"XLU" substring, which also appears in unrelated flags like
# ZMODE_OPA (a Z-buffer compare mode, nothing to do with surface
# transparency) and would false-positive on a real FORCE_BL call.
CANNED_XLU_RE = re.compile(r"\bG_RM_\w*XLU\w*\b")
CANNED_OPA_RE = re.compile(r"\bG_RM_\w*OPA\w*\b")


def classify_render_mode_args(args_text):
    # FORCE_BL is the actual "this surface blends" bit -- authoritative
    # regardless of what other flags (e.g. ZMODE_OPA) appear alongside it.
    if "FORCE_BL" in args_text:
        return "translucent"
    has_xlu = CANNED_XLU_RE.search(args_text) is not None
    has_opa = CANNED_OPA_RE.search(args_text) is not None
    if has_xlu and not has_opa:
        return "translucent"
    if has_opa and not has_xlu:
        return "opaque"
    return None


def nearest_render_mode(text, start, end):
    window_start = text.rfind("\n}", 0, start)
    window_start = 0 if window_start == -1 else window_start
    backward = text[window_start:start]

    forward_close = FUNCTION_CLOSE_RE.search(text, end, end + FORWARD_WINDOW_CHARS)
    forward_end = forward_close.end() if forward_close else min(end + FORWARD_WINDOW_CHARS, len(text))
    forward = text[end:forward_end]

    votes = set()
    for window in (backward, forward):
        for m in RENDER_MODE_CALL_RE.finditer(window):
            mode = classify_render_mode_args(m.group(1))
            if mode:
                votes.add(mode)
    return votes


def collect_i8_ids(i8_list_path):
    ids_to_paths = {}
    with open(i8_list_path) as fh:
        for line in fh:
            path = line.strip()
            if not path:
                continue
            m = re.search(r"/ASSET_([0-9A-Fa-f]+)_", path)
            if not m:
                continue
            ids_to_paths.setdefault(m.group(1).upper(), []).append(path)
    return ids_to_paths


# particleEmitter_setSprite(...) (src/core2/particle.c) is BK64's generic
# particle-drawing helper. Verified by hand: its own two render-mode DLs
# (D_80368940, using raw FORCE_BL flags; D_80368978, using the canned
# G_RM_XLU_SURF/XLU_SURF2 macros) are BOTH translucent -- any asset ID
# reaching it is unconditionally safe.
PARTICLE_SPRITE_CALL_RE = re.compile(
    r"particleEmitter_setSprite\s*\([^,)]+,\s*ASSET_([0-9A-Fa-f]+)_")


def particle_sprite_ids(decomp_root):
    c_files = glob.glob(f"{decomp_root}/src/**/*.c", recursive=True)
    ids = set()
    for path in c_files:
        try:
            with open(path, "r", encoding="utf-8", errors="ignore") as fh:
                text = fh.read()
        except OSError:
            continue
        for m in PARTICLE_SPRITE_CALL_RE.finditer(text):
            ids.add(m.group(1).upper())
    return ids


def classify(decomp_root, ids):
    c_files = glob.glob(f"{decomp_root}/src/**/*.c", recursive=True)
    votes = {i: set() for i in ids}
    id_res = {i: re.compile(rf"\bASSET_{i}_") for i in ids}
    for path in c_files:
        try:
            with open(path, "r", encoding="utf-8", errors="ignore") as fh:
                text = fh.read()
        except OSError:
            continue
        for hex_id, pattern in id_res.items():
            m = pattern.search(text)
            if not m:
                continue
            votes[hex_id] |= nearest_render_mode(text, m.start(), m.end())

    translucent, ambiguous = [], []
    for hex_id, modes in votes.items():
        if modes == {"translucent"}:
            translucent.append(hex_id)
        elif len(modes) > 1:
            ambiguous.append(hex_id)
    return translucent, ambiguous


def emit(paths, out_dir):
    header_path = os.path.join(out_dir, "bk64_i8_transparency_map.h")
    cpp_path = os.path.join(out_dir, "bk64_i8_transparency_map.cpp")

    with open(header_path, "w") as out:
        out.write("#pragma once\n\n")
        out.write("#include <string>\n\n")
        out.write("namespace bitdeck {\n\n")
        out.write("// True if archivePath is a BK64 I8 texture confirmed safe for the\n")
        out.write("// 'Preview I8 transparency from brightness' toggle (see\n")
        out.write("// tools/gen_bk64_transparency_map.py) -- its ASSET_<id> was referenced\n")
        out.write("// only alongside a translucent render mode wherever this scan found it.\n")
        out.write("bool bk64I8TextureIsTranslucent(const std::string& archivePath);\n\n")
        out.write("} // namespace bitdeck\n")

    with open(cpp_path, "w") as out:
        out.write('#include "bk64_i8_transparency_map.h"\n\n')
        out.write("#include <unordered_set>\n\n")
        out.write("namespace bitdeck {\n\n")
        out.write("namespace {\n\n")
        out.write("const char* const kBk64TranslucentI8Textures[] = {\n")
        for p in sorted(paths):
            out.write(f'    "{p}",\n')
        out.write("};\n\n")
        out.write("} // namespace\n\n")
        out.write("bool bk64I8TextureIsTranslucent(const std::string& archivePath) {\n")
        out.write("    static const std::unordered_set<std::string> kSet(\n")
        out.write("        std::begin(kBk64TranslucentI8Textures), std::end(kBk64TranslucentI8Textures));\n")
        out.write("    return kSet.count(archivePath) != 0;\n")
        out.write("}\n\n")
        out.write("} // namespace bitdeck\n")
    print(f"wrote {len(paths)} translucent-I8 paths to {cpp_path}")


if __name__ == "__main__":
    decomp_root = sys.argv[1]
    i8_list_path = sys.argv[2]
    out_dir = sys.argv[3]

    ids_to_paths = collect_i8_ids(i8_list_path)
    print(f"{len(ids_to_paths)} distinct I8 asset IDs ({sum(len(v) for v in ids_to_paths.values())} archive paths)")

    translucent, ambiguous = classify(decomp_root, ids_to_paths)
    particle_ids = particle_sprite_ids(decomp_root) & set(ids_to_paths)
    print(f"direct: {len(translucent)} translucent, {len(ambiguous)} ambiguous, "
          f"{len(ids_to_paths) - len(translucent) - len(ambiguous)} not found")
    print(f"plus {len(particle_ids)} via particleEmitter_setSprite")

    paths = set()
    for hex_id in set(translucent) | particle_ids:
        paths.update(ids_to_paths[hex_id])
    emit(paths, out_dir)
