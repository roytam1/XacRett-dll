# XacRett.dll — Common Archivers Library (0.49.17 reconstruction)

A from-source reconstruction of **XacRett.dll 0.49.17** (2002–2006), a
Windows Common Archivers Library: one DLL that lists and extracts dozens
of archive and compression formats through a single uniform API.
Rebuilt from the incomplete `src-0.49.11` tree plus the IDA disassembly
(`XacRett.lst`) and Hex-Rays output (`XacRett.c`) of the official
binary, with binary-only dependencies eliminated from the link.

## Common Archivers Library: the concept

Instead of shipping one tool per format, a Common Archivers Library puts
every supported format behind the same handful of entry points
([spec](https://www.madobe.net/archiver/lib/main-e.html)). A host
program (filer, viewer, installer) links one DLL and gets uniform
answers to three questions, no matter the format:

1. **What is this file?** — `XacRettGetArchiveType` / `XacRettCheckArchive`
   identify the format by extension first, then by content sniffing.
2. **What is inside?** — `XacRettOpenArchive` + `XacRettFindFirst` /
   `XacRettFindNext` enumerate members into a fixed `INDIVIDUALINFO`
   record (name, sizes, CRC, date/time, attributes, compression method).
3. **Get the bytes out** — `XacRett` runs a command line naming the
   archive, the output directory, and switches (`-n1` hides progress,
   `-o1` overwrites silently), or the host drives extraction itself
   from the listing.

This is the same API family as the UNLHA32/UNZIP32/UNRAR32 archiver
DLLs, so existing hosts work unchanged. Format quirks (solid archives,
multi-volume sets, SFX stubs, OEM filenames, password prompts,
overwrite confirmations) are handled inside the library.

## Common Archivers Library APIs (11 exports)

Command-style extraction:

- `int XacRett(HWND hWnd, LPCSTR szCmdLine, LPSTR szOutput, DWORD dwSize)`
  — runs a command line (`x/e/l/v/t` style verbs with `-n`, `-o`, `-j`
  switches; `-o1` overwrites without prompting, `-o2` skips existing,
  `-n1` hides the progress dialog). Returns `0` on success or an
  `ERROR_*` code (`0x8000` range, see `XacRett.h`).

Identification:

- `int XacRettGetArchiveType(LPCSTR path)` — returns an `ARCHIVETYPE_*`
  number (`XacRett.h`, currently `0`–`64`), or `0` (`UNKNOWN`).
- `BOOL XacRettCheckArchive(LPCSTR path, int mode)` — nonzero if the
  file is a supported archive.
- `BOOL XacRettQueryFunctionList(int func)` — reports which `ISARC_*`
  service numbers are implemented.

Handle-style listing:

- `HARC XacRettOpenArchive(HWND, LPCSTR path, DWORD mode)` — opens an
  archive, `NULL` on failure.
- `int XacRettFindFirst(HARC, LPCSTR wildcards, INDIVIDUALINFO*)` and
  `int XacRettFindNext(HARC, INDIVIDUALINFO*)` — `0` per member,
  nonzero at end. The info struct is zeroed by the callee each call;
  `szMode[8]` carries a readable compression-method name (`lh5`,
  `deflate`, `stored`, `m3`, `crunch`, `MSZIP`, `ultra`, …).
- `int XacRettCloseArchive(HARC)`.

Housekeeping:

- `WORD XacRettGetVersion()` / `XacRettGetSubVersion()` — `49` / `17`.
- `BOOL XacRettGetRunning()` — nonzero while an operation runs.

## Supported formats (65 types)

Multi-file archives: ACE (store/LZ77/blocked+filters), ARC (m1–m9),
ARJ, AR, RAR, CAB (MSZIP/Quantum/LZX), LZH/LZS/PMA (all methods),
ZIP (store…deflate), ZOO, GCA, SQX, YZ1, AIN (M1–M4), LBR (stored/
squeezed/crunched), TAR + Tbz/Tgz/TaZ/Taz, CPIO (+gz/Z/bz2), ISO, DMG,
ALZ, JAK, IS (InstallShield), IMP, ISH, REZ, ZAC, ASD, QPAK, CPT,
DZ, BGA, ARG, BEL, JAM, YNC, ASK, LZX, and more —
see `ARCHIVETYPE_*` in `src-0.49.17/XacRett.h` for the full list.

Single-stream codecs: GZ (deflate), BZ2, Z/LZW (compress), JCALG1,
aPLib, SZDD (MSCompress), PACKZ, Freeze, Jam, BLZ, SPL (splitter),
plus text encodings (BinHex, Base64, uuencode, yEnc, Boo, AtoB) and
plain containers (MacBinary, shar, Fish, Hqx, Mime).

Notable behaviors: official 0.49.17 cannot extract ARC methods 4–9
(analyzed in `AGENTS.md`; this tree deliberately fixes it);
encrypted ACE/RAR members are refused; multi-volume sets gate on
volume 0; unknown methods yield an empty file rather than an error.

## Building

Visual C++ 6.0 only, via `src-0.49.17\XacRett.dsp` (compile every
`SOURCE=` file with `/MT /W3 /GX /O2` plus `NO_GZCOMPRESS`, then link
per `XacRett.def`). Adding a format means adding its `SOURCE=` blocks
to the `.dsp`. Result: `XacRett.dll`, i386, `FileVersion 0.49.17`,
11/11 exports, linked against system libraries plus in-tree `UnRAR`/
`ZData`/`fdi`/`jcalg1` sources only.

## Verification status

- 11/11 exports identical to the official binary; `FileVersion 0.49.17`.
- 18/18 synthesized ARC fixtures extract byte-exact (methods 1–9).
- LZH/ARC/CAB/SQX/ZIP listings and extractions verified against
  reference corpora (lhasa, hyc/arc, vendor SDK samples).
- 659/659 MSVC LIB members, 3/3 + 8/8 CP/M LBR members (stored,
  squeezed, v2-crunch), 9/9 AIN corpus members, 9/9 authentic WinACE
  archives — all byte-exact.
- See `AGENTS.md` for build/encoding constraints, per-format findings,
  reference handling (clean-room ports, reference-use-only sources),
  and the pre-commit checklist.

## References and provenance

- `XacRett.lst` (disassembly), `XacRett.c` (decompiler output),
  official `XacRett.dll` 0.49.17 — comparison targets, not sources.
- Vendored decode sources live in-tree (`unrar-3.6.1`, `yz32lib`,
  `ZData-recon`, `lhasa`, `unsqx`, `fdi`, `jcalg1lib`, `ZData`);
  clean-room ports (`LbrTool`, `AinCodec`/`AinTool`, `AceBlock`,
  `complib` glue) were written from reference documents and validated
  against third-party corpora, never copied.
