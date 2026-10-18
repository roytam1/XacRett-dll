# AGENTS.md — XacRett.dll 0.49.17 reconstruction notes

Session memory for coding agents. Read this first; it records constraints
and verified findings that cost real debugging time. Do not re-derive them.

## What this is

Reconstructing `XacRett.dll` 0.49.17 from the incomplete `src-0.49.11/`
tree plus `XacRett.lst` (IDA disassembly, 2527 procs) plus `XacRett.c`
(Hex-Rays, 124630 lines). Goals, in order: version/exports/behavior match
against the official 0.49.17 binary, reproducible `src-0.49.17/` tree,
no binary-only dependencies (AMD64 port preparation).

**STATUS: the original goal is COMPLETE** (recorded 2026-10-18).
"Re-create the missing parts so the incomplete source builds" is done:
all 220 `XacRett.dsp` units plus the 49 vendored UnRAR units build
from a clean tree with no binary-only dependencies, under BOTH VC6 and
MSVC 4.x, and the result matches the official 0.49.17 binary on
version (`FileVersion 0.49.17`) and on all 11 exports.  Behavior has
been verified member-for-member against official across RAR, ZIP,
SQX, 7z, ACE, LBR, YZ1 and the tar family.  Do not re-open the
reconstruction; work here is now ADDITIVE (new formats), and each
new provider is verified against an authentic fixture the same way.

Two policies came out of that work and still decide every judgement
call, so keep applying them:

- **Deliberate divergence where official is broken.**  We FIX official
  and say so in the per-format notes, rather than copying a defect:
  ARC methods 4-9 (official yields nothing), YZ1 (official faults with
  0xC0000005, and a gauge-index crash of ours was fixed), the 8.3
  output-name fallback, `-p<password>` for headless runs, and the
  whole `.lz`/`.xz`/`.zst`/`.tar.*` provider family.  Match official
  only where official is actually correct.
- **Do not import a symbol just to match the import table.**  Official
  imports IMAGEHLP/WINMM for exactly one symbol each, both of which
  we have already replaced or never needed; adding them would only
  widen our dependency surface.  `GetProcAddress` conversion is
  warranted only for OUR OWN call sites to post-95 APIs (the
  `VC6_GSPN` / `VC6_AFAA` cases), never for CRT startup (see the
  `FreeEnvironmentStrings*` note below).

## Environment

- Windows, Visual C++ 6.0 at
  `C:\Program Files (x86)\Microsoft Visual Studio\VC98`.
  Use `VCVARS32.BAT` via `cmd /c` with a `.bat` file. The `bash` tool host
  is PowerShell 5.1: no `call`, no `&&`, no `rg`/`python3` aliases.
  Python 3.13 lives at `C:\Program Files\Python313\python.exe`
  (invoke by absolute path).
- MSVC 4.x (CL 10.10.6038) also installed, at
  `C:\MSDEV4\BIN\VCVARS32.BAT`. All `XacRett.dsp` units AND the
  49-unit vendored UnRAR lib are VC4 compile-clean, and the VC4 DLL
  links and extracts correctly (measured, not guessed - see
  "MSVC4 compatibility" below).
- Workarea root (also the git repo): `\\Roy-xp\c\devel\XacRett-dll`.
- Git: pass `-c user.name=roytam1 -c user.email=roytam@gmail.com` on every
  command. Never touch git config. Never push unless asked. Commit per
  logical part with short messages (see `git log --oneline` for style).

## Build

- Generators (in TEMP, not the repo):
  `C:\Users\Roy\AppData\Local\Temp\opencode\genbuild.ps1` writes
  `%TEMP%\dlbuild\build.bat`; `genlink.ps1` writes `link.bat`.
- **`genlink.ps1` must run AFTER `build.bat`, never straight after
  `genbuild.ps1`.**  `genbuild.ps1` begins with
  `Remove-Item -Recurse -Force $tmp`, so it WIPES
  `%TEMP%\dlbuild` including the `obj` directory.  Run `genlink`
  at that point and it globs an EMPTY obj dir and writes a
  `link.bat` with no objects at all (578 bytes).  The symptom is
  deeply misleading: the link then fails with
  `LNK1120: 11 unresolved externals` naming the 11 **exports**,
  which reads like a missing-exports problem, not a
  missing-object-list problem -- and it is the SAME symptom as a
  genuinely broken export table, so it invites the wrong
  investigation.  Correct order is `genbuild` -> `build.bat` ->
  `genlink` -> `link.bat`, and the cheap tell is the size of
  `link.bat`: ~15 KB (224 inputs) when it is right, 578 bytes
  when the object list is empty.  Always check that count.
- **Removing an unused object can change the DLL's bytes without
  changing its size.**  `/OPT:REF` discards unreferenced
  *functions* but KEEPS external-linkage *data*, and adding or
  removing an input object shifts the linker's layout and even
  `e_lfanew` (0x100 vs 0x108).  So a byte-diff of two builds is
  meaningless unless you first control for it: link the SAME
  object set twice and diff those (that is only 2 bytes, the PE
  timestamp) to establish the noise floor, and compare
  per-section sizes, not file size.  Verify a change by BEHAVIOUR
  (extract and compare) -- never by size or hash.
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
- Current output `%TEMP%\dlbuild\XacRett.dll`: 1015808 bytes, i386,
  `FileVersion 0.49.17`, 11/11 exports identical to the 724992-byte
  target (checked with `dumpbin /exports`).  Imports are only
  KERNEL32/USER32/LZ32/ADVAPI32 -- SHLWAPI was dropped in `8345737`.
  The target alone still imports SHLWAPI, IMAGEHLP and WINMM, but
  those two are **one symbol each** and are NOT a gap for us:
  IMAGEHLP is only `MakeSureDirectoryPathExists`, which `pathMake`
  already does itself with `CreateDirectory` (`kiutil.cpp:535,561`),
  and WINMM is only `timeGetTime`, confined to a single official
  function that brackets the command-line phase to record a
  centisecond (`/0xA`) duration for the gauge.  Neither symbol is
  in any of our 224 objects or in the vendored `UnRAR.lib`, so
  importing them would only widen our dependency surface for no
  gain.  The real floor is the `EnvironmentStringsW` pair below.
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
  124/124, crafted method-9 ZIP PASS (= official). ZIP legacy methods
  verified on authentic PKZIP fixtures: `PKZ080.ZIP` 10/10 CRC-OK
  (method 6 implode, incl. binaries), `pkz090.zip` 10/10 CRC-OK
  (method 1 shrink x5 + method 4 reduce x5). Reduce factors 1-3 and
  higher members untested; fixtures untracked in workarea root.
  ZIP fixes (vs Xtro-ZIP unzip60 comparison): small-file ZIP64 via
  0x0001 extra parse in both `read_header` copies (refuse >2GB-1 instead
  of hanging); bit-11 UTF-8 names convert via hand-rolled decoder +
  `CP_ACP` (`kiutil::utf8toAnsi`, no `CP_UTF8` dependency for retro
  Windows); `fopen` failure aborts the extract (except dirs/empty
  stripped names) instead of silently skipping with rc=0.
  Data descriptors via central-dir size resolve (matched by local
  offset + bit 3, so stale-offset SFX can't inject garbage).
  Listing now covers methods 0, 8, 9, 10, 12, 14, 18, 95, 98, 99;
  everything else is refused (7/11/13 are reserved; 93 needs a
  full zstd decoder, see below). Unsupported-method members are
  skipped-and-continued via the `doHeader` resync scan (both
  listing and extraction): verified (good,bad,good),
  (bad,good), descriptor-hole and 5 MB-hole sandwiches - members
  after a hole list and extract byte-exact, holes never appear in
  listings (matches official behavior), all-exotic archives fail
  `Check` honestly with no crash. No ZIP64 central-dir attrs;
  no spans.
- ZIP method 10 (DCL Implode) is implemented via vendored `pkdcl.c`/
  `pkdcl.h` (`src-0.49.17/pkdcl/`, PKDCLX 1.1.2, MIT-0, taken from
  the Xtro-Zip `unzip60` reference tree). `CZipTool::UnDcl` feeds
  `csz` bytes through `dclRead`/`dclWrite` wrappers over
  `zipread`/`zipwrite`; an empty member (exactly 4 bytes) uses
  `pkdcl_explode_ex` because the legacy entry point rejects a first
  fill of <=4 bytes. `szMode` is `dcl`. Three traps, all cost
  real debugging time: (1) `pkdcl_read_func` returns the BYTE
  COUNT, not a boolean - returning 1 silently truncates the
  stream; (2) PKWARE's original `blast.c` was vendored first and
  discarded - it keeps its Huffman tables in file-scope statics
  (`litcnt`/`litsym`/...), so it is unsafe for repeated or
  concurrent calls in a long-lived DLL, whereas pkdcl keeps all
  decoder state in a stack-local `struct dec_state`; (3) a
  hand-rolled Python DCL encoder produces streams that `blast`
  decodes but that are NOT spec-conformant - `pkdcl` rejects
  them, which proves the FIXTURE was wrong, not the decoder.
  Always build DCL fixtures with pkdcl's own `pkdcl_implode_ex`
  (`PKDCL_DICT_AUTO`). The ZIP payload is the raw DCL stream
  INCLUDING its 3-byte prologue (ctype, dict-size bits, initial
  bit buffer); there is no ZIP framing. Verified 3/3 PASS
  (`%TEMP%\zipcmp\fix\m10_dcl.zip`: litonly 31 B, matches 234 B,
  binary 3000 B, all CRC-exact) plus repeated-call decode in one
  process (4x, deterministic). `PKDCL_ENABLE_LEGACY_API` stays
  undefined so the legacy `crc32` symbol cannot collide with
  zlib's.
- ZIP method 14 (LZMA) is implemented via the vendored 7-Zip
  `LzmaDec.c`/`LzmaDec.h` decoder subset (`src-0.49.17/lzma/`,
  public domain) plus a minimal VC6 `Precomp.h` shim. Only
  `LzmaDec.c` is compiled in; `Alloc.c` is skipped in favour of
  a tiny malloc-backed `ISzAlloc` in `ZipTool.cpp`. `CZipTool::
  UnLzma` accepts the Xtro/PKZIPC framing (`ver0 ver1 props_size
  props stream`, GP bit 1 = EOS marker present) and tolerates a
  bare 5-byte-props prefix. Output is capped at `usz`, dicts are
  clamped to 4KB..64MB pre-allocation, and a no-progress guard
  keeps corrupt streams from spinning. `szMode` is `lzma`.
  Three gates must agree for a new method: `CZipTool::Check`'s
  header scan (`> 12` refuses 14 unless exempted), the `ZipTool::
  read_header` gate, and the `OpenZip::read_header` gate - the
  scan was missed on the first pass and the whole archive failed
  to open (`OpenArchive failed`). Verified 6/6 PASS
  (`%TEMP%\zipcmp\fix\m14_lzma.zip`: 31 B .. 300 KB, incl. a
  64 KB-dictionary wrap case, an 8 MB-dict case, and a no-prefix
  member, all CRC-exact). `Lzma2Dec.c/.h` are vendored but unused
  so far (reserved for XZ-95).
- ZIP method 99 (WinZip AES) is implemented via vendored `wzaes.c`/
  `izsha1.c` (`src-0.49.17/wzaes/`, wzaes public domain + SHA-1 MIT-0,
  from the Xtro-Zip `unzip60` reference tree). `CZipTool::aes_init`
  parses extra field `0x9901` (AE-1/AE-2 + `AE` vendor + strength
  1/2/3 + real method), reads salt + 2-byte verifier, derives via
  PBKDF2 and compares the verifier; `zipread` gains a CTR-decrypt
  hook so every inner decoder (0/8/12/14/95/98 verified, 1-6 by
  construction, 10 untested - no DCL+AES writer obtainable) works
  unchanged, and the trailing 10-byte auth tag
  is verified after decode (mismatch deletes the output and fails
  loudly). Unsupported inner methods are skipped-and-continued.
  `szMode` is `aes128`/`aes192`/`aes256`. Passwords reuse the modal
  dialog (headless runs hang - verify with a temp seed/stub, both
  reverted before commit). VC6 port traps, all real: `iz_arm_caps.h`
  was missing from the vendor set (copy from upstream, inert on
  x86); `izsha1_ni.h` is absent upstream (local stub, NI path never
  enabled); VC6's `windows.h` pulls an EMPTY `wincrypt.h` and
  poisons `__WINCRYPT_H__`, so `NOCRYPT` + explicit include with
  `_WIN32_WINNT=0x0400` default (`wzaes.c` only, never global
  flags); `CRYPT_SILENT` postdates VC6 headers (local define).
  CryptoAPI stays GetProcAddress-based: `dumpbin /imports` shows no
  new dependency (KERNEL32/USER32/LZ32/ADVAPI32 only). Verified 9/9
  PASS against authentic 7-Zip fixtures (128/192/256 x stored/
  deflate/bzip2/lzma inners, CRC-exact) plus verifier-reject,
  rewind-and-retry, and bad-auth-tag negative paths.
- ZIP method 98 (PPMd) is implemented via vendored `ppmd8.c`/
  `ppmd8dec.c` (`src-0.49.17/ppmd8/`, public-domain 7-Zip lineage
  via Xtro-Zip `unzip60`). `ppmd.h` + `ppmd8dec.c` were missing
  from the first vendor pass (nothing links without them). Glue
  lives in its own `ZipPpmd.cpp` because `ppmd.h` redefines
  `ISzAlloc`/`UInt32` that `7zTypes.h` already gives `ZipTool.cpp`;
  the header keeps only `UnPpmd(DWORD,DWORD)` plus a forward-
  declared `ppmdRead` callback. Framing is a 2-byte LE props word
  (order/mem/restore) + range stream ending in END; mem is
  clamped to 1..64 MB pre-allocation, output is exactly `usz`
  symbols, and a corrupt tail truncates short so the size check
  fails loudly. `szMode` is `ppmd`. Fixtures: authentic 7-Zip
  (orders 4/6/8, RESTART only) plus a TEMP-only encoder driver
  around zip30's `ppmd8enc.c` for CUT_OFF/FREEZE members (never
  vendored; FREEZE needs SetLegacyFreeze). Verified 8/8 PASS
  CRC-exact (234 B .. 300 KB) plus a corrupt-stream negative.
- ZIP method 18 (Xceed BWT) is implemented via vendored `xceedbwt.c`
  (`src-0.49.17/xceedbwt/`, MIT-0, from the Xtro-Zip `unzip60`
  reference tree). Callback API (`iz_xceedbwt_decode` validates size
  + CRC internally, so no CRC callback is passed); `CZipTool::UnBwt`
  bounds reads by `csz` and forwards ZIP size/CRC. `szMode` is
  `bwt`. Only `<stddef.h>` is needed, so the glue lives in
  `ZipTool.cpp` directly (no TU split needed). Fixture trap: the
  Huffman length stream terminates EVERY symbol incl. k=0 (bzip2
  sendMTFValues convention) - the hand encoder first omitted it
  and the strict decoder rejected everything, which proves the
  FIXTURE was wrong (same DCL lesson). Verified 6/6 PASS
  CRC-exact (0 B .. 120 KB, multi-block/RLE-heavy/empty).
  AES-inner-BWT is refused (decoder forces a CRC-vs-header check
  but AE-2 stores no CRC); plain BWT is fully supported.
- ZIP method 95 (XZ) is one complete XZ stream per member, decoded
  via a hand-parsed container over the vendored `Lzma2Dec` chunk
  layer (`src-0.49.17/lzma/`, public domain; no liblzma vendored).
  Single LZMA2 filter only; None/CRC32/CRC64 checks (own bitwise
  CRC64); dicts clamped to 4KB..64MB; output capped at `usz` with
  an end-marker path that cannot stall; index/footer/CRC/exact-
  consumption all validated. `szMode` is `xz`. Traps, all earned:
  block-header padding sits BEFORE its CRC (not after); `Unpadded
  Size` excludes data padding (proven against liblzma sources, not
  memory); producers omit block sizes, so blocks decode to the
  LZMA2 marker; `LzmaDec_Allocate` reads dic/probs pointers, so
  `Construct` before `Allocate` is mandatory (`UnLzma` shipped
  without it on zero-stack luck - fixed here too). A second
  `UnLzma` trap in the same family: full input-buffer consumption
  (`slen==inlen`) must reset `inlen` to 0 or refill stalls and
  consumed bytes re-decode (invisible while every fixture stayed
  single-fill; proven by a 3 MB multi-fill member failing at the
  1 MB boundary, `m14_big3m.zip`). BCJ chains,
  SHA-256 checks and oversized dicts are refused. Verified 8/8
  PASS (7-Zip CRC32 incl. 300 KB, Python CRC32/CRC64/None/empty,
  hand-assembled 2-block, AES-inner-XZ) plus corrupt/truncated/
  SHA256 negatives (all loud, none hang).
- ZIP method 93 (Zstd) is implemented via vendored zstd 1.5.6
  decompression subset (`src-0.49.17/zstd/`, BSD/GPL, see
  `zstd/KI.README`). The first attempt (from-scratch Go port)
  failed on three spec traps (prevOffset repeat, FSE bitCount
  end, LE bit-reader order) and was abandoned for the official
  C source + mechanical VC6 shims (no ULL/long long/variadic
  macros/`declspec(deprecated|noinline)`/`_BitScan*`/intrin.h;
  clean-room `xxhash64.h` verified vs canonical vectors).
  Member data is one raw zstd frame; `CZipTool::UnZstd` reads
  exactly `csz` via zipread (AES-inner works via the CTR hook)
  and one-shots into a `usz` buffer (short fails loudly).
  `szMode` is `zstd`. Gates: Check scan + both read_header
  copies + AES-inner refusal all exempt 93; `Zstd=93` lives in
  both ZipMethod enums (ZipTool.h + OpenZip.h).
  Verified 8/8 PASS (`%TEMP%\zstdfix\m93_zstd.zip` 5 +
  `m93_mixed.zip` 3, CRC-exact, mode=`zstd`) plus trunc loud-
  fail, skip-prefix/dict/level-19 edge proofs in-harness, and
  27/27 cross-method regression (0/8/10/12/14/18/98).
  AES-inner-Zstd untested (no writer obtainable, same as DCL).
- Standalone `.lzma` (LZMA Alone) is a full provider (`CLzmaTool`/
  `COpenLzma`, `ARCHIVETYPE_LZMA` 65): `Check` validates the props
  byte (no magic exists), `Extract` strips `.lzma` (else appends
  `unlzma`), listing shows the true output name (NOT GzTool's
  `.ungzip`-append quirk), sizes from the 8-byte field (0 when
  unknown), no CRC in the format. The LZMA drive loop lives in
  shared `LzmaDecode.h/.cpp` (`LzmaDecodeStream` over fill/write
  callbacks, exact/uncapped modes); `CZipTool::UnLzma` was rewired
  onto it with full m14-matrix equivalence proof (7/7 incl. the
  multi-fill `m14_big3m.zip`, which also forced the `inlen=0`
  reset fix). Fixtures in `%TEMP%\lzmafix\` (EOS-marker + patched
  known-size; no authentic known-size writer obtainable, and the
  marker-kept variant exercises the identical code path).
  Verified 5/5 PASS CRC-exact.
- Standalone `.xz` is a full provider (`CXzTool`/`COpenXz`,
  `ARCHIVETYPE_XZ` 66): `Check` reads the XZ magic, `Extract`
  strips `.xz` (else appends `unxz`), listing parses the trailer
  (footer backward-size + index) for exact sizes without decoding,
  CRC shown as 0 (none upfront). The XZ container lives in shared
  `XzDecode.h/.cpp` (`XzDecodeStream` over fill/write callbacks,
  exact/uncapped modes); `CZipTool::UnXz` was rewired onto it with
  full m95-matrix equivalence proof (8/8 incl. twoblock). Two traps
  caught by standalone testing (the ZIP path masks engine-bool
  failures behind its size check): the index accumulator must stop
  BEFORE taking the stored CRC itself, and `indexLen` must be
  captured before the footer take (else +12 skew breaks backward).
  Fixtures in `%TEMP%\xzalone\` (CRC32/CRC64/None/big) plus the
  `%TEMP%\xzfix\` ZIP-method set. Verified 5/5 PASS CRC-exact.
- `.tar.lzma` (incl. `.tlz`) is `CTlzmaTarTool`/`COpenTlzma`
  (`ARCHIVETYPE_TLZMA` 67) subclassing the tar providers: decode
  the whole stream to a fixed-name temp file, then run base tar
  over it (inherited Check/Extract/List just work, full validation
  reused). Temp is self-healing (removed on open/close; at most one
  stale file, deleted on next open). Traps: base `TarOpen` calls
  virtual `TarClose` first, which would delete the just-decoded
  temp - guarded by a live flag; `CTarTool(const char*)` never
  initialised `fp` (all tar subclasses shared it; `fclose` on
  garbage on first open in a reused heap - fixed with `fp=NULL`).
  Fixtures in `%TEMP%\tarfix\` (Python tarfile + lzma, incl. a
  subdir; `-j` flattening matches `.tgz` parity). Verified 3/3
  PASS plus `.tlz` alias and plain-`.lzma`-stays-standalone.
- `.tar.xz` (incl. `.txz`) is `CTxzTool`/`COpenTxz`
  (`ARCHIVETYPE_TXZ` 68), same temp-delegation design over
  `XzDecodeFile` (full XZ validation reused, incl. index/footer).
  Fixtures in `%TEMP%\tarfix\` (Python tarfile + XZ CRC32, incl.
  a subdir). Verified 3/3 PASS plus `.txz` alias and plain-`.xz`-
  stays-standalone.
- Standalone `.zst` is a full provider (`CZstdTool`/`COpenZstd`,
  `ARCHIVETYPE_ZSTD` 69): `Check` reads the LE32 magic number
  `0xFD2FB528` (on-disk bytes `28 B5 2F FD` - the hex rendering
  is NOT the byte order; a first cut compared `FD 2F B5 28`
  and every `.zst` fell through to the LZMA provider, whose
  lax props-byte `Check` (`hdr[0] <= 224`) accepts anything),
  `Extract` strips `.zst` (else appends `unzst`) via a new
  streaming `ZstdDecodeFile` (multi-frame tolerant, strict tail:
  trailing garbage/truncation fails), listing walks frames for
  exact sizes without decoding (unknown size -> 0, multi-frame
  sums). Two routing traps, both real: the new pairs must be
  registered BEFORE `CLzmaTool` (same lax `Check` beats any
  later-registered provider in the fallback sweep), and
  `FindFirst` must rewind `fp` to 0 after the magic read (else
  the full-file `fread` comes up short and every file refuses).
  Fixtures in `%TEMP%\zstalone\` (7: empty..300KB + no-content-
  size + 2-frame). Verified 7/7 PASS CRC-exact.
- `.tar.zst` (incl. `.tzst`) is `CTzstTool`/`COpenTzst`
  (`ARCHIVETYPE_TZST` 70), same temp-delegation design over
  `ZstdDecodeFile`. Fixtures in `%TEMP%\tarzst\` (basic +
  subdir + `.tzst` alias). Verified 3/3 extracts + listings
  PASS; tar-content-named-`.zst` routes to tar (house rule).
  Negatives: truncated/fake-magic `.zst` refuse loudly, none hang.
  All six file formats (`.lzma`/`.xz`/`.zst`/`.tar.lzma`/
  `.tar.xz`/`.tar.zst`) are now covered end to end.
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
- **The last two post-NT4 static imports are gone.**  Both came
  from the vendored **unrar**, not from our own code, so a byte scan of
  the 221 build objects finds neither -- look in `unrar/UnRAR.lib`:
    - `AreFileApisANSI` (2000+) in `unrar/dll.cpp:54`.
    - `GetShortPathNameW` (2000+) in `unrar/filcreat.cpp`.
  `GetShortPathNameW` is the exact same case as `GetLongPathNameW`
  six lines above it, which was already converted -- it was simply
  overlooked; the fix mirrors that block (`VC6_GSPN`).  `AreFileApisANSI`
  becomes a local `RarFileApisANSI()` (`VC6_AFAA`) that resolves it
  dynamically and, when absent, decides locally: the file APIs are ANSI
  exactly when we are not on NT (`GetVersion` high bit set), which is
  what `AreFileApisANSI` reports on those systems.  KERNEL32 drops from
  136 imports to 134 and the import table now matches official's for
  every symbol that is Windows 95/NT4 or older.  RAR is unaffected:
  all 6 fixtures (incl. the 72-member `aida64.rar`) extract byte-for-byte
  identically to the pre-fix DLL, and `runnewmatrix.py` is unchanged.
  **Rebuild `UnRAR.lib` after touching ANY unrar source or header**
  (the lib is tracked and the DLL links it statically):
  `%TEMP%\opencode\buildunrar7repo.bat`, which mirrors the upstream
  7.x lib OBJECT list minus `rar.cpp` plus `motw.cpp`.  An edit alone
  changes nothing until that runs and the DLL relinks.  **This bites
  headers hardest**: `327feb9` (the Win95 ANSI gate) touched
  `unrar/win32scompat.h` only, and because the VC6 DLL links the
  PREBUILT lib the header edit reached the VC4 build (which compiles
  unrar from source) but NOT the VC6 DLL, which silently kept the
  old gate.  Nothing in the tree reports this; compare the lib mtime
  against the header, or just always rebuild after an unrar edit.
- **The `FreeEnvironmentStrings*` pair is NOT a gap -- leave it alone.**
  Those, plus `GetEnvironmentStrings{,W}`, come from the CRT
  (`libc.lib`/`libcmt.lib`, i.e. `/MT` startup), **not from our code**,
  and the official 0.49.17 binary imports all four as well.  The W
  variants exist on NT and the A variants on Win95, so the combined
  set simply pins the floor at **NT4** -- which is where official sits
  anyway (it imports `SHLWAPI.dll`, `IMAGEHLP.dll` and `WINMM.dll`,
  none of which Win95 has).  Dropping them would mean dropping the
  CRT, which is not a trade worth making.  Useful way to tell the
  three cases apart: scan the build `.obj`s first (a hit there is our
  code), then the tracked `.lib`s (`UnRAR.lib` hit = vendored), then
  the VC98 `LIB` dir (a hit in `libc.lib` = CRT, leave it).
  Verify with `dumpbin /imports` diff vs official after any unrar change.
- bzip2 fixtures: upstream `sample1/2/3.bz2` + `.ref` (in TEMP
  `bz2test/`), plus generated multi-block + ZIP-method-12 fixtures
  (`mkbz2.py`, `runbz2matrix.ps1` — 6/6 MATCH).
- Standalone `.bz2` was misrouted to the LZMA provider and is now
  FIXED (found while dropping blocksort.c; the drop was innocent).
  The routing logic is worth stating because it is NOT a single
  sweep: `XacRettGetArchiveType` takes a first pass over the tool
  list for the first tool whose `IsType(ext)` matches, and
  **only if that yields nothing** a second pass takes the first
  tool whose `Check()` passes.  A `.bz2` file got no first-pass
  candidate at all -- `CBz2Tool` does not override `IsType` and
  the base returns false, while `CTbzTool::IsType` matches only
  `"tar"` -- so the answer came from the second pass, where
  `CLzmaTool::Check` (nothing but `hdr[0] <= 224`) accepted a
  bzip2 stream's `'B'` = 0x42 = 66.  Fix: register
  `CBz2Tool`/`COpenBz2` immediately BEFORE `CLzmaTool` in
  `DllSub`.  `CBz2Tool::Check` is strict (`"BZh"` plus a real
  `BZ2_bzread`), so registration order was the only thing wrong.
  Verified: `.bz2` now reports `type=11` and extracts byte-exact;
  `runbz2matrix.ps1` went 5 MISSING -> **6/6 MATCH**; `.lzma`
  still routes to 65, `tree.tar.bz2`/`.tbz` to 46, ZIP m12 to
  43, all unchanged; VC4 (`VC4_RC=0`) agrees byte-for-byte.
  Known limit: a **multi-stream** `.bz2` (two concatenated
  streams, legal per bzip2) yields only the first, because
  `CBz2Tool::Extract` loops on `BZ2_bzread`, which is
  single-stream.  Real `bzip2` keeps reading past the first
  stream footer; matching that needs `BZ2_bzReadOpen` /
  `BZ2_bzReadGetUChar`, or a concatenated-stream loop.  The
  matrix does not cover it (single-stream fixtures only).
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
  (`76 FE`) members are v2.x adaptive LZW (9-12 bit MSB-first codes,
  EOF 256 / RESET 257 / filler 258-259 / dict from 260, RLE90 layer,
  table-full victim replacement — NOT the fixed-12-bit hash scheme of
  ARC m5): header is magic + NUL-terminated name + ref/sig/err/spare
  (sig >= 0x20 selects v2; the name runs to the first 00, then data;
  checksum only when errdetect != 0). `crunch28.lbr`: 8/8 members
  (6 crunched + 2 stored) list and extract byte-exact, verified against
  80un's `uncrunch` (`TEMP\ref80un\crunch.py`, GPL, reference use only —
  never vendor; our `LbrTool.cpp` port is clean-room). V1 crunch
  (sig < 0x20) is rejected, not decoded. CrLZH (`76 FD`,
  ext middle `Y`) members are LHArc-lh1-lineage LZSS + adaptive
  Huffman (2K window, spaces init, stop symbol 256, NO RLE layer,
  LE16 byte-sum checksum after the codes when errdetect == 0;
  `used` >= 0x21 refused, >= 0x20 selects the v2 11-bit position
  code else v1 12-bit, both via d_code/d_len tables, MSB-first).
  `76 FD` used to be misrouted to the crunch path (always failed
  loudly); it is now exclusive to CrLZH, `76 FE` to crunch.
  `crlzh20.lbr` (workarea root, untracked): 30/30 members
  (26 v2 CrLZH + 4 stored) list and extract byte-exact vs the
  80un oracle; v1 path proven by 21/21 on TEMP `crlzh11.lbr`
  plus a 3/3 v2-crunch synthetic-LBR regression (authentic 80un
  samples re-wrapped with valid directory CRCs) and a correct
  v1-crunch refusal. Traps, all real: zstd-style LE confusion
  does not apply here (magic is a byte string, not a number);
  the checksum sits at the first byte boundary after the final
  code (NOT at member end - 1A pad follows; a ceil-based skip
  overshoots by one when codes end byte-aligned, caught by the
  47/47 LE16 probe); the reference zero-pads reads at EOF, ours
  fails loudly on overrun instead (identical on valid inputs).
  Whole-compressed libraries (`.lqr`/`.lzr`/`.lyr`) are NOT
  supported (explicit decision): `Check` needs the raw directory
  control entry at offset 0, so they refuse up front. They were
  rare even in-period (members, not archives, were compressed),
  and no authentic fixture exists in squsq/80un/format wikis;
  without one the temp-delegation path + SQ whole-file checksum
  semantics cannot be proven, so this stays a documented gap. CP/M dates convert to DOS dates (pre-1980 → 0);
  `uOSType` 15 matches LZH `C`.
- Command line: `XacRett()` takes `-n1` (no dialog), `-o1` (keep
  existing files) and `-j` (flatten paths). **`-p<password>` was added
  here**: it sets a non-interactive password, so `CTool::GetPassword()`
  returns it and never opens the modal dialog. That makes batch/headless
  extraction work for every provider that asks - 7zAES, ZIP WinZip-AES,
  RAR, ARC, ACE, SQX - instead of hanging on the dialog (which is what a
  headless run does today). Verified with `%TEMP%\aesfix\` and
  `m93_aes.7z`; a wrong password still fails per-member exactly as the
  dialog path does. Implementation notes:
  - `CSplitCmdLine::Analysis` takes everything after the option letter as
    the password, so `-psecret` and `-p secret` both work;
  - a token is a switch when it starts with `-`, or with `/` followed by
    one of `nNoOjJpP` ONLY. Do not treat every `/` token as a switch or
    absolute paths like `/tmp/x` get swallowed as unknown options;
  - the password lives in `CTool::m_pPassArg` (a static), so it applies to
    every tool in the chain, not just the one that parsed it. It is visible
    in the process command line, same as every other archiver; `-p` is
    opt-in and the dialog remains the default when it is absent.
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
- 7z support (DONE): clean-room container parser
  (`SevenZDecode.cpp`) over the vendored SDK cores (`sevenz/`,
  public domain, Pavlov; AES/SHA-256, Bcj2, Bra+Bra86, Delta,
  Ppmd7). Signature + start-header CRC + header CRC, plain and
  `kEncodedHeader`, PackInfo/UnPackInfo/SubStreamsInfo/FilesInfo,
  folder coder graph with bind pairs, one-entry folder decode cache
  for solid archives, UTF-16LE names, attributes, FILETIME, dirs,
  empty-file bit vector, zip-slip path sanitising. Coders: Copy,
  LZMA, LZMA2, Delta, BCJ/ARM/ARMT/PPC/SPARC/IA64/RISCV, BCJ2,
  PPMd7, Deflate, BZip2, **ZSTD (`04F71101`, 5-or-3 props
  `[major,minor,level,0,0]`, plain zstd frame)** and 7zAES.
  `ARCHIVETYPE_7Z` 71; registered BEFORE the LZMA/XZ providers so
  their lax `Check` cannot steal `.7z`.
  `sevenz/7zTypes.h` and `lzma/7zTypes.h` share the include guard
  `ZIP7_7Z_TYPES_H`, so the TU split is mandatory: `SevenZCore.cpp`
  = sevenz only, `SevenZDecode.cpp` = lzma+zlib+bzip2+zstd via a
  plain-byte `SevenZCore.h`.
  The two real SDK port bugs are unchanged: upstream enables SHA-NI on
  `_MSC_VER >= 1200` and AES-NI on a bare `#elif defined(_MSC_VER)`, but
  **VC6 reports _MSC_VER == 1200**, so both match and then reference
  assembly-only `*_HW` symbols (LNK2001); narrowed both to `> 1200`.
  Traps recorded in `sevenz/KI.README`: `Sha256Prepare()` once first;
  AES key schedule at `Aes()+4`, buffer `AES_NUM_IVMRK_WORDS + 3`
  words; decrypt needs `Aes_SetKey_Dec`; X86 branch lives in `Bra86.c`
  only; Delta is not self-inverse per call pair; **`IByteIn` in this
  SDK vintage is a direct-call interface (`IByteIn_Read(p)` is
  `p->Read(p)`), so a PPMd buffer descriptor cast straight to
  `IByteInPtr` gets its `buf` pointer CALLED -> 0xC0000005 under both
  /Od+/RTC1 and /O2 - embed `IByteIn` as the first member of the
  wrapper instead**; and a 7z PPMd stream has **no end marker** (the
  encoder only flushes the range coder), so requiring
  `PPMD7_SYM_END` / `IsFinishedOK` / `pos==len` rejects every valid
  archive - check only the exact output length and that the coder
  never read past the packed stream.
  Verified 25/25 fixtures byte-exact in `%TEMP%\7zfix\`: 17 from the
  real 7-Zip 26.04 CLI (lzma2, lzma, copy, bzip2, deflate, delta,
  ppmd, bcj, bcj2, aes, hdr on/off, non-solid, bigdict, mx0/mx9,
  multi-solid) + 8 py7zr-written zstd fixtures (level 1/3/19, 3-byte
  and 5-byte props, zstd+bzip2, zstd+copy, x86+zstd, arm+zstd) + the
  authentic 7-Zip 19.00+zstd archive in the workarea root (BCJ +
  ZSTD, `7z.exe` byte-exact vs py7zr). Five corruption negatives
  (payload byte, zstd magic, truncated pack, deep entropy bit, header
  CRC) all fail loudly and fast; a corrupt 7z is then claimed by the
  lax standalone-LZMA provider (same house behaviour as `.xz`/`.zst`)
  but fails fast without the 0xFFFFFFFF size allocation.
  7zAES needs three things that are all silent when wrong (no crash, no
  error, just a key that decrypts nothing): the KDF hashes the password
  as **UTF-16LE** (`MultiByteToWideChar(CP_ACP,...)` at the call site -
  hashing `strlen()` ANSI bytes gives a valid-looking key that opens
  nothing); the CBC decrypt works IN PLACE so the ciphertext must be
  memcpy'd out of `src[0]` into the freshly `new`ed `dst` first; and a
  7zAES payload carries **no embedded digest** - the encoder rounds the
  plaintext up to 16 with ZEROS and the header records that padded
  size, so any tail-digest check is meaningless (integrity comes from
  the entry CRCs instead). With all three fixed the key matches
  py7zr's `calculate_key` bit for bit; m93_aes.7z/"secret" and
  aes4.7z/"testpw" extract byte-exact and a wrong password fails loudly
  one stage later.
- YZ1 (`.yz1`, `ARCHIVETYPE_YZ1` 45) password support is DONE. The
  format is trivial once decoded, and the official 0.49.17 binary simply
  **cannot** open these - it faults with 0xC0000005 on both listing and
  extraction - so we deliberately FIX it. Format taken from `yzdec.c`
  (Hex-Rays) and verified byte-exactly against `yzenc.exe -p<PASSWORD>`:
  - `hdr[6]` bit 2 (`& 4`) set = payload is encrypted. `hdr[6..7]` is the
    ASCII literal "00" on a plain archive.
  - `hdr[7]` = check byte = `(strlen(pw) + sum(pw[k]^k)) & 0xFF`. `v33` is
    an **8-bit** quantity in the original; that is what makes multi-char
    passwords work, and it is the trap here (naive 32-bit arithmetic gives
    the wrong answer).
  - `hdr[0..19]` is never encrypted. `hdr[20..]` is XOR with a repeating
    4-byte key indexed by `(offset - 20) & 3`, the key being the
    big-endian bytes of `v37 = i + v34 + v99*v98` where
    `v34=v99=v98=i=len(pw)` and, per byte with `c=pw[k]`, `v36 = k ^ c`
    then `v33=(BYTE)(v33+(c^k)); v99+=v36; v98-=v36; v34*=v36; i^=v36`.
    The wider accumulators wrap at 32 bits.
  - Implemented as `YzDeriveKey`/`YzApplyPassword` in
    `yz32lib/yz32lib_v1.cpp`, called from both `LoadArchive` call sites.
    It uses the `MessINPUT` hook the engine already declared but never
    called, so no new plumbing: extraction reaches `CTool::GetPassword()`
    (so `-p<pw>` works and headless runs do not hang), while
    `COpenYz1::CYzEv::MessINPUT` returns "" so **listing never prompts** and
    a password archive simply lists as 0 members.
  - **The password is asked exactly ONCE per extraction.**
    `yzFileDecode::DecodeFile` parses the header and then extracts, and each
    step runs its own `LoadArchive` -> `YzApplyPassword`, so an unguarded
    hook prompts TWICE (measured with an instrumented
    `CTool::GetPassword`: 2 calls on success, 1 on a wrong password - the
    wrong case already aborted on the header parse). Both loads share one
    `V2Bridge` (it is the `yzFileEv` they are both constructed with), so
    `V2Bridge::MessINPUT` caches a non-empty password for the session.
    An EMPTY result is deliberately NOT cached: the listing event returns
    `""` and must keep asking. Measured after: 1 call, 1 call.
  - A wrong/absent password fails with -1 at the check byte rather than
    decoding to garbage. Official `yzdec.exe` instead accepts a wrong
    password and emits a garbage name and negative size
    (`TestFile(1/1):  (-318767104 byte)`) before failing on `fopen`.
  - **Also fixed a pre-existing crash that hit EVERY successful YZ1
    extraction**, with or without a password: the engine calls
    `GaugeSetOutputValue(i+1)`, so `gauge_WriteInfo`'s `no` is 1-based, but
    the body indexed `m_file_name_tbl[no]` directly - one past the end for
    the last member. The garbage `std::string`'s `c_str()` reached
    `kiutil::StrMatch` and faulted (+0x2c into that function, i.e.
    `mov dl,byte ptr [ecx]`). It now uses the `yzFileStat&` the bridge
    already builds from the correct 0-based index. Official does not have
    this bug. Worth remembering: it was hiding because the YZ1 extraction
    checks compared content and ignored the harness exit code.
  - Verified: 3-member archives (text/binary/70k) plain and passworded,
    8 single-member password variants (case, punctuation, digits, 21
    chars, 1 char), 4 negatives (wrong/empty/case-mangled/absent) all
    clean with no crash, hang or partial output, and the `yzenc.yz1`
    workarea sample (password `yzenc`) extracting `yzenc.txt` byte-exact.
    Every member also matches `yzdec.exe`. Re-checked on the VC4 build.
  - Reference is the shipped `yzenc.exe`/`yzdec.exe` plus `yzdec.c` /
    `yzdec.lst` in the workarea root - never vendored.
- YZ1 (`.yz1`, `ARCHIVETYPE_YZ1` 45, `Yz1Tool` + `COpenYz1` over
  `yz32lib`) listing was silently broken: the listing constructor
  `yzFileDecode(InEv&, yzFileEv*, const char*)` only REMEMBERS the archive
  path - it never opens the file or reads the header - so
  `dec_file_count` stayed 0 and every `.yz1` listed as empty while the
  official binary listed its members. Extraction always worked, because
  `CYz1Tool::Extract` calls `DecodeFile(0)`; the listing path is now
  `YzFile_DecodeHeader(COpenYz1::szArcFileName)` (the call that is what
  actually fills `dec_file_count`). `alice.yz1` now lists 1 member at
  152089 bytes and extracts byte-identical to the official binary.
  Trap worth keeping: `dec.DecodeFile(0)` is NOT a substitute here -
  it is the v1 entry point and leaves `dec_file_count` at 0, and
  `m_arc_name` is private anyway.
- Known gaps: GCA non-`GCAX` paths are stubbed; `OpenDummy`/`OpenCompactC` have no live listing path.
- **8.3 fallback for file systems with no long names** (Win32s on
  Windows 3.1, Win95 with `lfn=off`). Reported symptom: the VC4 build on
  Win32s returns **0x8013 `ERROR_CANNOT_WRITE`** for any archive with a
  member whose name does not fit 8.3 - `fopen("...","wb")` simply fails,
  the provider returns false, and `XacRett.cpp:1129` maps that to 0x8013.
  That is an OS limit, not a bug (the official binary fails the same way),
  so the fix is deliberate divergence: `kiutil::longNameOK()` +
  `kiutil::pathMake83()` in `kiutil.cpp` mangle such names the way the
  era's short-name generators did (`LongFileName.txt` -> `LongFi~1.txt`,
  `short.LONGEXTENSION` -> `short.LON`, `CON.txt` -> `CON~1.txt`,
  DBCS-aware, base 6 chars + `~N` with N counted up from 1 and the last
  component probed against the disk).
  Design points that matter:
  - **the probe is lazy**: `XACRETT_8DOT3` unset, `p83need()` decides from
    the string alone whether anything needs mangling, and only then is
    `longNameOK()` asked (it creates+deletes `XacRettLongNameProbe.tmp` in
    the CWD, i.e. the output dir, once per process). On any system with
    long names there is no probe, no rename and no extra syscall - proven
    by a base-vs-new DLL diff on the whole regression set.
    `XACRETT_8DOT3=1` forces the mangling on and is how the fallback is
    tested on a machine that does have long names.
  - **the probe re-opens the file it just created.** Create-only is not
    enough: a 16 bit file system driver can accept the create and then
    keep the file under some mangled name, which would make the probe
    answer "long names work" when they do not. `OPEN_EXISTING` on the
    same name is the check that cannot lie that way - and neither can
    a driver that truncates *consistently* (create and re-open both
    succeed on the truncated name), so the probe also reads the name
    back with `FindFirstFile` and requires it to match: only the exact
    long name counts as long-name support.
  - **the probe is DIFFERENTIAL, and the error code is never read.**
    Do not go back to deciding from `GetLastError`: the whole point of
    the four "cannot tell" codes (ACCESS_DENIED / SHARING_VIOLATION /
    WRITE_PROTECT / DISK_FULL) was to protect a read-only output dir
    from renaming, but a 16 bit FAT driver is free to report a
    too-long name as any of them, and every such code has to answer
    "long names are fine" - so the fallback silently switched itself
    back off on exactly the platform that needs it. Reported as
    "only 8.3 names extract" on `aida64.7z`. Instead: create the long
    probe name; if that is refused, create a SHORT 8.3 control name
    (`XPRB~1.TMP`) in the same directory - if the control lands, the
    directory is writable and only the name was refused, so mangle.
    Both failing still means "cannot write here at all", which keeps
    the old read-only behaviour for free. Proven with
    `proveprobe.py`: a directory squatting the long probe name (so
    CreateFile on it fails for a reason unrelated to LFN) makes
    `aida64.exe.manifest` -> `aida64~1.man` and `kerneld.ia64` ->
    `kerneld.ia6`; the pre-fix DLL keeps both long and drops the
    other 50. `probereadonly.py` covers case C with an ACL.
  - **intermediate path components are mangled from the string only**,
    never probed. Probing them would split one archive across two
    directories: the first member creates `LONGDI~1`, the second sees
    `LONGDI~1` taken and picks `~2`.
  - `pathMake` now creates its directories AFTER `pathMake83`, otherwise
    the unmangled directory name is what gets created and the mangled file
    has nowhere to go. Same for `-j`, which still runs last.
  - Call sites: `pathMake` covers ~45 providers. The 12 single-file tools
    that build the name themselves (AtoB, Bel, Blz, Bz2, CompactC,
    CompressZ, Frz, Gz, Jam, Mime, Packz, UnixDecoder) get an explicit
    `kiutil::pathMake83()` before their `fopen`; the derived-name branch
    matters there because `.gz`->`""` etc. still leaves things like
    `LONGNAME.DATA.ungzip`, which is not 8.3 either.
  - **RAR**: `CRar3Tool` (unrar 7.2.7) IS the registered provider -
    `XacRett.cpp:20` defines `RAR_3X`, so the `#ifndef RAR_3X` branch
    that registers `CRarTool` (RarTool.cpp, the in-house reader) is
    never taken; RarTool.cpp is compiled but dead. So every RAR
    extract goes through `pathMake` + `RARProcessFile`, and the
    mangled name did reach unrar - `pathMake` rewrites `hdr.FileName`
    in place - but that was implicit, so `Rar3Tool::Extract` now
    passes the returned `name` to `RARProcessFile` explicitly. That
    also fixes a crash on the same lines: `fclose(out)` ran BEFORE
    the `if(out)` NULL test, so any `fopen` failure (which is
    exactly what happens on Win32s) was `fclose(NULL)`.
    Trap: a member called `CON.txt` (any device name) still breaks
    the RAR chain on a modern OS - `fopen` succeeds and writes to
    the device, and every later member is then skipped. Under the
    8.3 fallback the name becomes `CON~1.txt` and the chain holds.
  - **YZ1 and GCA are now covered too** (DMG is the last holdout).
    YZ1 builds its own output path in `yz32lib` (`yzDecHead::FileWrite`
    plus two sites in `yz32lib_v1.cpp`): each does
    `MakePathForFile(out)` then `fopen(out,"wb")`, so the fix is one
    `kiutil::pathMake83(out);` **immediately before** `MakePathForFile`
    -- mangle first or the unmangled directory names get created and the
    mangled file has nowhere to go.  GCA builds a `std::string` in
    `GcaSDK/gca_api.cpp:gcaWriteOne`; the call goes after the `/`->`\`
    conversion and before the `mkdir -p`, and the mangled string is
    assigned back with `out.assign(tmp83)` (`assign`, not a
    `std::string(buf,n)` temporary, which is a most-vexing-parse on VC6)
    so the later `fopen`/`CreateFileA`/`SetFileAttributesA` all use the
    mangled name.  Both files already pull `<windows.h>`, so the added
    `#include "../kiutil.h"` needs nothing else.
  - YZ1/GCA 8.3 verified with fresh fixtures built by the period tools
    (`yzenc.exe` takes a FOLDER, `gcac.exe a <arc> <folder>`), both over
    the same tree (`OK.TXT`, `LongFileName.txt`, `LongFileName2.txt`,
    `short.LONGEXTENSION`, `with space.txt`, `A.B.C`, and two nested
    long dirs).  Forced runs give 8/8 files, every path component legal
    8.3 (`LongFi~1.txt`/`LongFi~2.txt`, `short.LON`, `A_B~1.C`, and the
    directory `dir with long name` -> `dir wi~1`), contents identical to
    the baseline run, and the member paths are reproduced verbatim when
    the fallback is off.  All 20 pre-existing GCA fixtures re-extract
    identically with the fallback forced on, and the password-protected
    `yzenc.yz1` (`yzenc`) is byte-identical both ways.  VC4 and VC6 agree
    on both name and bytes.
  - **The fallback can write MORE than the baseline, and that is the
    point**: three GCA fixtures (`b_none/b_norm/b_solid.gca`, 306 members)
    contain one DBCS-named member whose name has no legal Win32 path, so
    the plain run drops it (305 files) while the forced run materialises
    it as `_____y~1.txt` (306).  A regression harness must therefore
    assert the forced run is a SUPERSET, never that it is equal.
  - **DMG was never actually a gap** -- the note above was wrong.
    `DmgTool.cpp:548` already calls `kiutil::pathMake( outname )` before
    its `fopen`, and there is no conditional compilation anywhere in that
    file, so the fallback applies.  `OpenDmg.cpp` writes only to a
    `GetTempFileName` name (`XacXXXX.tmp`), which is 8.3-legal by
    construction.  An audit of every `fopen(...,"wb")` /
    `CreateFile(...CREATE_ALWAYS)` in the tree now leaves nothing
    uncovered: 70 write sites, all either guarded or machine-named.
  - **`pathMake` passed the WRONG pointer to the 8.3 helper, and it cost
    real data.**  It advanced `st` past the drive, then called
    `pathMake83( st )` -- so the collision probe inside
    `pathMake83`->`p83fit`->`p83taken` was a `GetFileAttributes` on a
    *drive-relative* `prefix+candidate`, which resolves against the CWD
    and fails with ERROR_PATH_NOT_FOUND.  A candidate therefore always
    looked free.  With `-j` (which flattens to a bare basename) two
    members whose mangled names collide silently overwrite each other:
    `long83.sqx` yielded 6 files for 7 members, with `LongFi~1.txt`
    holding the *second* member.  Fixed by passing the full path
    (`pathMake83( path )`); `pathMake83` keeps a leading `X:` intact and
    rewrites in place, so `st` stays valid for the directory loop and for
    the callers that use the returned pointer.  Behaviour where long
    names work is unchanged (`longNameOK()` short-circuits all
    mangling), proven by a baseline-vs-fixed A/B: `runnewmatrix.py`
    output is byte-identical, ARC 18/18 MATCH, SQX matrix still 202, and
    the forced-8.3 harness goes from SQX 6/7 to 7/7.
  - Verified on Windows (VC6 and VC4 DLLs behave identically): a
    long-name ZIP (`OK.TXT`, `LongFileName.txt`, `LongFileName2.txt`,
    `short.LONGEXTENSION`, `CON.txt`, `with space.txt`,
    `dir with long name/nested file.txt`,
    `dir with long name/deeper/deep file here.bin`, `A.B.C`, a DBCS name)
    extracts 10/10 rc=0 under `XACRETT_8DOT3=1` with every component a
    legal 8.3 name and the bytes unchanged; the same archive without the
    variable is byte-for-byte the old behaviour; a second run into the
    same directory bumps `~N` instead of overwriting; `-j` still
    flattens. `tar.gz` gives `a very~1/deeper~1/leaf.dat`. RAR needs a
    hand-built fixture (no RAR writer is available): a store-only
    RAR 4.x written from the layout in `unrar/arcread.cpp` plus
    `RarTool.h`, and the authentic `GPLv2-rar4.rar` with its member
    name rewritten (NameSize/HeadSize/head CRC recomputed; the head
    CRC is the LOW 16 bits of a PLAIN crc32 over header[2:], NOT
    inverted - getting that wrong makes unrar list the archive but
    extract nothing from it, which is how it was found). Both give
    8/8 rc=0 with byte-identical content under `XACRETT_8DOT3=1`
    (`LONGNA~1.txt`, `dir wi~1/deeper/deep f~1.bin`). 7z was checked the
    same way and needs no special case (SevenZTool goes through
    pathMake like everything else): py7zr-written fixtures with long
    names, nested long dirs, empty files, solid folders, 8/3 boundary
    names, plus all 25 authentic fixtures under `XACRETT_8DOT3=1` -
    30 trees / 166 files, zero names outside 8.3 in every forced run
    (and the 13 long names all appear in the *unforced* control runs,
    which is the point). Regressions:
    ARC 18/18 MATCH, 7z 25 fixtures OK (118/157 members, the documented
    baseline), ZIP matrix and the 33-case sweep identical to a DLL built
    from the pre-change tree (the 6 zstd-sandwich failures in
    `runnewmatrix.py` are stale TEMP expectations, present before too).
- JCALG1 (`.jc1`, `ARCHIVETYPE_JC1` 59, `Jc1Tool` + the clean-room
  `jcalg1lib/jcalg1.cpp` decompressor) is now correct. Two bugs had been
  hiding in it and BOTH were silent:
  - **the 10-byte block header was read at the wrong offsets.** The real
    layout is `+0 WORD 'J','C'` / `+2 DWORD uncompressed size` /
    `+6 DWORD checksum` / `+10 bitstream`. The port checked the signature
    at +8 and read the size at +4, so `JCALG1_GetUncompressedSizeOf...`
    returned 0, `CJc1Tool::Check()` failed and the file was misrouted to
    the much laxer standalone-LZMA provider (`jcalg1_test.jc1` listed as
    `jcalg1_test.jc1.unlzma` with orig=0xFFFFFFFF). Authority is
    `JCALG1_gusocb.asm` ("cmp word ptr [eax],'CJ' / mov eax,[eax+2]")
    plus `CompressedData.cpp`, which starts its bit reader at
    `pSource+10`. Note the revision note says the signature is 'JC' while
    the asm compares 'CJ' - the FILE is 'JC', and the size DWORD is at
    +2 either way.
  - **`CopyMatch` never advanced the output pointer.** It pushed bytes
    into a private 64K history ring and threw them away, so every phrase
    match produced ZERO bytes: 53248 came out as 12168. The reference
    uses the destination buffer itself as the history
    (`TransferMatch`: `s = p - MatchOffset`), which is also what lets
    JCALG1 support 4K..1M windows - a 64K ring silently truncates any
    stream compressed with a bigger window. The port now writes through
    the output pointer, with a `SafeDist` clamp so a corrupt stream can
    never read before the destination, and a hard stop at the size in
    the header.
  Reference is jeremycollake/jcalg1 (r5.32) - TEMP only, NEVER vendored:
  `jcalg1_static.lib` is MASM and the repo ships both the lib and
  `jcalg1_test.exe`, which makes it a perfect oracle. `%TEMP%\jc1fix\`
  holds 20 fixtures built with it at window levels 1/5/6/9 (4K..1M) over
  exe/dll/text/zeros/1-byte inputs; all 20 extract byte-exact, and
  `jcalg1_test.jc1` is byte-identical to both the official DLL and the
  reference `jcalg1_test.exe`. Five corruption negatives (truncated, bad
  signature, mangled size field, 0x7FFFFFFF size field, flipped payload
  bit) produce no hang and no crash. The reference compressor REFUSES
  incompressible input (random data), so there is no such fixture.
- SQX method 6 (`SQX_C1_EX_NORMAL`, on-disk `0x06` = `text(PPMd)`, fmt 2.0
  only) is DONE (`SqxPpmd.h/.cpp` + dispatch in `ExtractArcFile`, session
  reset per archive from `CSqxTool::Extract`). PPMd variant I over the SQX
  carryless range coder, driven through the tree's vendored `ppmd8/` model
  core (same 7-Zip lineage the reference was ported from); only the SQX
  front end (word-swapped bit reader, TOP/BOT range layer, cont/order/mem
  header) and the symbol loop live here, mirrored against our core's model
  API (`Update1/Update1_0/Update2/UpdateModel/MakeEscFreq`). Reference is
  seb3773/Squeez-c (`TEMP\sqxref`, MIT - reference use only, never
  vendored). Two transliteration traps, both caught before they ever ran:
  the escape-miss tail masks only the counted `low_states` window
  (masking the whole context undercounts later orders and diverges), and
  the escape-hit walk is a plain subtract over the pre-filtered list (no
  mask check). `Ppmd8_UpdateModel` is global in `ppmd8.c` but absent from
  `ppmd8.h`, so it is declared in `SqxPpmd.cpp` (link fails loudly if it
  ever goes away). `szMode` is unset (matches official, which yields
  nothing here). The old "no method-6-specific decode path" note is
  retired; the `SQX_EX_FILE_HEAD` trap stands (`ReadNextArcFHDR()` only
  uses it with `SQX_FILE64_FLAG`, i.e. the >4GB extension sharing
  `cHeadType == SQX_FILE_HEAD_ID` - reading it as "the method-6 header"
  gives the wrong name/size offsets).
  Verified: `2.sqx` byte-exact; full `packed_corpus` pass 135 members
  byte-exact incl. every method-6 member (hello/eng/code/nested, fmt 2.0),
  all v1.1 LZ and all stored; 12/12 generated fixtures (orders
  2/4/6/8/12/16, mems 1/4/32 incl. 1 MB arena recycle, 637 KB rescale
  workout, solid two-file session persistence, binary input, all
  CRC-exact); VC4 DLL byte-identical on `2.sqx` + 45 KB `code.c`.
  Corruption negatives (truncations, payload bit flips, garbage/empty,
  damaged archive) all fail loudly in <0.1 s, none hang or crash.
  Format facts worth keeping: the header file CRC is standard CRC32
  WITHOUT the final xor (the harness must expect the std CRC of the
  reference bytes, not the header value); `sqx.exe create` emits method 6
  only as fmt 2.0; `cont=1` (solid continuation) comes only from the
  oracle `sqc`, never from `sqx.exe create` - every obtainable stream has
  cont=0, so the 3-line cont path (refuse without a live model, else
  continue) mirrors the reference but is untested. Gaps: method 7+
  ultra is XFAIL (the reference cannot extract it either).  The other
  gap noted here -- fmt-2.0 LZ methods 1-4 failing -- is now FIXED; see
  the "SQX format-2.0 LZ" section below.
  Plumbing fixes this required (both pre-existing, both hit every SQX
  harness run with `-j` + absolute outdir, proven by the whole matrix):
  `CSqxTool::Extract` now uses `pathMake`'s adjusted pointer (house
  pattern, cf. `ZipTool`) - ignoring it left a `"C:\name"` stub that
  `CreateFile` refuses with ACCESS_DENIED - gated on `-j` and never for
  UNC, so all previously-working paths are byte-identical; and
  `os_create_ouput_file` skips dir creation for an empty dir (bare
  basename = CWD, which `MeltArchive` already chdir'd to).
  Harness trap: `dlltest` checks `outdir\member`, but `-j` flattens
  subdir members to `outdir\basename` - compare the flattened path.
  TEMP-side trap fixed along the way: `vc4check.py:objname()` stripped 2
  chars (`[:-2]`), naming every `.cpp` object `X.c.obj`; those strays
  share `%TEMP%\vc4build\obj` with nmake and the link glob eats both
  copies (LNK2005). Strip the real extension instead.
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

## MSVC4 compatibility (full build + link + extract, all green)

Question answered by experiment: **can this tree build under MSVC 4.x?**
Compile-clean: yes, and it **links and runs**. **All 220 compile units
in `XacRett.dsp` (148 .cpp + 72 .c) plus all 49 units of the vendored
UnRAR lib compile clean under CL 10.10.6038**, with zero change to VC6
behavior (full build + relink + 25/25 7z matrix + ZIP-AES/XZ/JCALG1/YZ1
spot checks after every fix). `nmake -f Makefile.vc4 link` produces a
working `XacRett.dll` (1030144 bytes) from scratch.
NOTE: `arccmt.cpp` hangs the VC4 compiler outright and is NOT in the 49;
nor are `rar.cpp`/`int64.cpp`/`recvol.cpp`/`rs.cpp`/`savepos.cpp`/
`ulinks.cpp`, which the checked-in VC6 `unrar\UnRAR.lib` also omits.

Re-measure (TEMP harness, nothing in the repo; one file per `cl` with
a 120 s timeout each, `<dir>_<file>.obj` naming, flags mirroring
`genbuild.ps1` plus `/FI vc4compat.h /I STL /I vc4compat`):
  `python C:\Users\Roy\AppData\Local\Temp\opencode\vc4check.py`

Or build it declaratively (checked in): `src-0.49.17/Makefile.vc4`
is a plain NMAKE makefile generated from the same `XacRett.dsp`
SOURCE lines (generator: TEMP `genmakevc4.py`; re-run it when sources
are added or removed). After `VCVARS32.BAT` (MSDEV4):
  `nmake -f Makefile.vc4` (default OUTDIR `%TEMP%\vc4build`)
  `nmake -f Makefile.vc4 OUTDIR=C:\vc4out` (redirect outputs)
  ONE command does EVERYTHING: 220 + 49 objects, `UnRAR.lib`,
  `XacRett.dll`.  `unrar` (lib only) and `link` (alias of the DLL
  target) remain as explicit spellings.
Proven: full 220-unit + 49-unit + .res build, zero errors, then a
successful DLL link; an incremental re-run recompiles nothing and just
relinks. `$(UNRARLIB)` deliberately builds into `$(OUTDIR)` - the
checked-in `unrar\UnRAR.lib` is VC6-generated and must NOT be reused.
Both `lib` and `link` glob their objects into a response file: 49 (or
220) literal paths overflow the VC4 command-line limit (U1095).
NMAKE traps hit while adding this: no line continuation inside a macro
or recipe (put each on ONE line), `if not exist` must be tab-prefixed,
macro definitions must appear BEFORE the first rule that uses them
(1.61 evaluates in file order), and `link`/`unrar` must depend on
`dirs` or the obj dirs do not exist yet. `STL/` (STLport 2.033, Nov 1997, SGI-STL-2.03 lineage,
permissive license) is committed at the workarea root so the VC4
path is self-contained; it stays unconfigured and untouched.

Landed in `src-0.49.17/vc4compat/`, all `#if _MSC_VER < 1200`-guarded
and reached only via `/FI`, so VC6 never sees them:
- `vc4compat.h`: `bool`/`true`/`false` as `unsigned char`, AFTER
  windows.h/ole2.h/objidl.h (`#define bool int` collides with
  STLport's `operator[]`); `ULONG_PTR` family; empty `__forceinline`
  (vendored zstd/sevenz C sources use it, C2054 otherwise); STL
  includes under `#ifdef __cplusplus` (without it every .c file
  dies C2282 - C has no namespaces).
- `string`: the subset of `std::string` the tree uses (STLport v2
  ships no `<string>`). Exactly ONE `erase` overload, and NO implicit
  `const char*` conversion (real std::string has none either) - with
  it, every `s[i]` is a 3-way C2666 on VC4 (two member `[]`s plus
  convert-then-builtin-`[]`).
- `basetsd.h`: VC4 ships none (fdi/Fdi.h needs it), `INT_PTR` and
  `UINT_PTR` included (MS FDI.H names them).
- `wincrypt.h`: VC4 ships none (wzaes.c needs it); only the
  GetProcAddress-era types/constants, no link dependency (same as
  the VC6 path).

Portable source fixes this forced (each verified VC6-clean):
- declarations in `if`/`while` conditions hoisted (VC4 C2062;
  `for`-init is fine): IshTool, YncTool, OpenYnc x3.
- `using std::vector;` is ill-formed (C2955 on VC4) - made
  conditional, 12 bare uses qualified (yz32lib).
- `std::string` uses qualified where a TU also has `using namespace
  std;` (XacRett.cpp:17 plus yz32lib.h's `using std::string;` reads
  as C2872 ambiguous on VC4).
- `std::vector<yzFileStat>` needs a default ctor under STLport's
  eager member instantiation (C2512): trivial ones added (yz32lib,
  yz32lib_v1.h).
- `map<K,V>` needs the 3rd arg with this STLport (C2976, its adaptor
  has no defaults): `std::less<K>` spelled out (DmgTool, OpenDmg,
  XacRett).
- this STLport's `search`/`min` take ONE iterator/type (C2665):
  pattern pairs cast to `const BYTE*` (all-ASCII patterns,
  behavior-identical), `min` sides cast to `UINT64`.
- member functions in local classes lifted to file scope (C2601:
  the Dmg/OpenDmg RAII tmp-file structs, same behavior everywhere).
- `__declspec(align(16))` replaced by over-allocate-and-round-up
  (SevenZCore AES state; VC4 has no align).
- `<stdio.h>` where VC6 got it transitively (yz32lib.cpp).
- `<shlwapi.h>` + pragma guarded out (XacRett.cpp; used only via
  GetProcAddress anyway) and a `(DLGPROC)` cast on the info-dialog
  proc (VC4 STRICT-era headers spell HWND incompatibly; ABI equal).
- `::yzDecHead` vs `nsYZ32v2::yzDecHead`: VC4 emits one `__delDtor`
  identity for both when deleted in one TU (C2084, proven by
  TEMP-copy bisection); the v1 delete moved to `yzDestroyDecHead`
  in yz32lib_v1.cpp.
- `std::deque<INDIVIDUALINFO>` needs element comparison (this
  STLport's deque has a MEMBER `operator<`, eagerly instantiated;
  vector's is a non-member and stays lazy): by-filename `==`/`!=`
  `/<` added as MEMBERS (OpenArchive.h) - VC4 has no
  argument-dependent lookup, so globals stay invisible inside STL.
- sevenz `CpuArch.c`: VC4 takes the existing no-cpuid stub branch
  (naked `__asm` is C2400 there); feature queries report 0, handled.

`STL/` stays unconfigured and untouched: `stl_config.h` has no MSVC++
section (SGI default) and `README.VC++` warns about VC++ 4.0
namespaces. Every workaround above lives in our own headers/sources.
UnRAR VC4 port notes (all measured, none guessed):
- the 49-unit set is what `Makefile.vc4` builds; it differs from the
  43 lines in `UnRAR.dsp` (adds blake2s/hash/headers/largepage/motw/
  qopen/rs16/secpassword/sha256/smallfn/threadpool/ui, drops
  rar/int64/recvol/rs/savepos/ulinks).
- `/D UNICODE /D _UNICODE` are MANDATORY on the command line: `os.hpp`
  includes windows.h before its own defines.
- `os.hpp`: VC4 lacks several post-95 SDK headers, so those includes
  are guarded; `crypt.cpp` resolves CryptoAPI via GetProcAddress;
  `filefn.cpp`/`pathfn.cpp` do the same for
  `GetDiskFreeSpaceExW`/`SHGetPathFromIDListW`.
- `vc4compat/new` (forwards extensionless `<new>` to VC4 `new.h`),
  `vc4compat/stdexcept`, and `vc4compat/wstring` (generated from
  `string`, `wchar_t` flavour) were all required by UnRAR.
- **`vc4compat/string` and `wstring`: an empty string leaves `p_`
  NULL, and `operator[]`/`at`/`erase` must all tolerate that.** UnRAR
  does `Cmd->Command[0]` on a default-constructed `std::wstring`; the
  unguarded version is a NULL dereference that showed up as
  `CmdExtract::ExtractArchiveInit` + 14 = `mov ax,word ptr [eax]`.
  VC4 port traps: a ternary yielding a pointer cannot bind to a
  reference return (C2446/C2616), and VC4 will not bind a non-lvalue
  to `T&` (C2616) - use an explicit
  `if (p_) return p_[i]; return nul();` with a function-local static
  null char.
  - **`MainHeader::Reset()` left every POD uninitialised on VC6.**
    `*this=MainHeader()` does not zero POD members when the struct also
    holds members with constructors (`std::wstring`/`RarTime`) - the
    same `T var()` trap as the `yzDestroyDecHead` note above, but here
    it bit upstream code: `QOpenOffset`/`RROffset`/`Locator` and friends
    are only assigned when the corresponding extra records exist. A
    locator record with a zero quick-open offset (like `aida64.rar`)
    never assigns `QOpenOffset`, so `QOpen.Load()` ran on stack garbage
    and the archive listed 0 members with `ERAR_BAD_DATA` - while
    `GPLv2-rar5.rar` worked by luck (its nonzero offset overwrote the
    garbage). Proven with a traced TEMP unrar build (`QOPEN LOAD at
    30`, inside the main header itself) after byte-swap bisection
    against the working fixture. `Reset()` now zeroes the PODs
    explicitly, in the style of `FileHeader::Reset()` (which was already
    safe). The only `*this=T()` left in unrar; `EndArcHeader::Reset`
    and friends use explicit assignments. Rebuild `UnRAR.lib`
    (`buildunrar7repo.bat`, TEMP) after touching any unrar source -
    the lib is tracked, the DLL links it statically.
  - **RAR on Win32s needs an ANSI fallback in two layers.** `os.hpp`
    forces `UNICODE` for the whole unrar build, so every `CreateFile`/etc.
    is really the W entry point - which resolves to a dead stub on
    Win32s. RAR was the only format affected because everything else
    goes through ANSI CRT calls. Two independent gaps, both ASCII-safe:
    - file layer (`unrar/win32scompat.h`, header-only so no build-list
      change): `Win32NoUnicode()` gates on GetVersion high bit SET
      (non-NT) plus major/minor, plus a `CreateFileW`-not-exported
      check, then converts to ACP and calls the A entry point.
      **Win95 belongs in the fallback too** (high bit set, major 4,
      minor 0): it also chokes on the Unicode file APIs.  This was
      believed otherwise here ("Win95 (set/4) and all NT keep the W
      path, whose file APIs work there") and that wrong assumption is
      what made every RAR open fail on Win95 with `ERAR_EREAD` (18,
      i.e. `RARX_FATAL`/`RARX_READ`) while listing nothing.  Fixed in
      `327feb9` by adding the `major==4 && minor==0` arm; the minor
      check deliberately leaves Win98+ on the W path, whose W APIs do
      work.  A note on the two log lines that pointed at it: they came
      out because `Makefile.vc4` had `/D XACRETT_RAR_DEBUG` on
      `CFLAGS` but **not** on `UNRAR_CFLAGS`, so every unrar-side
      `DbgLog` compiled away and the log stopped at our own two lines.
      Keep the flag on both lines when debugging (it is runtime-gated
      on the `XACRETT.DBG` file, so it is safe to leave on).
      GetVersion high bit clear means NT, which keeps the W path (an
      earlier high-bit-clear formulation never fired on the test box).
      The ANSI path itself is proven on modern Windows
      via `XACRETT_NOUNICODE=1` (`runansi.ps1`: all 6 RAR fixtures
      list+extract identically through it). Proven by `win32sprobe.exe` (TEMP-built VC4
      binary, workarea root while needed) whose T1/T7 read `highbit=1
      major=3` and `CreateFileW short -> 120
      ERROR_CALL_NOT_IMPLEMENTED`.
      Pure-ASCII names never touch any NLS API (byte copy).
      Patched call sites: `File::Open`/
      `File::Create` (file.cpp), `MakeDir`/`GetFileAttr`/`SetFileAttr`/
      `RenameFile`/`DelFile`/`DelDir`/`SetDirTime`/`SetFileCompression`/
      `ResetFileCache` (filefn.cpp), `ConvertNameToFull`/`SetCurDir`/
      `GetCurDir` (pathfn.cpp). `find.cpp`/`win32lnk.cpp` are not built.
  - Sequenced debug logging lives behind `XACRETT_RAR_DEBUG`
    (compile-time, default off = zero code emitted; `dbglog.h`). With
    it defined, logging is further gated at runtime on the
    `XACRETT.DBG` flag file in the run directory (env vars are hard
    on Win32s), appending `[#seq] ...` lines to `XacRett.log` in the
    CWD - sequence numbers tell lost writes apart from code that never
    ran. Covers the whole RAR open chain (`Win32NoUnicode` decision,
    `WinCreateFileA` results, `longNameOK` probe, `FindSub5` headers,
    `Rar3Tool::Extract`, `RAROpenArchiveEx` stages + catches).
  - **The thread pool is constructed eagerly and fatally.**
    `CmdExtract::CmdExtract` runs `Unp->SetThreads()` which does `new
    ThreadPool(...)` immediately - inside `new DataSet`, before any
    file is touched. Its constructor `Exit(RARX_FATAL)`ed when the
    semaphore/event creation failed, so on Win32s every RAR open died
    with `ERAR_EREAD` while listing nothing (proven by the sequenced
    debug log: `enter`, then `caught RAR_EXIT 2 Data=NULL`). The pool
    now degrades to inline single-threaded execution instead (`AddTask`
    runs the proc directly, `WaitDone` is a no-op, `NULL` handles are
    never waited on or released meaningfully) - no behavior change
    where creation works. `XACRETT_NOMT=1` forces degraded mode so the
    fallback is verifiable without Win32s (`pooltest.exe`: 100 tasks
    PASS threaded and inline; full RAR battery identical both ways).
      Import delta is exactly `GetFullPathNameA`/`MoveFileA`/`RemoveDirectoryA`
      (everything else was already imported); all exist on Win32s.
    - NLS layer (`unicode.cpp`, `_WIN_ALL` branches only): a dead
      `MultiByteToWideChar` zeroes the output, so the archive path came
      back empty and the open failed before any file call. `CharToWide`/
      `WideToChar` now bypass the OS for pure-ASCII strings (exact: ASCII
      maps 1:1 in every ACP); `wcsicomp`/`wcsnicomp` fall back to the
      manual compare when `CompareStringW` returns 0; `toupperw`/`tolowerw`
      fold A-Z/a-z without `CharUpperW`. Behaviour on NLS-working systems
      is provably identical (ASCII paths are exact, OS path otherwise kept).
  - VC6 port traps hit by the above (all real): `std::string` has no
    `clear()` on VC6 (use `resize(0)`); `std::string FA(buf, n)` is a
    most-vexing-parse function declaration (use `assign`); never name
    a variable `small` (`rpcndr.h` does `#define small char`); the VC4
    `string` shim has `clear`/`assign`/`reserve`/`+=` but exactly one
    `erase` - only `clear()`/`resize()`/`+=`/`assign()` were used.
  - **VC4 miscompiles(?) a >260-byte stack array in `win32scompat.h`.**
    A `char buf[261+]` local inside `WinGetFullPathName` deterministically
    broke overload resolution 500 lines away in `pathfn.cpp`
    (`IsDriveLetter(&Name[4])`, `atoiw(&Name[VerPos+1])` suddenly
    C2664, while `char buf[260]` and every other shape compiled clean -
    bisected down to the array bound, name-independent. Heap (`vector`)
    instead of stack for the 520/1040-byte buffers; do not put big
    locals in headers compiled by VC4.
    `clear()` on VC6 (use `resize(0)`); `std::string FA(buf, n)` is a
    most-vexing-parse function declaration (use `assign`); never name
    a variable `small` (`rpcndr.h` does `#define small char`); the VC4
    `string` shim has `clear`/`assign`/`reserve`/`+=` but exactly one
    `erase` - only `clear()`/`resize()`/`+=`/`assign()` were used.
  - **NMAKE does not track header dependencies.** After editing
    `win32scompat.h`/`kiutil.h`/etc., the VC4 `UnRAR.lib` and DLL kept
    stale objects (`file.obj` older than the header) and silently linked
    the old code. Touch the including TUs (timestamp-only) and rebuild.
  - **`COpenRar::FindSub5` never skipped past file data.** The UnRAR
    API leaves the position at the start of the just-read members
    data, so the next `RARReadHeaderEx` parsed data as a header and
    only single-member RAR5 archives listed correctly. It now calls
    `RARProcessFile(rar5_h, RAR_SKIP, NULL, NULL)` before returning
    each member (a pure seek in `RAR_OM_LIST` mode, so solid archives
    stay fast to list). `aida64.rar`: 0 -> 72 members.
- **yz32lib: v1 and v2 both declare a class called `yzFileDecode`, and
  VC4 mangles enclosing namespaces inconsistently** - the v1 out-of-
  line definitions in `nsYZ32v1` still decorated as GLOBAL, colliding
  with the v2 copies (4x LNK2005). Fixed by renaming the v1 class to
  `yzFileDecodeV1` (3 files: `yz32lib_v1.h/.cpp`, `yz32lib.cpp`), which
  gives the two link identities that differ even namespace-stripped.
  The v1 destroy helper `yzDestroyDecHead` additionally took
  `yzDecHead*`, whose back-reference numbering differed per TU, so it
  never resolved (LNK2001); it now takes `void*` and its definition is
  `YZV1::`-qualified (VC4 also mis-scopes a global-scope free-function
  definition against a namespaced declaration).
- Verified end to end against the VC6 DLL on the same fixtures:
  RAR 1.5/2.0/2.9/4/5 all byte-identical (5/5), the 7z matrix
  118/157 members byte-exact, and a 33-case cross-format sweep
  (YZ1, ZIP m08/09/10/12/14/18/93/98/99, XZ, AES, PPMd, PKZ080,
  pkz090, CrLZH, 7z-zstd) 33/33 pass with 8/8 corrupt-archive negatives
  agreeing and none crashing or hanging.
Its import table can never match official: VC4 has no SHLWAPI.LIB, but
our SHLWAPI use is already GetProcAddress-based, so the VC4 build stays
95/NT4-loadable by design.

## SQX format-2.0 LZ (methods 1-4) -- DONE

`ctab.h/.c`, `uarde.h/.c`, `mm_coder.c`, `sqx_dec.c`.  Verified against
`packed_corpus` with `runmatrix2.py`: **202 member passes, ZERO
BYTES-DIFFER and ZERO MISSING**.  The only non-passes are 15 `REF-AMBIG`
(every `*_v2.0_ultra` = method 7, which the reference itself cannot
extract) and 10 `REF-REFUSES` (`sqxtest/DamagedArchive`).

Format facts (reference is seb3773/Squeez-c, TEMP only):
- fmt-2.0 litlen alphabet is `NC2`=311 and the groups SHIFT: `LEN2_START2`
  262, `LEN3_START2` 270, `LEN_START2` 286.  Symbol 257 (`SYM_COPY1`)
  copies 1 byte at the current distance, and there are 16 length-3
  codes (`Len3Offsets2`/`Len3EBits2`, 0..16384) instead of 15.
- fmt-2.0 has **no symbol shift**: `sym == raw_code`.  v1.1 shifts by
  +1 above 256 and +1 more above 283.  Getting this wrong costs you the
  whole stream, so it is worth stating.
- Length-2 / length-3 matches are fixed lengths (2 and 3), no +1.  The
  repeated-offset group is 258..261 with `k = sym - 257`, and adds +1 to
  the length only when the distance is `> 0x7FFF` (v1.1 uses
  `MIN_LEN3`/`MIN_LEN4` instead).
- Per-block `have16` bit introduces a 16-code low-distance Huffman table
  whose lengths are read with a 4-bit width field.  A dist code with
  `ebits >= 4` reads `ebits-4` raw high bits, then a `low16` Huffman
  symbol, and the distance is `base + (hi << 4) + low + 1`.
- **Dist alphabet is 54 codes and the base table is the 8 MB set, not the
  4 MB set.**  The reference picks base tables from `dic_size` in BYTES
  (`>= 8192` -> 8m), so fmt-2.0's 54 codes line up with the 54-entry
  table.  Pairing the 54 fmt-2.0 codes with our 52-entry 4 MB table
  indexes out of bounds past code 51; `SetBaseDistTabs` now selects
  `BaseDTable_8M`/`BaseDBits_8M` for `uFmt20`.  Codes >= 42 have
  different base/bits in the two tables, so this only shows up on large
  distances (>= ~1.5 MB) -- i.e. not on any current fixture, which is
  exactly why it needs saying out loud.
- MM range-coder initial frequency is **8** for fmt-2.0 (1 for v1.1).
- `DecodeFile` took a 4th arg `lArcerVer`; `uFmt20 = (lArcerVer >= 20)`
  and `sqx_dec.c` passes `ALNode->lVersion`.  All v1.1 len/dist tables in
  our tree are bit-identical to the reference's.

**The actual bug was a macro, not the tables.**  `NEEDBITS(NumBits)` and
`DUMPBITS(NumBits)` in `arcio.h` had no inner parens around the
parameter, so a compound argument reassociated:
`NEEDBITS(uEBits - 4)` expanded to `uBitBuf >> (32 - uEBits - 4)` --
**12 bits instead of 4**.  That silently ate 8 extra bits and corrupted
every fmt-2.0 distance code with `ebits > 4`, and the desync only became
visible ~6000 ops later as a wrong byte at offset 21504.  Fixed at the
call site (route the count through a local) AND the macros plus both
`PEEKCODE` forms in `ctab.h` were parenthesised.  An audit of the whole
`unsqx` tree showed these were the **only** compound-argument call
sites, so the macro hardening is behaviour-preserving for everything
else -- but any future `NEEDBITS(a - b)` would have been silently wrong,
so prefer an identifier.

Traps worth keeping:
- `NEEDBITS(n)` is `(uBitBuf >> (INBUF_BIT_SIZE - n))`, so it silently
  returns far too many bits rather than failing.  When a shift-derived
  value looks impossible, log the shift amount before suspecting tables.
- Bisecting with the reference is fast: `SQX_TRACE=1` makes it print
  `[trace] op: sym=N (bt=..., remaining R bytes)` and, for long matches,
  `[trace] match dist=.. len=.. code=.. ebits=.. base=..`.  Log our own
  `(origsize_left, code)` per op and align on `origsize_left` to find the
  first divergent op -- here op 6180 had the right length but a wrong
  distance, and one op later the `> 0x7FFF` length bump turned the error
  visible.  Trace files are UTF-16.
- `dlltest` extracts **0 bytes and still prints `rc=0`** if the outdir
  does not exist.  Create it first or you will chase a phantom
  regression.
- The matrix harness flattens with `-j`, so nested members collide on
  basename and are reported `REF-AMBIG`; that label also appears for
  every `REF-AMBIG` archive the reference declines to unpack.  Treat
  `BYTES-DIFFER`/`MISSING` as the only real failures.
- Left open on purpose: for **v1.1** the reference would also want the
  8 MB base table for dictionaries >= 8 KB, but v1.1 declares
  `DIST_CODES`=56 against a 54-entry table, and no fixture reaches dist
  code 42, so that change cannot be validated here.  v1.1 keeps the old
  exact-size switch.

## lzip (`.lz`) and `.tar.lz` -- DONE

`ARCHIVETYPE_LZIP` 72 / `ARCHIVETYPE_TLZ` 73 (`NUM_ARCHIVETYPE` 73).  New
files `LzDecode.h/.cpp`, `LzTool.h/.cpp`, `OpenArchive/OpenLz.h/.cpp`,
plus a `CLzTarTool`/`COpenTlz` pair in `TarTool`/`OpenTar` for the tar
form (`.tar.lz`, and the `.tlz` alias lzip itself uses).  Registration
order matters and mirrors LZMA: the tar variant is added **before** the
plain one, because `.tar.lz` is really a tar and the sweep picks the first
`IsType` match whose `Check()` passes.

Format facts (from the lzip spec, cross-checked against an authentic
`nicfix.tar.lz`):
- member = `"LZIP"` | VN(1) | DS(1) | LZMA stream | CRC32(4) | DataSize(8) |
  MemberSize(8) -- **6-byte header, 20-byte trailer**, all little-endian.
- There is **no properties byte and no size in the header**.  lzip always
  uses the default LZMA parameters (lc=3, lp=0, pb=2 -> props byte 0x5D,
  "LZMA-302eos") and always terminates the stream with an End Of Stream
  marker.  Passing the wrong props byte decodes nothing at all.
- DS holds the dictionary size: bits 4-0 are the base-2 log of the base
  size (legal 12..29), bits 7-5 a numerator subtracted as
  `frac * 2^(base-4)`.  Spec example: 0xD3 = 2^19 - 6*2^15 = 320 KiB.
- **The CRC is the ordinary reflected 0xEDB88320 CRC-32 (i.e. zlib's)**;
  lzip only stores it little-endian.  Verified by decoding a real lzip
  file: `zlib.crc32(output)` equals the trailer field exactly.  That check
  matters -- writing and reading your own CRC would happily validate a
  wrong variant, which is the fixture-validates-the-fixture trap.
- Trailing data after the last member is ignored, as lzip does by
  default.  Multimember files are supported.

**Why a dedicated engine instead of reusing `LzmaDecodeStream`:** the
container needs the EXACT compressed-stream length, because the trailer
cannot be located without it -- the member's compressed size is stored
nowhere; only the member size is, in the trailer at the END of the
member.  `LzmaDecodeStream` buffers internally and does not report bytes
consumed, which is also what makes multimember files and trailing data
untractable through it.  `LzDecodeStream` therefore keeps its own buffer
and advances its cursor by the `slen` that `LzmaDec_DecodeToDic` reports.

Traps worth keeping:
- `LzmaDec_DecodeToDic( p, limit, inbuf, &inPos, ...)` takes the available
  input count **IN** through `inPos` and returns the consumed count in it.
  Initialising it to 0 (as one naturally would) makes the decoder see an
  empty input and every member fails; assign `LzAvail()` first.  This cost
  real debugging time and is the same family of bug as the ZIP m95
  `inlen` note below.
- `ULONGLONG` does not exist in the VC4 SDK, so `LzDecode.h` typedefs
  `LzU64` as `unsigned __int64` (the `XzDecode.cpp`/`vc4compat/basetsd.h`
  precedent).  Using `ULONGLONG` compiles under VC6 and breaks VC4.
- **Do not regenerate `Makefile.vc4` with `genmakevc4.py`.**  It emits a
  much smaller makefile that loses the hand-tuned unrar object list, the
  response-file rules and the `clean` section (232 lines dropped).  Add new
  units by hand: one line in the object list plus one rule, mirroring
  `XzTool.obj`.  That is the only divergence from the documented
  "re-run the generator" advice.
- Listing reads the sizes from the **trailer index** (walking backwards
  from EOF, lzip's own distributed index) so it never decompresses; an
  index that does not tile the file exactly (trailing data, corruption)
  reports 0 rather than a guess.  The CRC cannot be shown up front
  because lzip stores it per member, so `dwCRC` stays 0.
- Dictionaries are clamped to 4 KiB..64 MiB, the same house cap as
  `LzmaDecodeStream`.  lzip allows up to 512 MiB, so a stream needing
  more than 64 MiB is a documented gap; it fails loudly via the CRC
  check rather than emitting wrong bytes.

Verified (`lztest.py`, TEMP): an authentic `nicfix.tar.lz` plus generated
fixtures -- empty, 31 B, 3000 B, 346 KB, 2-member, text -- every payload
byte-exact against a liblzma oracle; `.tar.lz` lists identically to the
equivalent plain `.tar` (12/12 entries); 9 corruption negatives (bad
magic/version/DS, truncated, flipped bit, bad CRC, bad data size, bad
member size, header-only) all refused with no output and no hang; VC4 and
VC6 byte-identical; `runnewmatrix.py` unchanged.

## Pre-commit checklist

1. Byte-diff audit clean (no SJIS churn).
2. Full `build.bat` with zero `FAIL` lines + relink.
3. Regression: `runarcwatch.ps1` 18/18 MATCH when ArcTool changed;
   `dlltest list` spot-checks when providers changed.
4. `git status` shows only intended files; commit with `-c` identity.
