# Brotli fixtures

- `LICENCE.txt.br`: the 18,340-byte GPLv2 `LICENCE` (the same file used as
  the house oracle throughout this tree) compressed with Python `brotli`
  1.2.0, quality 11, lgwin 22.  Verifies the standalone `.br` provider.
- `tree.tar.br`: a tar of the four files in `expected/` (text, binary,
  empty file, nested path), compressed the same way.  Verifies `.tar.br`.
- `tree.cpio.br`: the same four files as a `newc` cpio, compressed the
  same way.  Verifies `.cpio.br`.

`expected/` holds the original bytes; all three fixtures extract to it
byte for byte.  Python's `brotli` module is the independent oracle - the
in-tree Brotli decoder is the C reference, not the Python binding.
