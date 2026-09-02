# Regenerates a src/games/<game>_i8_transparency_map.cpp baked table of I8
# textures that are safe to preview with alpha derived from brightness (see
# scanDisplayListForTransparencyInfo / the "Preview I8 transparency from
# brightness" toggle).
#
# Render mode (opaque vs. translucent/blended) is a property of the
# material, not runtime-random state like prim color: an I8 texture drawn
# with a blend-enabled (FORCE_BL) render mode is meant to be seen through,
# one drawn opaque isn't. Two tiers:
#  1. The compiled archive's own DisplayList resources (see
#     scanDisplayListForTransparencyInfo, same binary scan as Apply IA
#     Color) -- covers any texture whose draw call is baked into an asset
#     object's static Gfx array.
#  2. A source-level fallback for textures only ever drawn by a
#     runtime-built Gfx array in game logic C code (never an archive
#     resource, e.g. particle effects) -- scans for the nearest enclosing
#     POLY_XLU_DISP/POLY_OPA_DISP queue macro before each texture symbol's
#     use, within the same function body. Only recorded if every occurrence
#     agrees; a texture drawn into both queues in different places is left
#     unresolved rather than guessed at.
#
# A large fraction of I8 particle textures are frame/variant siblings picked
# at runtime from a shared array or ternary (e.g. "sTextures[] = {
# gEffEnemyDeathFlame1Tex, gEffEnemyDeathFlame2Tex, ...}", or
# "Rand_ZeroOne() < 0.5f ? gEffBubble1Tex : gEffBubble2Tex"), drawn through
# one shared code path and sharing one render mode -- most siblings never
# appear anywhere else in source for the per-symbol scan to find a queue
# macro near. A third tier groups symbols that co-occur within a short span
# of each other (array literals/ternaries pack their entries within a few
# characters) and propagates any single resolved classification to the
# whole group.
#
# A fourth tier handles textures passed as an argument into a shared
# drawing helper (e.g. "EffectSs_DrawGEffect(play, this, waterSplashTextures
# [texIdx])" -- the call site itself never mentions a queue macro, only the
# helper's own body does. One hop only: find the called function's
# definition and vote from its body, same single-clean-type discipline as
# the direct per-symbol scan.
#
# A fifth tier handles an array indexed directly at the call site instead of
# passed into a helper (e.g. "gSPSegment(POLY_XLU_DISP++, 0x08,
# SEGMENTED_TO_VIRTUAL(sLightningTextures[eff->timer]))" -- the fourth
# tier's one-hop function-body lookup finds nothing here, since
# SEGMENTED_TO_VIRTUAL is a macro, not a function with a body to search):
# votes every member of an array literal with the queue(s) found around the
# array's own variable name, not just each member's own name.
#
# A sixth tier covers a texture whose only draw call lives in a compile-time
# static Gfx array baked from the ROM's own disassembly rather than built by
# game logic -- tier 1 can't resolve it either, since this pattern compiles
# to a plain segmented pointer, never an OTR hash. Scans ZAPD's
# extracted/**/*.inc.c disassembly (not source C, so outside every other
# tier's search) for a canned gsDPSetRenderMode(...) macro name textually
# next to the symbol, resolved against include/ultra64/gbi.h's own FORCE_BL
# bit rather than a hand-maintained table.
#
# Symbol names collide across unrelated files (e.g. "s1Tex"/"s2Tex" reused
# by many overlays with different meanings, "sTextures" reused by many
# actors as different arrays) -- every tier above scopes a colliding name's
# search to its own declaring file/overlay folder instead of matching any
# same-named symbol anywhere in the decomp.
#
# Usage: python3 gen_transparency_map.py <game> <decomp_root> <path-to-archive.o2r> ../src/games
import glob, os, re, sys, xml.etree.ElementTree as ET, zipfile

QUEUE_RE = re.compile(r"\b(POLY_XLU_DISP|POLY_OPA_DISP)\b")
SIBLING_GAP_CHARS = 100


def collect_i8_textures(xml_root, archive_names):
    """Returns (symbols, home_folder). symbols maps a lookup key to its
    archive path -- normally the bare C symbol name (e.g. "gDust1Tex"), but
    a name reused by multiple different <File> blocks (very common for
    generic per-overlay statics like "s1Tex"/"s2Tex", each overlay's own
    unrelated local variable) gets a synthetic "name@file_name" key instead,
    so the two don't collapse into one lookup entry. home_folder maps every
    synthetic-keyed symbol to its owning XML <File Name>, letting the
    source scan restrict its search to that overlay's own source folder
    instead of matching any file's same-named local by accident."""
    files = glob.glob(f"{xml_root}/**/*.xml", recursive=True)
    found = []  # (name, file_name, archive_path)
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
            for tex in file_el.findall("Texture"):
                fmt = (tex.get("Format") or "").lower()
                name = tex.get("Name")
                if fmt != "i8" or not name:
                    continue
                for candidate in (f"{category}/{file_name}/{name}", f"{file_name}/{name}"):
                    if candidate in archive_names:
                        found.append((name, file_name, candidate))
                        break

    # Some objects have a second "_pal" XML (e.g. object_bv_pal.xml
    # alongside object_bv.xml) redeclaring the exact same File Name and
    # Texture Name/path -- not a real collision, just a duplicate
    # declaration of the same asset. Dedupe before counting so those
    # don't falsely trigger the disambiguation path below.
    found = sorted(set(found))

    name_to_paths = {}
    for name, _file_name, path in found:
        name_to_paths.setdefault(name, set()).add(path)

    symbols = {}
    home_folder = {}
    for name, file_name, path in found:
        if len(name_to_paths[name]) > 1:
            key = f"{name}@{file_name}"
            home_folder[key] = file_name
        else:
            key = name
        symbols[key] = path
    return symbols, home_folder


FORWARD_WINDOW_CHARS = 3000
FORWARD_FUNCTION_HOPS = 3
# A bare function close ("\n}"), not an array/struct initializer's "\n};".
FUNCTION_CLOSE_RE = re.compile(r"\n\}(?!;)")


def surrounding_queues(text, start, end):
    """Queue macros in the enclosing function (scanning back to the
    previous function boundary) plus, if the symbol sits in a file-scope
    array/ternary declaration rather than inside a function body (the
    common "static void* sTextures[] = {...}; void Foo_Draw(...) {...
    sTextures[idx] ...}" pattern -- the array's own members never appear
    by name in the function that actually draws them), the next couple of
    function bodies that follow it."""
    backward_close = None
    for m in FUNCTION_CLOSE_RE.finditer(text, 0, start):
        backward_close = m
    window_start = backward_close.end() if backward_close else 0
    backward = text[window_start:start]

    forward_limit = end + FORWARD_WINDOW_CHARS
    forward_end = end
    for _ in range(FORWARD_FUNCTION_HOPS):
        forward_close = FUNCTION_CLOSE_RE.search(text, forward_end, forward_limit)
        if not forward_close:
            forward_end = min(forward_limit, len(text))
            break
        forward_end = forward_close.end()
    forward = text[end:forward_end]

    return set(QUEUE_RE.findall(backward)) | set(QUEUE_RE.findall(forward))


def bare_name(sym):
    """Strips a synthetic "name@file_name" disambiguation suffix (see
    collect_i8_textures) back to the real C symbol name to search for."""
    return sym.split("@", 1)[0]


def scoped_c_files(decomp_root, sym, home_folder, all_files, folder_files_cache):
    """The file list to search for sym's own text occurrences: just its
    owning overlay's source folder if sym is a disambiguated (collision-
    prone) symbol, every source file otherwise."""
    folder = home_folder.get(sym)
    if folder is None:
        return all_files
    if folder not in folder_files_cache:
        folder_files_cache[folder] = glob.glob(f"{decomp_root}/src/**/{folder}/*.c", recursive=True)
    return folder_files_cache[folder]


def direct_votes(decomp_root, symbols, home_folder):
    """For each symbol, every occurrence across every file casts a "vote"
    for its own local (backward+forward window) queue -- but only if that
    window is itself unambiguous. A giant multi-effect draw function (some
    OOT bosses mix opaque and translucent effects in one function) produces
    a mixed local window at one occurrence; that occurrence casts no vote
    rather than poisoning the symbol's result when other, cleaner
    occurrences elsewhere agree. A disambiguated symbol (home_folder) only
    searches its own overlay folder, never a same-named local elsewhere."""
    all_files = glob.glob(f"{decomp_root}/src/**/*.c", recursive=True)
    folder_files_cache = {}
    text_cache = {}
    votes = {sym: set() for sym in symbols}
    for sym in symbols:
        name = bare_name(sym)
        for path in scoped_c_files(decomp_root, sym, home_folder, all_files, folder_files_cache):
            if path not in text_cache:
                try:
                    with open(path, "r", encoding="utf-8", errors="ignore") as fh:
                        text_cache[path] = fh.read()
                except OSError:
                    text_cache[path] = None
            text = text_cache[path]
            if text is None:
                continue
            idx = text.find(name)
            if idx == -1:
                continue
            local = surrounding_queues(text, idx, idx + len(name))
            if len(local) == 1:
                votes[sym] |= local
    return votes


def classify_from_votes(votes):
    """Only votes that disagree with each other make a symbol ambiguous."""
    classified = {}
    ambiguous = set()
    for sym, queues in votes.items():
        if not queues:
            continue
        if queues == {"POLY_XLU_DISP"}:
            classified[sym] = "POLY_XLU_DISP"
        elif queues == {"POLY_OPA_DISP"}:
            classified[sym] = "POLY_OPA_DISP"
        else:
            ambiguous.add(sym)
    return classified, ambiguous


ARRAY_DECL_RE = re.compile(r"(\w+)\s*\[\s*\]\s*=\s*\{")
FUNC_DEF_RE = re.compile(r"\n\w[\w\s\*]*?\b(\w+)\s*\(([^;{}]*)\)\s*\{")
CALL_RE = re.compile(r"\b(\w+)\s*\(([^;{}()]*)\)")


def find_array_variables(decomp_root, symbols):
    """Maps each array-literal variable name (e.g. "waterSplashTextures" in
    "static void* waterSplashTextures[] = {gEffWaterSplash1Tex, ...};") to
    its known-symbol members."""
    c_files = glob.glob(f"{decomp_root}/src/**/*.c", recursive=True)
    array_vars = {}
    for path in c_files:
        try:
            with open(path, "r", encoding="utf-8", errors="ignore") as fh:
                text = fh.read()
        except OSError:
            continue
        for m in ARRAY_DECL_RE.finditer(text):
            close = text.find("};", m.end())
            if close == -1:
                continue
            body = text[m.end():close]
            members = [tok.strip() for tok in body.split(",")]
            members = [tok for tok in members if tok in symbols]
            if members:
                array_vars.setdefault(m.group(1), set()).update(members)
    return array_vars


def find_scoped_array_variables(decomp_root, symbols):
    """Like find_array_variables, but keeps each array literal's own
    declaring file attached instead of merging every same-named array
    across the whole decomp into one entry -- a generic name like
    "sTextures" is redeclared with completely different members by many
    unrelated actor files, and array_name_votes must never let one file's
    queue-macro usage vote for another file's same-named-but-unrelated
    array. Returns a list of (file_path, file_text, array_name, members)."""
    c_files = glob.glob(f"{decomp_root}/src/**/*.c", recursive=True)
    found = []
    for path in c_files:
        try:
            with open(path, "r", encoding="utf-8", errors="ignore") as fh:
                text = fh.read()
        except OSError:
            continue
        for m in ARRAY_DECL_RE.finditer(text):
            close = text.find("};", m.end())
            if close == -1:
                continue
            body = text[m.end():close]
            members = [tok.strip() for tok in body.split(",")]
            members = [tok for tok in members if tok in symbols]
            if members:
                found.append((path, text, m.group(1), members))
    return found


ARRAY_NAME_WINDOW_CHARS = 300


def array_name_votes(scoped_arrays):
    """Votes every array literal's members with the queue(s) found within a
    small fixed window around the array's own variable name -- covers a
    draw call that indexes the array directly at the call site (e.g.
    "gSPSegment(POLY_XLU_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(
    sLightningTextures[eff->timer]))") rather than passing it into a named
    helper function (see call_argument_votes, which only votes from a
    called function's own body and misses this pattern since
    SEGMENTED_TO_VIRTUAL is a macro, not a function with a body to
    search). A small window, not surrounding_queues' whole-enclosing-
    function scan, since the array name's own declaration and its one
    indexed use both sit inside the same large multi-effect draw function
    here -- a function-wide scan would see every other effect type's own
    (possibly different) queue in the same function and call the symbol
    ambiguous, when the actual queue macro is one line away from the
    array name at the real use site."""
    votes = {}
    for path, text, array_name, members in scoped_arrays:
        for sym in members:
            votes.setdefault(sym, set())
        pos = 0
        while True:
            idx = text.find(array_name, pos)
            if idx == -1:
                break
            pos = idx + len(array_name)
            window = text[max(0, idx - ARRAY_NAME_WINDOW_CHARS):idx + ARRAY_NAME_WINDOW_CHARS]
            local = set(QUEUE_RE.findall(window))
            if len(local) == 1:
                for sym in members:
                    votes[sym] |= local
    return votes


def build_function_index(decomp_root):
    """Maps every function name defined in source to its own body text (see
    FUNCTION_CLOSE_RE for how the body's end is found)."""
    c_files = glob.glob(f"{decomp_root}/src/**/*.c", recursive=True)
    functions = {}
    for path in c_files:
        try:
            with open(path, "r", encoding="utf-8", errors="ignore") as fh:
                text = fh.read()
        except OSError:
            continue
        for m in FUNC_DEF_RE.finditer(text):
            close = FUNCTION_CLOSE_RE.search(text, m.end())
            if not close:
                continue
            functions[m.group(1)] = text[m.end():close.end()]
    return functions


def split_args(arg_text):
    args, depth, current = [], 0, ""
    for ch in arg_text:
        if ch == "," and depth == 0:
            args.append(current)
            current = ""
            continue
        if ch in "([":
            depth += 1
        elif ch in ")]":
            depth -= 1
        current += ch
    if current.strip():
        args.append(current)
    return args


def call_argument_votes(decomp_root, symbols, array_vars, functions):
    """For every call site passing a known symbol or array variable as an
    argument, votes with the queue found in the CALLED function's own body
    (one hop only) -- covers textures whose actual draw call lives inside a
    shared helper the call site never mentions a queue macro near."""
    c_files = glob.glob(f"{decomp_root}/src/**/*.c", recursive=True)
    votes = {sym: set() for sym in symbols}
    base_re = re.compile(r"^(\w+)")

    for path in c_files:
        try:
            with open(path, "r", encoding="utf-8", errors="ignore") as fh:
                text = fh.read()
        except OSError:
            continue
        for m in CALL_RE.finditer(text):
            func_name = m.group(1)
            body = functions.get(func_name)
            if body is None:
                continue
            for arg in split_args(m.group(2)):
                base_match = base_re.match(arg.strip())
                if not base_match:
                    continue
                base = base_match.group(1)
                targets = [base] if base in symbols else array_vars.get(base, ())
                if not targets:
                    continue
                local = set(QUEUE_RE.findall(body))
                if len(local) == 1:
                    for sym in targets:
                        votes[sym] |= local
    return votes


RM_BASE_MACRO_RE = re.compile(r"#define\s+(RM_\w+)\(clk\)\s*\\\n((?:.*\\\n)*.*)", re.MULTILINE)
G_RM_ALIAS_RE = re.compile(r"#define\s+(G_RM_\w+)\s+(RM_\w+)\(\d+\)")
RENDER_MODE_CALL_RE = re.compile(r"gsDPSetRenderMode\s*\(([^)]*)\)")
CANNED_MODE_NAME_RE = re.compile(r"\bG_RM_\w+\b")
INC_C_WINDOW_CHARS = 1500


def find_gbi_h(decomp_root):
    """Locates gbi.h -- its path within a decomp varies (OOT keeps it at
    include/ultra64/gbi.h, MM at include/PR/gbi.h); tries known
    conventions first, then falls back to a recursive search rather than
    hardcoding one decomp's layout."""
    for candidate in ("include/ultra64/gbi.h", "include/PR/gbi.h"):
        path = os.path.join(decomp_root, candidate)
        if os.path.isfile(path):
            return path
    matches = glob.glob(f"{decomp_root}/include/**/gbi.h", recursive=True)
    if matches:
        return matches[0]
    raise FileNotFoundError(f"gbi.h not found under {decomp_root}/include")


def parse_render_mode_macros(decomp_root):
    """Parses gbi.h's canned RM_<BASE>(clk) render-mode macro bodies for
    the FORCE_BL flag (every translucent N64 render mode sets it, every
    opaque one leaves it clear -- same authoritative bit
    scanDisplayListForTransparencyInfo checks in the compiled binary), and
    each G_RM_<NAME> alias's own base macro, so a canned mode name as it
    appears in ZAPD's disassembled DL text (e.g. "G_RM_ZB_CLD_SURF2") can
    be resolved to translucent/opaque without hand-maintaining that table."""
    with open(find_gbi_h(decomp_root), "r", encoding="utf-8", errors="ignore") as fh:
        text = fh.read()
    rm_force_bl = {m.group(1): ("FORCE_BL" in m.group(2)) for m in RM_BASE_MACRO_RE.finditer(text)}
    g_rm_to_base = {m.group(1): m.group(2) for m in G_RM_ALIAS_RE.finditer(text)}
    return rm_force_bl, g_rm_to_base


def inc_c_render_mode_votes(decomp_root, symbols, home_folder):
    """Source-level fallback for a texture whose only draw call lives in a
    compile-time static Gfx array baked from the ROM's own disassembly
    (e.g. "static Gfx sMaterialDL[22] = { #include
    "assets/overlays/ovl_Arrow_Fire/sMaterialDL.inc.c" };"): these use a
    canned render-mode macro name textually next to the texture's own
    symbol name inside ZAPD's extracted/ disassembly -- files direct_votes
    never sees, since it only scans src/**/*.c -- and often a plain
    (non-OTR-hash-patched) SETTIMG scanDisplayListForTransparencyInfo
    can't resolve either, since a compile-time-local static array's own
    texture load compiles to a raw segmented pointer, never an OTR hash.
    Globs *.c (not *.inc.c) since this disassembly's own layout varies by
    decomp: OOT splits one .inc.c per symbol, MM bundles a whole
    overlay's disassembly into one plain .c file -- "*.c" matches both,
    since fnmatch treats "*.inc.c" as ending in ".c" too. Resolves each
    canned name via parse_render_mode_macros, then votes with a synthetic
    POLY_XLU_DISP/POLY_OPA_DISP so it flows through the same
    classify_from_votes/sibling-group machinery as every other tier. A
    disambiguated symbol (home_folder) only searches its own overlay's
    extracted folder, never a same-named local elsewhere."""
    rm_force_bl, g_rm_to_base = parse_render_mode_macros(decomp_root)

    def canned_force_bl(name):
        base = g_rm_to_base.get(name)
        return rm_force_bl.get(base) if base else None

    all_files = glob.glob(f"{decomp_root}/extracted/**/*.c", recursive=True)
    folder_files_cache = {}
    text_cache = {}
    votes = {sym: set() for sym in symbols}
    for sym in symbols:
        name = bare_name(sym)
        folder = home_folder.get(sym)
        if folder is None:
            files = all_files
        else:
            if folder not in folder_files_cache:
                folder_files_cache[folder] = glob.glob(f"{decomp_root}/extracted/**/{folder}/*.c", recursive=True)
            files = folder_files_cache[folder]
        for path in files:
            if path not in text_cache:
                try:
                    with open(path, "r", encoding="utf-8", errors="ignore") as fh:
                        text_cache[path] = fh.read()
                except OSError:
                    text_cache[path] = None
            text = text_cache[path]
            if text is None:
                continue
            idx = text.find(name)
            if idx == -1:
                continue
            window = text[max(0, idx - INC_C_WINDOW_CHARS):idx + INC_C_WINDOW_CHARS]
            local = set()
            for call_args in RENDER_MODE_CALL_RE.findall(window):
                for canned in CANNED_MODE_NAME_RE.findall(call_args):
                    fbl = canned_force_bl(canned)
                    if fbl is True:
                        local.add("POLY_XLU_DISP")
                    elif fbl is False:
                        local.add("POLY_OPA_DISP")
            if len(local) == 1:
                votes[sym] |= local
    return votes


class UnionFind:
    def __init__(self, items):
        self.parent = {item: item for item in items}

    def find(self, item):
        while self.parent[item] != item:
            self.parent[item] = self.parent[self.parent[item]]
            item = self.parent[item]
        return item

    def union(self, a, b):
        ra, rb = self.find(a), self.find(b)
        if ra != rb:
            self.parent[ra] = rb


def _union_cooccurring(uf, bare_to_sym, files):
    names_by_len = sorted(bare_to_sym, key=len, reverse=True)
    combined_re = re.compile(r"\b(?:" + "|".join(re.escape(n) for n in names_by_len) + r")\b")
    for path in files:
        try:
            with open(path, "r", encoding="utf-8", errors="ignore") as fh:
                text = fh.read()
        except OSError:
            continue
        matches = list(combined_re.finditer(text))
        for prev, cur in zip(matches, matches[1:]):
            if cur.start() - prev.end() <= SIBLING_GAP_CHARS:
                uf.union(bare_to_sym[prev.group()], bare_to_sym[cur.group()])


def find_sibling_groups(decomp_root, symbols, home_folder):
    """Groups symbols that co-occur within SIBLING_GAP_CHARS of each other
    in source -- catches array-literal/ternary sibling sets (see module
    docstring) without needing to understand C syntax. A disambiguated
    symbol (home_folder) only looks for co-occurrence within its own
    overlay folder, alongside that overlay's own other symbols."""
    all_files = glob.glob(f"{decomp_root}/src/**/*.c", recursive=True)
    uf = UnionFind(symbols)

    global_syms = [s for s in symbols if s not in home_folder]
    if global_syms:
        _union_cooccurring(uf, {bare_name(s): s for s in global_syms}, all_files)

    folder_to_syms = {}
    for sym, folder in home_folder.items():
        folder_to_syms.setdefault(folder, []).append(sym)
    folder_files_cache = {}
    for folder, syms in folder_to_syms.items():
        files = scoped_c_files(decomp_root, syms[0], home_folder, all_files, folder_files_cache)
        _union_cooccurring(uf, {bare_name(s): s for s in syms}, files)

    groups = {}
    for sym in symbols:
        groups.setdefault(uf.find(sym), []).append(sym)
    return [g for g in groups.values() if len(g) > 1]


def propagate_sibling_groups(classified, ambiguous, groups):
    for group in groups:
        modes = {classified[sym] for sym in group if sym in classified}
        if any(sym in ambiguous for sym in group) or len(modes) > 1:
            for sym in group:
                ambiguous.add(sym)
                classified.pop(sym, None)
        elif len(modes) == 1:
            mode = next(iter(modes))
            for sym in group:
                if sym not in ambiguous:
                    classified[sym] = mode


def emit(paths, game, out_dir):
    header_name = f"{game}_i8_transparency_map.h"
    header_path = os.path.join(out_dir, header_name)
    cpp_path = os.path.join(out_dir, f"{game}_i8_transparency_map.cpp")
    set_name = f"k{game.capitalize()}TranslucentI8Textures"
    func_name = f"{game}I8TextureIsTranslucent"

    with open(header_path, "w") as out:
        out.write("#pragma once\n\n")
        out.write("#include <string>\n\n")
        out.write("namespace bitdeck {\n\n")
        out.write(f"// True if archivePath is a {game.upper()} I8 texture confirmed safe for the\n")
        out.write("// 'Preview I8 transparency from brightness' toggle (see\n")
        out.write("// tools/gen_transparency_map.py) -- drawn with a blended render mode\n")
        out.write("// wherever this scan found its draw call, never an opaque one.\n")
        out.write(f"bool {func_name}(const std::string& archivePath);\n\n")
        out.write("} // namespace bitdeck\n")

    with open(cpp_path, "w") as out:
        out.write(f'#include "{header_name}"\n\n')
        out.write('#include <unordered_set>\n\n')
        out.write('namespace bitdeck {\n\n')
        out.write('namespace {\n\n')
        out.write(f'const char* const {set_name}[] = {{\n')
        for p in sorted(paths):
            out.write(f'    "{p}",\n')
        out.write('};\n\n')
        out.write('} // namespace\n\n')
        out.write(f'bool {func_name}(const std::string& archivePath) {{\n')
        out.write('    static const std::unordered_set<std::string> kSet(\n')
        out.write(f'        std::begin({set_name}), std::end({set_name}));\n')
        out.write('    return kSet.count(archivePath) != 0;\n')
        out.write('}\n\n')
        out.write('} // namespace bitdeck\n')
    print(f"wrote {len(paths)} translucent-I8 paths to {cpp_path}")


# Hand-verified additions the source-scanning tiers can't reach generically
# -- their render mode lives with the CALLER of a generic renderer (e.g. a
# skinned-skeleton actor's SkelAnime_Draw call), not with the texture's own
# display list or any call site the scanner can see. Verified by reading
# the compiled archive's own DL bytes directly plus the actor code, not
# guessed; each entry documents how. Merged into the generated table below
# rather than written to it directly.
MANUAL_TRANSLUCENT_I8 = {
    "oot": {
        # objects/gameplay_keep/{gGlowCircleTextureLoadDL,gGlowCircleDL,
        # gGlowCircleSmallDL} (fairy_skel.c's glow-ball sub-DLs) set no
        # render mode of their own -- but EnElf_Draw (z_en_elf.c, the
        # fairy/Navi actor) does Gfx_SetupDL_27Xlu then
        # "POLY_XLU_DISP = SkelAnime_Draw(...)" for the whole skeleton.
        "objects/gameplay_keep/gCircleGlowLTex",
        "objects/gameplay_keep/gCircleGlowRTex",
        "objects/gameplay_keep/gCircleGlowSLTex",
        "objects/gameplay_keep/gCircleGlowSRTex",
    },
}

# Deliberate overrides: the real game draws these with mixed render modes
# across different callers (confirmed, not a scanner gap) -- no single
# accurate answer exists. Each entry is an explicit call favoring editing
# convenience for texture pack makers over one of the in-game contexts.
# Kept separate from MANUAL_TRANSLUCENT_I8 above, which is unambiguous
# ground truth; these are judgment calls.
DELIBERATE_OVERRIDE_TRANSLUCENT_I8 = {
    "oot": {
        # z_eff_ss_bubble.c (POLY_OPA_DISP) and z_eff_ss_sibuki.c
        # (POLY_OPA_DISP) draw these opaque -- underwater air bubbles in
        # Zora's Domain/Lake Hylia/Gerudo Oasis and the Morpha boss fight.
        # z_eff_ss_dt_bubble.c (POLY_XLU_DISP) draws them translucent.
        # User's call: treat as translucent, the shape most texture pack
        # makers expect from a "bubble" texture.
        "objects/gameplay_keep/gEffBubble1Tex",
        "objects/gameplay_keep/gEffBubble2Tex",
    },
}


if __name__ == "__main__":
    game = sys.argv[1]
    decomp_root = sys.argv[2]
    archive = sys.argv[3]
    out_dir = sys.argv[4]

    z = zipfile.ZipFile(archive)
    archive_names = set(z.namelist())

    symbols, home_folder = collect_i8_textures(f"{decomp_root}/assets/xml", archive_names)
    print(f"{len(symbols)} I8 textures found in XML and present in archive "
          f"({len(home_folder)} disambiguated, name reused by another overlay)")

    array_vars = find_array_variables(decomp_root, symbols)
    functions = build_function_index(decomp_root)

    votes = direct_votes(decomp_root, symbols, home_folder)
    call_votes = call_argument_votes(decomp_root, symbols, array_vars, functions)
    for sym, qs in call_votes.items():
        votes[sym] |= qs

    scoped_arrays = find_scoped_array_variables(decomp_root, symbols)
    for sym, qs in array_name_votes(scoped_arrays).items():
        votes[sym] |= qs

    for sym, qs in inc_c_render_mode_votes(decomp_root, symbols, home_folder).items():
        votes[sym] |= qs

    classified, ambiguous = classify_from_votes(votes)
    direct_translucent = sum(1 for m in classified.values() if m == "POLY_XLU_DISP")
    print(f"direct+call-argument+array-name+inc.c: {direct_translucent} translucent, "
          f"{len(classified) - direct_translucent} opaque, {len(ambiguous)} ambiguous/mixed")

    groups = find_sibling_groups(decomp_root, symbols, home_folder)
    propagate_sibling_groups(classified, ambiguous, groups)
    total_translucent = sum(1 for m in classified.values() if m == "POLY_XLU_DISP")
    print(f"after sibling-group propagation ({len(groups)} groups found): "
          f"{total_translucent} translucent, {len(classified) - total_translucent} opaque, "
          f"{len(ambiguous)} ambiguous/mixed, "
          f"{len(symbols) - len(classified) - len(ambiguous)} still not found")

    paths = {symbols[sym] for sym, mode in classified.items() if mode == "POLY_XLU_DISP"}
    manual = MANUAL_TRANSLUCENT_I8.get(game, set())
    override = DELIBERATE_OVERRIDE_TRANSLUCENT_I8.get(game, set())
    print(f"plus {len(manual)} hand-verified additions, {len(override)} deliberate overrides")
    paths |= manual | override
    emit(paths, game, out_dir)
