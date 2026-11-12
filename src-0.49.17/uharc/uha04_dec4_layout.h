/* Fixed-region layout of the 0.4 core-4 decoder.
 *
 * The BSS above the DGROUP image is one contiguous byte array:
 * the tables the decoder addresses as arrays and the named cells between
 * them share it, so a cell's bytes live in exactly one place.  The
 * range comes from the address map's cell boundaries, the init's
 * d4v_fill / d4vc_fill16 plane lengths and the extent of every named
 * cell a table covers - see DEC4_CLEAN_PLAN.md.
 *
 * Regenerate uha04_dec4_state.h with:
 *   python tools/gen_dec4_state.py uha04_dec4_addr.h uha04_dec4.c \
 *       uha04_dec4_state.h d4v uha04_dec4_layout.h
 */
0x42E000 0x45D0C8
