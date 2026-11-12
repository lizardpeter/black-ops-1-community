#!/usr/bin/env python3
"""Inventory x86 assumptions in the patched enhanced multiplayer source."""
import argparse
import collections
from contextlib import contextmanager
import csv
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

REPO = Path(__file__).resolve().parents[1]
UPSTREAM = REPO / "upstream/kisakblack"
PORT = REPO / "enhanced/port/x64/game"
TEXT = re.compile(
    r'/\*.*?\*/|//[^\n]*|"(?:[^"\\]|\\.)*"|\'(?:[^\'\\]|\\.)*\'',
    re.DOTALL,
)
PATTERNS = {
    "fixed-layout-assert": r"\bstatic_assert\s*\(\s*(?:sizeof|offsetof)\s*\(",
    "legacy-fastfile-fixup": r"\bDB_(?:ConvertOffsetTo(?:Pointer|Alias)|InsertPointer)\s*\(",
    "legacy-fastfile-read": r"\bLoad_Stream\s*\([^;]*?,\s*(?:\d+\s*(?:\*\s*\w+)?|sizeof\s*\(\s*(?:Material|Gfx|XAsset)\w*\s*\))\s*\)",
    "integer-address-candidate": r"\(\s*(?:unsigned\s+(?:int|long|__int32)|int|DWORD|_DWORD|signed\s+__int32)\s*\)\s*(?:&\s*\w+|\*\s*(?:var\w+Ptr|\w+\.data)|\w*(?:[Pp]tr|[Pp]ointer|[Aa]ddress|[Cc]ursor|[Bb]uffer)\b|(?:this|NtCurrentTeb\(\)))",
    "raw-index-candidate": r"\*\s*\(\s*\(\s*(?:(?:unsigned\s+)?int|_DWORD|DWORD|float)\s*\*\s*\)\s*\w+\s*[+-]\s*(?:0x[0-9a-fA-F]+|\d+)\b",
    "atomic-address-candidate": r"\b_?Interlocked\w*\s*\(\s*\(\s*volatile\s+(?:unsigned\s+)?(?:long|int|__int32)\s*\*\s*\)\s*&[^,;]*(?:[Pp]tr|[Pp]ointer|[Cc]ursor|m_cur|[Bb]uffer)",
    "x86-instruction-candidate": r"\b(?:__asm|_asm|__readfsdword|__writefsdword|_M_IX86)\b",
    "native-memory-size-candidate": r"\b(?:PMM_(?:ALLOC|FREE|VALIDATE|PERM_ALLOCATE)|memcpy|memset|Com_Memcpy)\s*\([^;]*?,\s*(?:0x[\da-fA-F]+|\d{2,})(?:[uU])?\s*[,)]",
}
PATTERNS = {name: re.compile(pattern) for name, pattern in PATTERNS.items()}


def clean(source):
    def blank(match):
        return re.sub(r"[^\n]", " ", match[0])

    # Preserve line numbering and ignore literal comments and literal #if 0 arms.
    stack = []
    result = []
    for line in TEXT.sub(blank, source).splitlines(keepends=True):
        directive = re.match(r"^\s*#\s*(if|ifdef|ifndef|else|elif|endif)\b(.*)", line)
        if directive:
            operation, expression = directive.groups()
            if operation in {"if", "ifdef", "ifndef"}:
                stack.append(operation == "if" and expression.strip() == "0")
            elif stack and operation in {"else", "elif"}:
                stack[-1] = operation == "elif" and expression.strip() == "0"
            elif stack:
                stack.pop()
        result.append(re.sub(r"[^\n]", " ", line) if any(stack) else line)
    return "".join(result)


def source_paths():
    manifest = (UPSTREAM / "cmake_files.cmake").read_text()
    prefixes = {"SRC_DIR": "src", "TL_DIR": "tl", "DW_DIR": "DemonWare"}
    paths = {
        f"{prefixes[base]}/{file}"
        for base, file in re.findall(r'\$\{(SRC_DIR|TL_DIR|DW_DIR)\}/([^"\n]+\.(?:cpp|c|h|hpp))"', manifest)
    }
    cmake = (REPO / "enhanced/CMakeLists.txt").read_text()
    excluded = cmake.split("set(replaced_units", 1)[1].split(")", 1)[0].split()
    paths.difference_update([*excluded, "src/vpx/vpx.cpp"])
    return paths


@contextmanager
def staged_sources():
    paths = source_paths()
    patches = [
        PORT / name
        for name in re.findall(r'\$\{CMAKE_CURRENT_SOURCE_DIR\}/([^"\n]+\.patch)', (PORT / "CMakeLists.txt").read_text())
    ]
    patches.append(REPO / "enhanced/patches/kisakblack.patch")
    patch_paths = {
        name.decode()
        for patch in patches
        for name in re.findall(rb"^--- a/(.+)$", patch.read_bytes(), re.MULTILINE)
    }
    with tempfile.TemporaryDirectory(prefix="bo1-x64-audit-") as directory:
        root = Path(directory)
        for name in sorted(paths | patch_paths):
            target = root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes((UPSTREAM / name).read_bytes())
        env = {**os.environ, "GIT_CEILING_DIRECTORIES": str(root.parent)}
        for patch in patches:
            subprocess.run(["git", "apply", "--ignore-space-change", str(patch)], cwd=root, env=env, check=True)
        inputs = [(name, root / name) for name in sorted(paths)]
        cmake = (REPO / "enhanced/CMakeLists.txt").read_text()
        block = cmake.rsplit("target_sources(KisakBlack PRIVATE", 1)[1].split(")", 1)[0]
        inputs.extend((f"enhanced/{name}", REPO / "enhanced" / name) for name in block.split())
        yield root, inputs


def inventory(inputs):
    for name, path in inputs:
        original = path.read_bytes().decode("latin-1")
        active = clean(original)
        lines = original.splitlines()
        for kind, pattern in PATTERNS.items():
            for match in pattern.finditer(active):
                line = active.count("\n", 0, match.start()) + 1
                yield name, line, kind, lines[line - 1].strip()


def asset_dispatch(root):
    types = (root / "src/database/database.h").read_text()
    names = dict((int(value, 16), name) for name, value in re.findall(
        r"ASSET_TYPE_(\w+) = (0x[0-9A-F]+)", types.split("ASSET_TYPE_COUNT", 1)[0]))
    loader = (root / "src/database/db_load.cpp").read_text()
    dispatch = loader.split("void __cdecl Load_XAssetHeader(", 1)[1].split("void __cdecl Load_XAsset(", 1)[0]
    handlers = {}
    for cases, handler in re.findall(r"((?:\s*case \d+:)+)[^;]+;\s*(Load_\w+)\(atStreamStart\);", dispatch):
        for value in re.findall(r"case (\d+):", cases):
            handlers[int(value)] = handler
    native = {
        "Load_GfxImagePtr",
        "Load_MaterialHandle",
        "Load_GfxLightDefPtr",
        "Load_PhysPresetPtr",
        "Load_MapEntsPtr",
        "Load_XGlobalsPtr",
        "Load_LocalizeEntryPtr",
        "Load_PackIndexPtr",
        "Load_StringTablePtr",
        "Load_MaterialTechniqueSetPtr",
        "Load_RawFilePtr",
        "Load_SndDriverGlobalsPtr",
    }
    for value, name in sorted(names.items()):
        handler = handlers.get(value, "")
        status = "ported, retail unverified" if handler in native else (
            "legacy dispatch" if handler else "no dispatch case")
        yield value, name, handler, status


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    output = parser.add_mutually_exclusive_group()
    output.add_argument("--assets", action="store_true", help="list top-level fastfile asset dispatch")
    output.add_argument("--summary", action="store_true", help="print counts by subsystem and pattern")
    args = parser.parse_args()
    writer = csv.writer(sys.stdout, delimiter="\t", lineterminator="\n")
    with staged_sources() as (root, inputs):
        if args.assets:
            writer.writerow(["id", "asset", "loader", "status"])
            writer.writerows(asset_dispatch(root))
            return
        rows = sorted(set(inventory(inputs)))
    if args.summary:
        totals = collections.Counter(kind for _, _, kind, _ in rows)
        groups = collections.Counter((path.split("/")[1] if path.startswith("src/") else path.split("/")[0], kind)
                                     for path, _, kind, _ in rows)
        kinds = sorted(PATTERNS)
        writer.writerow(["subsystem", *kinds, "total"])
        for group in sorted({group for group, _ in groups}):
            counts = [groups[group, kind] for kind in kinds]
            writer.writerow([group, *counts, sum(counts)])
        writer.writerow(["total", *(totals[kind] for kind in kinds), len(rows)])
    else:
        writer.writerow(["staged_path", "line", "pattern", "source"])
        writer.writerows(rows)


if __name__ == "__main__":
    main()
