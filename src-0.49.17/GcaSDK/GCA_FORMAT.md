# GCA archive format (gcac.exe / GCA SDK)

Reverse engineered from `gcac.exe` — "*** GCA(GCompressionArchiver) Version 0.9k",
Copyright (c) 2000,2001 SYN(Shin-ichi TSURUTA).

Everything marked **[V]** below was verified by *running* `gcac.exe` and byte
diffing the archives it produces.  Items marked **[D]** come from the
disassembly (`gcac.exe.lst`, `gcac.exe.c` in the repository root).

The decoder in `GcaSDK/` implements this format; `tests/gcatest.cpp` is the
test driver.

---

## 1. Command line

```
gcac <command> [switches] <archive> <file or folder>

  a   create an archive from a file or folder
  s   create an archive in *solid* mode
  n   create an archive without compression
  e   extract files from an archive
  l   list the contents of an archive

  -o<file>     order/list file: the members to archive, one per line
  -p<password> password for encryption
  -k<keyfile>  key file for encryption
```

Notes:

* `gcac a <file>` derives the archive name from the source (`foo.bin` →
  `foo.gca`).  With two arguments the first is the archive name.
* Only **one** source path per invocation unless `-o` supplies a list file.
  Entries of a list file are stored by their **base name** (the directory
  part is stripped), whereas a folder argument keeps the relative path
  (`gcac a out.gca sub` stores `sub\inner.bin`).
* `-k` is not a different cipher: the key file is hashed with SHA-1 and the
  lower case hex digest is used as the password.  `gcac a x.gca -k key.bin`
  is byte identical to `gcac a x.gca -p <sha1hex(key.bin)>`. **[V]**

---

## 2. File layout

```
offset 0                "GCAX" | u32 0 | u64 dirOffset      (16 bytes)
offset 16               payload region: GCF frames (see §4)
offset dirOffset        "GCA3" directory header              (32 bytes)
offset dirOffset+0x20   entry table (32 bytes x numFiles)
                        names block (u16 length + name, per entry)
EOF
```

There is **no trailer** and no padding. **[V]**

### 2.1 GCAX header (16 bytes)

| off | size | value |
|-----|------|-------|
| +0x00 | 4 | `"GCAX"` |
| +0x04 | 4 | 0 |
| +0x08 | 8 | `dirOffset` — **relative** to the start of this header |

`dirOffset` is relative, not absolute.  `gcac.exe` writes
`directory position - header position` and its own reader adds the archive
start back on.  Verified by prefixing an archive with 100 filler bytes: the
relative interpretation lands exactly on the `"GCA3"` magic, the absolute one
does not. **[V]**

The header is always written in plaintext, even in encrypted archives. **[V]**

### 2.2 GCA3 directory header (32 bytes)

| off | size | value |
|-----|------|-------|
| +0x00 | 4 | `"GCA3"` |
| +0x04 | 4 | flags (see below) |
| +0x08 | 4 | `blockSize` = 32 + bodySize (the whole directory block) |
| +0x0C | 4 | 0 (crypto: part of the key material) |
| +0x10 | 4 | 0 (crypto: part of the key material) |
| +0x14 | 4 | CRC-32 of the directory block |
| +0x18 | 4 | `numFiles` (crypto: part of the key material) |
| +0x1C | 4 | 0 (crypto: part of the key material) |

**Flags** **[V]**

| bit | mask | meaning |
|-----|------|---------|
| 0 | `0x01` | the directory body is compressed |
| 1 | `0x02` | the directory body is encrypted |
| 4 | `0x10` | the payload is compressed |
| 5 | `0x20` | crypto is enabled |
| 8 | `0x100` | solid archive (one shared payload for every member) |

Values actually produced by `gcac.exe` **[V]**:

| archive | flags |
|---------|-------|
| `gcac a` (1-3 members) | `0x10` |
| `gcac a` (>= ~10 members, directory compression pays off) | `0x11` |
| `gcac s` | `0x110` |
| `gcac n` | `0x00` |
| any of the above with `-p` / `-k` | `+0x02` and `+0x20` (e.g. `0x32`) |

**Directory CRC** **[V]**

`CRC32` (the usual zlib/reflected CRC-32, poly `0xEDB88320`, init and final
XOR `0xFFFFFFFF`) over the **32 byte GCA3 header with the CRC field itself
zeroed**, followed by the directory body:

```
CRC32( GCA3[0x00..0x13] || u32 0 || u32 numFiles || u32 0 || body )
```

For an encrypted archive the header used here is the *decrypted* one: the
16 encrypted bytes at `+0x10` decrypt to `(0, dirCRC32, numFiles, 0)`, so
`+0x10..+0x13` and `+0x1C..+0x1F` are zero and `+0x18` holds the real file
count.  The CRC covers the directory only, never the payload.  Verified
against plain, solid and encrypted archives.

### 2.3 Entry table (32 bytes per member)

| off | size | value |
|-----|------|-------|
| +0x00 | 4 | CRC-32 of the **original (uncompressed)** file data |
| +0x04 | 4 | Win32 `dwFileAttributes` (`0x10` = directory) |
| +0x08 | 4 | FILETIME `dwLowDateTime` (last write time) |
| +0x0C | 4 | FILETIME `dwHighDateTime` |
| +0x10 | 8 | original size (u64 LE) |
| +0x18 | 8 | stored size (u64 LE) — bytes this member occupies in the payload |

* The CRC covers the plaintext, not the stored bytes.  It matches
  `zlib.crc32(original)` exactly. **[V]**
* `+0x04` really is the file attributes: a hidden file yields `0x22`, a
  read-only file `0x21`, a normal file `0x20`. **[V]**
* The stored size is the number of bytes the member contributed to the
  payload region (the sum of its frames).  In solid archives it is 0 for
  every member. **[V]**
* An empty file produces **no frame at all** and a stored size of 0. **[V]**

### 2.4 Names block

For each entry, in order: a **u16 little endian** length followed by that many
raw bytes of the stored path.  No NUL terminator.  The bytes are whatever the
platform produced (ANSI code page, no transcoding).  The legacy GCA0/GCA1
containers use a plain **one byte** length instead (see §7). **[V]**

### 2.5 Compressed directory body

When flag `0x01` is set the body (entry table + names) is compressed into two
separately framed blocks **[V]**:

```
u32  rawBodySize          (entries + names, uncompressed)
u32  0
GCF1 frame  -> the entry table          (numFiles * 32 bytes)
GCF1 frame  -> the names block
```

The 8 byte prefix is why the entry table appears at `GCA3 + 0x28` rather than
`GCA3 + 0x20` in that case.

---

## 3. Payload region

The payload starts at `GCAX + 0x10` and runs up to the directory.  It is a
sequence of frames.  With the `n` (non compress) command there is no framing
at all: the raw member bytes are stored verbatim and the `0x10` flag is
clear. **[V]**

---

## 4. Frames

### 4.1 GCF0 — stored (8 byte header)

| off | size | value |
|-----|------|-------|
| +0x00 | 4 | `"GCF0"` |
| +0x04 | 4 | raw length |

followed by that many raw bytes.  Used when the input is <= 8 bytes or when
compression did not pay off. **[V]**

### 4.2 GCF1 — 24 byte header

| off | size | value |
|-----|------|-------|
| +0x00 | 4 | `"GCF1"` |
| +0x04 | 4 | original size of the block |
| +0x08 | 4 | number of literals produced by the LZ stage (= BWT input length) |
| +0x0C | 4 | BWT primary index |
| +0x10 | 4 | length of payload 1 (the stage 1 output) |
| +0x14 | 4 | length of payload 2 (the LZ token stream) |

followed by payload 1 then payload 2. **[V]**

### 4.3 GCF2 — 28 byte header

Identical to GCF1 plus a `mode` word at `+0x18` that selects the stage 1
coders (MTF / RLE on/off, table mixing, adaptation shifts).  **[D]**

### 4.4 Chunking

The input is split into **1 MiB (0x100000) byte** blocks and every block
becomes exactly one frame.  A 3 000 000 byte file produces three frames of
1048576 + 1048576 + 402848 bytes. **[V]**

---

## 5. Compression pipeline

For one block **[D]**:

```
original bytes
   |
   |  LZ stage   (min match length 64, 64 KiB hash table, 2 byte context)
   v
literal stream  +  LZ token stream
   |
   |  BWT of the literal stream  ->  primary index
   v
BWT string
   |
   |  stage 1:  MTF + RLE, range coded over 12 bit probability tables
   v
payload 1  (stage 1)          payload 2  (LZ tokens)
```

The BWT is applied to the **literal** stream, not to the whole file, and the
LZ token stream is the *second* payload.  The decoder in `GcaSDK/` implements
exactly this order.

Probability model: four tables (`t256` 256, `t32` 32, `t64K` 65536,
`t8K` 8192 entries), all initialised to `0x800` (p = 0.5, 12 bit).  The range
coder is a standard 23/24 fraction bit coder.

Solid archives (`gcac s`) do **not** share an LZ dictionary: the hash table
and the probability tables are reset per frame.  "Solid" only means that all
members are concatenated into one stream and cut into 1 MiB frames, so a
member may straddle a frame boundary. **[D]**

---

## 6. Encryption

`gcac.exe` encrypts with a Mersenne-Twister based keystream.  The cipher
object is the "GflGCACryptograph" class at archiver offset `0x524`.

### 6.1 Keys

| region | key |
|--------|-----|
| payload of member *i* (sequential) | `<password>` + `<stored name of member i>` |
| payload (solid) | `<password>` |
| GCA3 +0x10..+0x1F and the directory body | `<password>` + `"Header"` |

An empty password is replaced by `"GflGCACryptograph"`.  The archive path and
the source directory do **not** influence the ciphertext; the stored member
name does.  **[V]**

### 6.2 What is encrypted

| region | encrypted |
|--------|-----------|
| GCAX header (16 bytes) | no |
| GCA3 +0x00..+0x0F | no |
| GCA3 +0x10..+0x1F | yes (keystream bytes 0..15) |
| GCA3 +0x20.. (directory body) | yes (keystream bytes 16..) |
| payload region | yes (own keystream per member) |

The plaintext of the 16 encrypted header bytes is `(0, dirCRC32, numFiles, 0)`
— the same information the unencrypted header would carry.  **[V]**

### 6.3 The keystream

```
state[624]  <- derived from the key (see below)
twist with 0x9908B0DF, temper with 0x9D2C5680 / 0xEFC60000   (MT19937)

every 4 output bytes:
    w[0..7] = 8 tempered generator words
    W = ROL32(w0 ^ w2, rot(w1 ^ w3)) ^ ROL32(w4 ^ w6, rot(w5 ^ w7))
    rot(x): t = x ^ (x >> 16);  rot = (t & 0x1F) ^ ((t >> 8) & 0x1F)
    emit W's bytes least significant first
```

Key derivation (624 dwords of generator state) **[D]**:

```
v19[0..623] = 0
for i, c in enumerate(key):
    idx = (i + 1) % 624
    v19[idx] = (v19[idx] << 8) ^ (c * 0x01010101)      # c sign extended

s = 4357
for i in 0..623:
    s2     = 69069 * s + 1
    state[i] = (s & 0xFFFF0000) | (s2 >> 16)
    s      = 69069 * s2 + 1
    state[i] ^= v19[i % min(len(key) + 1, 624)]
```

`GcaSDK/gca_crypt.cpp` implements this; `GcaSDK/gca_sha1.cpp` implements the
SHA-1 used by `-k`.

---

## 7. Legacy containers

`gcac.exe` 0.9k **only writes** the GCAX/GCA3 container described above, but it
**reads** two older layouts:

| magic | meaning |
|-------|---------|
| `GCA0` | legacy container, normal (one payload stream per member) |
| `GCA1` | legacy container, solid (one payload stream for every member) |
| `GCA3` | a directory block standing on its own, without a GCAX envelope — **not supported**, see §7.5 |

The layout was confirmed against two real archives, kept in `fixtures/`:
`fixtures/gcasdk_v09d.gca` (GCA1, written by the GCA SDK 0.9d) and
`fixtures/gcatxt.gca` (GCA0, written by the GCA SDK 0.9e).  `gcac.exe`,
the C tools in `cgca/` and the C++ SDK all extract both of them byte
identically. **[V]**

Both legacy layouts have a header variant word at `+0x04`:

* **variant 2** — the directory is stored in the clear;
* **variant 3** — the directory is stored as two `GCF1` frames.

### 7.1 Variant 2 — the plain directory

```
+0x00  4  "GCA0" / "GCA1"
+0x04  4  the header variant (2)
+0x08  4  the number of members
+0x0C  4  hdr — the offset of the payload region
+0x10  4  flags (meaning unknown)
+0x14  4  the CRC-32 of the first member's content
+0x18  4  (unidentified)
+0x1C  4  the first member's stored size
+0x20  4  the first member's original size
+0x24  .. the directory
```

The directory:

```
(cnt-1) 32 byte entries
the first member's 12 byte record: {u32 32, u32 FILETIME low, u32 FILETIME high}
the names pool: cnt times (one byte length + the name)
4 zero bytes
u32  the CRC-32 of everything from the start of the file up to and
     including those 4 zero bytes
```

The 32 byte entries:

| off | value |
|-----|-------|
| +0x00 | 32 (the entry size) |
| +0x04 | FILETIME low |
| +0x08 | FILETIME high |
| +0x0C | `dwFileAttributes` |
| +0x10 | CRC-32 of the content |
| +0x14 | (unused) |
| +0x18 | the stored size |
| +0x1C | the original size |

### 7.2 Variant 3 — the compressed directory

```
+0x00  4  "GCA0" / "GCA1"
+0x04  4  the header variant (3)
+0x08  4  the number of members
+0x0x0C 4  hdr — the offset of the payload region
+0x10  4  flags (meaning unknown)
+0x14  .. a GCF1 frame holding the entry table (cnt * 32 bytes)
         a GCF1 frame holding the names pool
         4 zero bytes
         u32 the CRC-32 of everything before it
```

The 32 byte entries here have a different field order:

| off | value |
|-----|-------|
| +0x00 | `dwFileAttributes` |
| +0x04 | CRC-32 of the content |
| +0x08 | 0 |
| +0x0C | the stored size |
| +0x10 | the original size |
| +0x14 | 32 (the entry size) |
| +0x18 | FILETIME (low, high) |

### 7.3 The payload

The payload region starts at `hdr` and holds ordinary frames — `GCF0`,
`GCF1` or `GCF2`.  In a `GCA1` (solid) archive the whole region is one
stream shared by every member and the members are sliced by their original
sizes; in a `GCA0` archive the members follow each other, each occupying
the number of bytes given in its entry.  The sample
`fixtures/gcasdk_v09d.gca` uses
a single `GCF2` frame with the mode word `0x00201030`.

### 7.4 The trailer

Both variants end their directory with four zero bytes followed by a
CRC-32.  The CRC covers **everything from the start of the file up to and
including the four zero bytes**, i.e. the header and the whole directory
but not the CRC field itself.  This is the same convention as the GCA3
directory CRC (§2.2).  **[V]**

### 7.5 A standalone "GCA3" file — not supported

`"GCA3"` is the **directory block** magic, not a file container.  Inside
a GCAX archive it sits at `dirOffset`, after the payload; it never
appears at offset 0 of a well formed archive.

`gcac.exe`'s reader nevertheless dispatches on a file that *starts* with
`"GCA3"`: it seeks to the start and hands the file to the same routine
that reads an embedded directory block (`gcac.exe.lst:40AC14` →
`sub_40B9E0`).  That routine takes the origin of the payload to be the
block position plus `0x10` (`gcac.exe.lst:40BA49`) — the same value it
uses for the GCAX envelope (`gcac.exe.lst:40ACBF`) — which only makes
sense for an enveloped archive.

Synthesising a standalone GCA3 file from a real archive shows that the
path is not functional:

| layout | what gcac.exe does |
|--------|--------------------|
| the directory block first, the payload behind it | reads the directory correctly, then decodes the payload from the wrong offset and writes a garbage file |
| the payload first, the directory block behind it | rejects the file outright |

**We do not implement this variant.**  `GcaSDK` returns
`GCA_PARSE_UNSUPPORTED` for a top level `"GCA3"`, and `gcac_unpack`
reports it as an unsupported container.  A standalone GCA3 file is
treated as an unknown format rather than a GCA archive.  **[V]**

---

## 8. Implementations in this repository

| where | what |
|---|---|
| `GcaSDK/` | the C++ decoder (nsGcaSDK compatible API) |
| `cgca/` | a portable **C** packer and unpacker (`gcac_pack`, `gcac_unpack`) |

The C tools are interoperable with `gcac.exe` in both directions: archives
created by `gcac_pack` are extracted correctly by `gcac.exe`, and archives
created by `gcac.exe` are extracted correctly by `gcac_unpack`.  This was
verified for the `a`/`s`/`n` commands, for plain, `-p` and `-k` encryption,
and for single and multi member archives.  They also read the legacy
`GCA0`/`GCA1` containers (§7).

```
cd cgca
build.bat          (Visual C++ 6)   or   make   (anything else)
build\gcac_pack.exe   out.gca  file.txt folder\
build\gcac_unpack.exe out.gca -o dest

python roundtrip.py    # packs, extracts with gcac.exe and with the C
                       # tools and compares everything byte for byte
```

## 9. Reproducing / verifying

```
build.bat                 -> build\gcasdk.lib, build\gcatest.exe
build\gcatest.exe <archive.gca> [outdir] [spell]
```

`spell` is the password (the raw `-p` text, or the lower case hex SHA-1 of the
`-k` key file).  Without it, encrypted archives report `OPEN_FAILED`.

The two legacy archives in `fixtures/` are checked by `cgca/roundtrip.py`
as well:

```
build\gcac_unpack.exe fixtures\gcasdk_v09d.gca -o dest   # GCA1, solid
build\gcac_unpack.exe fixtures\gcatxt.gca       -o dest   # GCA0, plain
```

The corpus used while writing this document was produced with:

```
gcac a out.gca      rnd.bin                 # random data  -> GCF0 (stored)
gcac a txt.gca      text.txt                # text         -> GCF1
gcac a pat.gca      pat.bin                 # patterned    -> GCF1, LZ heavy
gcac a multi.gca    -o list.txt             # several members
gcac s multiS.gca   -o list.txt             # solid
gcac n nc.gca       -o list.txt             # non compress
gcac a mm.gca       -o list12.txt           # compressed directory body
gcac a pw.gca       -p secret123  rnd.bin   # encrypted
gcac a kf.gca       -k key.bin     rnd.bin   # key file
```

All of them parse and extract byte identically with the SDK decoder.
