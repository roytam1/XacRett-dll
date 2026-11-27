# ZIP fixtures

- `PKZ080.ZIP`, `pkz090.zip`: authentic PKZIP fixtures used to verify the
  legacy Shrink (method 1) and Reduce (methods 2-5) decoders, 10 members
  each, CRC-checked against the archives.
- `platform.ja`: a Firefox/UXP platform JAR from the workarea root.  Its
  central directory is at offset 4 (Mozilla's rearranged-container layout)
  and 1325 of its 1481 members use Mozilla's private ZIP method `0x81`
  (`MOZ_JAR_BROTLI`), the rest are stored.  It is the authentic fixture for
  the Brotli decoder; the oracle is an independent Python `brotli`+`zlib`
  decode of the central-directory entries, which also verifies every
  stored CRC-32.  All 1481 extracted files match it byte for byte.
