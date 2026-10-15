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
- MSVC 4.x (CL 10.10.6038) also installed, at
  `C:\MSDEV4\BIN\VCVARS32.BAT`. All `XacRett.dsp` units are VC4
  compile-clean (measured, not guessed - see "MSVC4 compatibility"
  below); no VC4 build script or link yet.
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
  **SQX method 6 (`SQX_C1_EX_NORMAL`) emits a zero-byte member.**
  Deliberately NOT worked around - we are waiting on upstream `unsqx`.
  What is already established, so nobody re-derives it:
  - methods 6-10 (`EX_NORMAL/GOOD/HIGH/BEST/AUDIO`) are just the
    "EX" compression levels of the same SQ codec. `ExtractArcFile()`
    routes everything except STORED (0) and AUDIO (5) into the ONE
    shared `DecodeFile(lCompFlags, lSolidFlag, 1<<(((lFlags>>8)&15)+15))`
    in `unsqx/uarde.c`, and the dictionary size comes from the header
    flags, not from the method number. So there is no method-6-specific
    decode path to get wrong.
  - **the trap that makes this look like a header bug:** `SQX_EX_FILE_HEAD`
    is *not* the EX-method header. `ReadNextArcFHDR()` only uses it when
    `usHeadFlags & SQX_FILE64_FLAG` is set, i.e. it is the >4GB
    `SQX_FILE_HEAD` extension, and it shares `cHeadType ==
    SQX_FILE_HEAD_ID`. Reading it as "the method-6 header" leads straight
    to the wrong name/size offsets.
  - consequence in our tree: the member is created, decodes to nothing,
    and is left at 0 bytes (the house skip-and-continue then reports
    success). Everything else about the format behaves.
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

## MSVC4 compatibility (all XacRett.dsp units compile clean)

Question answered by experiment: **can this tree build under MSVC 4.x?**
Compile-clean: yes. **All 220 compile units in `XacRett.dsp` (148 .cpp
+ 72 .c) compile clean under CL 10.10.6038**, with zero change to VC6
behavior (full build + relink + 25/25 7z matrix + ZIP-AES/XZ/JCALG1/YZ1
spot checks after every fix). Not yet done: the separate
`unrar/UnRAR.dsp` static lib under VC4 (earlier rough pass: many
failures, and `arccmt.cpp` hangs the VC4 compiler outright), and any
VC4 link.

Re-measure (TEMP harness, nothing in the repo; one file per `cl` with
a 120 s timeout each, `<dir>_<file>.obj` naming, flags mirroring
`genbuild.ps1` plus `/FI vc4compat.h /I STL /I vc4compat`):
  `python C:\Users\Roy\AppData\Local\Temp\opencode\vc4check.py`

Or build it declaratively (checked in): `src-0.49.17/Makefile.vc4`
is a plain NMAKE makefile generated from the same `XacRett.dsp`
SOURCE lines (generator: TEMP `genmakevc4.py`; re-run it when sources
are added or removed). After `VCVARS32.BAT` (MSDEV4):
  `nmake -f Makefile.vc4` (objects + resources to `%TEMP%\vc4build`)
  `nmake -f Makefile.vc4 OUTDIR=C:\vc4out` (redirect outputs)
  `nmake -f Makefile.vc4 link` (needs `unrar\UnRAR.lib`; open).
Proven: full 220-unit + .res build, zero errors, incremental no-op
clean. `STL/` (STLport 2.033, Nov 1997, SGI-STL-2.03 lineage,
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
Still open: the unrar static lib under VC4, then a VC4 link (its
import table can never match official: VC4 has no SHLWAPI.LIB, but
our SHLWAPI use is already GetProcAddress-based, so a VC4 link stays
95/NT4-loadable by design).
## Pre-commit checklist

1. Byte-diff audit clean (no SJIS churn).
2. Full `build.bat` with zero `FAIL` lines + relink.
3. Regression: `runarcwatch.ps1` 18/18 MATCH when ArcTool changed;
   `dlltest list` spot-checks when providers changed.
4. `git status` shows only intended files; commit with `-c` identity.
