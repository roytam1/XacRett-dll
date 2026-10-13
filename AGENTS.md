# AGENTS.md — XacRett.dll 0.49.17 reconstruction notes

Session memory for coding agents. Read this first; it records constraints
and verified findings that cost real debugging time. Do not re-derive them.

## What this is

Reconstructing `XacRett.dll` 0.49.17 from the incomplete `src-0.49.11/`
tree plus `XacRett.lst` (IDA disassembly, 2527 procs) plus `XacRett.c`
(Hex-Rays, 124630 lines). Goals, in order: version/exports/behavior match
against the official 0.49.17 binary, reproducible `src-0.49.17/` tree,
no binary-only dependencies (AMD64 port preparation).

## Environment

- Windows, Visual C++ 6.0 at
  `C:\Program Files (x86)\Microsoft Visual Studio\VC98`.
  Use `VCVARS32.BAT` via `cmd /c` with a `.bat` file. The `bash` tool host
  is PowerShell 5.1: no `call`, no `&&`, no `rg`/`python3` aliases.
  Python 3.13 lives at `C:\Program Files\Python313\python.exe`
  (invoke by absolute path).
- Workarea root (also the git repo): `\\Roy-xp\c\devel\XacRett-dll`.
- Git: pass `-c user.name=roytam1 -c user.email=roytam@gmail.com` on every
  command. Never touch git config. Never push unless asked. Commit per
  logical part with short messages (see `git log --oneline` for style).

## Build

- Generators (in TEMP, not the repo):
  `C:\Users\Roy\AppData\Local\Temp\opencode\genbuild.ps1` writes
  `%TEMP%\dlbuild\build.bat`; `genlink.ps1` writes `link.bat`.
- Compile per file (cmd 8191-char limit forbids batching). Flags:
  `/nologo /MT /W3 /GX /O2 /c /D WIN32 /D NDEBUG /D _WINDOWS /D _MBCS
  /D _USRDLL /D XACRETT_EXPORTS /D NO_GZCOMPRESS /I"."`,
  plus `/I"lhasa"`, plus `/D __SQX_OS_WIN32__` for `unsqx/`,
  `/Fd"%TEMP%\dlbuild\vc6.pdb"`.
- **Obj naming is `<dir>_<file>.obj`** (e.g. `OpenArchive_OpenCab.obj`).
  `link.bat` references these exact names. Ad-hoc rebuild scripts MUST use
  the same `/Fo` names or the link silently consumes stale objects
  (this has bitten before — always verify with a behavior probe, never
  by DLL size/date alone).
- Current output `%TEMP%\dlbuild\XacRett.dll`: 671744 bytes, i386,
  `FileVersion 0.49.17`, 11/11 exports identical to the 724992-byte
  target. Imports are only KERNEL32/USER32/LZ32/SHLWAPI/ADVAPI32
  (target additionally uses IMAGEHLP/WINMM — known gap).
- Test harness `%TEMP%\lha\dlltest.exe` (source
  `opencode\dlltest.c`, build via `opencode\mkdlltest.bat`):
  `dlltest <dll> <archive> <outdir> <member> <expcrc> <exptype>`,
  `dlltest list <dll> <archive>` (also prints `attr=`/`mode=`).
  ALWAYS pass absolute archive paths (MeltArchive chdirs to the outdir
  before opening the archive, so relative paths fail with 0x801A).
  ALWAYS pass `-o1` in the harness command (it is baked into dlltest.c):
  `-o0` raises a modal Yes/No overwrite prompt per existing file.

## SHIFT-JIS — READ THIS FIRST

- Every repo source file is Shift-JIS, not UTF-8.
- The Read/Edit/Write tools corrupt SJIS bytes on write. Proven twice
  (see commits `2f7e931` and the szMode repair + audit scripts).
- **NEVER use Edit/Write/shell-redirection on repo files.**
  Make ALL source edits with byte-level Python scripts: ASCII-only
  patterns against raw bytes, CRLF-aware, assert occurrence counts
  (and remember append-style patches are not idempotent — never run
  an apply script twice without `git checkout` first).
- Pure-ASCII files are safe to edit normally; verify with a byte scan.
- **Before EVERY commit, run a byte-diff audit**: every `+`/`-` line must
  be an intended ASCII change; any byte >= 0x80 in a changed line must be
  pre-existing and preserved. Templates in TEMP opencode/:
  `auditszmode.py`, `auditbytes.py`, `auditar.py`.
- `OpenJak.cpp`/`OpenRez.cpp` contain corrupted (non-SJIS) bytes from
  upstream. Leave them alone (explicit user decision).

## References and samples (mostly TEMP, not repo)

- `hyc/arc` mirror (`opencode\arc-ref`): authoritative for ARC methods
  1-9 (`arclzw.c` crunch/compress, `arcusq.c` squeeze, `arcunp.c`
  dispatch, `arcpack.c` encoder side).
- LZH samples: `opencode\lhasa\test\archives` (all methods incl. pm0-2).
  CAB: `opencode\cabtest`, `opencode\isbisect`. SQX: `opencode\sqxtest`.
  ZIP: `opencode\ismedia`. `LIBCMT.LIB` + `cpmshar.lbr` in workarea root.
- Vendored sources and their commits: `unrar-7.2.7`, `yz32lib`,
  `ZData-recon`, `src-0.49.17/lhasa`, `src-0.49.17/unsqx`,
  `src-0.49.17/fdi`, `src-0.49.17/jcalg1lib`, `src-0.49.17/ZData`.
- Bundled decompression libs are version-pinned deliberately: `zlib`
  1.2.13 + `libbzip2` 1.0.8, both with compression sources stripped
  (`NO_GZCOMPRESS`, `#ifdef XACRETT_BZ_COMPRESSION`; see
  `libbzip2/KI.README`, `zlib/KI.README`). The 1.0.8 bump fixes
  CVE-2019-12900 (`nSelectors` clamp in `decompress.c`, verified
  present). The 1.2.2->1.2.13 bump carries all upstream security fixes
  (incl. the inftrees/inflate hardening); the xsrc51 Deflate64 patch
  (`XACRETT_DEFLATE64`, `MAX_WBITS` 16, dual lbase/lext/dbase/dext in
  `inftrees.c`, op-mask hunks in `inflate.c`/`inffast.c`) was ported hunk
  by hunk, with `gzio.c` replaced by the 1.2.13 gz split
  (`gzlib.c`/`gzread.c`/`gzclose.c` + `gzguts.h`, read side only).
  Port traps: `#endif`-glued code lines never compile (broke LENEXT/
  DISTEXT slow path); `inflateReset2` grew a `windowBits < -15` floor
  (patch to `-MAX_WBITS` for `-16` raw Deflate64); the `#else` branches
  are dead code in our build (dispatch is always 32-based, tables switch
  at runtime). Verified: tar.gz 278 list + 171/171 extract MATCH, ZIP
  124/124, crafted method-9 ZIP PASS (= official).
- UnRAR 7.2.7 (RAR 1.5-5.x; deliberate divergence, official does 2.x/3.x
  only): stable `RAR*` API made it a source drop-in (`Rar3Tool.cpp` glue
  untouched); VC6 port via `vc6compat/` shims (`stdint.h`, SDK headers)
  + `patchunrar7*.py` (`wchar_t`, `constexpr`/NSDMI/`auto`/range-for/
  `nullptr`=`NULL`, `override` shim, enum-hack in-class consts,
  `vector::data()`->`&[0]`, `string::clear/back/pop_back` rewrites,
  `_snwprintf`+NUL, `USE_SSE` off, `nullptr` caveat: overload risk);
  `/MT` (not `/ML`) to avoid LIBC/LIBCMT clash; `motw.cpp`+`isnt.cpp`
  added (lib omits them, our link needs them); `TzSpecific...`/
  `CheckTokenMembership` via GetProcAddress (XP load safety);
  `SetSuspendState` stubbed (tool-only). RAR5 listing is a new
  `FindSub5` path in `COpenRar` (Ex API; dirs skipped, volumes refused,
  UTF-8 names pass through, >4GB sizes truncate to DWORD).
  Port traps: `#define wchar_t` rewrites system headers (use raros
  typedef stanza); `#endif`-glued lines; `for`-scope (rename inner or
  brace-wrap); `T var{}` -> `T var`/`memset` per POD-ness; `T var()` on
  POD leaves garbage (use memset); `explicit member-template<T>()`
  calls (use plain new/delete, RARDLL has large pages off anyway).
  Verified: RAR4 list+extract PASS (byte-identical vs official),
  RAR5 list+extract PASS (same bytes via unpack50), RAR 1.5/2.0/2.9
  list+extract PASS (same 48959-byte RAR.DOC, CRC 0753067E, identical
  across all three; official PASSes 2.0/2.9 identically but fails 1.5).
  Fixtures: `GPLv2-rar4/5.rar` + `rar15/20/29.rar` workarea root
  (all untracked, like `LIBCMT.LIB`).
  Retro-Windows load safety (95/NT4 floor = official's import table):
  post-95 static imports converted to GetProcAddress with fallbacks
  (`CreateHardLinkW`, `GetLongPathNameW`, `SetThreadExecutionState`,
  `AllocateAndInitializeSid`/`FreeSid`/`LookupPrivilegeValueW`,
  `GetProcessAffinityMask`, `FoldStringW`,
  `SystemTimeToTzSpecificLocalTime`, `PathSetDlgItemPathA` (fallback
  `SetDlgItemTextA`; drops the whole SHLWAPI.dll dependency, which
  stock 95/NT4 lack entirely); `SetSuspendState` stubbed).
  Verify with `dumpbin /imports` diff vs official after any unrar change.
- bzip2 fixtures: upstream `sample1/2/3.bz2` + `.ref` (in TEMP
  `bz2test/`), plus generated multi-block + ZIP-method-12 fixtures
  (`mkbz2.py`, `runbz2matrix.ps1` — 6/6 MATCH).
- ARC fixtures: `opencode\mkarc1/2/3.ps1` generate `%TEMP%\arctest`
  (18 fixtures + manifest + `plain/` originals). `runarcwatch.ps1` runs
  the matrix with a 60s watchdog per case (modal dialogs block headless
  runs — a hang usually means a dialog, not a loop).

## Verified findings (do not re-derive)

- **Official 0.49.17 cannot extract ARC methods 4-9** (always yields
  nothing; its crunch decoder returns immediately for any non-empty file
  — inverted `if (lzsize--) return`, see `XacRett.lst:1000BA80`). Our tree
  deliberately FIXES this (diverges from the official binary, matches the
  reference sources). m1-m3 are official-verified; m4-m7 reference-
  verified; m8 streams carry the RLE (NCR) layer like m6/m7; block-mode
  width bumps use the pre-add free count; PowerShell `@{}` is
  case-insensitive (use `Dictionary[string,int]` for match tables).
- oldh/newh in the official binary are bit-identical to hyc/arc
  (strength-reduced by the compiler; verified in disassembly).
- M10 (unknown method): official and ours both keep a 0-byte file
  (CRC 0==0). This matches; do not "fix".
- `szMode`: official leaves it empty everywhere. We set readable names
  in all providers (user decision; `lh5`, `stored`/`deflate`, `m0`-`m5`,
  `crunch`, `MSZIP`, `GCA`/`SQX`/`YZ1`, ...). CAB reads per-folder
  `typeCompress` into `COpenCab::folderComp`.
- LBR has no provider (type 0 is correct). `CArgTool::Check` and
  `COpenArg::FindSub` require a non-blank entry name so CP/M LBR
  directory padding is rejected.
- MSVC LIB is ar with `/` linker members (skip), `//` NUL-terminated
  string table (capture, never list), `/offset` long names (resolve),
  trailing-`/` strip. 659/659 members list and extract byte-exact on
  `LIBCMT.LIB` (official extracts a single mangled file).
- CP/M LBR support (`ARCHIVETYPE_LBR` 63, `CLbrTool`/`COpenLbr`):
  128-byte sectors, 32-byte entries (status 0/0xFE/0xFF, 8.3 blank-padded
  names, LE index/length/CRC, LU86 date layout, pad count at [26]).
  Detection = directory control-entry fatal rule (active + blank name +
  index 0 + non-zero length fitting the file) + 128-byte-multiple size.
  Members are stored verbatim (CCITT CRC over full sectors + CRC
  high-first must be zero) or SQ-squeezed (`76 FF` + BE16 checksum +
  NUL name + Huffman tree/bits, raw output, no NCR layer — validated by
  decoding `SHAR.CQ` to 12115 bytes of C source in Python first).
  `cpmshar.lbr`: 3/3 members list and extract byte-exact. Crunched
  (`76 FE/FD`) members are v2.x adaptive LZW (9-12 bit MSB-first codes,
  EOF 256 / RESET 257 / filler 258-259 / dict from 260, RLE90 layer,
  table-full victim replacement — NOT the fixed-12-bit hash scheme of
  ARC m5): header is magic + NUL-terminated name + ref/sig/err/spare
  (sig >= 0x20 selects v2; the name runs to the first 00, then data;
  checksum only when errdetect != 0). `crunch28.lbr`: 8/8 members
  (6 crunched + 2 stored) list and extract byte-exact, verified against
  80un's `uncrunch` (`TEMP\ref80un\crunch.py`, GPL, reference use only —
  never vendor; our `LbrTool.cpp` port is clean-room). V1 crunch
  (sig < 0x20) is rejected, not decoded. CP/M dates convert to DOS dates (pre-1980 → 0);
  `uOSType` 15 matches LZH `C`.
- Adding a new provider requires: `XxxTool.h/.cpp` + `OpenArchive/OpenXxx.h
  /.cpp`, `SOURCE=` blocks in `XacRett.dsp` (pure ASCII, Edit-safe;
  `genbuild.ps1` picks them up automatically, `genlink.ps1` globs the
  obj dir), `ARCHIVETYPE_*` + `NUM_ARCHIVETYPE` in `XacRett.h`, includes
  + `DllSub` pair + `ArcNameToNum` branch in `XacRett.cpp`
  (byte-script only — SJIS file; routine names end with the SJIS suffix
  `93 57 8A 4A`, same as every other tool).
- DllTest command parsing: `XacRett()` needs `-n1 -o1 -j`; `type` numbers
  are `ARCHIVETYPE_*` in `XacRett.h` (AR=5, ARC=2).
- AIN support (`ARCHIVETYPE_AIN` 64, `CAinTool`/`COpenAin` + shared
  `AinCodec` Huffman session engine): 24-byte header (`21` magic, method
  `11`-`14`, `(sum[0..21] ^ 5555)` checksum — all verified), solid
  LZ77+Huffman data stream sliced by `orig_size`, M1-compressed index,
  whole-stream checksum on the last entry (strict: mismatch fails the
  extract). M1/M2/M3 share one decode path (they differ only on encode);
  M4 is verbatim. `szMode` is `ultra`/`normal`/`fast`/`store` (user
  decision). 9/9 fixtures byte-exact vs upstream corpus (HELLO/ENG
  x M1-M4 + CODE.AIN). Multi-volume/SFX/2.2-fragments deferred.
  Reference is seb3773/AIN (`TEMP\aintest\src_ain.c`, no explicit code
  license — reference use only, never vendor; our port is clean-room).
  Samples live in TEMP (`aintest\packed_corpus_*` + `corpus_*`), not the
  repo.
- Known gaps (untouched): JCALG1 is clean-room and unverified; SQX
  method-6 EX emits zero bytes; GCA non-`GCAX` paths are stubbed;
  `OpenDummy`/`OpenCompactC` have no live listing path.
- ACE unpacker (`CAceTool`, UNACE v1.2b lineage) handles STORE + LZ1
  (`TYPE 0/1`); ACE 2.0 blocked mode (`TYPE 2`) is decoded by the
  clean-room `AceBlock` engine (bitstream/Huffman/LZ77 core + EXE/DELTA/
  SOUND/PIC inverse filters, solid-dictionary support, per-member CRC
  verify with delete-on-mismatch). 9/9 authentic WinACE samples verify
  (`testdata/winace`: stored/m1 byte-exact incl. HELLO.TXT vs corpus,
  m2-plain cross-validated vs the m1 member with identical header CRC,
  EXE/DELTA/SOUND/PIC by header-CRC32 match). Encrypted members are
  refused up front (`ACE_PASSW`); multi-volume gates on vol 0.
  Reference is seb3773/ace-c (`TEMP\aceref`, BSD — reference use only,
  never vendor; our port is clean-room). LZ1↔blocked cross-registration
  for mixed-solid archives is skipped (no sample; stored→blocked is
  registered).

## Pre-commit checklist

1. Byte-diff audit clean (no SJIS churn).
2. Full `build.bat` with zero `FAIL` lines + relink.
3. Regression: `runarcwatch.ps1` 18/18 MATCH when ArcTool changed;
   `dlltest list` spot-checks when providers changed.
4. `git status` shows only intended files; commit with `-c` identity.
