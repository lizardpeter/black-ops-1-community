# BO1 multiplayer x64 audit

Baseline 3e38fe8, with the pinned KisakBlack source and the complete enhanced patch stack. The verified retail Nuketown launch is Win32. The full x64 Windows build has yet to link and boot.

## Reproduce the inventory

    python3 scripts/audit_bo1_x64.py --summary
    python3 scripts/audit_bo1_x64.py --assets
    python3 scripts/audit_bo1_x64.py > x64-candidates.tsv

The scanner stages the patches in build order and reports locations in the resulting source. It scans the upstream CMake manifest and enhanced target sources, with the default VP8 exclusion. It ignores comments and literal disabled branches. It does not evaluate every preprocessor condition, follow unlisted transitive includes, or infer the type of a cast operand. Some matches represent correct fixed-width data or architecture-specific fallback code. Compiler and retail execution can expose additional issues.

| Pattern | Locations |
| --- | ---: |
| Fixed-size or offset assertions | 85 |
| Legacy fastfile reads with fixed record sizes | 359 |
| Legacy fastfile reference and alias fixups | 128 |
| Integer casts that may hold native addresses | 220 |
| Raw indexed memory access | 371 |
| Constant-size allocation or memory operations | 451 |
| Pointer-like interlocked operations | 1 |
| x86 instruction or architecture references | 15 |
| **Total** | **1,630 across 268 files** |

The database accounts for 541 matches, physics for 212, D3D9 for 172, and shared memory and utility code for 86. Multiple matches can describe the same underlying issue.

## Fastfile dispatch

The top-level enum has 43 entries. Four handlers have x64 conversions awaiting retail verification. Another 29 entries still dispatch to legacy loaders. Ten have no top-level dispatch case and need to be checked against the retail asset inventory and nested loader usage.

| Group | Legacy top-level entries |
| --- | --- |
| Physics and models | PHYSPRESET, PHYSCONSTRAINTS, DESTRUCTIBLEDEF, XANIMPARTS, XMODEL |
| Materials and rendering | MATERIAL, GFXWORLD, LIGHT_DEF, FX, IMPACT_FX, GLASSES |
| Audio | SOUND, SOUND_PATCH |
| Maps and game worlds | CLIPMAP, CLIPMAP_PVS, COMWORLD, GAMEWORLD_SP, GAMEWORLD_MP, MAP_ENTS |
| UI and localization | FONT, MENULIST, MENU, LOCALIZE_ENTRY, STRINGTABLE, EMBLEMSET |
| Game definitions | WEAPON, PACK_INDEX, XGLOBALS, DDL |

The four ported handlers are TECHNIQUE_SET, IMAGE, SNDDRIVER_GLOBALS, and RAWFILE. Nested callers can still have Win32 assumptions. The asset inventory prints every ID and handler, including the ten without dispatch cases.

## Conversion batches

| Area | Change | Verification |
| --- | --- | --- |
| Fastfile records and references | Generate readers from verified disk schemas where layouts are regular. Preserve wire sizes, alignment, offsets, aliases, delayed reads, registration, and zone lifetimes. Store native pointer-bearing records separately. | Compare Win32 and x64 asset counts, stream consumption, alias identity, and unload/reload across startup zones and Nuketown. |
| Native memory and object layout | Batch verified pointer casts, pool indexes, allocation sizes, pointer atomics, and decompiler offsets across physics, renderer commands, world geometry, animation, UI, and effects. | Test addresses above 4 GB, allocator lifetime, resource creation, and repeated map changes. |
| Network, demos, and persistence | Preserve the byte and bit layouts used by fixed-offset field tables. Audit player, entity, and client snapshots, archive and demo records, script values, and saved data. | Compare serialized bytes with Win32 and exercise a local match, demos, and saved state. |
| Windows build and imports | Resolve compiler and linker failures, verify PE machine 0x8664, and inspect imports. The current x64 target uses D3D9. | Build the actual game and launch it with the retail asset set. Retain full compiler output and an in-game capture. |
| Renderer and audio modernization | Integrate native Vulkan and WASAPI into the game. Port material, shader, draw, and presentation paths. | Reuse the retail gameplay checks and verify imports, rendering, and audio. |

Repeated typed array indexes and verified disk schemas are suitable for bulk conversion. Integer casts need type and data-flow review because they can represent native addresses, handles, hashes, or serialized references.
