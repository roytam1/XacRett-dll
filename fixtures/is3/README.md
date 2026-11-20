# InstallShield 3 (.Z) fixtures

These archives were produced with the original **ICOMP.EXE 3.00.062**
(InstallShield File Compressor for Windows 95), from `icomp95.zip` as
mirrored at `sac.sk` / the Internet Archive.  The reference unpacker is
the GPLv3 `idecomp.py` / `pwexplode.py` in the workarea-root `i3comp/`
project (study only -- not vendored).

| archive | made with | what it proves |
|---|---|---|
| `mixed-sh.Z` | `ICOMP ... -sh` (highest compression) | five members over three directories, all DCL implode, byte-exact vs `idecomp.py` |
| `mixed-sm.Z` | `ICOMP ... -sm` | the same tree at medium compression |
| `mixed-sl.Z` | `ICOMP ... -sl` | the same tree at lowest compression |
| `mixed-sn.Z` | `ICOMP ... -sn` | the same tree **stored** (file flag 0x10); empty member has comp==orig==0 |
| `small.Z` | `ICOMP ... -sh` | a simple three-member archive (no subdirectories) |

`expected/` holds the five source files that `mixed-*.Z` were made from;
`expected-small/` holds the three source files for `small.Z`.  A verified
extraction matches those files byte for byte (the test harness flattens
the paths with `-j`, so compare by base name).
