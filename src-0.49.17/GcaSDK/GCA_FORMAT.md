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
format uses a 1 byte length unless the next byte is `< 0x20`, in which case a
16 bit length follows — that rule does **not** apply here. **[D]**

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

`gcac.exe` 0.9k **only writes** the GCAX/GCA3 container described above.  The
older `"GCA0"` / `"GCA1"` magics are read-only in that binary; they use a
completely different layout (a 36 byte header, 1/2 byte name lengths, a
different directory shape).  `GcaSDK/gca_arc.cpp` still carries a reader for
them.

---

## 8. Reproducing / verifying

```
build.bat                 -> build\gcasdk.lib, build\gcatest.exe
build\gcatest.exe <archive.gca> [outdir] [spell]
```

`spell` is the password (the raw `-p` text, or the lower case hex SHA-1 of the
`-k` key file).  Without it, encrypted archives report `OPEN_FAILED`.

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
