/* Fixed-region layout for uha03_dec4.
 * The globals are one contiguous byte array the typed state owns: the
 * DGROUP slice the decoder loads at 0x428000, the tables it addresses as
 * arrays and the named cells between them share it, so a cell's bytes
 * live in exactly one place.  The range comes from the address map's
 * cell boundaries, the init's fills and the extent of every named cell
 * a table covers; it ends at the last named cell, 0x4450FC.
 *
 * Regenerate uha03_dec4_state.h with:
 *   python tools/gen_dec4_state.py uha03_dec4_addr.h uha03_dec4.c \
 *       uha03_dec4_state.h d3u uha03_dec4_layout.h
 */
0x428000 0x445100
