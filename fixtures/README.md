# fixtures

The sample archives that the providers are verified against.  They used
to live untracked in the workarea root, which meant that the evidence
for the work was not in the repository; everything here is tracked, so
a fresh checkout can reproduce the measurements.

Every file is a *sample*, not a test harness: the harnesses (the
`dlltest` driver, the matrix scripts) live in TEMP and are described
in `AGENTS.md`.  What is here is the input side of those checks.

## The groups

| directory | what it holds | where it came from | what it proves |
|---|---|---|---|
| `mscompress/` | the `COMPRESS.EXE` samples: `SZDD` (`*.TX_`), `KWAJ` methods 1-4 and the `.expected` files with the original bytes | made with the format description, and the `KWAJ` ones by the period tools | the SZDD and the KWAJ decoders give the original bytes back |
| `cab/` | one cabinet per compression method and per window size: `LICENCE-mzsip.cab` (MSZIP), `LICENCE-x15.cab` .. `LICENCE-x20.cab` and `LICENCE-lzx.cab` (LZX, window bits 15 to 21), `LICENCE-q15.cab` .. `LICENCE-q20.cab` and `LICENCE-quantum.cab` (Quantum, window bits 15 to 21) | made with `makecab` | the Cabinet decoders (none, MSZIP, LZX, Quantum) all give the same 18340 bytes as the `LICENCE` member of `fixtures/rar/GPLv2-rar4.rar` |
| `rar/` | the RAR 1.5/2.0/2.9/4.x/5.x samples and the 72 member `aida64.rar` | `rar15.rar` ... `rar29.rar` from the era, `GPLv2-rar4/5.rar` and `aida64.rar` made for this tree | the RAR provider, and the RAR5 listing |
| `7z/` | `aida64.7z` and `7z-zstd.7z` (the authentic 7-Zip 19.00 + zstd archive) | the 7-Zip command line tool | the 7z provider, including the zstd coder |
| `zip/` | `PKZ080.ZIP` and `pkz090.zip` (the old PKZIP samples with the shrink and the reduce methods), plus `aida64.zip` | the PKZIP distribution | the legacy ZIP methods, and the long name handling |
| `lzh/` | the whole fragglet/lhasa test corpus: `archives/` and the expected `output/` | the upstream lhasa checkout | the LZH decoder, member for member, against the oracle's own expected names and sizes |
| `asd/` | `LICENCE-015.asd` (0.1.5), `LICENCE-020.asd` (0.2.0) and `multi.asd` | the ASD 0.2.0 archiver | the ASD provider, both versions |
| `jam/` | `LICENCE-may96.jam`, `LICENCE-aug96.jam` and `M_PLAIN` | the `jam` package in the workarea root, built with its own `JAM.COM` | the JAM provider: the May/96 and the Aug/96 variants |
| `shar/` | `LICENCE-text.shar` and `LICENCE-uu.shar` | made with GNU shar | the shar provider, including the uudecode payload |
| `uu/`, `bel/`, `boo/`, `jak/`, `b2a/` | `LICENCE.uue`, `LICENCE.xxe`, `LICENCE.bel`, `LICENCE.BOO`, `LICENCE1.jak`, `LICENCE2.jak`, `LICENCE.B2A` | the period encoders (uuencode, `BTOA.EXE`, `MSBMKB.COM`, Jack32) | those providers, and that their output is the same `LICENCE` file |
| `ain/` | `LICENCE.ARG` | the AIN archiver | the AIN provider |
| `lbr/` | `crlzh20.lbr` | the 80un CP/M tools | the CP/M LBR provider, the CrLZH variant |
| `jcalg1/` | `jcalg1_test.jc1` | the jcalg1 reference compressor | the JCALG1 provider |
| `dgd/` | the six spanning files: the plain pair, the pair with the recovery data and the encrypted pair (all of `16Edit_axp32.exe`, 86016 bytes) | made with the original GUI | the DGD provider: every part set gives the same byte-exact file, the encrypted one with `-ppassword`, and the `.dgdr` repairs a damaged part |
| `dgc/` | the twelve DGCA archives: the plain, the stored, the "one block per file", the "beta" and the "v100e" variants, the 5981 member `names.dgc`, the encrypted one, the `.dgcr` with the recovery data, and `dgca110-column.dgc` (one 131072 byte 16 bit matrix, stored as the "column" method with stride 12) | made with `dgcac.exe` 1.10 (`dgcac a dgca110-column.dgc mat16.bin`) | the DGC provider: every file comes out byte for byte identical to what the recon's `dgcunp.c` writes, the encrypted one decrypts with `-ppassword`, the `.dgcr` repairs a damaged archive, and `dgca110-column.dgc` pins the column method's short last row (`total % stride` = 8, which a `total/stride` de-interleave silently truncates) |
| `gca/` | the plain, the password and the key file archives of the `ctest` corpus plus the `key.bin` they need, and the legacy set shared with the `gca` project: `gcatxt.gca` (GCA0), `gcasdk_v09d.gca` (GCA1 solid), `gcacv09e_{plain,solid,none,encrypted,encrypted_solid,encrypted_key}.gca` and `gcacv09e_key.bin` | `gcac.exe` 0.9k for the modern ones, `gcac_v09e.exe` 0.9e for the legacy ones | the GCA provider: the `GCA3` container and the encrypted archives (`-p secret123`, `-p#key.bin`), the legacy `GCA0`/`GCA1` containers including the encrypted version 6 (`-p fixture`, `-p#gcacv09e_key.bin`), and that a directory member is extracted as a directory |
| `yz1/` | `yzenc.yz1`, `yzencpw.yz1`, `lib.yz1`, and `yzenc-empty.yz1` / `yzenc-mixed.yz1` (an archive whose members are all empty, and one with a 5 byte member plus two empty ones in a subdirectory) | the `yzenc.exe` 1.05a in the workarea root | the YZ1 provider, plain and with a password, and that zero-length members are extracted (the all-empty case has `all_size == head_size` and used to be refused) |
| `lzip/` | `nicfix.tar.lz` and `lzsfx33.lzs` | an upstream lzip file and the lhasa corpus | the lzip provider |
| `compact/` | `LICENCE.txt.C` (Unix `compact` format, `ff 1f` magic) | added for the CompactC provider | the CompactC provider lists it (`LICENCE.txt`, `orig=0` -- the format stores no size) and extracts the 18340-byte `LICENCE` |
| `packz/` | `LICENCE.txt.Z` (PACKZ format, `1f 1e` magic) | added for the Packz provider | the Packz provider lists it (`LICENCE.txt`, `orig=0`) and extracts the 18340-byte `LICENCE` |

## The oracle

Most of the samples hold the same file: the `LICENCE` text of the
`GPLv2-rar4.rar` member, 18340 bytes, sha256 `6C4BEACE9AD98F65...`.
That is the point: one file, many formats, so a decoder can be checked
against a known answer without needing a reference implementation of
that format.

## What is NOT here

* The reference tools.  The `jam` package, the `lhasa` checkout, the
  `asd_src` sources, `yzdec.c`, `msdos.exe` and the DOS binaries stay
  untracked in the workarea root: they are upstream sources or
  third party programs, not samples of this tree's formats.
* The generated fixtures.  The ZIP method 10/14/18/93/95/98/99
  matrices, the lzip, xz, zstd, tar and 7z matrices and the ARC
  samples are generated on the fly by the scripts in TEMP, because they
  are large and easy to rebuild.
* The things that are gone.  `LIBCMT.LIB`, `cpmshar.lbr` and
  `jcalg1_test.exe` are referenced by older notes in `AGENTS.md` but
  are not in the tree any more.
* The rest of the GCA corpus.  The `gca/` directory holds the small
  members of the `ctest` corpus, which is what covers the plain, the
  password and the key file cases; the big ones (the same cases with a
  2.5 MB payload) and the wider corpus stay with the project that
  reverse engineered the format, together with its own test driver.
