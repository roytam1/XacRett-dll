# Supported formats

Source of truth for what this tree claims to open. Generated 2026-10-26
from the source (`src-0.49.17/*Tool.cpp`, `OpenArchive/Open*.cpp`,
`XacRett.cpp`, `XacRett.h`) and the verification notes in `AGENTS.md`.
`docs/` holds project documentation; the file itself is plain ASCII.

How routing works (read before adding or reordering a provider):
`XacRettGetArchiveType` / `XacRettOpenArchive` / `XacRettCheckArchive`
each make **two passes** over the tool list. Pass 1 takes the FIRST tool
whose `IsType(ext)` matches and calls only its `Check`. Only if nothing
matched, pass 2 scans the whole list and takes the first tool whose
`Check()` passes. Consequences:

- A provider with no `IsType` override (base returns false) is invisible
  to pass 1. Those rows are marked `Check-gated` below: the extension is
  tested inside `Check`, so they only win in pass 2, and only if no
  earlier lax `Check` claims the file first.
- `IsType` ties are won by registration order. Compressed-tar/cpio twins
  therefore sit right after their tar twin and before the plain
  single-file provider, so `.tar.*` still hits tar first and a non-tar
  stream falls through to the plain provider.
- `CPmdTool` is commented out (`XacRett.cpp`), so PMD is not claimed.

Conventions used below:

- `C++ objects`: extract/list class pair (`CxxxTool` / `COpenXxx`).
- `Ext`: pass-1 extension(s) from `IsType`, or the `Check`-gated
  extension. Case follows the source (`strcmp` = case-sensitive).
- `Magic`: the `Check` auto-detect test, bytes as hex/ASCII. `LE32` is a
  little-endian 32-bit field.
- `Tests`: tracked fixture under `fixtures/` plus the measured status
  from `AGENTS.md`; `TEMP-only` means the proof lives in a TEMP harness,
  not in the repo.
- House oracle: most `LICENCE*` fixtures hold the 18340-byte `LICENCE`
  text of `fixtures/rar/GPLv2-rar4.rar` (sha256 `6c4beace...`).
- Passwords: `-p<password>` on the command line (any provider), key file
  as `-p#<file>`. Listings never prompt; they use the command-line
  password only.
- Long names on 8.3-only systems are mangled by `kiutil::pathMake83`
  (opt-in `XACRETT_8DOT3=1` on systems that have LFN); providers that
  build their own output name call it explicitly.

## Multi-file archives

| Type | C++ objects | Ext | Magic | Tests | Remarks |
|---|---|---|---|---|---|
| ACE 1 | `CAceTool` / `COpenAce` | `ace` | scan for `**ACE**` (`2A 2A 41 43 45 2A 2A`), header CRC match | `TEMP-only` (`testdata/winace`): 9/9 vs WinACE incl. EXE/DELTA/SOUND/PIC by header-CRC32 | STORE + LZ1 + ACE 2.0 blocked (own `AceBlock` engine, solid dict, CRC verify + delete-on-mismatch). Encrypted refused (`ACE_PASSW`). Vol 0 only. |
| ARC 2 | `CArcTool` / `COpenArc` | `arc` | `1A` + type `00`/`01`-`0B`/`1E` + NUL in name bytes; SFX `MZ` + stub scan | `TEMP-only` (`%TEMP%\arctest`, 18 fixtures): 18/18 MATCH | Methods 1-3 official-verified; 4-9 deliberately FIXED (official yields nothing, inverted crunch return). M10 keeps 0-byte file like official. |
| ARG 3 | `CArgTool` / `COpenArg` | `arg` | `00` + name + `LE32@22` entry size, next-entry byte 0/1 | status not separately recorded | Non-blank entry name required, so CP/M LBR padding is rejected. |
| ARJ 4 | `CArjTool` / `COpenArj` | `arj` | scan `60 EA`, header size sane, `~CRC32` match | status not separately recorded | -- |
| AR 5 (deb) | `CArTool` / `COpenAr` | `deb` | `!<arch>\n` or `!<bout>\n`, backtick + `0A` at 66 | 659/659 members list+extract byte-exact (`LIBCMT.LIB`, TEMP-only; official extracts one mangled file) | MS LIB (`//` string table, `/offset` long names, trailing-`/` strip). |
| ASD 6 | `CAsdTool` / `COpenAsd` | `asd` | `ASD01 1A` / `ASD02 1A` (`41 53 44 30 31/32 1A`) | `fixtures/asd/` (`LICENCE-015.asd`, `LICENCE-020.asd`, `multi.asd`): all byte-exact, sizes/CRCs match reference | 0.1.x is plain LZSS, 0.2.x is LZ77 + dual adaptive Huffman (bit-packed params, minhit 2, window/buffer bits clamped; buffer_bits overflow refused). 0.2.x verifies member CRCs; 0.1.x does not (matches official). Member count is 2 bytes (0.1.x) vs 3 bytes (0.2.x). |
| BGA 9 | `CBgaTool` / `COpenBga` | `gza`, `bza` | scan `GZIP` / `BZ2\0` + additive checksum + bounded `fnlen` | status not separately recorded | Refuses foreign archives that merely share the extension list. |
| CAB 12 | `CCabTool` / `COpenCab` | `cab` | scan `MSCF 00 00 00 00` + field parse (SFX-tolerant) | `fixtures/cab/` (per-method/window: mszip, x15-x21, lzx, q15-q21, quantum): all 18340 B = oracle | Own container + `none`/MSZIP/LZX/Quantum decoders (MSZIP shares window via `inflateResetKeep`; LZX bit order is word-swapped vs Quantum; Quantum arithmetic models halve exactly like the writer). System FDI kept as fallback (`FDI.DLL` then `cabinet.dll`). Listing uses same parser as extraction. |
| CPIO 15 | `CCpioTool` / `COpenCpio` | `rpm`, `cpio` | `070701` / `070702` / `070707` / u16 `070707` octal (+swapped) | `%TEMP%\cpiofix\tree.cpio` + 7-Zip oracle (non-empty members): 5/5 OK | `rpm` and `cpio` share the provider. |
| CPT 16 | `CCptTool` / `COpenCpt` | `cpt` | `01` at 0 (or +128 MacBin gap) + BE32 size + header CRC | status not separately recorded | -- |
| DZ 17 | `CDzTool` / `COpenDz` | `dz` | `DZ` + ver <= 2, `indexPos`/`fileNum` sane | status not separately recorded | -- |
| GCA 20 | `CGcaTool` / `COpenGca` | `gca` | SDK `GetNumFiles` with `-p` spell, else `GCA` signature fallback | `fixtures/gca/` (29: modern plain/pw/key + `key.bin`, legacy GCA0/GCA1 incl. encrypted v6 + key bin): all byte-identical to `gcac.exe`, 0 regressions | GCA3 + legacy GCA0/GCA1 + encrypted (cipher, KDF, encrypted dir). `-p#file` = SHA-1 hex of key file (like `gcac -k`). Encrypted lists 0 members without `-p`; wrong password fails loudly. Dir members created as dirs; GCA3 dir CRC validated. |
| HQX 22 | `CHqxTool` / `COpenHqx` | `hqx` | `(This file must be converted with BinHex 4.0)` + BinHex4 header/CRC (`1021`) | status not separately recorded | -- |
| IMP 23 | `CImpTool` / `COpenImp` | `imp` | `IMP\n` + header CRC, or SFX `MZ` + scan | status not separately recorded | -- |
| IS 25 | `CIsTool` / `COpenIs` | `hdr` | `ISc(` (`49 53 63 28`) + version bit | status not separately recorded | InstallShield cabinet. |
| ISH 24 | `CIshTool` / `COpenIsh` | `ish` | `<<< ... aish ... lines` + `!`/`#` jis7/8 lines + CRC16 | status not separately recorded | Declarations-in-condition hoisted for VC4. |
| JAK 26 | `CJakTool` / `COpenJak` | `jak` | `This_Is_Jack_File` (+`MZ` SFX scan), name bounds | `fixtures/jak/` (`LICENCE1.jak`, `LICENCE2.jak`): 18340 B MATCH x2 | File SPLITTER: `Extract` combines `1.jak`, `2.jak`, ... so either half yields the full file. |
| JAM 27 | `CJamTool` / `COpenJam` | `jam`, `jam5`, `jam8` | ext-only (`IsType`) | `fixtures/jam/` (`LICENCE-may96.jam`, `LICENCE-aug96.jam`, `M_PLAIN`): byte-exact vs real tools; 10 generated Aug/96 round-trip vs `AUG96\UNJAM.COM` | May/96 + Aug/96 (reflection layer, 32-bit limit `0x11170`). `.jam` auto-detects (May/96 first, Aug/96 reflect-share heuristic, 75% threshold; May/96 never regresses), `.jam5`/`.jam8` force. Stored + key-CRC-checked encrypted supported. Aug/96 has no checksum/tag: undetected Aug/96 keeps the old May/96 result. |
| LZH 28 | `CLzhTool` / `COpenLzh` | `lzh`, `pma` | scan `-lh?-/pm?-` + `-lx1-` + `ReadHeader` validation (levels 0-3, OS/2 LV3 32-bit ext chain, OS/9 +1-byte tail, LArc7 `-lz1/2/3/8-` level-field params, `-lz5-` init pattern) | `fixtures/lzh/archives/` (fragglet/lhasa corpus, 209): 143 exact + 53 case-insensitive + 0 no-output; ours==official on 189, 0 where official beats ours; 9xPMA + 4xLH4/LHX fixed vs official; `fixtures/lzh/unlha32-lx1.lzh` byte-exact | Header CRC is CRC-16/ARC (init 0) over `raw[0..hdrlen]` with common-CRC zeroed; level 2 `hdrlen` is a word only. `-lz5-` window = 13x each value, ascending, descending, 128 zeros, 110 spaces, 18 zeros. LArc7 members report `crc 0000` like the reference. 10 symlink cases materialise target as file (cannot symlink). `-lx1-` is UNLHA32's experimental format, NOT `-lh6-` (same 510-code literals, 32 KiB window, but per-block 2-bit offset mode with 4 coders); decoded by vendored `lhasa/lx1_decoder.c`. |
| PIT 33 | `CPitTool` / `COpenDummy` | `pit` | `PMa` at 0 (or +128) + `SyncHeader` + `pitcrc` | status not separately recorded | Listing path is `COpenDummy` (no live listing). |
| QPAK 34 | `CQPakTool` / `COpenQPak` | `pak`, `wad`, `pff` | `PACK`/`SPAK`/`WAD2`/`WAD3`/`IWAD`/`PWAD`/GoldSrc `PFP`-ish + bounds | status not separately recorded | -- |
| RAR 35 | `CRar3Tool` (live; `CRarTool` compiled but unregistered under `RAR_3X`) / `COpenRar` | `rar` | `Rar!\x1A\x07\x00` / RAR5 `Rar!\x1A\x07\x01\x00` (+`MZ`/`PE` SFX) | `fixtures/rar/` (1.5/2.0/2.9/4.x/5.x + 72-member `aida64.rar`): all byte-identical incl. prebuilt-lib rebuild | Vendored UnRAR 7.2.7 (RAR 1.5-5.x; official does 2.x/3.x only). RAR5 listing via `FindSub5` (`RAR_SKIP` past file data; dirs skipped, UTF-16 names pass through, >4 GB truncated to DWORD). `CRarTool` (in-house) is compiled but dead: `RAR_3X` always registers `CRar3Tool`. 8.3 names via `pathMake` + explicit `ProcessFile` name; Win32s ANSI fallback in `win32scompat.h`; thread pool degrades inline. |
| REZ 36 | `CRezTool` / `COpenRez` | `rez` | `REZ\0` | status not separately recorded | Contains non-SJIS bytes (left alone). |
| SPL 38 | `CSplTool` / `COpenSpl` | `sp`, `spl` | `93 B9 06` + NUL in name field | status not separately recorded | -- |
| SQX 61 | `CSqxTool` / `COpenSqx` | `sqx` | scan `-sqx-` + main-head ID `0x52` | `TEMP-only` (`Squeez/packed_corpus`): m6 135 members + 12/12 generated CRC-exact; m7 13 ultra byte-exact; fmt-2.0 LZ 202 passes, ZERO BYTES-DIFFER/ZERO MISSING | Methods: stored, fmt-1.1/2.0 LZ 1-4 (`NEEDBITS`/`DUMPBITS` fully parenthesised; fmt-2.0: `NC2`=311, no symbol shift, 54-code 8 MB dist table, MM init 8), PPMd-I m6 (own range front over `ppmd8/` core), Ultra m7 (probabilistic range coder, house `WriteOutBuf` streaming). `szMode` unset for m6. Solid keeps models only. |
| TAR 39 | `CTarTool` / `COpenTar` | `tar` | 512 B header checksum (`sum + 8 spaces == octal field`) | `%TEMP%\tarfix\`: `.tar.lz` lists 12/12 = plain tar | -- |
| YZ1 45 | `CYz1Tool` / `COpenYz1` | `yz1` | `yz01` (+SFX tail-size scan); `all_size`/`head_size`/`fnum` sane | `fixtures/yz1/` (plain/pw/lib + all-empty + mixed): 176 archives 0-diff vs SDK impl, key cases = `yzdec.exe` | Password via `MessINPUT` hook: listing uses `-p` only (never prompts, 0 members without it); extraction prompts once per session (cached; empty result not cached). `all_size >= head_size` tolerated. Single-file tools below with `pathMake83` + YZ1/GCA covered. |
| ZIP 43 | `CZipTool` / `COpenZip` | `zip` | scan `PK\x03\x04` + method gate + `read_header` re-parse | `fixtures/zip/` (`PKZ080.ZIP`, `pkz090.zip` 10/10 CRC-OK each, `aida64.zip`) + `TEMP-only` matrices | Methods 0/8/9/10/12/14/18/95/98/99 (DCL via `pkdcl`, LZMA via `LzmaDec`, AES via `wzaes` + PBKDF2/CTR/auth-tag, PPMd via `ppmd8/`, BWT via `xceedbwt`, XZ via `Lzma2Dec` chunk layer + own CRC64, Zstd via zstd 1.5.6 subset, Deflate64). Exotics skipped-and-continued via `doHeader` resync (holes never listed). AES salt/verifier/HMAC framing; inner-BWT refused. No ZIP64 central-dir/attrs/spans; small-file ZIP64 via `0x0001` extra (refuse >2 GB-1, no hang). Bit-11 UTF-8 via hand decoder + `CP_ACP`. Data descriptors via central-dir size. |
| ZOO 44 | `CZooTool` / `COpenZoo` | `zoo` | LE32 start/minus (`start+minus==0`) + `FDC4A7DC` tag | status not separately recorded | -- |
| ZAC 42 | `CZacTool` / `COpenZac` | `zac` | `FF 00 26 01/02 [D1]` (as coded, AND binds tighter, so `01`/`02` bypass the `D1` test) | status not separately recorded | -- |
| 7Z 71 | `C7zTool` / `COpenSevenZ` | `7z` | `37 7A BC AF 27 1C 00` + full header parse (start/header CRC, encoded header) | `fixtures/7z/` (`7z-zstd.7z` + TEMP-only 25-fixture matrix): 25/25 byte-exact; `m93_aes.7z`/`aes4.7z` byte-exact | Own container over SDK cores (Copy/LZMA/LZMA2/Delta/BCJ(+2)/ARM../PPC/SPARC/IA64/RISCV/PPMd7/Deflate/BZip2/ZSTD `04F71101`/7zAES). Folder cache for solid; UTF-16LE names; zip-slip sanitising. 7zAES needs UTF-16LE KDF + in-place CBC + padded-size (no tail digest). `IByteIn` wrapper embeds interface first (no direct cast). PPMd end = exact length, never `PPMD7_SYM_END`. |
| ALZ 55 | `CAlzTool` / `COpenAlz` | `alz` | scan `ALZ\x01` | `TEMP-only` (`alzgg/packed_alz_8.0`): 82 pass / 0 fail | Shared chain reader (`AlzDecode`): volumes `.a00`..., `crc32` over uncompressed data, `csize` excludes 12-byte ZipCrypto verifier (last two bytes `= crc>>16`). Encrypted = `-psecret` (3 tries, then delete + fail). Methods 1/3 skipped, never listed; member CRC mismatch deletes output + fails archive. |
| EGG 76 | `CEggTool` / `COpenEgg` | `egg` | scan `EGGA` within first 4 KB | `TEMP-only` (`alzgg/packed_egg{,_8.0}` 284 archives): 68 content groups ZERO mismatches; encrypted 304 ok / 4 expected failures | Container + store/deflate/bzip2/AZO/LZMA (`04 41 05 00` + SDK props, raw stream, 64 MiB dict cap) blocks; volumes by header id + name-affinity tie-break; solid = one stream split on sizes. Encryption ZipCrypto/AES-128/256/LEA-128/256 (CP1252 `?`-mapping, `-p`; listing never asks; solid+encrypted refused). Unknown block/encryption method preflighted before writing. `XACRETT_EGG_DBG=1` logs failure reason. |
| DGC 74 | `CDgcTool` / `COpenDgc` | `dgc`, `dgcr` | `DGCA` + `20 00 00 00` | `fixtures/dgc/` (12 incl. `names.dgc` 5981 entries, `dgca110-column.dgc`, password + `.dgcr` recovery): byte-identical to `dgcunp.c` | Chunk tree (`DGCA/DATA/DGCC/INFO/IARC/IFDT/IFNM`), context-mix + LZ + implicit-distance blocks, tag-2 column method (short last row: walk each column to its own length). Index `origSize` locates data; index CRC is the oracle. Encrypted: per-`DGCC` (headers under `SHA-512(zeros+pw)`, payloads under `SHA-512(plain-hdr+pw)`), `-p`/`-p#` (hex SHA-512). `.dgcr` = archive + `DGCR` 2-D parity (repair before decrypt). Subtraction-form bounds everywhere. |
| DGD 75 | `CDgdTool` / `COpenDgd` | `dgd`, `dgdr` | `DGCD` + `20 00 00 00` | `fixtures/dgd/` (6: plain/`.dgdr`/password pairs): same 86016 B file, VC6+VC4 | Spanning parts `<base>%04d.dgd` told apart by whole-file SHA-512 + part count; combos tried until hash matches. Same cipher keyed on `INFO` material. `.dgdr` repairs damage (`.dgd` fails `0x8003`, nothing written). |
| AIN 64 | `CAinTool` / `COpenAin` | `ain` | `21 11`-`14`, bit6 clear, `idx_pos` sane, header CRC `^0x5555` | `TEMP-only` (upstream corpus: HELLO/ENG x M1-M4 + CODE.AIN): 9/9 byte-exact | Solid LZ77+Huffman stream sliced by `orig_size`, M1-compressed index, whole-stream CRC (strict). M1/M2/M3 share decode; M4 verbatim. Multi-volume/SFX/2.2-fragments deferred. |
| LBR 63 | `CLbrTool` / `COpenLbr` | `lbr` | size % 128 == 0 + dir control entry (`00` + 11 spaces + `0000` + dirSectors) | `fixtures/lbr/crlzh20.lbr`: 30/30 (26 v2 CrLZH + 4 stored) vs 80un oracle | Stored (CCITT CRC, high-first == 0) or SQ-squeezed (`76 FF`), crunched (`76 FE`, v2.x adaptive LZW 9-12 bit MSB + RLE90, sig >= `0x20`; v1 refused), CrLZH (`76 FD`, lh1-lineage LZSS + adaptive Huffman, v1 12-bit / v2 11-bit position). CP/M dates -> DOS (>= 1980 else 0). Whole-compressed `.lqr`/`.lzr`/`.lyr` NOT supported (no authentic fixture). |
| YNC 41 | `CYncTool` / `COpenYnc` | `ntx` | scan `=ybegin ` + ` name=` | status not separately recorded | yDecode provider. |
| BLZ 54 | `CBlzTool` / `COpenBlz` | `blz` | `blz\x1A` + ver 1 + packed/block size caps | status not separately recorded | BriefLZ. |
| LZX 58 | `CLzxTool` / `COpenLzx` | `lzx` | `LZX` | status not separately recorded | -- |
| JCALG1 59 | `CJc1Tool` / `COpenJc1` | `jcalg1`, `jc`, `jc1` (case-sensitive `strcmp`) | `JC` + LE32 size + checksum, bitstream at +10 (`...!= 0`) | `fixtures/jcalg1/jcalg1_test.jc1` = official = reference; 20/20 window levels 1/5/6/9 byte-exact | Window up to 1 MB via destination-as-history + `SafeDist` clamp; hard stop at header size. Reference compressor refuses incompressible input (no such fixture). |
| APK 60 (aPLib) | `CaPLibTool` / `COpenaPLib` | `apk` | `AP32 18 00 00 00` | status not separately recorded | aPLib-compressed single file. |
| ASK 62 | `CAskTool` / `COpenAsk` | `ask`, `apk` (Check-gated, no `IsType`) | ext-only | 42/42 fuzz cases clean (16 pre-fix crashes incl. `C00000FD` stack overflow + 39 MB runaway + double-free, all shared with official) | Memory-safety hardening (NUL-terminated root, bounded indices, `length` capped 256 MB and `file_length*4096`, back-ref distance validated). NO authentic sample: verified as no-longer-crashing, not as decoding-correctly. |
| Compressed tar twins | `CTgzTool`/`COpenTgz` (47), `CTbzTool`/`COpenTbz` (46), `CTaZTool`/`COpenTaZ` (48), `CTazTool`/`COpenTaz` (49), `CTxzTool`/`COpenTxz` (68), `CTzstTool`/`COpenTzst` (70), `CTlzmaTarTool`/`COpenTlzma` (67), `CLzTarTool`/`COpenTlz` (73) | `gz`/`tgz`, `bz2`/`tbz`, `Z`/`taZ` (case-sensitive), `z`/`taz` (case-sensitive), `xz`/`txz`, `zst`/`tzst`, `lzma`/`tlz`, `lz`/`tlz` | decode whole stream to fixed-name temp, then base-tar `CheckSum` | `%TEMP%\tarfix`: 3/3 `.tar.lzma`+`.tlz`, 3/3 `.tar.xz`+`.txz`, 3/3 `.tar.zst`+`.tzst` PASS; `.tar.lz` 12/12 = plain tar | Same temp-delegation design as cpio twins (self-healing fixed-name temp; `fp=NULL` ctors). `.gz` hits `CTgzTool` in pass 1 before `CGzTool` (falls through to plain gzip only if not a tar). |
| Compressed cpio twins | `CCpioGzTool`/`COpenCpioGz` (50), `CCpioZTool`/`COpenCpioZ` (51), `CCpioBzTool`/`COpenCpioBz` (52), `CCpioLzmaTool`/`COpenCpioLzma` (77), `CCpioXzTool`/`COpenCpioXz` (78), `CCpioZstTool`/`COpenCpioZst` (79) | `gz`/`rpm`, `Z` (case-sensitive), `bz2`, `lzma`, `xz`, `zst` | decode whole stream to fixed-name temp, then base-cpio magic | `%TEMP%\cpiofix` (`tree.cpio` text/binary/zeros/empty/subdir + 3 compressed): 4/4 PASS types 15/77/78/79; 8 negatives loud | Registered right after each tar twin, before the plain provider: `.tar.*` still hits tar first; valid-but-not-cpio falls through (`.lzma` -> type 65). Also fixed `CCpioTool(const char*)` uninitialised-`fp`. |

## Single-file codecs

| Type | C++ objects | Ext | Magic | Tests | Remarks |
|---|---|---|---|---|---|
| GZ 21 | `CGzTool` / `COpenGz` (Check-gated, no `IsType`) | `gz` (handled by `CTgzTool` in pass 1; plain gzip only via pass-2 fallback) | `1F 8B` / `1F 9E` | `TEMP-only` (`%TEMP%\bz2test`-era `sample1/2/3.bz2` analogues for gzip; tar.gz 278 list + 171/171 MATCH) | Output name: strip `.gz` else append (`.ungzip` quirk kept for the plain provider's listing). |
| BZ2 11 | `CBz2Tool` / `COpenBz2` (Check-gated) | `bz2` (pass 1 goes to `CTbzTool` first) | `BZh` + live `BZ2_bzread` probe | `TEMP-only` (`runbz2matrix.ps1` 6/6 MATCH; `tree.tar.bz2`/`.tbz` unaffected) | Single-stream only: multi-stream yields the first stream. Registered immediately before `CLzmaTool`. |
| Compress(Z) 14 | `CCompressZTool` / `COpenCompressZ` (Check-gated) | `Z` (case-sensitive) | `1F 9D` | `TEMP-only` (PACKZ negatives live outside the sweep) | Unix `compress`. `Z` vs `z` (PACKZ) are different providers and case-sensitive. |
| FRZ 19 | `CFrzTool` / `COpenFrz` | `F`, `xF` | `1F 9E` / `1F 9F` | status not separately recorded | Output rule also accepts `frz`. Single-file tools build their own name + `pathMake83`. |
| PACKZ 32 | `CPackzTool` / `COpenPackz` (Check-gated) | `z` (case-sensitive) | `1F 1E` | `fixtures/packz/LICENCE.txt.Z`: lists `LICENCE.txt` `orig=0`, extracts 18340 B | Loop bounded by `PFILE::orig_len` (BE header); partial output removed on failure. Junk `.pkz` used to disk-fill (~6 MB/s, official too); now 0.03 s rc=0. |
| COMPACT 13 | `CCompactCTool` / `COpenCompactC` | `C` (case-sensitive `strcmp`) | `FF 1F` | `fixtures/compact/LICENCE.txt.C`: lists `LICENCE.txt` `orig=0`, extracts 18340 B | Listing is live since 2026-10-26 (was 0 members). `orig=0`: the format stores no size. Name rule mirrors `Extract` (`C`-strip else `.uncompact`). LZW. |
| LZIP 72 | `CLzTool` / `COpenLz` | `lz`, `tlz` | `LZIP` + ver 1 + `DS` base_log 12-29 | `fixtures/lzip/nicfix.tar.lz` + generated (empty/31 B/3000 B/346 KB/2-member): byte-exact vs liblzma oracle | Own `LzDecodeStream` (reports bytes consumed; trailer-located). 6-byte header + 20-byte trailer (CRC32 reflected, LE sizes). Trailing data ignored; multimember OK. Dicts clamped 4 KB-64 MB (>64 MB refused loudly). |
| LZ 73 (tar form) / plain | `CLzTool` covers both; tar twin `CLzTarTool` / `COpenTlz` | `lz`, `tlz` | same | same | `.tlz` is the lzip alias; `.tar.lz` routes to the tar twin first. |
| LZMA 65 | `CLzmaTool` / `COpenLzma` | `lzma`, `tlz` | props `<= 224` AND `dict = LE32(+1)` power-of-two `>= 4096` | `TEMP-only` (`%TEMP%\lzmafix` + xz-5.8.2 suite): 8/8 byte-exact | LZMA-Alone only (no magic exists): the dict test is the discriminator (SDK rejects non-pow2). Shared `LzmaDecodeStream` (exact/uncapped). Corrupt `.lzma` leaves no partial file; `too_small_size` bad-`*` files still yield their declared size (correct: trailing data ignored). |
| XZ 66 | `CXzTool` / `COpenXz` | `xz`, `txz` | `FD 37 7A 58 5A 00` | `TEMP-only` (`%TEMP%\xzalone` + `%TEMP%\xzfix`): 5/5 + m95 8/8 PASS | Shared `XzDecodeStream` (exact/uncapped). Single LZMA2 filter; None/CRC32/CRC64; dicts 4 KB-64 MB; BCJ/SHA-256/oversize refused. Index accumulator stops before stored CRC; `indexLen` captured before footer take. |
| ZSTD 69 | `CZstdTool` / `COpenZstd` | `zst`, `tzst` | `28 B5 2F FD` (LE32 `FD2FB528`) | `TEMP-only` (`%TEMP%\zstalone` 7 + `%TEMP%\zstfix` 8): all PASS | zstd 1.5.6 subset + clean-room `xxhash64` (canonical vectors). One raw frame, one-shot into `usz`. Multi-frame tolerant streaming for standalone; strict tail. AES-inner untested (no writer). Dict/level edge proofs in-harness. |
| MSCOMPRESS 31 | `CMSCompressTool` / `COpenMSCompress` | any `*_` (last char) | SZDD `53 5A 44 44 88 F0 27 33`+`41` / QB `53 5A 20 88 F0 27 33 D1` / KWAJ `4B 57 41 4A 88 F0 27 D1`+method 0-4 + size guard | `fixtures/mscompress/` (SZDD + KWAJ m1-m4 incl. `licence-szdd.tx_`, `LICENCE-kwaj{2,3,4}.tx_` + `.expected`): 18340 B = oracle | Own `MsCompress.cpp` (no `lz32.dll`): SZDD+QB LZSS; KWAJ 0 stored / 1 XOR / 2 QB-LZSS / 3 LZ+Huffman (six nybbles up front, five trees type 0-3) / 4 MS-ZIP (zlib). Shared `MsCompressMakeName` (listing == extract; `SMALL.TX_` -> `SMALL.TXT`). `szMode` SZDD/SZDD-QB/KWAJ-STO/XOR/LZ/HUF/ZIP. |
| MACBIN 29 | `CMacBinTool` / `COpenMacBin` | `bin` | len/fork sizes, `mBIN`, zeros, CRC | status not separately recorded | MacBinary I/II/III. |
| ATOB 7 | `CAtoBTool` / `COpenAtoB` (Check-gated) | `*.B2A` proves it (`xbtoa Begin` marker) | scan `xbtoa Begin` + CR/LF | `fixtures/b2a/LICENCE.B2A` (`BTOA.EXE` output): 18340 B MATCH | BTOA output IS the xbtoa format, so ATOB is verified although no `.atob` file exists. |
| FISH 18 | `CFishTool` / `COpenFish` (Check-gated) | content-addressed (`begin 86~ `/`24~ `/`4649 ` names the output) | scan `begin ` + `86~ `/`24~ `/`4649 ` + name | no authentic sample; 30-fuzz-adjacent work did not cover it | `Fish=18` lives in both `ZipMethod` enums; keep in sync. |
| BEL 8 | `CBelTool` / `COpenBel` (Check-gated) | `bel` + 32 MB cap | header `LE24` size + `LE16` doff validated in `Extract` | `fixtures/bel/LICENCE.bel`: 18340 B MATCH (VC6+VC4+official) | Fuzz-hardened (fs2/doff/read/repeat clamped; `fread` checked). A hand-made `.bel` crashes official too (bad fixture, not a defect). |
| BOO 10 | `CBooTool` / `COpenBoo` (Check-gated) | `boo` | ext-only | `fixtures/boo/LICENCE.BOO`: 18342 B (18340 exact + 2; all 3 builds agree) | Off-by-two from final group `2Y2@` (decodes 3 bytes where 1 wanted). Left UNFIXED: the leading-char-is-count rule is disproved by the first group. No second sample; documented, not guessed. |
| MIME 30 | `CMimeTool` / `COpenMime` (Check-gated) | `b64`/`mim`/`mime` (ext check commented out; content scan decides) | `DoHeader` base64/Content scan | reports UNKNOWN like official | -- |
| UUDECODE 40 / XXDECODE 53 | `CUueTool`/`CXxeTool` (`CUnixDecoderTool`) / `COpenUue`/`COpenXxe` (Check-gated) | ext `uue`-ish / `xxe`-ish + scan `begin ` | `begin ` line + mode/name parse | `fixtures/uu/LICENCE.uue` -> type 40 18340 B MATCH; `LICENCE.xxe` -> type 53 18340 B MATCH | `sscanf("begin %o ...")` returns 2: test for 1 rejects every payload. Shar reuses `UnixDecodeUueToFile` (standalone providers untouched). |
| SHAR 37 | `CSharTool` / `COpenShar` (Check-gated) | `shar`, `sh` | ext-only | `fixtures/shar/LICENCE-text.shar` 18340 B MATCH; `LICENCE-uu.shar` 18340 B MATCH (official: nothing) | Understands `sed`/`gres`/`cat` + `uudecode` heredoc (`<< 'SHAR_EOF'` quote skipped; stops at first `SHAR_EOF`; branch tested BEFORE `sed`). Deliberate divergence (feature, not defect fix). |

## Disk / image formats

| Type | C++ objects | Ext | Magic | Tests | Remarks |
|---|---|---|---|---|---|
| DMG 56 | `CDmgTool` / `COpenDmg` | `dmg` | Apple `plist` DTD search (`ER\x02\x00` branch / zlib branch) | status not separately recorded | Fully covered by the 8.3 fallback audit (only `pathMake`-guarded + `GetTempFileName` writes). |
| ISO 57 | `CIsoTool` / `COpenIso` | `iso` | `CD001` at sector 16 + vol type 1/2 | status not separately recorded | -- |

## Not claimed

| Format | State |
|---|---|
| PMD (`CPmdTool`) | Registration commented out (`XacRett.cpp`); type 0 (UNKNOWN) is correct. |
| LBR whole-compressed libraries (`.lqr`/`.lzr`/`.lyr`) | Explicitly unsupported: `Check` needs the raw directory control entry at offset 0. Rare in-period, no authentic fixture. |
| JAM Feb/96 (`.jam2`) | No reference implementation in `jam/`; extension left unclaimed rather than mis-decoded. |
| ZIP method 93 inner variants / AES-inner-DCL, dict > 64 MB (LZMA/XZ), `.zst` > `usz` one-shot, >2 GB members, ZIP64 central-dir/attrs/spans, multi-volume ZIP | Refused loudly (skip-and-continue or fail, never wrong bytes). |
| 7z volumes / RAR volumes (non-zero) | Refused up front. |
| EGG solid+encrypted | ALZip cannot produce it; refused. |
| ASK/APK decode-correctness | No authentic sample exists; crash-hardening only. |
| BOO trailing 2 bytes | Documented, unfixed (needs a second sample). |
| AIN multi-volume/SFX/2.2-fragments | Deferred. |
| BZ2 multi-stream | First stream only. |
| AUG96 JAM auto-detect edge (`LICENCE-aug96.jam` scores inside the May/96 range) | Use `.jam8` to force. |
