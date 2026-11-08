# Surviving an Edge update, feature by feature

Goal: when Edge updates, only the features whose Edge code changed stop, and
they stay off until `offsets.h` is ported by hand; every other feature keeps
working. Nothing is turned away on its version number alone.

## How a build is judged

```
panel                               DLL (inside Edge)
-----                               -----------------
inject into any browser   ------->  running build == pinned?  yes -> offsets.h as it is ("pinned")
except a build the DLL                    | no
refused before                            v
                            search msedge.dll as the file on disk has it, once (~1.3 s)
                            for every offsets.h entry's signatures (signatures.g.h)
                                          |
             same major.minor.build (a patch)        another line
                     |                                    |
         believe what the signatures             believe an entry only when its
         place (measured safe)                   BODY is unchanged (below)
                     \                                  /
            a feature with any entry not placed stands down alone
            (availability.h: no hooks, never applied, not even as "off")
                                          |
                        edgeplus-status.bin: mode, build, available features
                                          |
panel pauses exactly the   <--------------+
features the DLL could not run
```

| piece | where |
|---|---|
| the table, one row per `rva` entry, with its features and body | `app/dll/src/edge/signatures.g.h`, made by `tools/re/New-SignatureTable.ps1` |
| row types, feature bits | `app/dll/src/edge/signature_table.h` |
| pattern scan; verdict on the hits | `signature.h`, `resolution.h` (tested) |
| the body check | `body.h` (tested) |
| scan the image, fill the translation and the availability | `resolve.h`, `translation.h`, `availability.h` |
| `edge::At` / `edge::Callable` land where the build has the entry | `edge.h` |
| what the DLL tells the panel | `core/status.h` -> `dll_status.rs`, `edge_support.rs` |

## The body: why "found" is not enough across lines

A signature finds where a function is. It cannot tell whether that function
still reads the same struct fields, vtable slots and ids: across lines those
move (of the 210 offsets 155.0.4283.62 and 156.0.4314.50 share, nine moved;
between 156.0.4314.42 and .50, none of 233).

So each row carries its **body**, fingerprinted when the table is signed:
the whole function the entry stands on (the function itself, the one a patch
site or return address sits in, or the one that refers to a datum), hashed
with only the bytes a rebuild moves masked (rel32 branch targets and
RIP-relative displacements). On another line an entry is used only if that
whole function hashes the same and, when the exception table lists it, has
the same size. A field offset, slot or id inside it that moved changes the
hash, and every feature that uses the entry stands down.

## Which features an entry belongs to

Every entry carries the features that use it, from three sources the
generator adds up (it never takes one away):

- the `offsets.h` banner it sits under (`EdgePlus.Signatures.psm1`);
- usage: each feature's own source files (the same module lists them) and
  every entry they reach, directly or through `edge.h`'s helpers;
- `hooks.h`: every `Install(Feature::kX, ...)` names the feature its hook
  serves, and what that hook's detour reaches is that feature's too. The DLL
  skips the hooks of a feature that stood down.

Shared code that still reaches an entry of a stood-down feature calls a stub
(`edge::Callable`) that logs once and returns zero, never a null pointer.

## Edge 157.0.4341.42 (no public PDB yet), measured in a throwaway Edge

116 of 295 entries unchanged, 146 found but changed (WebContentsImpl's
accessors among them, so its layout moved), 31 not found, 2 ambiguous. Every
feature stands down: correct for a line whose layouts moved. The fewest
blockers, the first to port: live preview 2, link handlers 3, WebUI follows
theme 4, reading theme 5, theming 6, the Cyberpunk skin 29, find 68, split 93.
The DLL's log lists every one with its features and why.

## The manual fix

Port `offsets.h` to the new build (`tools/re/README.md`: `Port-Offsets.ps1`
with both PDBs, or by hand with `Show-Disassembly.ps1` where there is no PDB
yet), verify live, then re-sign the table (`New-SignatureTable.ps1`). The new
pinned build is trusted as it is; the old one becomes "another build".

## What it does not catch

- Struct offsets the DLL reads that no function it stands on contains: the
  body check vouches only for the code it hashes.
- A function whose body is unchanged but whose callers now pass something
  else.
- A function's cold part (a separate exception-table entry) that changed.
- The pinned patch line is trusted on signatures alone, as measured.
