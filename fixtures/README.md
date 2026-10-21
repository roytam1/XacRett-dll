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
| `cab/` | one cabinet per compression method: `LICENCE-lzx.cab`, `LICENCE-mzsip.cab`, `LICENCE-quantum.cab` | made with `makecab` | the Cabinet decoders (none, MSZIP, LZX, Quantum) all give the same 18340 bytes as the `LICENCE` member of `fixtures/rar/GPLv2-rar4.rar` |
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
| `yz1/` | `yzenc.yz1`, `yzencpw.yz1` and `lib.yz1` | the `yzenc.exe` in the workarea root | the YZ1 provider, plain and with a password |
| `lzip/` | `nicfix.tar.lz` and `lzsfx33.lzs` | an upstream lzip file and the lhasa corpus | the lzip provider |

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
