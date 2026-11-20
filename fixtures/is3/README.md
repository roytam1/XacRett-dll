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

`multipart/` is a real five-disk InstallShield 3 set (`DATA.1` ..
`DATA.5`, ~3.8 MB total) from the `PCCLIN/` installation package in the
workarea root. Its 51 members include files wholly in one part and four
files split across consecutive parts (`CPCSCAN.DAT` 1..2, `PCC95RES.DLL`
2..3, `MFC40.DLL` 3..4, `WEBSTER.OCX` 4..5). Opening **any** of the five
parts lists all 51 members and extracts them byte-identically to the
oracle (`idecomp.py ... -a`); this pins the multipart logic: the
authoritative file list lives in part 1, split members concatenate a
slice from each part (bounded by that part's `dirs_list_offset` or
`start_integral_data`), and the DCL stream runs continuously across the
slices.

`expected/` holds the five source files that `mixed-*.Z` were made from;
`expected-small/` holds the three source files for `small.Z`.  A verified
extraction matches those files byte for byte (the test harness flattens
the paths with `-j`, so compare by base name).
