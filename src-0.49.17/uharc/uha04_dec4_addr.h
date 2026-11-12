/* UHARC 0.4 core-4 (uha04_dec4.c) arena address names. See DEC4_ADDRMAP.md.
 * Phase 1 naming layer: each #define preserves the exact original
 * 32-bit DGROUP address so G/M macro expansions are unchanged. */
#ifndef UHA04_DEC4_ADDR_H
#define UHA04_DEC4_ADDR_H

/* --- range coder, I/O, dict, block pointers (semantic) --- */
#define D4V_DGROUP_BASE 0x42E000UL /* orig 0x42E000 -- DGROUP image base (memcpy) */
#define D4V_TAIL_GATE_FLAG 0x431209UL /* orig 0x431209 -- tail-scan gate */
#define D4V_PREFIX_SUM_TMP 0x43120CUL /* orig 0x43120C -- running prefix sum -> BA50 */
#define D4V_BLK_PTR_00_DICT 0x433B40UL /* orig 0x433B40 -- dict+0x100 */
#define D4V_BLK_PTR_01 0x433B44UL /* orig 0x433B44 -- 0x800000 fill 0x100000 */
#define D4V_BLK_PTR_02 0x433B48UL /* orig 0x433B48 -- 0x140000 */
#define D4V_BLK_PTR_03 0x433B4CUL /* orig 0x433B4C -- 0x200000 fill FF */
#define D4V_BLK_PTR_04 0x433B50UL /* orig 0x433B50 -- 0x200000 fill FF */
#define D4V_BLK_PTR_05 0x433B54UL /* orig 0x433B54 -- 0x40000 PPM ctx */
#define D4V_BLK_PTR_06 0x433B58UL /* orig 0x433B58 -- order table, stride 4 */
#define D4V_BLK_PTR_07 0x433B5CUL /* orig 0x433B5C -- PPM 0x80000 */
#define D4V_BLK_PTR_08 0x433B60UL /* orig 0x433B60 -- PPM 0x80000 */
#define D4V_BLK_PTR_09 0x433B64UL /* orig 0x433B64 -- PPM 0x40000 */
#define D4V_BLK_PTR_10 0x433B68UL /* orig 0x433B68 -- PPM 0x40000 */
#define D4V_BLK_PTR_11 0x433B6CUL /* orig 0x433B6C -- PPM 0x40000 */
#define D4V_BLK_PTR_12 0x433B70UL /* orig 0x433B70 -- PPM 0x1C0000 */
#define D4V_BLK_PTR_13 0x433B74UL /* orig 0x433B74 -- PPM/emit 0x40000 */
#define D4V_BLK_PTR_14 0x433B78UL /* orig 0x433B78 -- PPM u16 model 0x61A00 */
#define D4V_BLK_PTR_15 0x433B7CUL /* orig 0x433B7C -- PPM u16 model 0x61A00 */
#define D4V_BLK_PTR_16 0x433B80UL /* orig 0x433B80 -- S4 model 0x0C000 */
#define D4V_BLK_PTR_17 0x433B84UL /* orig 0x433B84 -- u16 counts 0x5FE */
#define D4V_BLK_PTR_18 0x433B88UL /* orig 0x433B88 -- S4 model 0x0C0 */
#define D4V_BLK_PTR_19 0x433B8CUL /* orig 0x433B8C -- S4 model 0x0C0 */
#define D4V_IO_ERR 0x436EC8UL /* orig 0x436EC8 -- sticky 3 EOF / 6 emit */
#define D4V_IO_IN_PTR 0x436ED0UL /* orig 0x436ED0 -- in-buffer base (=p_inb) */
#define D4V_IO_OUT_PTR 0x436ED4UL /* orig 0x436ED4 -- out-buffer base (=p_outb) */
#define D4V_TBL_PRICE_SRC 0x437966UL /* orig 0x437966 -- price src (BSS-zero) */
#define D4V_TBL_PRICE_SUM 0x437968UL /* orig 0x437968 -- price prefix sums */
#define D4V_PRICE_DONE 0x43F968UL /* orig 0x43F968 -- one-time flag */
#define D4V_IO_SNAP_IN 0x440E70UL /* orig 0x440E70 -- refill snapshot in */
#define D4V_IO_SNAP_OUT 0x440E74UL /* orig 0x440E74 -- refill snapshot out */
#define D4V_ALLOC_CURSOR 0x440E78UL /* orig 0x440E78 -- bump cursor */
#define D4V_ALLOC_HI16 0x440E7AUL /* orig 0x440E7A -- cursor hi (==440E78+2) */
#define D4V_TBL_NEG_44D920 0x44D920UL /* orig 0x44D920 -- neg/identity -x/x */
#define D4V_RSLOT_LO 0x45AF68UL /* orig 0x45AF68 -- range slot lo */
#define D4V_RSLOT_MID 0x45AF6CUL /* orig 0x45AF6C -- range slot mid */
#define D4V_RSLOT_HI 0x45AF70UL /* orig 0x45AF70 -- range slot hi */
#define D4V_FILETAB_PTR 0x45AFB0UL /* orig 0x45AFB0 -- file-boundary tab */
#define D4V_SHIFT_R0 0x45BA18UL /* orig 0x45BA18 -- LZP shift reg 0 */
#define D4V_SHIFT_R1 0x45BA1CUL /* orig 0x45BA1C -- LZP shift reg 1 */
#define D4V_SHIFT_R2 0x45BA20UL /* orig 0x45BA20 -- LZP shift reg 2 */
#define D4V_DICT_SIZE 0x45BA30UL /* orig 0x45BA30 -- dict size 1<<(10+m%16) */
#define D4V_DICT_CURSOR 0x45BA34UL /* orig 0x45BA34 -- dict cursor (36x in lzp) */
#define D4V_DICT_MASK 0x45BA3CUL /* orig 0x45BA3C -- dict mask size-1 */
#define D4V_LZP_TRIGGER 0x45BA50UL /* orig 0x45BA50 -- 45CD28>= -> lzp (=43120C) */
#define D4V_IO_OUT_CAP 0x45CD10UL /* orig 0x45CD10 -- out-cap 65536 flush thr */
#define D4V_IO_OUT_LEN 0x45CD14UL /* orig 0x45CD14 -- out_len terminator */
#define D4V_IO_IN_POS 0x45CD18UL /* orig 0x45CD18 -- in-buffer pos */
#define D4V_IO_OUT_POS 0x45CD1CUL /* orig 0x45CD1C -- out-buffer pos */
#define D4V_IO_IN_VALID 0x45CD20UL /* orig 0x45CD20 -- bytes valid in inb */
#define D4V_IO_IN_TOTAL 0x45CD24UL /* orig 0x45CD24 -- total consumed */
#define D4V_IO_OUT_TOTAL 0x45CD28UL /* orig 0x45CD28 -- total produced */
#define D4V_IO_IN_CAP 0x45CD2CUL /* orig 0x45CD2C -- read size 65536 */
#define D4V_RC_CARRY_CNT 0x45D0B0UL /* orig 0x45D0B0 -- FF run count (esi) */
#define D4V_RC_CODE_LO 0x45D0B4UL /* orig 0x45D0B4 -- 64-bit code lo */
#define D4V_RC_CODE 0x45D0B8UL /* orig 0x45D0B8 -- decode code reg */
#define D4V_RC_RANGE 0x45D0BCUL /* orig 0x45D0BC -- decode range (0xFFFFFFFF) */
#define D4V_RC_PENDING_B 0x45D0C0UL /* orig 0x45D0C0 -- pending byte >>24 */
#define D4V_RC_CODE_HI 0x45D0C4UL /* orig 0x45D0C4 -- 64-bit code hi */

/* --- remaining arena cells (generic names; do NOT guess semantics) --- */
#define D4V_TBL_433B90 0x433B90UL /* orig 0x433B90 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_433BA4 0x433BA4UL /* orig 0x433BA4 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_433BE8 0x433BE8UL /* orig 0x433BE8 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_433E77 0x433E77UL /* orig 0x433E77 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_433E8C 0x433E8CUL /* orig 0x433E8C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_433FCC 0x433FCCUL /* orig 0x433FCC -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_43404C 0x43404CUL /* orig 0x43404C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_43407C 0x43407CUL /* orig 0x43407C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_43408C 0x43408CUL /* orig 0x43408C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_43409C 0x43409CUL /* orig 0x43409C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_43419C 0x43419CUL /* orig 0x43419C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_43429C 0x43429CUL /* orig 0x43429C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_4342C4 0x4342C4UL /* orig 0x4342C4 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_4342EC 0x4342ECUL /* orig 0x4342EC -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_434314 0x434314UL /* orig 0x434314 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_43431E 0x43431EUL /* orig 0x43431E -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_434328 0x434328UL /* orig 0x434328 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_434338 0x434338UL /* orig 0x434338 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_434438 0x434438UL /* orig 0x434438 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_434538 0x434538UL /* orig 0x434538 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_434550 0x434550UL /* orig 0x434550 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_434D50 0x434D50UL /* orig 0x434D50 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_435150 0x435150UL /* orig 0x435150 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_435550 0x435550UL /* orig 0x435550 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_435950 0x435950UL /* orig 0x435950 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_435D50 0x435D50UL /* orig 0x435D50 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_436150 0x436150UL /* orig 0x436150 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_436152 0x436152UL /* orig 0x436152 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_4362C0 0x4362C0UL /* orig 0x4362C0 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_436450 0x436450UL /* orig 0x436450 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_43691C 0x43691CUL /* orig 0x43691C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_436976 0x436976UL /* orig 0x436976 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_436B8A 0x436B8AUL /* orig 0x436B8A -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_436ED8 0x436ED8UL /* orig 0x436ED8 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_451920 0x451920UL /* orig 0x451920 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_451924 0x451924UL /* orig 0x451924 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_451928 0x451928UL /* orig 0x451928 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45192C 0x45192CUL /* orig 0x45192C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_451930 0x451930UL /* orig 0x451930 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_451934 0x451934UL /* orig 0x451934 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_451938 0x451938UL /* orig 0x451938 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45193C 0x45193CUL /* orig 0x45193C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_451B3C 0x451B3CUL /* orig 0x451B3C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_451D3C 0x451D3CUL /* orig 0x451D3C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_451D40 0x451D40UL /* orig 0x451D40 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_451D44 0x451D44UL /* orig 0x451D44 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_451D48 0x451D48UL /* orig 0x451D48 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_451D4C 0x451D4CUL /* orig 0x451D4C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_451D50 0x451D50UL /* orig 0x451D50 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_451D54 0x451D54UL /* orig 0x451D54 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_452154 0x452154UL /* orig 0x452154 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_453560 0x453560UL /* orig 0x453560 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_453564 0x453564UL /* orig 0x453564 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_453568 0x453568UL /* orig 0x453568 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45356C 0x45356CUL /* orig 0x45356C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_453570 0x453570UL /* orig 0x453570 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_453574 0x453574UL /* orig 0x453574 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_453578 0x453578UL /* orig 0x453578 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45357C 0x45357CUL /* orig 0x45357C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_453580 0x453580UL /* orig 0x453580 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_453584 0x453584UL /* orig 0x453584 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_453588 0x453588UL /* orig 0x453588 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45358C 0x45358CUL /* orig 0x45358C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_453590 0x453590UL /* orig 0x453590 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_453594 0x453594UL /* orig 0x453594 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_453598 0x453598UL /* orig 0x453598 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45375C 0x45375CUL /* orig 0x45375C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_453760 0x453760UL /* orig 0x453760 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_455F60 0x455F60UL /* orig 0x455F60 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_456F78 0x456F78UL /* orig 0x456F78 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_457360 0x457360UL /* orig 0x457360 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_457364 0x457364UL /* orig 0x457364 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_457368 0x457368UL /* orig 0x457368 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45736C 0x45736CUL /* orig 0x45736C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_457370 0x457370UL /* orig 0x457370 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_457374 0x457374UL /* orig 0x457374 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_457378 0x457378UL /* orig 0x457378 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_458778 0x458778UL /* orig 0x458778 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_458788 0x458788UL /* orig 0x458788 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_458794 0x458794UL /* orig 0x458794 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_458798 0x458798UL /* orig 0x458798 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45879C 0x45879CUL /* orig 0x45879C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_458B94 0x458B94UL /* orig 0x458B94 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_458B98 0x458B98UL /* orig 0x458B98 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_458B9C 0x458B9CUL /* orig 0x458B9C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_458F94 0x458F94UL /* orig 0x458F94 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_458F98 0x458F98UL /* orig 0x458F98 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_459398 0x459398UL /* orig 0x459398 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45939C 0x45939CUL /* orig 0x45939C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_4593A0 0x4593A0UL /* orig 0x4593A0 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_4597A0 0x4597A0UL /* orig 0x4597A0 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_459BA0 0x459BA0UL /* orig 0x459BA0 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_459BA4 0x459BA4UL /* orig 0x459BA4 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_459BA8 0x459BA8UL /* orig 0x459BA8 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_459E38 0x459E38UL /* orig 0x459E38 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_459E3C 0x459E3CUL /* orig 0x459E3C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_459E3E 0x459E3EUL /* orig 0x459E3E -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_459E40 0x459E40UL /* orig 0x459E40 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_459E42 0x459E42UL /* orig 0x459E42 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_459E44 0x459E44UL /* orig 0x459E44 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_459E48 0x459E48UL /* orig 0x459E48 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_459E4C 0x459E4CUL /* orig 0x459E4C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_459E50 0x459E50UL /* orig 0x459E50 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_459E54 0x459E54UL /* orig 0x459E54 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_459E58 0x459E58UL /* orig 0x459E58 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45A258 0x45A258UL /* orig 0x45A258 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45A25D 0x45A25DUL /* orig 0x45A25D -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45A25F 0x45A25FUL /* orig 0x45A25F -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45A267 0x45A267UL /* orig 0x45A267 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45A268 0x45A268UL /* orig 0x45A268 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45A270 0x45A270UL /* orig 0x45A270 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45A272 0x45A272UL /* orig 0x45A272 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45A278 0x45A278UL /* orig 0x45A278 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45A27A 0x45A27AUL /* orig 0x45A27A -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45A27C 0x45A27CUL /* orig 0x45A27C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45A27E 0x45A27EUL /* orig 0x45A27E -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45A280 0x45A280UL /* orig 0x45A280 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45A281 0x45A281UL /* orig 0x45A281 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AA7A 0x45AA7AUL /* orig 0x45AA7A -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AA7C 0x45AA7CUL /* orig 0x45AA7C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AA7E 0x45AA7EUL /* orig 0x45AA7E -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AA80 0x45AA80UL /* orig 0x45AA80 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AA82 0x45AA82UL /* orig 0x45AA82 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AC78 0x45AC78UL /* orig 0x45AC78 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AC7A 0x45AC7AUL /* orig 0x45AC7A -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AC7C 0x45AC7CUL /* orig 0x45AC7C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AC7E 0x45AC7EUL /* orig 0x45AC7E -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AE80 0x45AE80UL /* orig 0x45AE80 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AF00 0x45AF00UL /* orig 0x45AF00 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AF04 0x45AF04UL /* orig 0x45AF04 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AF08 0x45AF08UL /* orig 0x45AF08 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AF0C 0x45AF0CUL /* orig 0x45AF0C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AF10 0x45AF10UL /* orig 0x45AF10 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AF14 0x45AF14UL /* orig 0x45AF14 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AF28 0x45AF28UL /* orig 0x45AF28 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AF2C 0x45AF2CUL /* orig 0x45AF2C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AF30 0x45AF30UL /* orig 0x45AF30 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AF34 0x45AF34UL /* orig 0x45AF34 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AF38 0x45AF38UL /* orig 0x45AF38 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AF3C 0x45AF3CUL /* orig 0x45AF3C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AF40 0x45AF40UL /* orig 0x45AF40 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AF44 0x45AF44UL /* orig 0x45AF44 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AF48 0x45AF48UL /* orig 0x45AF48 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AF4C 0x45AF4CUL /* orig 0x45AF4C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AF50 0x45AF50UL /* orig 0x45AF50 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AF54 0x45AF54UL /* orig 0x45AF54 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AF58 0x45AF58UL /* orig 0x45AF58 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AF5C 0x45AF5CUL /* orig 0x45AF5C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AF60 0x45AF60UL /* orig 0x45AF60 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AF64 0x45AF64UL /* orig 0x45AF64 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AF74 0x45AF74UL /* orig 0x45AF74 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AF78 0x45AF78UL /* orig 0x45AF78 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AF7C 0x45AF7CUL /* orig 0x45AF7C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AF80 0x45AF80UL /* orig 0x45AF80 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AF84 0x45AF84UL /* orig 0x45AF84 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AF88 0x45AF88UL /* orig 0x45AF88 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AF8C 0x45AF8CUL /* orig 0x45AF8C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AF90 0x45AF90UL /* orig 0x45AF90 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AF94 0x45AF94UL /* orig 0x45AF94 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AF98 0x45AF98UL /* orig 0x45AF98 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AF9C 0x45AF9CUL /* orig 0x45AF9C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AFA0 0x45AFA0UL /* orig 0x45AFA0 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AFA4 0x45AFA4UL /* orig 0x45AFA4 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AFA8 0x45AFA8UL /* orig 0x45AFA8 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AFAC 0x45AFACUL /* orig 0x45AFAC -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AFB4 0x45AFB4UL /* orig 0x45AFB4 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AFB8 0x45AFB8UL /* orig 0x45AFB8 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AFBC 0x45AFBCUL /* orig 0x45AFBC -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AFC0 0x45AFC0UL /* orig 0x45AFC0 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AFC4 0x45AFC4UL /* orig 0x45AFC4 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AFC8 0x45AFC8UL /* orig 0x45AFC8 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AFCC 0x45AFCCUL /* orig 0x45AFCC -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AFD0 0x45AFD0UL /* orig 0x45AFD0 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AFD4 0x45AFD4UL /* orig 0x45AFD4 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45AFD8 0x45AFD8UL /* orig 0x45AFD8 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45B5F4 0x45B5F4UL /* orig 0x45B5F4 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45BA24 0x45BA24UL /* orig 0x45BA24 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45BA28 0x45BA28UL /* orig 0x45BA28 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45BA2C 0x45BA2CUL /* orig 0x45BA2C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45BA38 0x45BA38UL /* orig 0x45BA38 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45BA40 0x45BA40UL /* orig 0x45BA40 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45BA44 0x45BA44UL /* orig 0x45BA44 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45BA48 0x45BA48UL /* orig 0x45BA48 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45BA4C 0x45BA4CUL /* orig 0x45BA4C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45BA54 0x45BA54UL /* orig 0x45BA54 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45BA58 0x45BA58UL /* orig 0x45BA58 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45BA59 0x45BA59UL /* orig 0x45BA59 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45BA5A 0x45BA5AUL /* orig 0x45BA5A -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45BA5B 0x45BA5BUL /* orig 0x45BA5B -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45BA5C 0x45BA5CUL /* orig 0x45BA5C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45BA5D 0x45BA5DUL /* orig 0x45BA5D -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45BA5E 0x45BA5EUL /* orig 0x45BA5E -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45C044 0x45C044UL /* orig 0x45C044 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45C05C 0x45C05CUL /* orig 0x45C05C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45C45C 0x45C45CUL /* orig 0x45C45C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_TBL_45C622 0x45C622UL /* orig 0x45C622 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C65A 0x45C65AUL /* orig 0x45C65A -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C65B 0x45C65BUL /* orig 0x45C65B -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C65C 0x45C65CUL /* orig 0x45C65C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C65D 0x45C65DUL /* orig 0x45C65D -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C65E 0x45C65EUL /* orig 0x45C65E -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C675 0x45C675UL /* orig 0x45C675 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C676 0x45C676UL /* orig 0x45C676 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C677 0x45C677UL /* orig 0x45C677 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C678 0x45C678UL /* orig 0x45C678 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C679 0x45C679UL /* orig 0x45C679 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C67A 0x45C67AUL /* orig 0x45C67A -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C67B 0x45C67BUL /* orig 0x45C67B -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C67C 0x45C67CUL /* orig 0x45C67C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C67D 0x45C67DUL /* orig 0x45C67D -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C67E 0x45C67EUL /* orig 0x45C67E -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C67F 0x45C67FUL /* orig 0x45C67F -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C680 0x45C680UL /* orig 0x45C680 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C681 0x45C681UL /* orig 0x45C681 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C682 0x45C682UL /* orig 0x45C682 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C683 0x45C683UL /* orig 0x45C683 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C684 0x45C684UL /* orig 0x45C684 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C685 0x45C685UL /* orig 0x45C685 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C686 0x45C686UL /* orig 0x45C686 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C687 0x45C687UL /* orig 0x45C687 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C688 0x45C688UL /* orig 0x45C688 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C689 0x45C689UL /* orig 0x45C689 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C68A 0x45C68AUL /* orig 0x45C68A -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C68B 0x45C68BUL /* orig 0x45C68B -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C68C 0x45C68CUL /* orig 0x45C68C -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C68D 0x45C68DUL /* orig 0x45C68D -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C68E 0x45C68EUL /* orig 0x45C68E -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C68F 0x45C68FUL /* orig 0x45C68F -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C690 0x45C690UL /* orig 0x45C690 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C691 0x45C691UL /* orig 0x45C691 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C692 0x45C692UL /* orig 0x45C692 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C693 0x45C693UL /* orig 0x45C693 -- arena cell, see DEC4_ADDRMAP.md */
#define D4V_CTX_B_45C694 0x45C694UL /* orig 0x45C694 -- arena cell, see DEC4_ADDRMAP.md */

#endif /* UHA04_DEC4_ADDR_H */
