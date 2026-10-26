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
- The sample archives the providers are verified against are tracked
  under `fixtures/` (one directory per format, with a README that
  says where each one came from).  The harnesses that drive them
  are in TEMP, not in the tree.
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
- LZH samples: `fixtures/lzh/archives` (all methods incl. pm0-2), with
  the oracle's own expected output in `fixtures/lzh/output`.
  CAB: `opencode\cabtest`, `opencode\isbisect`. SQX: `opencode\sqxtest`.
  ZIP: `opencode\ismedia`.  (`LIBCMT.LIB` and `cpmshar.lbr` are
  referenced by older notes but are not in the tree any more.)
- The fixture files are named by their bare name in the notes below;
  they live under `fixtures/`, one directory per format, and
  `fixtures/README.md` says where each one came from and what it
  proves.
- The GCA format is described in `src-0.49.17/GcaSDK/GCA_FORMAT.md`,
  which was reverse engineered from `gcac.exe` 0.9k and marks every
  point that was verified by running the tool.
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
  higher members untested; the fixtures are in `fixtures/zip/`.
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
- `.cpio.lzma`/`.cpio.xz`/`.cpio.zst` are `CCpioLzmaTool`/
  `CCpioXzTool`/`CCpioZstTool` + `COpenCpioLzma`/`COpenCpioXz`/
  `COpenCpioZst` (`ARCHIVETYPE_CPIOLZMA` 77 / `CPIOXZ` 78 /
  `CPIOZST` 79), the same temp-delegation design over the tar
  providers' decode helpers (`LzmaDecodeFile`/`XzDecodeFile`/
  `ZstdDecodeFile`): decode the whole stream to a fixed-name temp
  file, then run base cpio over it (inherited Check/Extract/List
  just work, full validation reused). Registration sits right
  after each tar twin and before the plain single-file provider
  (`CTxzTool`->`CCpioXzTool`->`CTzstTool`->`CCpioZstTool`->
  `CZstdTool`, `CTlzmaTarTool`->`CCpioLzmaTool`->`CLzmaTool`), so
  pass-1/fallback routing is unchanged: `.tar.*` still hits tar
  first, a non-cpio stream still falls through to the plain
  provider (a valid `.lzma` that is not cpio extracts as LZMA,
  type 65). Also fixed the matching `CCpioTool(const char*)`
  uninitialised-`fp` bug (same class as the `CTarTool` one:
  `fclose` on garbage on first open) -- `fp=NULL` now.
  Fixtures in `%TEMP%\cpiofix\` (hand-rolled newc `tree.cpio`
  over text/binary/zeros/empty-file/subdir + the three compressed
  forms; the cpio tiles exactly per cpio(5) and the non-empty
  members match 7-Zip's own extraction, whose "Headers Error"
  on the zero-size member is its own quirk). Verified 4/4 PASS
  (type 15/77/78/79, every member byte-exact incl. the 0-byte
  file); tar/zip routing spot checks unchanged (`.tar.lzma` 67,
  `.tar.xz` 68, `.tgz` 47, ZIP m10 43); 8 truncation/flip/garbage
  negatives refuse loud with no output and no hang.
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
  Fixtures: `fixtures/rar/GPLv2-rar4.rar`, `fixtures/rar/GPLv2-rar5.rar`
  and `fixtures/rar/rar15.rar`, `fixtures/rar/rar20.rar`,
  `fixtures/rar/rar29.rar` (all tracked).
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
  `fixtures/lbr/crlzh20.lbr`: 30/30 members
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
  RAR, ARC, ACE, SQX, GCA - instead of hanging on the dialog (which is
  what a headless run does today). A GCA key file is given as
  `-p#<file>`, which is hashed with SHA-1 and the hex digest is used as
  the password - the same thing `gcac.exe`'s `-k` does. Verified with
  `%TEMP%\aesfix\` and
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
  authentic 7-Zip 19.00+zstd archive in `fixtures/7z/7z-zstd.7z` (BCJ +
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
    `COpenYz1::CYzEv::MessINPUT` returns the password that came in on
    the command line (and "" when there is none) so **listing never
    prompts**, and a password archive lists as 0 members unless the
    password was given with -p.
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
  - **Re-synced 2026-10-24** from the `yz32lib/` tree in the workarea
    root, which is the other recon project's rewrite of the SDK (it
    targets the ORIGINAL vendor header, so only the decoder *behaviour*
    was taken, never its files).  Two real decoder gaps came out of
    comparing the two implementations over the 176 `.yz1` files in the
    workarea (their `sdktest.exe` is an independent decoder, and
    `yzdec.exe` 1.05a is the oracle):
    - **an archive whose members are ALL empty was rejected outright.**
      `CYz1Tool::Check` demanded `all_size > head_size`, so `yzenc`'s
      `total == head_size` case never even routed to the provider and
      extracted nothing.  `yzdec.exe` writes the empty files.  The test
      is `all_size >= head_size` now (and the file count's third byte
      was read from `head[16]` instead of `head[18]` - a typo that only
      bites a count >= 65536).
    - **a declared `all_size` larger than the members is tolerated by
      both `yzdec.exe` and their SDK** (a mutated header still extracts
      its members).  `YZDecodeImage` now sizes the image by
      `head_size + sum(sizes)` and only requires the members to fit
      inside `all_size`; equality is no longer demanded.  Measured on
      `%TEMP%\opencode\yzbighdr\big.yz1` (all_size says 512 MB, the
      members are 2x1000 bytes): identical output to `yzdec.exe`.
    - Verified: 176 archives, our DLL vs their SDK, 0 differ (7 differed
      before); the 6 key cases (empty, 100 empty, mixed, big, alice,
      yzenc/yzencpw) match `yzdec.exe` file for file.  Two authentic
      `yzenc.exe` 1.05a fixtures were added for this:
      `fixtures/yz1/yzenc-empty.yz1` (2 empty members) and
      `fixtures/yz1/yzenc-mixed.yz1` (5 byte member + 2 empty ones, one
      in a subdirectory).
    - **Not taken, on purpose**: `YZ_MsgTbl.*` (a different message table;
      ours is the small original one), the `pub-` password convention
      (`pub-xxxxxxxx-xx` carries the 4 key bytes and the check byte
      directly - that is a cracker/tooling aid, not a format feature, and
      it would make `-p` ambiguous), the streaming `StreamDecoder` and the
      whole-file API split (architecture, no behaviour gain - their SDK
      reads the file into memory too), the encoder, and their path
      handling: `normalize_path` only maps `/` to `\` and does NOT strip
      `..`, while our `CleanRelName` does (so a hostile member name
      cannot escape the output directory here).
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
- Known gaps: `OpenDummy`/`OpenCompactC` have no live listing path.
  (The GCA formats older than `GCAX`/`GCA3` are decoded since the
  2026-10-24 SDK re-sync - see the GCA section.)
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
    CompressZ, Frz, Gz, Jam, Mime, Packz, UnixDecoder, and the
    COMPRESS.EXE provider) get an explicit `kiutil::pathMake83()` before
    their `fopen`; the derived-name branch
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
  continue) mirrors the reference but is untested. The other gap
  noted here -- fmt-2.0 LZ methods 1-4 failing -- is now FIXED; see
  the "SQX format-2.0 LZ" section below. Method 7 Ultra is DONE
  too; see the "SQX method 7 (Ultra)" section below.
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

**The VC4 link fails in this environment with "Internal error during
Pass1".**  It is a limit of this `link.exe`, not a code problem, and it
is not caused by any change of ours:

- it fails the same way at HEAD with the working tree stashed;
- it fails the same way with and without the objects of the newest
  provider, and it names a DIFFERENT object on every run;
- the same `cl` command, run by hand, always succeeds (five runs in a
  row for the file it names);
- 60 objects link fine (only the expected `LNK2001: XacRett`), 120 and
  more fail, so it is a scale limit.

So the per-file check is the meaningful one here: compile every new
unit with the VC4 flags and confirm the errorlevel is 0, which is how
`DgdDecode.cpp`, `DgdTool.cpp`, `OpenArchive\OpenDgd.cpp` and
`DgcDecode.cpp` were verified.  The full VC4 link works on the
maintainer's machine.

## SQX method 7 (Ultra) -- DONE

`SqxUltra.h/.cpp` + dispatch in `ExtractArcFile` (`sqx_dec.c`), session
reset per archive from `CSqxTool::Extract`. Reference is the `Squeez/`
tree in the workarea root (MIT, never vendored -- the port below is a
clean-room VC6/C89 transcription of its `ultra.c`, verified against
`Squeez/packed_corpus`).

Format (on-disk `0x07`, SQX 2.0 only; see Squeez `FORMAT.md` SS3.6):
LZ77 driven by a probabilistic range coder (TOP `0x1000000` / BOT
`0x10000`, and the range init reads 4 code bytes straight -- NOT the
GFL dummy-byte skip). Binary `is_match` (64 = 4x4x4 state planes) then
`is_rep` (16 = 4x4), both incr 8 / maxf 1023. Repeat path
`RC(len_rep` alphabet 6: `4` = copy 1 byte at current distance, `5` =
repeat last match, `0..3` = reuse MRU slot k = g+1 with length from
`RC(len_sub` alphabet 9 (length base table 2,3,4,6,10,18,34,66,130 with
secondary alphabets 2,4,8,16,32,64,128; `257` escapes to `257 +
raw(14)`, max match 16640). Fresh path: length from `RC(len_new` (same
9-group table), distance class by length (2/3/4/other select the 4
`len_dist` models, alphabet 50), distance symbol (base from
dword_420B70, ebits from dword_420C38; `ebits < 4` appends raw bits,
else `dist = base + (raw(ebits-4) << 4) + RC(dist_low)` nibble model).
Literals: 2 planes x 256 prev-byte contexts, each a 17 x 16-symbol
nibble model (`hi = RC(16)`, `byte = 16*hi + RC(lo[hi])`; plane 0 after
a literal, plane 1 after a match). After any match the prev-byte
context reloads as `dict[(curr-pos - dist) & mask]` (the next source
byte, NOT the last output byte); a solid archive keeps only the
probability models across members (MRU, distances, state, prev byte and
dict position reset per file, like the reference).

Traps, all real:
- **the `*_ultra.sqx` file names lie for small files.** The Squeez
  creator names files by the *requested* method but falls back per
  file, so `hello/code/eng` (+ `nested` member 1) are really method
  **6** (PPMd) and the old path already decoded them -- that is why
  "ultra" looked half working before the port (3/13 rc=0, byte-exact).
  Only 8 members are true method 7 (`biased/zeros/mixed/records/
  pattern/random/image/image24` + 6 more inside `nested`): all failed
  loud (`rc=0x8013`, 0 bytes, no hang) before, all byte-exact after.
  Always check the `cArcMethod` byte, never the file name.
- **`sqx_bits` is the SqxPpmd reader.** The reference `bitio.h` (LE
  word, `>> (32-n)` peek, zero-pad past end) is semantics-identical to
  the `SqxBits*` reader already in `SqxPpmd.cpp`, and `sqx_crc32_raw`
  is the house `crc32` (reflected 0xEDB88320, init 0xFFFFFFFF, no final
  xor -- `WriteOutBuf` already maintains it). The port reuses both and
  streams through `WriteOutBuf` (CRC/progress/volumes/split handling
  come free) instead of buffering the member like the reference.
- Port traps: no `stdint.h` / `static inline` / C99 for-init / mixed
  declarations under VC6 C (house `UWORD32`, plain `static` helpers,
  C89 block-top declarations); `unsigned __int64` is never needed
  because csz/usz above 0x7FFFFFFF are refused up front (house cap, cf.
  `SqxUnPpmd`); the reference `rc_raw` divides by a range that can be
  0 on corrupt input (0xC0000094), so the port returns -1 instead
  (loud refusal, same class as the ARC/YZ1/DGC hardenings).
- The dispatch passes the per-FILE solid bit (`ALNode->lFlags &
  SQX_SOLID_FILE_FLAG`, the same value LZH gets -- Squeez keys its
  model persistence on `b.flags & 0x0004`, not the archive bit), and
  the dictionary needs the 2.0 extended formula (0x0800 set means
  8MB<<idx, else 32KB<<idx; the old LZH call site ignores the ext
  bit, Ultra does not, and anything above 32 MB is refused).

Verified: all 13 `Squeez/packed_corpus/*_ultra.sqx` extract rc=0 and
byte-exact vs `Squeez/corpus` (12 single + the 7-member `nested`; its
`hello.txt` is the 67 B `corpus/nested/dir1/` one, NOT the 63 B
top-level one -- the `-j` harness flattens, so compare by content).
`runmatrix2.py` stays at its 202 baseline with ZERO BYTES-DIFFER and
ZERO MISSING (ultra members remain REF-AMBIG there only because that
harness's old `sqx.exe` reference cannot extract them -- the direct
corpus comparison is the real proof). Corruption negatives
(truncations, payload flips, garbage, a 1 GB-dict header) all fail
loud (rc=1), none hang or crash; a flip inside unconsumed trailing
padding still extracts byte-exact (correct: only `orig_size` worth is
consumed, same trailing-data policy as lzip). VC4 compiles all three
TUs clean (`VC4_RC=0`).

## SQX format-2.0 LZ (methods 1-4) -- DONE

`ctab.h/.c`, `uarde.h/.c`, `mm_coder.c`, `sqx_dec.c`.  Verified against
`packed_corpus` with `runmatrix2.py`: **202 member passes, ZERO
BYTES-DIFFER and ZERO MISSING**.  The only non-passes are 15 `REF-AMBIG`
(every `*_v2.0_ultra` = method 7 -- REF-AMBIG only because that
harness's old `sqx.exe` reference cannot extract them; ours are
proved byte-exact against the corpus directly, see the Ultra
section) and 10 `REF-REFUSES` (`sqxtest/DamagedArchive`).

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

## Provider routing: the two-pass sweep (audit of all 73 providers)

**Read this before adding or reordering any provider.**  The routing rule
is NOT what it looks like, and one provider's `Check` was silently
stealing a dozen others.

`XacRettGetArchiveType` / `XacRettOpenArchive` / `XacRettCheckArchive`
each make **two** passes over `g_pToolList`:

1. first pass: take the first tool whose `IsType(ext)` matches, then
   `break` -- only ONE candidate ever gets its `Check` called;
2. only if that yields nothing: scan the whole list and take the first
   tool whose `Check()` passes.

Two traps follow, both measured:

- **`CTool::IsType` is declared in the base and returns `false`.**  So a
  provider is invisible to pass 1 unless it *overrides* `IsType` --
  and the usual way to find out is wrong, because a naive scan of the
  class body finds the base's declaration and reports every provider as
  visible.  Resolve inheritance and ask whether the NEAREST
  `IsType`-declaring ancestor is `CTool` itself.
- **Doing extension dispatch inside `Check` instead of `IsType` makes a
  provider pass-1-invisible.**  `CSharTool`, `CBelTool`, `CJamTool`,
  `CBooTool`, `CAskTool`, `CUnixDecoderTool` (uue/xxe) all gate on
  `pathExt` *inside* `Check`.  Same test, wrong method: it only runs in
  pass 2, where position decides the winner.

Audit result over the 73 live registrations (74 minus `CPmdTool`, which
is commented out at `XacRett.cpp:649`): **14 are pass-1-invisible, and
12 of them were unreachable.**  `CLzmaTool::Check` was
`hdr[0] <= 224` (nothing but the props byte, because LZMA-Alone has no
magic) and sat at line 568 -- ahead of 12 of them.  Every one of those
12 files started with a byte <= 224, so LZMA claimed them first:

    COMPRESSZ(576) PACKZ(600) MIME(655) UUDECODE(659) XXDECODE(663)
    SHAR(667) AtoB(671) FISH(675) BEL(679) JAM(682) BOO(685) ASK(688)

Measured with the same fixtures against the OFFICIAL binary: official
routes uu/xxe/shar/jam/boo/compressZ correctly and we sent all of them
to LZMA.  **This was a regression WE introduced by adding LZMA** --
official 0.49.17 has no LZMA support at all (it returns UNKNOWN for a
`.lzma`), so there was no upstream behaviour to inherit.  Only
`CGzTool` and `CBz2Tool` escaped, purely because they happen to sit at
532/564, i.e. before 568.

The general lesson: **do not judge a provider's safety by how strict its
own `Check` is.**  What matters is whether some *earlier* lax `Check`
matches its files first.  My first pass got this wrong for CompressZ
and Packz -- their `Check`s are strict (2-byte magic) and it did not
matter, because LZMA's one-byte test matched first.

## LZMA-Alone detection: the real header test (fixes the 12)

`CLzmaTool::Check` now requires a well-formed LZMA-Alone header, not
just a props byte:

    siz >= 13
    hdr[0] <= 224                       ((pb*5+lp)*9+lc)
    dict = LE32(hdr+1);  dict >= 4096 && (dict & (dict-1)) == 0

The power-of-two dictionary is the discriminator, and it is safe
because **the LZMA SDK / 7-Zip encoder rejects a non power-of-two dict**
(`LzmaEncProps_Init`), so no genuine `.lzma` can fail it.  Verified
before shipping, because over-tightening here would be self-inflicted:

- 18 genuine `.lzma`/`.tlz` payloads scanned: all 18 pass (props 0x5D,
  dicts 4096 / 65536 / 8 MB).
- The only rejects are correctly NOT bare LZMA-Alone: `tree.tlz` is a
  *lzip* stream (`"LZIP"|VN|DS` -> dict 0x0150495A) and the six
  `m14_lzma.zip` members are Xtro/PKZIPC-framed (`ver0 ver1 props_size
  props`).  Neither path ever reaches `CLzmaTool`.
- End-to-end against the **authentic xz-5.8.2 test suite** plus our own
  `lzmafix` fixtures and a Python `FORMAT_ALONE` file: 8/8 byte-exact.
  BEL is verified separately against an authentic sample (below).
  The upstream `good-*` files carry their uncompressed size at bytes
  5..12, which gives a free oracle: every one matched that size exactly.

Result: 13 of the 12 previously-unreachable providers now route
correctly and match official.  Note the standalone `.bz2` reorder (see
the "Standalone `.bz2`" note) turns out to be belt-and-braces -- the
Check tightening alone would have fixed `.bz2` too, since a bzip2
stream's bytes 1..4 are `Z`,`h`,9,.. and never a power of two.

**Authentic-fixture confirmation.**  Synthetic probes only prove the
route, so four real files were placed under `fixtures/` and checked
against an independent oracle (the `LICENCE` member of
`GPLv2-rar4.rar`, 18340 bytes, sha256 `6c4beace9ad98f65`, itself proven
equal to `LICENCE.bel`'s output):

| fixture | before this fix | after | result |
|---|---|---|---|
| `fixtures/uu/LICENCE.uue` | type 65 (LZMA), 0 files | type 40 UUDECODE | 18340 B, MATCH |
| `fixtures/uu/LICENCE.xxe` | type 65 (LZMA), 0 files | type 53 XXDECODE | 18340 B, MATCH |
| `fixtures/shar/LICENCE-text.shar` | type 65 (LZMA), 0 files | type 37 SHAR | 18340 B, MATCH |
| `fixtures/shar/LICENCE-uu.shar` | type 65 (LZMA), 0 files | type 37 SHAR | no output |

All three agree **byte-for-byte in VC6, VC4 and official**.  The
"before" column is the whole bug in one line each: every one of these
files used to be claimed by LZMA and yield nothing, and each is now
byte-identical to the oracle.

`LICENCE-uu.shar` (a shar whose payload is uudecode) yields nothing --
but **so does official**, in all three builds.  We decode it anyway
(see the shar section below), which is a deliberate divergence: it is
a feature rather than a defect fix, and the official still produces
nothing.  Recorded so nobody reads the empty official output as the
expected behaviour.

**A second batch of authentic fixtures closed three more.**  Same
oracle (the `LICENCE` member of `GPLv2-rar4.rar`), same three builds:

| fixture | made by | route | result |
|---|---|---|---|
| `fixtures/jak/LICENCE1.jak` / `fixtures/jak/LICENCE2.jak` | Jack32 | 26 JAK | 18340 B, MATCH |
| `fixtures/b2a/LICENCE.B2A` | `BTOA.EXE` | 39 ATOB | 18340 B, MATCH |
| `fixtures/boo/LICENCE.BOO` | `MSBMKB.COM` | 10 BOO | 18342 B, 18340 exact + 2 |

`LICENCE.B2A` matters more than it looks: it starts `xbtoa Begin`, which
is exactly the marker `CAtoBTool` looks for, so **ATOB is verified
end-to-end even though no `.atob` file exists** -- BTOA output *is* the
xbtoa format.  A provider can therefore be certified through a sibling
format that shares its encoding, which is worth remembering before
concluding a format is unprovable.

**JAK is a file SPLITTER, and `CJakTool::Extract` implements the
combine, not a plain extract.**  Given either half it derives the base
name, then walks `1.jak`, `2.jak`, ... appending each part's payload
until a part is missing.  That is why `LICENCE1.jak` and `LICENCE2.jak`
both produce the identical complete 18340-byte file, and it is correct
behaviour -- worth knowing before "fixing" it as a duplicate-output bug.
All three builds agree.

BOO is off by exactly two trailing bytes (`90 90`), identical in ours,
VC4 and official, with the first 18340 bytes byte-exact.  It comes from
the final group `2Y2@`, which the decoder turns into 3 bytes where only
1 is wanted.  **Left unfixed on purpose**: the obvious rule -- leading
character is the group byte count -- is disproved by the *first* group
of the same file, which also starts `2` and does need 3 bytes, so
patching it would need a second sample to avoid breaking valid files.
Documented rather than guessed.

Still routing-only (no authentic sample available anywhere): `JAM`,
`BOO`, `ASK`.  `AtoB`/`Fish`/`Mime` report UNKNOWN, which matches
official, so there is nothing to close there.


## PACKZ: unbounded output on malformed input (disk-fill DoS)

`CPackzTool::Extract` looped `while( s=P_read(...) )` with no bound.
`P_read` only stops when the end-of-block symbol decodes
(`P_readonce` sets `m_eof`), which never happens on a corrupt tree, so
a 62-byte junk `.pkz` wrote **~6 MB/s until the disk filled** --
measured 55 MB in 10 s under VC6 and 56 MB in 10 s under the OFFICIAL
binary, i.e. a shared defect.  Observed as a hang under any watchdog.

This is a *masked* bug, not one the LZMA fix created: PACKZ and CompressZ
both start with `0x1f`, so LZMA's lax `Check` stole every PACKZ file and
the provider was unreachable for any input until this change.

Fix: `PFILE::orig_len` holds the original length, read big-endian by
`read_tree` (`complib.cpp:203-205`), so the loop is bounded exactly
rather than by a guessed compression ratio.  If `total != cap` the
partial file is removed, so a failed extract never leaves a truncated
lie.  Deliberate divergence, same class as the ARC and YZ1 fixes.
Result: `packz.pkz` / `packz_alt.pz` / `xpk3.pk3` now finish in 0.03 s
with rc=0; official still times out on all three.  VC4 agrees.

Remaining, pre-existing and NOT regressions:
- A **synthetic** `.bel` (I wrote `beg^IaGVsbG8=` by hand) returns
  0xC0000005 under **both** ours and official.  That is my fixture's
  fault, not a defect: an authentic `fixtures/bel/LICENCE.bel`
  extracts cleanly and **byte-identically in all three builds** --
  ours (VC6 and VC4) and official all give 18340 bytes, sha256
  `6c4beace9ad98f65...`, matching the `LICENCE` member of
  `GPLv2-rar4.rar`.  So BEL is now verified end to end against an
  authentic sample, and it is one of the 12 providers the LZMA
  `Check` fix brought back; before that it was unreachable.
- A corrupt `.lzma` used to leave a partial file behind, and is now
  FIXED: `CLzmaTool::Extract` calls `DeleteFile` when
  `LzmaDecodeFile` fails, which is what the two tar callers
  (`OpenTar.cpp:301`, `TarTool.cpp:352`) already did -- the standalone
  provider was the only one that forgot.  Measured before/after on
  the `xz-5.8.2` suite: `bad-unknown_size-without_eopm` and
  `bad-too_big_size-with_eopm` go 13 B -> nothing on disk.
  Worth stating precisely, because the other three `bad-*` files
  (`too_small_size` 1/2/3) still produce 12/3/9 bytes and that is
  CORRECT: they decode successfully to their declared size and only
  "fail" upstream because trailing data follows, which we ignore by
  the same trailing-data policy lzip uses.  So "produced bytes" is
  not by itself a failure signal -- compare against the size field.

## BEL: the decoder crashes on malformed input (FIXED)

`CBelTool::Extract` validated **nothing**.  The 8-byte header is
`0..2` uncompressed size (LE24), `3..4` offset of the RLE data (LE16),
`5..7` output extension, `8..` control bytes -- and the code derived
pointers from it immediately:

    int fs2 = bf1[0]+(bf1[1]<<8)+(bf1[2]<<16);
    BYTE *p1 = bf1+8, *p2 = bf1+bf1[3]+(bf1[4]<<8);

`doff` indexes straight into `bf1`, so a corrupt header walked `p2`
arbitrarily far past the allocation.  A **14-byte** junk `.bel`
declaring `doff=18814` therefore read ~18 KB out of bounds ->
**0xC0000005**.  `fs2==0` was equally fatal: `new BYTE[0]` and then an
unconditional `*(p)=*p2` wrote out of bounds.  The loop advanced `p1`
and `p2` with no end-of-input guard at all, and the RLE repeat count was
never clamped to the output buffer.

Fix: reject `fs1<8`, `fs2<=0`, `fs2>16 MB` and `doff<8 || doff>=fs1`;
give `p1`/`p2` an end pointer; clamp the repeat so it can never pass
`bf2[fs2-1]`; clamp `p` before the final store; and check `fread`.
Note `p-bf2` tracks `i` exactly (both advance by the same amounts), so
bounding `i` bounds the write pointer too.

Measured with a 30-case fuzz set (empty, 1-7 bytes, `fs2` 0 / max / 16 MB,
`doff` 7 / == size / past end / 65535, truncated, all-00, all-FF, 12
random blobs, plus the authentic file): **6 cases crashed with
0xC0000005 before, 0 after**, and the authentic `LICENCE.bel` still
gives 18340 bytes.

Worth remembering: three of the `bad` cases still produce output (12/3/9
bytes) because they decode successfully to their declared size.  That is
correct, not a leftover -- the same "produced bytes are not a failure
signal" trap as the corrupt-`.lzma` note above.

## shar: uudecode payloads (FIXED, deliberate divergence)

GNU shar stores a binary member as a heredoc piped into a decoder:

    sed 's/^X//' << 'SHAR_EOF' | uudecode &&
    begin 600 LICENCE.txt
    ...
    `
    end
    SHAR_EOF

`CSharTool::Extract` only understood `sed`/`gres`/`cat`, so this was
silently skipped and **official 0.49.17 produces nothing either** --
matching it was correct, but it is a feature gap, and supporting it means
the extractor recognises the form (not running shell code; the payload
is decoded directly).  Two traps, both hit:

- **The branch must be tested BEFORE the `sed` branch**, because the line
  literally starts with `sed` and was being parsed as a plain sed
  redirection.
- **`SHAR_EOF` appears twice** in the archive: the payload terminator and
  later the `md5sum -c` heredoc.  Stopping at the first is correct.

`getheredoc()` must skip the quote: shar writes `<< 'SHAR_EOF'`, and a
parser that treats `'` as the terminator start returns empty and the
branch never runs.

The decode is a new additive `UnixDecodeUueToFile()` in
`UnixDecoderTool.{h,cpp}`; `CUnixDecoderTool::Extract` is untouched, so
the standalone `.uue`/`.xxe` providers cannot regress.  One trap worth
keeping: `sscanf("begin %o %4999s", ...)` makes **two** conversions and
returns 2, so testing for 1 silently rejected every payload (found by
instrumenting, not by reading).

Verified: `LICENCE-uu.shar` extracts 18340 bytes, sha256
`6c4beace9ad98f65`, matching the `GPLv2-rar4.rar` oracle, in VC6 and
VC4.  Official still produces nothing.

## PACKZ negatives live outside the all-formats sweep

Official 0.49.17 fills the disk on a junk `.pkz` -- we left a **10.5 GB**
`packz.pkz.unpack` behind during the audit, while ours produced 0 bytes.
Any test that runs packz MUST have a hard timeout and a size guard.
The packz fixtures therefore live in `%TEMP%\packzneg\` and are only
scanned by the dedicated watchdog tests (`hangprobe.py`,
`packzgrowth.py`); `%TEMP%\magicprobe\`, which the all-formats routing
sweep walks, no longer contains them.

## ASK/APK: stack overflow and disk fill on malformed input (FIXED)

Fuzzing the three providers the LZMA `Check` fix made reachable found
that **ASK is a memory-safety hole**, and every one of its failures is
**shared with official** (identical exit codes).  `JamTool` and
`BooTool` survived 14 cases each unchanged.

Measured on 42 crafted `.jam`/`.boo`/`.ask`/`.apk` files: **16 problem
cases before, 0 after** -- 15 crashes (13x `0xC0000005`, one
`0xC00000FD`, one `0xC00000FD` **stack overflow**) plus one 39 MB
runaway.  All identical in official except `ask_hdr4.ask`.

The defects, all in `AskTool.cpp`:

- **`Cut()` did `sscanf((char*)(root+p),"%[^\n]",fd_[i].name)` with no
  width limit, into a `char[30]`, on a buffer that had been `malloc`ed
  and `fread` with no NUL terminator at all.**  So the scan read off
  the heap and smashed the stack -- that is the `0xC00000FD`.
- `Cut()` advances `p` by a file-supplied name length and then by a
  file-supplied size, never checking either against `file_length`, so
  every later `root[p]` read is out of bounds.  `fd_` also leaked, and
  the `m_Endflg` early return leaked both `fd_` and `root`.
- **`UnPack()` took `length` straight from the first four bytes of the
  file** and handed it to `malloc`: a **1-byte** `.ask` asked for 4 GB
  and then wrote 39 MB of output.
- `UnPack()` wrote `buffer[0..2]` before knowing `length>=3`.
- `UnPack()` read `root[pr]` with no `file_length` check, let
  `buffer[pb+j]` run past the end, and let `buffer[pb-d+j]` index
  *before* the buffer when the back-reference distance exceeded what had
  been decoded.
- `UnPack()` called `free(root)` on its own failure path while the
  caller freed it too: a **double free**, and in `Cut()` a free of the
  interior pointer `root+p`.

Fix: NUL-terminate `root` (`malloc(file_length+1)`), bound every index,
give `sscanf` a width and bound the copy by hand, validate the
back-reference distance, require `file_length>=7`, drop the inner
`free`, and free `fd_`.  The length is now checked both absolutely
(`<=256 MB`) and against the input (`file_length*4096`, floor 1 MB) --
an absolute ceiling alone still let a 2 KB file write 249 MB.  Same
reasoning as the PACKZ bound.

Also hardened `Extract()`'s extension sniffing, which read
`buffer[8..11]` regardless of `len`.

Verified: 42/42 fuzz cases clean, VC4 `VC4_RC=0` and the formerly
crashing cases now return rc=0/1 with no output.  No authentic `.ask`
or `.apk` sample exists anywhere, so this is verified as *no longer
crashing*, not as *still decoding correctly* -- if one turns up, that is
the remaining gap.

## LZH: the lhasa upstream corpus (209 archives)

`fixtures/lzh/archives\` is the full fragglet/lhasa test corpus
and it is the real oracle for this decoder -- no hand-made fixtures.
lhasa records its OWN expected output per archive:
`test/output/<dir>/<arc>-e.txt` lists the exact member names (first
tab-delimited field) and `-l.txt` gives the uncompressed sizes.  Harness:
`%TEMP%\opencode\lhacase.py` (case-sensitive then case-insensitive),
`lha_ab.py` (ours vs official per case), `lha_size.py` (sizes).

Result, ours vs OFFICIAL over all 209 archives:

Scores AFTER adding OS/2 LV3 + OS/9 LV2 (see the section below);
the "before" column is the same `lha_ab.py` run against the HEAD DLL,
so the delta is like-for-like.

| `lhacase.py` | before | after |
|---|---|---|
| exact member-name match vs lhasa | 137 | **143** |
| + match ignoring case | 52 | **53** |
| genuinely different | 13 | 13 |
| **no output at all** | **7** | **0** |

| `lha_ab.py` (ours vs OFFICIAL) | before | after |
|---|---|---|
| identical to official | 196 / 209 | 189 / 209 |
| differing | 13 | 20 |
| ours matches lhasa, official does not | 0 | **6** |
| **official matches lhasa, ours does not** | **0** | **0** |

The 13 that differ are unchanged by this work, and all of them are
cases where we are **better**:

- **9 `.pma` archives extract fully for us and produce NOTHING for
  official** (rc=1).  PMA support is ours, not upstream's.
- **4 LH4/LHX archives (`lha_amiga_122/lh4*.lzh`,
  `unlha32/*lhx*.lzh`) decode correctly for us and produce 0-BYTE
  files for official** -- sha `e3b0c442...`, the hash of an empty file.
  lhasa's own expected sizes are 18092 and 1241658, which is exactly
  what we produce.  This is an official silent-corruption bug we fix.

The 7 that moved from "identical to official" into "differing" are
exactly the 7 OS/2 LV3 + OS/9 LV2 archives, which official cannot
open at all.  **Zero** cases where official matches lhasa and we do
not, before or after.

Two scoring traps, both worth knowing before reading those numbers:

- **Never compare member names case-sensitively.**  lhasa's expected
  text is lowercase, but the archives store MS-DOS names UPPERCASE --
  verified in the raw header of `lha213/lh5.lzh`, which contains
  `05 47 50 4C 2D 32` (len 5, "GPL-2") and zero lowercase occurrences.
  That alone accounts for 52 of the apparent mismatches.
- **lhasa's own harness skips the symlink archives on non-Unix**
  (`test_extract.sh`: `if [ "$build_arch" != "unix" ] && echo "$1" |
  grep -q symlink; then return; fi`).  We cannot create symlinks, so we
  materialise the target as a real file; 10 of the 13 "genuinely
  different" are that, and they are identical in ours and official.

The 7 OS/2 level-3 (`lha_os2_208/h3_*`) and OS/9 level-2
(`lha_os9_211c/h2_*`) no-output cases are now **DONE** -- see below.
Official still cannot open any of them.

## LZH level-3 (OS/2) and OS/9 level-2 -- DONE

The last 7 corpus gaps.  Both are in `CLzhTool::ReadHeader`
(`LzhTool.cpp`), which is our own parser -- the vendored `lhasa/` is a
**decoder-only** subset (no `lha_file_header.c`, no `ext_header.c`), so
the header layout had to come from the lhasa checkout under TEMP
`lhasa/lib` (the upstream checkout in the workarea root).

Format facts, all verified numerically against the fixtures before a
line of C was written (`%TEMP\opencode\verify_hdr.py` = 9/9 MATCH):

- **Header CRC = CRC-16/ARC (poly `0xA001`, init `0`) over
  `raw_data[0:hdrlen]` with the 2 common-CRC bytes zeroed.**  That is
  lhasa's rule verbatim (`lha_file_header.c:137-144` `check_common_crc`
  plus `ext_header.c:94-102`).  lhasa's `crc16_table` is bit-identical
  to our `crctable` in `LzhTool.h:16-25`, and its init is `0` --
  **`LzhTool.cpp:119` is `sum=0,crc=0`, NOT `crc=0xffffffff`.**  Getting
  that init wrong makes a correct parser reject everything; it cost real
  time here.  The zeroing is handled by the existing
  `crc=tmpcrc; buf[1]=buf[2]=0; UpdateCRC(buf,ehs)` in the `0x00` case.
- **level 1/2**: `hdrlen` = `LE16(buf[0..1])`, ext chain starts at
  offset 24 with 16-bit length fields, base header 26 bytes.
- **level 3 (OS/2)**: `LE16(buf[0..1])` is a *word size* and must be
  4; base header is **32** bytes; `hdrlen` = `LE32(buf[24..27])`;
  ext chain starts at offset **28** with **32-bit** length fields
  (`lha_file_header.c:780-847`, `decode_extended_headers` switches
  `field_size` on `header_level == 3`).  Method/sizes/timestamp/crc/os
  sit at the SAME offsets as level 2 (2/7/11/15/21/23), and byte 20 is
  still the level -- so only the base length, the chain origin, the
  length-field width and the name/path payload size actually differ.
  Name/path payload is `ehs-5` for level 3 vs `ehs-3` for level 1/2.

Three traps, all of which cost real debugging time here:

- **The declared `hdrlen` is a WORD at `buf[0..1]` for level 2 ONLY.**
  In level 0/1, byte 0 is the *base* header size and byte 1 the header
  *checksum* -- `LE16(buf[0..1])` there is garbage.  The first cut
  applied it to all levels and **broke every level 0/1 archive in the
  corpus** (`lhark04d/lh0.lzh`, `maclha_224/l1_lh0.lzh`,
  `lhmelt_16536/h1_lh5.lzh` all went `type=28` -> `type=0`).  The
  `hdrlen-consumed` trailing fold then read garbage.  Worth stating
  plainly: a change that only ever *adds* a format can still break
  unrelated ones, so the 209-archive corpus -- not the 7 fixtures -- is
  the acceptance test.
- **OS/9 headers are one byte longer than their ext chain**, and the
  extra byte IS counted in `hdrlen` (`h2_lh0`: chain ends at 56,
  `hdrlen` 57).  The chain walk alone therefore under-reads by one and
  the header CRC fails.  `Extract` positions with
  `long base = ftell(lzh)` (`LzhTool.cpp:37`), so this is also a
  **payload misalignment**, not just a CRC issue -- the fold is
  required, not cosmetic.
- **Do not trust C instrumentation read through a truncating stderr
  pipe.**  Two rounds of trace probes produced confident, completely
  wrong conclusions here (they appeared to show the tail read failing,
  and then showed a previously-working archive failing).  PowerShell's
  `Select-Object -First N` plus `NativeCommandError` text was eating
  the lines, and one probe crashed the harness (`rc=0xC0000005`) because
  it passed the not-yet-NUL-terminated `buf[2]` to `%s`.  The decisive
  move was to reimplement `ReadHeader` exactly in Python
  (`%TEMP\opencode\lzhmodel.py`) where every byte is visible, and to
  A/B against a stashed pre-patch DLL before believing anything.

Verified: all 7 extract with the exact sizes lhasa records in
`test/output/<dir>/*-l.txt` (6829, 18092, 14, 14, 6829, 18092, 12);
`lhacase.py` no-output 7 -> **0** with no new REAL-DIFF; VC4
(`Makefile.vc4`, `VC4_RC=0`) agrees byte for byte.  Byte-diff audit
clean -- the only touched non-ASCII is the pre-existing SJIS comment
`// LV3 ???` on the `h_Level` line, unchanged.

Note `h3_lh5` scores `ours=DIFF` in `lha_ab.py` purely on the
case trap below (we write `GPL-2`, lhasa's expected text says
`gpl-2`); the size and content are right.

Three further cases (`lha_amiga_122/level1.lzh`, `level2.lzh`,
`sfx.run`) look wrong -- a `-j` flattening artefact produces
`subdir2hello.txt`, and `sfx.run` yields `SFXUsage.txt` instead of
`gpl-2` -- but they are **byte-identical to official**, i.e. shared
behaviour rather than a regression.  Likely an artifact of the `-j`
that dlltest bakes in, so don't "fix" it without checking that first.

## JAM: stored + encrypted (May/96), and what Aug/96 really is

`CJamTool` was a faithful port of `jam/SOURCE/UNJAM0.C` (May/96).  The
`jam/` tree in the workarea root is the upstream package: DOS binaries
in `MAY96/` and `AUG96/`, full C source in `SOURCE/`, the 4k-dictionary
JAMKIT library, and `JAM.DOC` / `SOURCE.DOC` / `CRYPTION.DOC`.

**The `AUG96/*.lst` files are IDA listings of the UNPACKED programs.**
The shipped `.COM` files are PKLite-packed, so their bytes do NOT match
the listing addresses -- searching them for the transform constants
finds nothing.  The mapping is `segment offset = file offset + 0x100`
and it checks out exactly (`add di,0x100` sits at file 0xCE7 =
seg000:0DE7 in `JAM.lst`; `add di,0x101` at file 0xC44 = seg000:0D44 in
`UNJAM.lst`).  `msdos.exe` in the workarea root runs the DOS binaries,
which turns them into an oracle.

Authentic fixtures, all round-tripping exactly under the reference
tools (`%TEMP\jamdos`):

| fixture | made by | header | size |
|---|---|---|---|
| `M_PLAIN.JAM` | MAY96/JAM.COM | `00 00 00` | 946 |
| `M_STORED.JAM` | MAY96/JAM.COM, incompressible input | `01 00 00` | 3003 |
| `M_ENC.JAM` | MAY96/JAM.COM, key `secretpw` | `02 2a 7f` | 946 |
| `fixtures/jam/LICENCE-may96.jam` | shipped sample | `00 00 00` | 8841 |
| `fixtures/jam/LICENCE-aug96.jam` | shipped sample | `00 00 00` | 8835 |

**Stored (header bit 0) and encrypted (header bit 1) are now supported.**
Neither was supported before, in ours or in official 0.49.17, which
emitted 3 and 2 bytes of garbage respectively -- so this is a
deliberate divergence.  `CRYPTION.DOC` is exact, and its 16-bit key CRC
is confirmed by the fixture: `secretpw` yields `0x7f2a`, i.e. bytes
`2a 7f`, precisely `M_ENC.JAM`'s header.  Encryption is a byte-wise XOR
over the bytes leaving the code refiner (`veil` = XOR of key bytes;
per byte `p = ct ^ prev ^ veil ^ key[i]`, then `key[i] += i+1`,
`i = (i+1) % n`, `prev = p`), so it is applied inside `read_bits`
through a new `jam_getc()`.  The password comes from
`CTool::GetPassword()`, so `-p<pw>` works and headless runs do not
hang.  A wrong password fails on the key CRC and deletes the partial
output, which matches the reference tool exactly -- `UNJAM.COM` prints
"Invalid password.", writes nothing, and the key is case-sensitive.
7/7 cases byte-exact against the real tools' output, VC6 and VC4
agreeing, with `LICENCE-may96.jam` still 18340 / sha256
`6c4beace9ad98f65`.  Extract() now parses the 3-byte header instead of
blindly retrying at offsets 0 and 3, and deletes the output on failure.

**Aug/96 is not May/96-compatible and remains unsupported.**  Both
official and we silently emitted 19324 bytes of garbage for
`LICENCE-aug96.jam` while reporting `rc=0`, and that is unchanged: the
format carries **no checksum and no version tag**, so both variants
have header `00 00 00` and both decode to a clean EOF under either
model.  "Fail loudly" is therefore not implementable as a reliable
per-file detector.  What is established:

- May/96 is exact: `SOURCE/JAM0.C` compiled with VC6 reproduces
  `LICENCE-may96.jam` **byte for byte**.
- Aug/96 changes only the LZ78 -> refiner stage; the refiner body is
  bit-identical to May/96 (the `0xFE01` vs `0xFF01` difference in the
  listings cancels against the missing `+base`).  The added layer picks
  a complement using two 32-bit accumulators, both halved when either
  exceeds `0x1170`.
- `JAM0.C:20` starts the ENCODER's `available` at `fst_code` (257)
  while `UNJAM0.C:21` starts the DECODER's at `fst_code-1` (256).  That
  `+1` is why `JAM.COM` computes `comp = available - code + 0x100` and
  `UNJAM.COM` computes `available - raw + 0x101`: both are the same
  quantity, `available_dec + 257 - code` (confirmed in every observable
  case).
- Transcribing `sub_10DCA` / `sub_10C1E` faithfully reproduces the
  first 152 bytes of `LICENCE-aug96.jam` and then diverges by exactly
  **one** accumulator decision.  Against byte-exact oracles (the real
  `JAM.COM` and `UNJAM.COM`, plus an oracle-guided decoder that
  recovers the true code sequence from the known plaintext) no
  variation of comparison direction, complement base `0x0FC-0x104`,
  gate, update mode, halving limit `0x200-0x40000`, halving trigger,
  halving position, or reset policy reproduces it.  Two steps with an
  identical `acc_code - acc_comp` (both -5, at idx 54 and idx 133)
  require opposite decisions, so the deciding state is provably not
  just those two accumulators.  The shipped unpacker *does* read its
  own packer's output correctly -- `msdos.exe AUG96\UNJAM.COM`
  reproduces the 18340-byte original -- so the remaining piece is a
  genuine unknown, not a broken format.

Do not "fix" this by shipping a variant that merely decodes further: a
partial Aug/96 decode still writes wrong bytes.  Treat it as an open
gap, like the LBR whole-compressed libraries.

## UI strings: ARCEXTRACT_WRITEFILE_OPEN moved to a resource

The overwrite prompt in `XacRett.cpp`'s `ARCEXTRACT_WRITEFILE_OPEN`
handler had its message and caption as inline SJIS literals.  They now
live in `XacRett.rc` as a dual-language STRINGTABLE, loaded with
`LoadString(Instance, ...)` -- `Instance` being the `DllMain` global the
tree already uses for dialogs, NOT `AfxGetResourceHandle`.

    IDS_OVERWRITE_MSG      106   "%s\n<jp>"      "%s\nAlready exists. Overwrite?"
    IDS_OVERWRITE_CAPTION  107   "<jp>"          "Overwrite"

Both language blocks carry the same IDs, so Windows resolves per the
user's UI language and falls back to the other.  Measured with a probe
(`TEMP\opencode\strprobe.c`, TEMP only): a 0404 / Traditional Chinese
box resolves to 0409 and `LoadStringA` returns
`"%s\nAlready exists. Overwrite?"` / `"Overwrite"` byte-clean; forcing
`SetThreadUILanguage(0x0411)` resolves to 0411 and returns the Japanese
text.  VC6 and VC4 `rc` invocations are byte-identical
(`/l 0x411 /d NDEBUG` in both), verified by SHA256 of the loaded
resource data.  If `LoadString` returns 0 both call sites fall back to
`"%s"` / `"XacRett"` rather than showing an empty box.

Four traps, all of them real:

- **`resource.h` is included at `XacRett.rc:3`, but the APS "next value"
  defines live inside `#ifdef APSTUDIO_INVOKED`**, which is never
  defined in a build.  Adding an ID next to `_APS_NEXT_RESOURCE_VALUE`
  therefore compiles in Resource Editor and then fails the real build
  with `RC2104: undefined keyword`.  New IDs must go ABOVE that block.
- **A `STRINGTABLE` cannot be named.**  `STRINGTABLE DISCARDABLE "X"`
  is rejected with `RC2105: BEGIN expected in string table`, so the two
  language blocks MUST both be unnamed and be told apart by `LANGUAGE`
  alone.
- **RC stores STRINGTABLEs as Unicode** (length in characters, `len*2`
  bytes), even when every character is ASCII.  Read them as ANSI and the
  first NUL high byte truncates the string to one character.
- **`LoadStringA` converts the Unicode table through the SYSTEM ACP.**
  On a Japanese ACP (932) that is lossless and byte-identical to the old
  SJIS literals, so nothing changes on the intended platform; on a
  non-Japanese ACP the Japanese variant degrades to `?`.  Accepted
  deliberately: the caller is `MessageBoxA` in an MBCS build, and the
  old inline literals were equally unreadable under a non-Japanese ACP
  (as raw mojibake rather than `?`).

## ASD 0.2.0 support (LZ77 + dual adaptive Huffman) -- DONE

`CAsdTool` used to handle **only** ASD 0.1.x, and 0.1.x turned out to
be plain LZSS.  ASD 0.2.0 uses a completely different scheme, so this
was a new decoder, not a tweak.

**Format.**  Header is `"ASD"` + two version bytes (`"01"` / `"02"`) +
`0x1A`.  The **member count is 2 bytes wide in 0.1.x and 3 bytes wide
in 0.2.x** (`asd.c:316`, `unasd.c:121`); that shifts every member
header by one byte, so both the extractor and the lister are
version-aware now.  A member header is `name_len(1) + name +
size(4) + crc(4) + time(4) + attributes(2)`, all little endian.

**0.2.x compression** is LZ77 with a window plus TWO adaptive
Huffman trees (one for literals, one for offsets), not LZSS:

    bit-packed parameter block, MSB first:
      window_bits(4) buffer_bits(5)
      offset_huff_rescale(7) offset_huff_rehuff(7)
      literal_huff_rescale(7) literal_huff_rehuff(7)
    LITERAL_NUM_CODES = 256 + (window_bits-1)*4
    OFFSET_NUM_CODES  = buffer_bits*2
    minhit = 2   (0.1.x uses 3)

The Huffman code is the reference's own two-tree layout: counts start
at 1, the tree is rebuilt by repeatedly merging the two smallest
counts, and the decoder walks down from node `2*NUM_CODES-2` choosing
`high_tree`/`low_tree` on each bit.  A literal code > 255 is a match:
`256..263` give the length directly, `> 263` means a 2-bit selector
plus extra bits; the offset code gives the distance the same way.
`window_bits` and `buffer_bits` are **clamped** (1..15 and 2..24) --
the format allows `buffer_bits` up to 31, which would ask for a 2 GB
window, so a hostile file is refused instead.

**The 0.2.x path verifies every member CRC** (zlib's `crc32`, the same
function the reference uses) and fails loudly on a mismatch, so a
corrupted payload no longer produces silently wrong bytes.  The 0.1.x
path still does not verify -- pre-existing, and it matches official.

Traps, all real:

- **`fread(buf, 1, n, f)` returns the number of ITEMS read, not 1.**
  A guard written as `1!=fread(...)` therefore always fires; it has to
  be `n!=fread(...)`.  This one cost the most time here: the first
  build failed with no output at all and the header parse looked
  perfect.
- **The `version` byte is read at offset 4 but the member count is at
  offset 6**, so the sniff must not consume the 6 header bytes twice --
  the 0.1.x path then has to seek with `SEEK_SET`, not `SEEK_CUR`.
- **`#pragma code_page` / RC are irrelevant here; the ASD streams are
  byte oriented**, but the *0.2.x* stream is bit packed, so the file
  position after a member is NOT byte aligned and the members cannot
  be located by seeking: the whole stream has to be decoded in one
  pass, switching output files at the member boundaries.  The 0.1.x
  path can seek per member because its stream is byte aligned.
- `kiutil::pathMake` rewrites the name buffer IN PLACE, so each member
  needs its own copy of the name.

**Verified** against the reference `unasd.exe` 0.2.0 (built from
`asd_src/`, untracked) and the archiver `asd.exe` 0.2.0:

| fixture | ours | reference |
|---|---|---|
| `fixtures/asd/LICENCE-015.asd` (0.1.5) | 18340 B, sha `6c4beace` | identical |
| `fixtures/asd/LICENCE-020.asd` (0.2.0) | 18340 B, sha `6c4beace` | identical |
| `fixtures/asd/multi.asd` (0.2.0, 5 members) | 480/256/200000/1/700 B | identical, all 5 sha256 match |

The listing matches the reference member for member (names, sizes and
CRCs, e.g. `C60F157A / 29058C73 / 42C68D3F / 8CDC1683 / 098DB714`).
VC6 and VC4 agree byte for byte.

Negatives (all fail, none hang, none crash): truncated archive, header
only, magic only, bad version byte, `buffer_bits` overflow (refused
before any big allocation), a flipped payload bit (caught by the CRC),
trailing garbage (ignored, as the reference does).

## LZH: the -lz5- initial history buffer (upstream lhasa 37a20c6)

`CLzhDecoder2` (our own port of "LHa for UNIX 1.14d") handles -lz5-,
and it fills the 4 KB history window with a fixed pattern that archives
in the wild copy from.  The pattern was **almost** right: the first 18
bytes were left as spaces instead of zeros, so any archive that copies
from the very start of the history decoded wrongly.

The reference decoder (lhasa, commit 37a20c6 "lz5: Fill initial
history buffer correctly", upstream issue #13) fills its 4096 byte
**ring buffer** as: 13 copies of each byte value, then all values
ascending, then all descending, then 128 zeros, then 110 spaces, then
**18 zeros**.  Our decoder uses a **linear** window that is rotated by
`START_OFFSET` (18) relative to that ring buffer, so the ring buffer's
trailing 18 zeros belong at the **FRONT** of our window -- the port
left them as spaces.  Proven exactly: with that one line the window
becomes byte-identical to lhasa's ring buffer (0 differing bytes out
of 4096; before, exactly 18 differed, at indices 0..17, 0x20 vs
0x00).

Verified against lhasa's own crafted test archive
`test/archives/larc333/initial.lzs`, which exists precisely to dump
the initial history buffer (lhasa's expected CRC is 6005):

| fixture | before | after | expected |
|---|---|---|---|
| `larc333/initial.lzs` | C81A | **6005** | 6005 |
| `larc333/lz5.lzs` | A33A | A33A | A33A |
| `larc333/long.lzs` | 6A7C | 6A7C | 6A7C |
| `larc333/lz4.lzs`, `sfx.com` | B6D5 | B6D5 | B6D5 |

The 209 archive corpus is unchanged (`lhacase.py`: 143 exact / 53
case-insensitive / 13 different / **0** no-output) and so is the
ours-vs-official comparison (`lha_ab.py`: 189 identical / 20 differing,
and still **0** cases where official matches lhasa and ours does not).
VC6 and VC4 agree byte for byte.

Two traps worth keeping:

- **The -lzs- sibling really is all spaces.**  lhasa's `lha_lzs_init`
  does `memset(ringbuf, ' ', RING_BUFFER_SIZE)` with no pattern, so
  the blanket `memset(text,' ',dicsiz)` in our port is correct there.
  Only -lz5- has a pattern, and only its trailing 18 zeros were wrong.
  Do not "fix" -lzs- on the strength of the -lz5- finding.
- **The LZH header CRC is 16 bits (CRC-16/ARC), but `dlltest` compares
  a 32-bit zlib CRC.**  So `dlltest` reports FAIL for *every* LZH
  member, including ones that decode perfectly: for
  `lzsfx33.lzs`/`LZSFX.PAS` the extracted file's zlib crc32 is
  E3796208 while the archive's stored CRC-16 is 7CBA, and our output
  matches the stored CRC exactly.  Check the CRC-16, not the harness.

## JAM: Aug/96 support (the reflection layer) and the auto-detect

`CJamTool` implemented only the **May/96** variant of the JAM format.
**Aug/96** is a different scheme and is now supported as well.

**What the difference is.**  The two formats share the header, the
member table and the LZ78 dictionary coder; they differ only by a
**reflection layer** in the dictionary decoder.  When the Aug/96
decoder reads a dictionary code `val` it may replace it with
`available - val + 257` instead, and it decides that with two
accumulated costs: the alternate is taken while the accumulated cost of
the alternatives stays below the accumulated cost of the codes actually
used.  The two costs are halved once either grows past **0x11170**.

That 0x11170 is worth spelling out, because the disassembly does not
show it: `AUG96\UNJAM.lst` has `cmp word_119B4, 1170h`, which is the
**low word** comparison, and separately `cmp word_119B6, 1` on the high
word.  Together they are a 32 bit limit of 0x11170, not 0x1170.  Using
0x1170 scores 4 of 7 archives where 0x11170 scores 7 of 7, so the
distinction is real.

**The auto-detect is a heuristic, and it has to be.**  Nothing in the
archive says which variant was used: both variants write the same
three byte header (`00 00 00` for an unencrypted archive), the two
real DOS tools (`MAY96\UNJAM.COM` and `AUG96\UNJAM.COM`) are not
interchangeable, and every other signal was measured and found
useless:

| signal | result |
|---|---|
| header bytes | identical |
| bytes of input consumed | identical (both read the whole stream) |
| how the decode ends | always a clean EOF |
| output length | the wrong variant can be longer **or** shorter |
| byte entropy of the output | picks the right one 8 times out of 12 |
| share of reflected codes | the two ranges overlap |
| "repeat the last match" codes | fires on a **garbled** decode, so it breaks working archives |

The last one is a trap: it looks decisive on Aug/96 archives
(99% against 1% for a highly repetitive file) but it also fires on the
*garbled* decode of a May/96 archive, because garbage is full of repeat
codes.  Measured on 78 archives it broke 12 May/96 archives, so it was
removed.

What is left is the share of reflected codes, with a threshold of 75%
that sits above every May/96 rate that has been measured, so a May/96
archive is never turned into a broken one.  Measured over 78 archives:
**70 correct, 8 wrong, and every one of the 8 is an Aug/96 archive
that stays undetected** (it keeps the wrong May/96 result it has
always had).  No May/96 archive regresses.

**The limit is worth stating plainly:** the share is not a reliable
discriminator.  For one and the same content the Aug/96 stream can
score *lower* than the May/96 one -- LICENCE.txt compressed by both
gives 36.2% for Aug/96 against 41.6% for May/96 -- so
`LICENCE-aug96.jam` (36.2%) sits inside the May/96 range and is **not**
detected.  The formats are simply not distinguishable from the bytes;
supporting both reliably needs the variant to be selected rather than
guessed.

**Also fixed: `CJamTool` had no `IsType()` override.**  The base
`CTool::IsType` returns false, so the provider was invisible to the
first pass of the routing sweep and a `.jam` file could be taken by
another provider's lax `Check()` -- the LZMA-Alone check accepts
`hdr[0] <= 224` with a power of two dictionary, which `spaces.jam`
happens to satisfy (it was reported as an LZMA archive).  This is the
"pass-1-invisible" trap recorded above.  `IsType("jam")` is now
implemented, so the routing is deterministic.

Verified against the authentic `AUG96\JAM.COM` encoder and
`AUG96\UNJAM.COM` decoder (both buildable from `jam/AUG96`, and
`msdos.exe` runs them): ten Aug/96 archives were generated and each one
was round trip verified with the authentic decoder.  The Aug/96 decode
is byte identical to the authentic `AUG96\UNJAM.COM` output, and the
May/96 path is unchanged.  VC6 and VC4 agree byte for byte.

## JAM: picking the variant with the file extension

The two JAM variants are mutually incompatible and nothing in the
archive says which one was used, so the variant has to be **selected**
rather than guessed.  A library user cannot pass a switch, but a file
can be renamed, so the extension carries the choice:

| extension | meaning |
|---|---|
| `.jam`  | detect it (May/96 first, then the Aug/96 layer) |
| `.jam5` | force **May/96** |
| `.jam8` | force **Aug/96** |

The digit is the month of the JAM release.  `jam/JAM.DOC` gives the
release history and says so in as many words:

    Feb/96:  The debut of jam/unjam. ... Not compatible with the
              previous release.
    May/96:  First public, independent release. ...
    Aug/96:  Slightly better compression rate. Not compatible with
              previous releases.

So the three variants are Feb/96, May/96 and Aug/96, and `.jam2`,
`.jam5` and `.jam8` name them.  Only May/96 and Aug/96 are supported:
the Feb/96 release was "designated to use within its host system,
CCDB", there is no reference implementation of it in `jam/`, and
nothing in the tree claims the extension, so a `.jam2` archive is left
unclaimed rather than mis-decoded.

**The output name does not change when the archive is renamed**, so
`LICENCE-aug96.jam` and `LICENCE-aug96.jam8` both extract to
`LICENCE-aug96`.  `kiutil::getOriginalName` is called with a NULL
extension for the three JAM extensions, which strips the whole
suffix; anything else keeps the old `.unjam` fallback.

Verified on the shipped samples and on the generated corpus:

| archive | `.jam` | `.jam5` | `.jam8` |
|---|---|---|---|
| `fixtures/jam/LICENCE-may96.jam` | match | match | 19058 (forced, as asked) |
| `fixtures/jam/LICENCE-aug96.jam` | 19324 | 19324 | **match** (18340) |

`.jam8` reproduces all seven compressed Aug/96 archives byte for byte,
including `text1k`, which the automatic detection misses.  The stored
archives (the Aug/96 encoder stores small and incompressible files
rather than compressing them) are the same under all three
extensions, because the stored flag is variant independent.  VC6 and
VC4 agree byte for byte.

## LZH: the LArc 7 methods -lz1-, -lz2-, -lz3- and -lz8-

LArc 7 (the overlay that LArc 3.33 loads with OVRINIT) writes members
with four method IDs that this tree did not know:

| method | algorithm |
|---|---|
| `-lz8-` | the same as `-lz5-` |
| `-lz1-` | bit oriented LZSS, 2 KiB window (11 bit position, 4 bit length) |
| `-lz2-` | bit oriented LZSS, 4 KiB window (12 bit position, 4 bit length) |
| `-lz3-` | bit oriented LZSS, 2 KiB window (11 bit position, 5 bit length) |

All four store a two byte **"level field"** where the CRC-16 normally
is, and for them **there is no CRC-16 to check**.

**The header layout differs from the standard LZH level 0 header.**
The level field comes after the file name, and the CRC-16 follows the
level field:

    21        name length
    22- n     file name
    22+n      LArc 7 level field (2 bytes)
    24+n      CRC-16 (2 bytes)
    26+n      the compressed data

so the data starts at `26+name_length` and not at `24+name_length`.
The header size byte still says `len(header)-2`, so the existing
`bshdr = buf[0]+2` arithmetic finds the right place.

**The parameters always come from the level field, never from the
method name.**  The field holds the number of bits used for the
position and the length of a copy operation, and the decoder is
parameterised accordingly:

    size      = 1 << pos_bits
    ce2       = (pos_bits + len_bits + 1) / 9
    threshold = ce2 + 1
    rotation  = (1 << len_bits) + ce2

The classic `-lzs-` is the `pos_bits=11, len_bits=4` case, which gives
size 2048, ce2 1, threshold 2 and rotation 17 -- exactly what the
existing code hardcodes, so the parameterised decoder is a strict
generalisation of it.  The rotation is the same offset the reference
decoder gets from where its ring buffer starts, so the source index is
`(position + rotation) & (size-1)`.

Only one method value is used for the three parameterised methods:
the method name selects the *algorithm*, the level field supplies the
parameters.

Verified against the four archives in the lhasa fork's
`test/archives/larc333/` (generated by its
`test/make_larc7_test_archives.py`, which is a reference encoder and
so the authoritative description of the bit stream).  All four decode
to the same two members and match the generator's payloads exactly:

| archive | HELLO.TXT | DATA.BIN |
|---|---|---|
| `lz1.lzs` | 79 B, sha `9b9c8d77138b` | 1500 B, sha `9c67837e79dc` |
| `lz2.lzs` | same | same |
| `lz3.lzs` | same | same |
| `lz8.lzs` | same | same |

The listing reports `crc: 0000` for them, as the reference does, and
the real CRC-16 for `-lz5-` and friends.  VC6 and VC4 agree byte for
byte, and the 209 archive corpus is unchanged (`lhacase.py`: 143 exact
/ 53 case-insensitive / 13 different / **0** no-output, and `lha_ab.py`
189 identical / 20 differing with still **0** cases where official
matches lhasa and ours does not).

Two traps, both real:

- **A new method value needs `init_getbits()`.**  The decoder's
  prologue initialises the bit reader per method, and a case that is
  not listed simply never initialises it, so the decode reads
  uninitialised bits and produces garbage of the right length.  This
  is the same shape as the "new provider needs an `IsType()`" trap
  above.
- **The LArc 7 header is not the standard LZH header.**  Reading the
  level field from where the CRC-16 normally is, or starting the
  compressed data two bytes early, gives a decode that is the right
  length and completely wrong content -- no error, no crash.

## MSCompress (SZDD/KWAJ) and the Cabinet FDI shim

Two independent things were sorted out here, both about the formats
that used to be handed to a system DLL.

**The Cabinet shim now also tries FDI.DLL.**  `fdi/fdi.cpp` is a
runtime shim: it resolves the `FDI*` entry points at run time so that
`CabTool.cpp` stays written against the FDI interface and nothing is
imported statically.  It used to load **`cabinet.dll` only**, but the
Cabinet interface was first shipped as **`FDI.DLL`** (the Windows 95
era name) and later renamed `cabinet.dll`; both export the same entry
points.  The retro targets have no `cabinet.dll`, so the shim found
nothing there and `.cab` support was dead on them.  It now falls back
to `FDI.DLL`.

**The KWAJ variant was being silently mis-extracted.**  `lz32.dll`
only implements the LZ/SZDD variant.  Handed a KWAJ file it does
*not* fail: `LZCopy` treats the input as uncompressed and copies the
bytes straight through, so the "extracted" file was the *compressed*
data and the extraction still reported success.  `MSCompressTool` now
checks the `LZCopy` result and refuses the KWAJ signature outright,
so the failure is honest instead of a corrupt file.  KWAJ is now
implemented as well: see "MSCompress: our own COMPRESS.EXE decoder"
below.

**SZDD fixtures, cross-validated.**  There were none in the tree, so
five were generated from the format description (COMPRESS.EXE's SZDD
and KWAJ formats) and then decoded through **Microsoft's own
`lz32.dll`**, which makes them an independent oracle rather than a
self-made fixture:

| fixture | raw | packed | result |
|---|---|---|---|
| `fixtures/mscompress/SMALL.TX_` | 26 | 44 | match |
| `fixtures/mscompress/TEXT.TX_` | 672 | 195 | match |
| `DATA.BI_` | 1500 | 1511 | match |
| `fixtures/mscompress/REPEAT.TX_` | 6000 | 728 | match |
| `fixtures/mscompress/ONE.TX_` / `fixtures/mscompress/EMPTY.TX_` | 1 / 0 | 16 / 14 | match |

They are tracked under `fixtures/mscompress/`, each with a
`.expected` file holding the original bytes.  The name derivation works
too: `SMALL.TX_` extracts to `SMALL.TXT`, because the character
missing from the compressed name is stored in the header.

**The format facts the clean-room work needs** (from the format
description, and confirmed by the fixtures):

    SZDD header (14 bytes)
      0..7    "SZDD" signature 53 5A 44 44 88 F0 27 33
      8       compression mode, only "A" (0x41) is valid
      9       the character missing from the end of the file name
      10..13  the length of the file when unpacked (32 bit LE)

    the data is LZSS: a control byte with eight flags, LSB first;
    a set flag is a literal byte, a clear flag is a two byte match
    (absolute window position, 4 bit length).  The window is 4096
    bytes of spaces and the write position starts at 4096-16 (4096-18
    for the QBasic variant).  A match is encoded as
    pos low byte then (pos high nibble << 4) | (length - 3).

    KWAJ header: the same 8 byte "KWAJ" signature, then the
    compression method (0-4), the offset of the compressed data and
    the header extension flags.  The methods are: 0 stored, 1 stored
    and XORed with 0xFF, 2 the QBasic LZSS, 3 the LZ+Huffman scheme
    and 4 MS-ZIP, which is DEFLATE in 32 KB blocks with a shared
    history -- and therefore zlib's problem, not ours.

## MSCompress: our own COMPRESS.EXE decoder (no lz32.dll)

`lz32.dll` is gone from the import table: the SZDD and KWAJ formats
are now decoded by `src-0.49.17/MsCompress.cpp` (with the header in
`MsCompress.h`), a clean-room engine written from the published format
description.  It is used by both the extract path
(`MSCompressTool.cpp`) and the listing path
(`OpenArchive/OpenMSCompress.cpp`), so the two can never disagree.

Why: `lz32.dll` only implements the LZ/SZDD variant, is missing on
the retro targets (NT 3.51 has neither `cabinet.dll` nor `FDI.DLL`
either), and copies unknown input through unchanged -- so a KWAJ file
used to be "extracted" to the *compressed* bytes with a success code.

**The formats.**

    SZDD header (14 bytes)
      0..7    "SZDD" signature 53 5A 44 44 88 F0 27 33
      8       compression mode, only "A" (0x41) is valid
      9       the character missing from the end of the file name
      10..13  the length of the file when unpacked (32 bit LE)

    SZDD QBasic variant (12 bytes)
      0..7    53 5A 20 88 F0 27 33 D1
      8..11   the length of the file when unpacked (32 bit LE)

    KWAJ header (14 bytes)
      0..7    "KWAJ" signature 4B 57 41 4A 88 F0 27 D1
      8..9    the compression method (0-4)
      10..11  the offset of the compressed data
      12..13  the header extension flags
      the extensions follow in the order the flags give: the unpacked
      length (4 bytes), an unknown field (2 bytes), a length + data
      block, the file name (8 characters), the file extension (3
      characters) and a second length + data block.
      Methods: 0 stored, 1 stored and XORed with 0xFF, 2 the QBasic
      LZSS, 3 the LZ+Huffman scheme, 4 MS-ZIP (DEFLATE in 32 KB
      blocks with a shared history, which is zlib's problem).

**KWAJ method 3** is the hard one, and two things about it are worth
writing down because both cost real time:

- The data starts with **six nybbles**; the first five are the
  encoding type of the five Huffman length lists and the sixth is
  padding.  **All six have to be read**, even though only five are
  used -- reading five leaves the whole stream misaligned by four
  bits, which shows up as a plausible looking length list and then
  nothing at all.
- The nybbles all come **before** the length lists, not one nybble
  per list.

The five trees, in order, are 16 match run lengths, 16 match run
lengths after a short literal run, 32 literal run lengths, 64
offsets and 256 literals.  Each length list is encoded in one of four
ways (the "type" nybble): type 0 is a fixed length for every symbol,
type 1 is a one bit selector with a delta of 0 or +1, type 2 is a two
bit selector with a delta of -1, 0 or +1, and type 3 is four bits per
symbol.  The first length of a type 1/2/3 list is always four bits.

A match length symbol of 0 means "a literal run follows": the next
symbol from the literal run length tree gives the number of literals
(plus one), and a run of 32 ends the run.  A match is the offset
symbol (six bits) shifted left by six and ORed with six more bits,
and the length is the match length symbol plus two.

**Traps, all of them real:**

- The name derivation had an **off by one on the extension**: `ext`
  points at the dot, so the two characters of the extension are
  `ext[1]` and `ext[2]` and the name ends at `ext[3]`.  The check
  `!ext[2]` therefore tested the *second* extension character against
  zero and the two character extension table never fired: `SMALL.TX_`
  listed as `SMALL.TX` while the extract produced `SMALL.TXT`.  The
  listing and the extract now share one function
  (`MsCompressMakeName`), so they cannot drift apart again.
- **The QBasic header also starts with 'S'**, so the old code
  (`if( head[0]=='S' )` -> append the missing character) appended a
  byte of the *length* as a file name character.  The missing
  character now comes from the header parser, which only sets it for
  the real SZDD signature.
- `MsCompressReadHeader` leaves the file positioned at the
  compressed data and the extract path uses that position instead of
  re-reading the file.

**Verified** against real files, in both VC6 and VC4, byte for byte:

| fixture | format | result |
|---|---|---|
| `fixtures/mscompress/licence-szdd.tx_` | SZDD | 18340 B, sha `6C4BEACE9AD98F65` |
| `fixtures/mscompress/LICENCE-kwaj2.tx_` | KWAJ method 2 | 18340 B, sha `6C4BEACE9AD98F65` |
| `fixtures/mscompress/LICENCE-kwaj3.tx_` | KWAJ method 3 | 18340 B, sha `6C4BEACE9AD98F65` |
| `SMALL/TEXT/DATA/REPEAT/ONE/EMPTY.TX_` | SZDD | match |

The oracle is the `LICENCE` member of `GPLv2-rar4.rar` (18340 bytes,
sha `6C4BEACE9AD98F65`), so all three real formats are decoded
against a file this tree has already proven.  The SZDD fixtures were
additionally decoded through Microsoft's own `lz32.dll`.

`szMode` is now the real thing: `SZDD`, `SZDD-QB`, `KWAJ-STO`,
`KWAJ-XOR`, `KWAJ-LZ`, `KWAJ-HUF`, `KWAJ-ZIP` (it has to fit in the
eight bytes `INDIVIDUALINFO::szMode` has).

**Still open:** nothing.  All four KWAJ methods and both SZDD
variants are decoded, and there is a fixture for each of them under
`fixtures/mscompress/` (the KWAJ method 4 one is `LICENCE-kwaj4.tx_`).
The Cabinet format is decoded by our own code as well, so a `.cab`
also works on a target that has neither `cabinet.dll` nor `FDI.DLL`
(NT 3.51); see the Cabinet sections below.

## Cabinet: our own decoder (the container, and the none and the MSZIP methods)

`src-0.49.17/CabDecode.cpp` (with the header in `CabDecode.h`) is a
clean-room decoder for the Microsoft Cabinet format, so that a cabinet
can be opened on the targets that have neither `cabinet.dll` nor
`FDI.DLL` (NT 3.51 has neither).  The container, the `none` method and
the MSZIP method are done; the Quantum and the LZX methods are not yet,
so a cabinet that uses one of them is still left to the system decoder
when the target has one at all.

**The container.**  All the multibyte fields are little-endian:

    CFHEADER   the fixed part (36 bytes), then, when the reserve flag
               is set, the sizes of the three reserve areas and the
               header reserve area itself, then the names of the
               previous and of the next cabinet of the set
    CFFOLDER   coffCabStart(4), cCFData(2), typeCompress(2) and the
               reserve area of the folder
    CFFILE     cbFile(4), uoffFolderStart(4), iFolder(2), date(2),
               time(2), attribs(2) and the NUL terminated name
    CFDATA     csum(4), cbData(2), cbUncomp(2), the reserve area of
               the block and the data

The header flags are 1 (the previous cabinet of the set is named), 2
(the next one is) and 4 (the reserve areas are there).  The names come
**after** the header reserve area, not inside it -- a cabinet with the
reserve flag set and no names in the set is the normal case, and the
reserve area is then just vendor data.

**The checksum of a data block** is not a CRC: the 32 bit words of the
data are XORed together, the bytes that are left over are packed into
the last word, and the four bytes of the block header that hold the
two sizes are XORed in as well.

**MSZIP** is a raw deflate stream per block, with the two byte "CK"
signature in front of it, and **the window is shared by the blocks of
a folder** -- that is the whole point of the method, and it is where
the first attempt failed:

- **zlib's `inflateReset` drops the window.**  In zlib 1.2.13 it
  clears `wsize`, `whave` and `wnext`, so the second block of a
  folder cannot reach back into the first one and the decoder stops
  with "invalid distance too far back".  `inflateResetKeep` is the
  one that keeps the history, and that is the one to use.
- Every block is a **complete deflate stream of its own**, so the
  state is reset between the blocks but the window is kept.  (Python's
  `zlib.decompressobj` cannot be used to look at this: it cannot be
  reused after a stream has ended, so it returns nothing at all for
  the second block, which is a property of the Python class and not
  of the format.)

**The system decoder is kept as a fallback**, so nothing that worked
before stops working.  It is only reached when our own decoder refuses
the cabinet.

Verified against Microsoft's own decoder and against the files that
`makecab.exe` produced:

| cabinet | method | result |
|---|---|---|
| `CbsPersist_20261014113909.cab` (Windows) | MSZIP | 20746185 B, byte for byte the same as `expand.exe` |
| `Appraiser_AlternateData.cab` (Windows) | MSZIP | three files, byte for byte the same as Microsoft's `cabinet.dll` |
| `test_MSZIP.cab` (`makecab`) | MSZIP | three files, byte for byte the originals |
| `test_LZX.cab` (`makecab`) | LZX | three files, byte for byte the originals (the system decoder still does this one) |

**Still open:** nothing.  All four methods (none, MSZIP, LZX and
Quantum) are decoded by our own code now; see the two sections below.

**The listing had a parser of its own**, and it only accepted an
archive whose `cbCabinet` field was exactly the size of the file, so
every cabinet that is not exactly one cabinet long listed as empty:
the `Appraiser`, the `DesktopTargetCompDB` and the `FoDMetadata`
cabinets of this machine all listed 0 members while the extraction
worked.  The listing now uses the same parser as the extraction
(`CabReadHeader`), so the two can never disagree.

The window sizes are covered by the fixtures: `fixtures/cab/` has one
cabinet per LZX window size (15 to 21) and one per Quantum window size
(15 to 21), and all of them give the same 18340 bytes as the `LICENCE`
member of `GPLv2-rar4.rar`.

## Cabinet: the LZX method (our own decoder)

`src-0.49.17/CabLzx.cpp` (with the header in `CabLzx.h`) is a
clean-room decoder for the LZX method of the Cabinet format.  The
stream is a bit stream divided into frames of 32 KB, and each frame is
divided into blocks, and every block starts with its type (1 is the
plain one, 2 is the one with its own little tree for the offsets and 3
is the one that is stored as it is) and its length.

The trees are read with a "pre tree", whose twenty code lengths are
stored in four bits each, and which then says how the code lengths of
the real tree are coded: a symbol of 17 is a run of zeros, 18 a longer
run of zeros and 19 a run of the length of the symbol before.

Match offsets are not coded as they are: a "position slot" and a few
extra bits give the offset, and the three offsets that were used last
are kept for the three first slots.  The base of every slot is worked
out from the rule in the reference, so the table of 290 numbers does
not have to be a list.

The "intel" translation of the x86 call instructions is undone a frame
at a time, and the last ten bytes of a frame are left alone.

**Two traps, and both of them cost real debugging time:**

- **The bits of the stream come from the top of sixteen bit words, and
  the two bytes of a word are in the file in the other order.**  The
  first byte of the file is the LOW byte of the word, so the word has
  to be put together as `(second << 8) | first`.  Reading the bytes
  as they are shifts the whole stream by eight bits, and then every
  block header reads as a type that does not exist (5) with a length
  of twelve million.  The Quantum method reads its words the other way
  round, so the two methods cannot share the reader.
- **The window position has to be moved on by the length of the
  match, and the copy loop has to use a copy of that length.**  Using
  the length after the copy loop has counted it down to zero makes
  every match produce nothing, and the frame then comes out far too
  short.

Verified against Microsoft's own decoder and against the files that
`makecab.exe` produced:

| cabinet | result |
|---|---|
| `fixtures/cab/LICENCE-lzx.cab` | 18340 bytes, the same as the `LICENCE` member of `fixtures/rar/GPLv2-rar4.rar` |
| `DesktopTargetCompDB_Neutral.xml.cab` (Windows) | 422698 bytes, the same as Microsoft's `cabinet.dll` |
| `FoDMetadata_Client.cab` (Windows) | 338 files, 2677274 bytes, the same as Microsoft's `cabinet.dll` |
| `test_LZX.cab` (`makecab`) | three files, the same as the originals |

## Cabinet: the Quantum method (our own decoder)

`src-0.49.17/CabQuantum.cpp` (with the header in `CabQuantum.h`) is a
clean-room decoder for the Quantum method, the last of the four.

Quantum is an arithmetic coder: a symbol is read from one of eight
models, and the models are kept up to date as the symbols come.  A
literal is coded with one of four models of 64 symbols each, and a
match with one of three models, the last of which codes the length as
well.  The models are halved and put in order again when their
frequencies have grown too big, which is the part that has to be done
exactly the same way as the writer did it, or the stream goes out of
step.

The bits come from the top of sixteen bit words here too, but the two
bytes of a word are in the file in the same order, so a word is read
as they are.

The data of a folder is a series of frames of 32 KB.  At the end of a
frame the bits are put back in line with the bytes, and the padding
that follows is skipped by looking for the byte 0xFF, which the writer
puts at the end of every block.  That is why the Quantum path of the
folder reader has to hand the decoder that byte.

| cabinet | result |
|---|---|
| `fixtures/cab/LICENCE-quantum.cab` | 18340 bytes, the same as the `LICENCE` member of `fixtures/rar/GPLv2-rar4.rar` |
| `fixtures/cab/LICENCE-mzsip.cab` | 18340 bytes, the same |

## GCA: the new SDK (the GCA3 format and the encrypted archives)

`src-0.49.17/GcaSDK/` now holds a decoder that was written from the
format description in `GcaSDK/GCA_FORMAT.md` (the reverse engineering
of `gcac.exe` 0.9k, with the verified points marked as such).

What it brought:

- **The `GCA3` container.**  The old SDK only knew the `GCAX` one, so
  **90 of the 145 sample archives now extract** where none of them did
  before -- and none of the 55 that worked changed (0 regressions).
- **The encrypted archives**: the cipher, the key derivation and the
  encrypted directory.

The password is taken the same way as everywhere else: `-p<password>`
on the command line, and a key file as `-p#<file>` (the key file is
hashed with SHA-1 and the lower case hex digest is used as the
password, which is exactly what `gcac.exe`'s `-k` switch does;
`-p##text` means the literal text).

The SDK does **not** ask for the password: it reports that the archive
needs a key and stops, so the provider has to hand the password over
*before* the archive is read.  The order is the same as the YZ1
provider uses, and it matters: the archive is first read **without** a
password, so that a plain archive never opens the password dialog; only
when it turns out to be encrypted is the password that came in on the
command line tried (it never opens the dialog either), and only when
that is missing or wrong is the user asked.

The check that decides whether a file is a GCA archive at all uses only
the password that came in on the command line (never the dialog,
because the check runs while the archive is being routed) and falls
back to the signature, since an encrypted archive has no readable file
list at all.  The listing behaves like the YZ1 one: it uses the
password that came in on the command line and never asks, so an
encrypted archive lists as 0 members unless the password was given
with -p.

A wrong password fails loudly: the provider remembers that the archive
was encrypted and the extraction returns false instead of quietly
producing nothing.

Two things were needed to make it build:

- `GcaSDK/gca_crypt.cpp` and `GcaSDK/gca_sha1.cpp` had to be added to
  `XacRett.dsp` and `Makefile.vc4`; without them the link failed with
  seven unresolved externals.
- `gca_arc.cpp` used `vector::assign` with an iterator range, which the
  1997 STLport of the VC4 build does not have; the copy is done by
  hand now.

Verified: the plain, the password and the key file archives of the
`ctest` corpus give the same 2537600 bytes in the VC6 and the VC4
build, and the whole corpus of 145 archives shows 0 regressions against
the old SDK.

**Re-synced on 2026-10-24** from the `gca` project, which owns this SDK
(the copy in the workarea root `GcaSDK/` is that project's tree).
`gca_arc.cpp` and `gca_api.cpp` gained:

- **the legacy `GCA0`/`GCA1` containers**, header variants 2 and 3: the
  rotated 32 byte entries (an entry carries the metadata of the
  *previous* member and the CRC/sizes of the *next* one), the 12 byte
  first-member record, the trailer CRC over everything up to its four
  zero bytes, and **encrypted version 6** (`+0x10..hdr-1` under
  `<password>Header`, a member's payload under `<password><stored
  name>`, a solid payload under `<password>`);
- **directory members are created as directories** - a member with
  attribute bit `0x10` used to be written as an empty file;
- the **`GCA3` directory CRC is validated**
  (`gcaDirCrc(hdr, flags, body, bodyLen, numFiles)` against `+0x14`);
- a **solid archive whose first member is empty** decodes, which is why
  the guard in `gcaDecodeFile` is `e.expSize == 0 && !solid` and not
  the plain `e.expSize == 0`.

Measured on the legacy fixtures (now in `fixtures/gca/`):
`gcatxt.gca` (GCA0), `gcasdk_v09d.gca` (GCA1 solid) and
`gcacv09e_{plain,solid,none}.gca` already decoded before the re-sync,
but `gcacv09e_encrypted.gca` (`-p fixture`),
`gcacv09e_encrypted_solid.gca` and `gcacv09e_encrypted_key.gca`
(`-p#gcacv09e_key.bin`) produced **0 files**.  All eight now extract
byte-identically to `gcac.exe` (and the modern side is unchanged: the
17 extractable `fixtures/gca` archives plus the two key-file ones match
`gcac.exe`, `gca_compare2.py` reports 0 regressions).  An empty
directory member now comes out as a directory, checked against
`gcac.exe` on a folder archive.

**When re-syncing, re-apply the VC4 rule**: `std::vector::assign(first,
last)` does not exist in STLport 2.033, and the new code uses it twice
(`out` in `gcaDecryptImage`, `arc.plain` in the version 6 path), so
both are a hand `clear`/`resize` + `memcpy` here.  Nothing else the new
copy needs is VC4-hostile (`VC4_RC=0` for both units).

Do **not** re-apply the old `plainFlags` clearing from
`gcaDecryptImage`: clearing the crypto bits in the decrypted image
changes the input of the new `GCA3` directory CRC, and an encrypted
modern archive then refuses to open.  Measured with a probe build of
the new SDK that has the clearing added: `g_pw.gca` and `e_my.gca`
return `OPEN_FAILED` where the unmodified build returns `EXTRACT_OK`.

## The command line password, and the listings

`-p<password>` on the command line sets one static,
`CTool::m_pPassArg`, and `CTool::GetPassword()` returns it without
opening the modal dialog, so a batch or headless run works for every
provider that asks for a password.

`CTool::GetPasswordArg()` is the same thing without the dialog: it
returns the command line password or NULL, and it never prompts.  The
listing paths use it, because a listing must never block on a dialog:
the YZ1 listing hands it to the engine's `MessINPUT` hook and the GCA
listing hands it to the SDK as the archive password.  So an encrypted
archive lists as 0 members when no password was given, and lists its
members when one was -- with no prompt either way.

## DGC (the ".dgc" archives of the DGC Archiver)

`ARCHIVETYPE_DGC` 74 (`DgcTool.cpp` + `OpenArchive/OpenDgc.cpp`, with
the engine in `DgcDecode.cpp`).  The format is described in
`dgca/NOTES-dgca-format.md`, which was reverse engineered from
`dgcac.exe` 0.9k.

The container is a tree of chunks: a 32 byte header (the id, the
header size, the payload size, two reserved words, the flags, the
alignment, the CRC-32 of the payload and the CRC-32 of the header with
the last field zeroed) and a payload that is the concatenation of the
children, each padded to the next 16 byte boundary.  The tree is

    DGCA
    +- DATA    the file payloads
    |   +- DGCC   the compressed blocks
    +- INFO    the metadata
        +- IARC   the archive index (80 bytes)
        +- IFDT -> DGCC   the index table (64 bytes per entry, transposed)
        +- IFNM -> DGCC   the name list

The compressed blocks are a context mixing entropy coder, an LZ stage
that permutes the token buffer and an LZ77 with an implicit distance,
in that order; the "column" method splits the data into interleaved
columns that are delta transformed and compressed one by one.

**Two things are worth knowing about the index.**

- The **"compressed size" and the offset of the index are not to be
  used**: they differ between a stored and a compressed archive of the
  same files.  The records are laid out one after the other in the
  data area and every one of them is as long as the entry's original
  size, so that is how the data is found.  The index's own fields are
  still shown in the listing, because that is what the tool prints.
- The **CRC-32 in the index is the oracle**.  The reference unpacker
  that comes with the recon is an older build and it decodes the
  compressed blocks wrongly -- it prints `MISMATCH` against the index
  for every compressed archive -- so the index is what the extraction
  is checked against.

The names are UTF-8 and the paths use backslashes, so they go through
`kiutil::utf8toAnsi` and `kiutil::pathMake` like the other providers.

**The encryption is supported** (`src-0.49.17/DgcCrypt.cpp`).  An
encrypted archive has every `DGCC` record encrypted - the 32 byte chunk
header AND the payload - while the outer chunk headers (the `DGCA`, the
`DATA`, the `INFO`, the `IARC`, the `IFDT` and the `IFNM`) are in the
clear, so the tree can still be walked and only the records have to be
decrypted one by one.  Two keys are used, and both are stretched over
39 rounds before they seed the MT19937 of the key stream:

    the chunk headers:  SHA-512( 8 zero bytes | password )
    the payloads:       SHA-512( the plaintext header | password )

The cipher is a self inverse XOR stream with one feedback byte.  Three
traps, all of them real:

- **the SHA-512 is the standard one, but its state is byte reversed.**
  `getState` returns the 64 raw bytes of the state with every 64 bit
  word's low dword first, so the digest comes out reversed per 8 byte
  group.  The padding's length field is **16** bytes wide (the high
  dword is zero), which for a short message is the same as the
  standard 8 byte field.
- **the "scratch" buffer of the key stream overlaps the upper half of
  the 128 byte rolling state**, so every feedback byte is stored
  TWICE and the hash of the rolling state sees it twice.  Getting this
  wrong breaks the key stream from the third prng call on - the first
  32 data bytes still match.
- **the decrypt's feedback byte is the ORIGINAL byte, not the
  decrypted one.**  The encrypt feeds back `plaintext ^ k1` and the
  decrypt feeds back `ciphertext ^ k2`, and those are the same value,
  so the cipher is its own inverse - but only if the feedback is taken
  from the input, not from the output.

The password comes from `-p<password>`, and a key file from
`-p#<file>`: the file's content is hashed with SHA-512 and the hex
digest of the hash is the password (the same thing the archiver's `-k`
switch does).  The listing uses the command line password only and
never asks, so an encrypted archive lists 0 members without one; the
extraction asks for it (like the YZ1 and the GCA do) and a wrong
password fails loudly with 0x8003.

**The recovery data is supported too** (a `DGCR` chunk).  A `.dgcr`
file is an archive with the recovery data appended, so the provider
takes it as an archive (`IsType` knows the extension) and uses the
recovery data on the fly.  The `DGCR` chunk holds an `INFO` chunk (the
parameters) and a `DATA` chunk with a 2 dimensional parity code over
the archive's 4 byte words:

    the archive = pages x rows x 256 columns of 4 byte words
    part 1 (rows*cols words): for every (row, column) the XOR of the
            words of all the pages
    part 2 (pages*rows words): for every (page, row) the XOR of the
            words of all the columns

The column parity locates the damaged words and the row parity says
which page they are in; the `INFO` chunk holds the archive's checksum
(the bytes in front of the `DGCR` chunk), which is the final check.  The
parity table is encrypted with the fixed key of the cipher (the
password `SecureCryptograph` and 8 zero bytes) and the three parts are
encrypted one after the other with the same cipher object, so the key
stream runs over the whole recovery data.

Two things are worth knowing:

- **the recovery data covers the archive AS IT IS STORED**, i.e. the
  encrypted bytes of an encrypted archive, so the repair has to happen
  BEFORE the decryption.
- **the repair runs before the checksums are read**, because a damaged
  word can be a checksum field - the `DGCA` chunk's own CRC is checked
  right after the repair and passes again.

**The column method** (the tag 2 blocks) is supported, and it hides a
trap that cost a real bug here: the block is split into `stride`
interleaved columns (column k = the bytes at k, k+stride, ...), every
column is delta transformed on its own (order 0/1/2, the flags' bits
16..19) and coded as its own tag 1 block with its own flags and
params.  **The columns of the last row are SHORTER when the size is
not a multiple of the stride** - the tool gives stride 12 to a 131072
byte block, which is 8 columns of 10923 bytes and 4 of 10922 - so the
de-interleave has to walk every column to its own length:

    out[i*stride + k] = col_k[i]      while i*stride + k < total

Walking `total/stride` rows instead drops the tail of the longer
columns: the output is `total % stride` bytes short and those bytes
stay zero, so the member's CRC fails.  It stayed hidden because the
tool's 1441792 byte blocks end on all-zero data (`zero.bin`), where
dropping zeros into zeros is invisible.  `fixtures/dgc/dgca110-column.dgc`
is the minimal authentic case: one 131072 byte `mat16.bin`, stride 12,
`total % stride` = 8, made with `dgcac.exe a dgca110-column.dgc mat16.bin`.

**The match coder** (the params' bit 1, `DgcCmMatch`) is implemented
too, so an older note here that called it "not implemented" is
retired.  The tool switches it on for very uniform columns
(`spread(delta1) < 5`); it walks the same 9 bit tree as the literal
coder but updates `lit[]` in the main loop instead of using the
prepare/restore passes.  It is exercised by the recon's `all.dgc` /
`fl2.dgc` blocks (`params` `46443322`), which decode byte exact.

One more correction to the format notes: **the recovery table's part 3
is not used by either reader**, and the recon's own notes disagree
about what the writer puts there (the row block offsets in section 7.1,
the CRC-32 of every row in section 12.5).  Nothing depends on it.

**Malformed input is refused, not followed.**  A corrupt chunk header
could declare a payload far larger than the file, and the walk (and its
CRC-32) followed it straight out of the image: an 1808 byte `.dgc` whose
root payload size says `0x7FFFFFF0` crashed the DLL with 0xC0000005 in
BOTH listing and extraction.  A chunk is now required to fit inside its
parent (and the parent inside the file) before it is recorded, the chunk
CRC refuses a chunk that runs past the image, the DGCC child walk
bounds its payload the same way, and the recovery table's
`rows*cols + pages*rows + rows` size check is done in 64 bits so a
corrupt `DGCR` header cannot overflow it.  A one byte truncation used to
extract "successfully" from a one byte out-of-bounds read (the `IFNM`'s
trailing padding) and is now refused as well.

Two more of the same family, both worth remembering because the check
looks right when you read it:

- **a size check of the form `p + 32 + ps > end` can be defeated by the
  wrap.**  With `ps = 0xFFFFFFE0` the sum comes back to `p`, the loop
  never advances and the DLL spins forever - the top level `DGCR` scan
  and the `.dgd` chunk walk both did this on a 1808 byte file.  The
  comparison has to be the subtraction form, `ps > end - p - 32`, which
  cannot wrap.
- **the `.dgd` part count is a file value** and `DgdCombos` indexes
  `g_combo[ncombo][part]`, whose row is `DGD_MAX_PART` (64) long, so a
  `.dgd` claiming more parts wrote past the row; it is refused now.

**The spanning files are supported too** (`ARCHIVETYPE_DGD` 75,
`DgdDecode.cpp` + `DgdTool.cpp` + `OpenArchive/OpenDgd.cpp`).  A file
that is too big for a single archive is split into parts, and every part
is a chunk tree of its own:

    DGCD  (the root)
    |-- INFO  (116 bytes)
    |     +0x00 u32  the flags, bit 0 = encrypted
    |     +0x04 u32  the index of this part
    |     +0x08 u32  the number of the parts
    |     +0x10 u32  the size of this part
    |     +0x18 u32  the size of the whole file
    |     +0x20 64B  the SHA-512 of the whole file
    |     +0x60 u32  the length of the name
    |     +0x64      the name (UTF-8)
    |-- DFDT  (32 bytes)  the date and the time of the file
    `-- DATA  (this part's bytes, not compressed)

The parts are named `<base>%04d.dgd` and a `.dgdr` has the same
structure with a `DGCR` chunk (the recovery data) appended.  The parts of
two different files can lie in the same directory, so the parts are
told apart by the SHA-512 of the whole file and by the number of the
parts, and the combinations whose part sizes add up to the size of the
whole file are tried one after the other until the SHA-512 of the
reassembled file matches.

The encryption is the one of the archives, but the keys come from the
material of the INFO chunk:

    the 92 bytes of the material at INFO+4: SHA-512( 8 zeros | password )
    the name, the DFDT and the DATA:
            SHA-512( the plaintext 96 byte material | password )

and the name region, the DFDT and the DATA each get their own key
stream.  The password comes from `-p<password>` and a key file from
`-p#<file>` (the hex text of the SHA-512 of the file, like the GCA).

The recovery data of a `.dgdr` covers the bytes of the `.dgdr` in front
of its `DGCR` chunk, so a damaged part is repaired before it is used.

Verified: the six files in `fixtures/dgd/` (the plain pair, the pair
with the recovery data and the encrypted pair) all give the same
86016 byte file, byte for byte, in the VC6 and the VC4 build.  The
control: the same damage in a `.dgd` is NOT repaired (the extraction
fails with 0x8003 and writes nothing) while the same damage in a
`.dgdr` is repaired by the parity table and gives the byte-exact file.

Verified: the twelve archives in `fixtures/dgc/` give every file
byte for byte identical to the one the recon's `dgcunp.c` writes,
`names.dgc` included (5981 entries and their Japanese names), in the
VC6 and the VC4 build.  `dgca110-column.dgc` is the column method case
above: before the de-interleave fix it extracted NOTHING while `dgcunp`
gave the 131072 byte file.  The encrypted one
(`dgca110-password.dgc`, the password `password`) gives a file that is
byte identical to the one of the plain archive of the same tree, and
the archives that the archiver's command line tool makes with `-p` and
with `-k` decrypt the same way.

The recovery data is verified with a control: the same damage in the
data area, the index table, the name list, the archive index and a
chunk header is repaired by the `.dgcr` file (all five give the
byte-exact file) and is NOT repaired without it (all five fail), in
the VC6 and the VC4 build.

## ALZ (ALZip) -- the 8.0 container, split volumes and ZipCrypto

The ALZ provider was re-cut onto a shared chain reader.  `AlzDecode.h/.cpp`
(ASCII, new) owns the volume stack and the member walk; `CAlzTool::Extract`
(`AlzTool.cpp`) and `COpenAlz` (`OpenArchive/OpenAlz.cpp`) both drive it, so
listing and extraction can never disagree.  Reference is seb3773/alzgg
(`alzgg/`, external tree, never vendored) plus its `FORMAT-ALZ.md`; the 86
archives of `alzgg/packed_alz_8.0/` are the fixtures.

**Format, verified byte by byte on the corpus:**

    8   "ALZ\001" + 4 bytes (byte 6 = volume index, not used)
        member: "BLZ\001" | name_len u16 | attr u8 | dos_time u32 |
                fd u8 | 1 unknown (0)            <- 9 fixed bytes
        when fd & 0xF0:  method u8 | 1 unknown | crc32 u32 |
                         csize nb | usize nb      (nb = fd>>4, 1/2/4/8)
        name (code-page bytes, NOT NUL terminated)
        12-byte ZipCrypto verifier when fd & 1, then csize bytes of data
    16  "CLZ\001" + 8 zero + "CLZ\002" (last volume) / "CLZ\003"

Data region is `[8, filesize-16)` per volume; continuations are
`<base>.a00`, `.a01`, ... (`.a00` is volume 2) and are not named in the
archive.  `fd` bit 0 = encrypted; high nibble 0 means no optional part, no
data, and the name starts right after the fixed part (empty files and
directory entries).

* `crc32` is over the UNCOMPRESSED data, and `csize` EXCLUDES the 12-byte
  verifier -- `Extract` does `csz += 12` before `read_init`, which subtracts
  it back on success (same as the old code and the format doc).
* The verifier's last two bytes, low first, are `crc >> 16`, i.e.
  `plain[10] = crc>>16`, `plain[11] = crc>>24`.  The cipher is plain
  ZipCrypto, so the tree's decrypt_byte/update_keys/zdecode is reused.
* ALZ 8.0 passwords in the corpus are `secret` (`-psecret`).  Do NOT reuse
  the EGG passphrase `password` -- different lot, and it fails ALZ.
* Attr bits map to the house string as `"----W"` with `[4]='R'` (0x01),
  `[2]='H'` (0x02), `[3]='S'` (0x04), `[1]='D'` (0x10), `[0]='A'` (0x20).
  Directory entries are `fd` hi = 0 + attr 0x10 and become
  `CreateDirectory(pathMake(name))`.

**Three traps, all measured here:**

* **`read_init` must RETURN.**  It is `bool` and the first splice lost
  `return ans;`, so it fell off the end and returned stack garbage: every
  encrypted archive failed with `rc=0x8013`, wrote nothing, and the
  password path looked broken.  Same class as the `T var()` traps above --
  after any splice, check that every non-void function still ends with a
  return.
* **A lister must skip the member's data.**  `AlzNextMember` expects to be
  at a header, so returning a member without `AlzSkipMember` makes the next
  call read member data as a header: `meta_alz_m1.alz` listed 1 of 10
  members.  Extraction was unaffected only because the decoder consumes
  exactly `csize` bytes, which hid the bug from the extract path.
* **The optional part starts one byte later than it looks.**  The fixed
  part is NINE bytes (one unknown byte after `fd`), so method/crc sit at
  fixed+9/+11, not fixed+8/+10.  Reading crc one byte early gives
  `0x575A2400` where the header says `0x50575A24`, and the member stops
  listing as deflate.

Behaviour decisions (deliberate, consistent with the ZIP house pattern):
unsupported methods 1/3 (bzip2 / deflate3) are skipped and never listed
(the CLI aborts); a CRC mismatch deletes the output, continues, and makes
the archive fail; a wrong password is three `GetPassword()` tries and then
delete + fail (the old code returned true with no file); a structural error
or a missing continuation volume fails with no output.

**Verified:** every archive in `packed_alz_8.0` gives the corpus content
byte for byte -- 78 pass the full list+extract+hash check; 2 need more than
20 MiB and were verified separately (50 MB and 4.4 GB of zeros; the 4.4 GB
one shows `dwOriginalSize` truncating to a DWORD, a house-wide limit); 2
use a `euro` password that is fed as ANSI `0x80` and cannot be typed on a
cp950 ACP; 2 (`unicode_basic*`) store cp1252 single-byte accents that this
ACP cannot represent -- 2 of their 3 members still extract byte-exact and
the third is `?`-mangled by the WRITER itself (the stored name is literally
`???`).  Negatives: truncated, bad magic, header-only, junk after the
magic, flipped payload byte, missing `.a05` of a 16-volume chain -- all fail
loudly, no partial file, no hang, no crash.  VC6 and VC4 compile the three
units clean.

The old `BLZ`/`size_len` parser (`read_header`/`doHeader`, `FILE *zip`) is
left in place but is unreachable: it describes a different (pre-8.0) header
layout for which no fixture exists.

## EGG (ALZip) -- the 8.0 container and the five block methods

`ARCHIVETYPE_EGG` 76 (`EggTool.cpp`, `OpenArchive/OpenEgg.cpp`), with the
container and the codecs in `EggDecode.h/.cpp` and the AZO decoder in
`EggAzo.h/.cpp`.  Reference is seb3773/alzgg (`alzgg/src/egg.c`,
`egg_azo.c`, `egg_lzma.c`, `egg_bzip2.c`, `FORMAT-EGG.md`); the AZO file is
a decode-only port, the rest are clean-room rewrites of the observed
format.  Registered right after ALZ, and `IsType("egg")` is implemented so
the extension always wins pass 1.

**Format.**  `EGGA` + 14 bytes, then a prefix section (`Split`, `Solid`,
`Dummy`, `Skip`, `Global Encrypt`) closed by an END marker or by the first
FILE header, then members, then archive-level `Comment`s, then a final END
marker that must be the last byte.  A member is `FILE` (file id + u64
size), sub-headers (`Filename`, `Comment`, `Windows File Info` = FILETIME
+ attribute byte, `Encrypt Info`) closed by END, then blocks; sub-headers
may resume between blocks.  A block is an 18-byte header (method, hint,
usize, csize, crc32), a 4-byte END marker, then the payload.  An extra
field is `flags(1) + size(2|4)`; `flags & 1` selects the 32-bit size.
Filename flags: `0x08` = 2-byte locale, `0x10` = 4-byte parent path id.
Attributes are EGG's own bits: 0x01 R, 0x02 H, 0x04 S, 0x08 symlink,
0x80 directory.  The block CRC-32 is over the block's UNcompressed bytes;
there is no member-level CRC.

**Block methods**: 0 store, 1 raw deflate, 2 bzip2, 3 AZO, 4 LZMA.  Store
requires `csize == usize`; deflate is zlib raw (`inflateInit2(-15)`);
bzip2 is `BZ2_bzBuffToBuffDecompress`; LZMA blocks start with 9 bytes
(constant `04 41 05 00` + the 5 SDK props) followed by a raw stream with
**no end marker**, decoded with `LzmaDecodeStream(..., exact=true)`
(the shared engine's 64 MiB dictionary cap applies; the writer uses 16
MiB).  AZO: `'1'`, a flags byte, then 12-byte big-endian slices
(decompressed size, compressed size, size XOR check); `bcs == bus` is a
stored slice; every slice restarts the models/history; flag bit 0 means
the x86 call filter, decoder direction (`addr -= i`, step 5, 0xE8/0xE9
with a 0x00/0xFF high byte).

**Volumes.**  `Split Info` carries the header ids of the previous and the
next volume; the `.egg` files of the directory are indexed by header id,
the chain is walked from the head through `next`, and only the first
volume keeps its header and prefix (later ones contribute from their
payload offset).  Every id-based walk is ambiguous when unrelated archives
share ids -- see the trap below.  The whole logical stream is read into
memory, so a chain over 2 GB-1 is refused, and a member over 2 GB-1 is
refused too (`egg_normal_zero5g.egg`, a 5 GB fixture, fails loudly).

**Solid archives** (`Solid Info` in the prefix): the block payloads of ALL
members form ONE continuous stream in archive order and are decoded once,
then split on the block and member sizes.  Empty files and directories are
created at their position during that walk.  All blocks must share one
method.  Note that a solid member may carry zero blocks (the whole stream
can hang off the last member's blocks), so the listing's `szMode` is the
method of that member's first block, `stored` when it has none.

**Encryption**: ZipCrypto, AES-128/256 and LEA-128/256 are supported --
see the "EGG encryption" section below for the framing, the shared
primitives and the traps.

Traps, all real and all hit here:

- **The block CRC is the ordinary reflected CRC-32, i.e. zlib's.**  The
  first cut used the bare table loop with no initial/final inversion, so
  a correct decode reported `computed` = `raw0(data)` instead of
  `crc32(data)`: two, then seven, archives "failed" on a CRC that was
  right.  `EggCrc32` now inverts the incoming and outgoing state, which
  also makes chained (store) calls behave like `crc32(crc, buf, len)`.
- **Header ids are not unique across unrelated archives.**  The sample
  corpus has `records_compspeed_v100k.vol{1,2}.egg` and the ZipCrypto
  `..._zip.vol{1,2}.egg` with the SAME ids (edf3fdbe / 56ab3820), so the
  directory scan picks the wrong vol2 and the chain decodes to the wrong
  length (153284 instead of 153308).  The reference has the same flaw.
  Fixed with a name-affinity tie-break: on a duplicate id, keep the
  candidate whose file name shares the longest prefix with the requested
  one.  That is a heuristic, but it makes both spools work in place.
- **An inline constructor in the header means a stale `XacRett.obj`.**
  `CEggTool() : CTool("Egg <sjis>")` is defined in `EggTool.h`, so the
  string literal is instantiated in the TU that does `new CEggTool`,
  i.e. `XacRett.cpp`.  After changing the title and rebuilding only
  `EggTool.cpp`, `XacRettGetArchiveType` returned 0 while extraction
  still worked (the sweep itself does not care about the name) --
  `ArcNameToNum` simply never matched.  Same class as the NMAKE
  header-dependency note: rebuild the TU that includes the header.
  Worth knowing because "type 0 but extraction fine" reads like a
  registration bug, not a stale object.
- **A lister must not disturb the walk, and the walk is not seekable.**
  The engine is sequential (volumes concatenated, blocks consumed in
  order), which is why the listing and the extraction share the same
  parsed member table; there is no byte-alignment shortcut to a member.
- **Names are UTF-8 and may be unusable.**  `kiutil::utf8toAnsi` converts
  them; a name that cannot be represented, or that fails to convert, is
  skipped (the ALZ house behaviour).  The corpus contains writer-mangled
  names (`cafe_clair/???/???.txt`), which still extract.
- **AZO distances are validated, and a slice is bounded by 2^19.**  The
  decoder reports failure on `distance == 0 || distance > i`; the caller
  has already checked `bus <= usize - out`, so a corrupt stream cannot
  write past the block buffer.
- **LZMA and AZO are only reachable through correctly-routed blocks.**
  `EggDecode` preflights every member for an unknown encryption method or
  block method BEFORE writing anything, so an unsupported archive fails
  with no partial tree (mirroring the reference's preflight).

`XACRETT_EGG_DBG=1` appends the failure reason to `XacRett.egg.log` in
the current directory (the output dir, since MeltArchive chdirs first);
it was what turned the CRC problem and the volume-id collision from
guesses into facts.  It costs one `getenv` per failed extraction and is
otherwise inert.

Verified: **284 archives** (`alzgg/packed_egg` and `alzgg/packed_egg_8.0`,
everything up to 6 MB) list and extract with `rc=0` and no log line,
except the encrypted ones, which are refused loudly with zero output.
Grouping the results by member set gives **68 content groups with ZERO
hash mismatches** -- that is what proves the five methods agree: e.g. the
`two_*` / `egg_m*_two` / `egg_s_m*_two` families, `probe_*`, the
`nested_*` family, `records_*`, `zeros_*`, `image*`, `mz*` and the 420
member `splitmany_*` chains all produce byte-identical trees across
profiles.  The 16-volume `span_15vol_m0/m1` chains open from ANY volume.
Negatives (empty, header-only, bad magic, truncation at the header, in
the block header and in the payload, corrupted block CRC, block usize
over the member size, member usize 2^63/2^64, missing continuation
volume) all fail loudly, leave no file, and never hang.  VC6 and VC4
compile the four units clean (VC4 via `%TEMP%\opencode\vc4egg.bat`), and
the DLL's 11 exports and its KERNEL32/USER32/ADVAPI32 import set are
unchanged.  Harnesses: `%TEMP%\opencode\eggmatrix2.py` (the matrix and the
grouping) and `eggneg2.py` (the negatives).

## EGG encryption (ZipCrypto, AES-128/256, LEA-128/256) -- DONE

Encrypted EGG archives now extract.  The work also removed the duplicated
ZipCrypto cipher and exposed the primitives the tree already had:

- `ZipCrypto.h/.cpp` is the classic PKZIP stream cipher, now shared by
  `CZipTool`, `CAlzTool` and `EggDecode`.  Its `Crc()` is the ordinary
  reflected CRC-32 (zlib's), so the three providers' output checks and the
  cipher's raw `CrcStep()` use the same table; the 256-entry tables and the
  `calc_crc` macro that were duplicated in ZipTool.cpp/AlzTool.cpp are gone.
- `wzaes/wzaes.h` now exposes `iz_pbkdf2_hmac_sha1()` and
  `iz_hmac_sha1_{init,update,finish}()`; its declarations are inside
  `extern "C"` guards, so a C++ TU can include it directly (the old files
  wrapped the include by hand).  `pbkdf()` gained a rounds argument.
- `Lea.h/.cpp` is the LEA-128/256 forward block function (KS X 3246),
  ported from the reference; CTR only needs the forward direction.
- `EggCrypt.h/.cpp` holds the password encoding and one member's
  decryption state.

**Password encoding.**  ALZip converts the UTF-16 password to CP1252 and
maps anything outside it to `?` (euro -> 0x80, e-acute -> E9, Hangul ->
3F; measured on the oracle).  `EggPwEncode` parses UTF-8 and applies that
mapping; if the input is not valid UTF-8 it passes the bytes through
unchanged, because the modal dialog hands us system-ANSI text.  Without
that fallback an accented password typed in the dialog would be unusable
while ALZip itself accepts it (the reference rejects invalid UTF-8).

**Framing.**  The Encrypt Info data (after the method byte) is
`salt | verifier(2) | HMAC(10)` for AES/LEA with `saltlen = keylen/2`:
PBKDF2-HMAC-SHA1, 1000 rounds, `2*keylen + 2` bytes (enc key, MAC key,
verifier).  That is exactly WinZip AES, so `iz_aes_init` is reused
(strength 128/256) and `iz_aes_mac_update`/`iz_aes_auth`/`iz_aes_decrypt`
do the rest.  AES-CTR starts at counter 1 and increments the 16-byte
counter little-endian; LEA zeroes it and increments big-endian.  The HMAC
covers the CIPHERTEXT and is verified before anything is written.
ZipCrypto's Encrypt Info carries the 12-byte verifier ciphertext, which is
consumed by the key stream; the twelfth plaintext byte must equal
`crc >> 24` of the first block (single-block or empty member) or of the
WHOLE member CRC (multi-block: decoded once without writing, then again
for real, exactly like the reference).

**Provider flow.**  The engine has one password per archive.  A member
whose Encrypt Info exists and no password makes `Extract` fail with
`password required` and `PasswordError()` set BEFORE writing anything; the
provider then calls `CTool::GetPassword()` (so `-p<pw>` works headless) up
to three times and retries.  A wrong password fails on the verifier before
the file is created (or on the HMAC check), and a failed member deletes
its partial file.  Listing never asks for a password.  Solid + encrypted
is refused (ALZip cannot produce it).

Traps, all hit here:

- **`ArchiveListMap` is populated by the line AFTER each pair.**  When the
  EGG pair was appended after ALZ, the `ArchiveListMap[t]=o;` that
  belonged to ALZ ended up after the EGG pair: `CAlzTool` was unmapped,
  so `XacRettOpenArchive` returned NULL (`open failed`) for every ALZ
  while extraction kept working (extraction does not use the map).  Any
  provider insertion must keep the `t=...; o=...; ArchiveListMap[t]=o;`
  group intact, and ALZ listing (10 members on `meta_alz_m1.alz`) is the
  cheap regression that catches it.
- **The ZipCrypto 12-byte verifier is part of the key stream.**  Decrypt
  the 12 info bytes with the password-seeded keys and keep the resulting
  state; a fresh `Init` before the data would desynchronise the member.
  This differs from ALZ, where those 12 bytes sit at the head of the
  member data.
- **AES needs the HMAC over the whole member ciphertext before decoding**,
  and `iz_aes_decrypt()` is in-place (it XORs the buffer and also updates
  the MAC, which is harmless after the tag was taken).
- **LEA's counter is big-endian from byte 15, AES's little-endian from
  byte 0.**  They look alike and are not.
- **Non-ASCII passwords are UTF-8 on the command line but ANSI from the
  dialog** -- hence the fallback above.  `%TEMP%\lha\eggprobe.exe` takes
  the password as hex so the `enc_egg_napw_*` fixtures can be driven from
  a cp950 console: euro `e282ac`, Hangul `ed959c`, acute `c3a9`; raw `e9`
  also works through the ANSI fallback.

Verified: all encrypted fixtures extract byte-identically to their plain
counterparts -- ZipCrypto (single- and multi-block, incl. the 16-volume
`span_15vol_crypt` chain opened from any volume, password `secret`),
AES-128/256 and LEA-128/256 (`password`), across store/deflate/bzip2/AZO/
LZMA and the empty/directory cases, all of the 8.0 corpus plus
`records_compspeed_v100k_zip` and the `enc_egg_napw_*` samples.  The full
EGG matrix is 304 ok / 4 expected failures (the three `napw` fixtures pass
only with their non-ASCII password, verified via the probe;
`names_specials_m1` fails only because its 195-character member name plus
a long TEMP output path exceeds MAX_PATH -- a short outdir extracts 8/8),
68 content groups with zero mismatches.  ALZ: 82 pass / 0 fail, including
the encrypted samples (`secret`).  `runnewmatrix.py` still reports only
the six known-stale zstd-sandwich expectations.  Negatives (wrong password
for each method, flipped AES ciphertext, tampered zip member, truncated
archive) fail loudly with zero output and no hang.  VC6 and VC4 compile
the new units clean.

## Pre-commit checklist

1. Byte-diff audit clean (no SJIS churn).
2. Full `build.bat` with zero `FAIL` lines + relink.
3. Regression: `runarcwatch.ps1` 18/18 MATCH when ArcTool changed;
   `dlltest list` spot-checks when providers changed.
4. `git status` shows only intended files; commit with `-c` identity.
