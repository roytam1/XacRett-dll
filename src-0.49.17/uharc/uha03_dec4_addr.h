/* UHARC 0.3 core-4 (uha03_dec4.c) arena address names. See DEC4_ADDRMAP.md.
 * Phase 1 naming layer: each #define preserves the exact original
 * 32-bit DGROUP address so G/M macro expansions are unchanged. */
#ifndef UHA03_DEC4_ADDR_H
#define UHA03_DEC4_ADDR_H

/* --- pool, range coder, I/O, dict (semantic) --- */
#define D3U_CONST_FLAG_428ED9 0x428ED9UL /* orig 0x428ED9 -- file-boundary gate */
#define D3U_PRIMER_ACCUMULATOR 0x428EDCUL /* orig 0x428EDC -- primer total -> 444254 */
#define D3U_CONST_428F48 0x428F48UL /* orig 0x428F48 -- const via 40cecc -> 44422C */
#define D3U_BLK00_DICT_MASTER 0x4299B8UL /* orig 0x4299B8 -- dict+0x100 */
#define D3U_BLK01_DICT_COPY 0x4299BCUL /* orig 0x4299BC -- 0x800000 */
#define D3U_BLK02_MODEL_A 0x4299C0UL /* orig 0x4299C0 -- 0x140000 */
#define D3U_BLK03_MODEL_A_FF 0x4299C4UL /* orig 0x4299C4 -- 0x200000 fill FF */
#define D3U_BLK04_MODEL_B_FF 0x4299C8UL /* orig 0x4299C8 -- 0x200000 fill FF */
#define D3U_BLK14_CTX_CODER_A 0x4299F0UL /* orig 0x4299F0 -- 0x61600 ctx coder */
#define D3U_BLK15_CTX_CODER_B 0x4299F4UL /* orig 0x4299F4 -- 0x61600 ctx coder */
#define D3U_BLK17_SMALL_B 0x4299FCUL /* orig 0x4299FC -- 0x5FA word-filled 1 */
#define D3U_STATUS_ERR 0x42C9A0UL /* orig 0x42C9A0 -- 3 EOF / 4 write / 6 cancel */
#define D3U_IN_BUFFER_BASE 0x42C9A8UL /* orig 0x42C9A8 -- input arena 0x03800000 */
#define D3U_OUT_BUFFER_BASE 0x42C9ACUL /* orig 0x42C9AC -- output arena 0x03D00000 */
#define D3U_U16_DELTA_SRC 0x42D272UL /* orig 0x42D272 -- source deltas 0x4000 */
#define D3U_U16_CUM_DST 0x42D274UL /* orig 0x42D274 -- cumulative word table */
#define D3U_REBUILD_FLAG 0x435274UL /* orig 0x435274 -- rebuild trigger */
#define D3U_POOL_CURSOR 0x436E48UL /* orig 0x436E48 -- bump hi-water + size-class hi */
#define D3U_LIN_EXTEND_TBL 0x4398B0UL /* orig 0x4398B0 -- signed extend -2048..2048 */
#define D3U_RC_TOTAL 0x4441E8UL /* orig 0x4441E8 -- range total */
#define D3U_RC_PROB_LO 0x4441ECUL /* orig 0x4441EC -- interval low */
#define D3U_RC_PROB_HI 0x4441F0UL /* orig 0x4441F0 -- interval hi (lo+1) */
#define D3U_CUM_TABLE_PTR 0x44422CUL /* orig 0x44422C -- container cum file-end table */
#define D3U_PRIMER_TOTAL 0x444254UL /* orig 0x444254 -- published primer sum */
#define D3U_DICT_SIZE 0x444270UL /* orig 0x444270 -- 1<<((m-22)%14+10) */
#define D3U_DICT_CURSOR 0x444274UL /* orig 0x444274 -- dict cursor */
#define D3U_DICT_MASK 0x444278UL /* orig 0x444278 -- dict mask size-1 */
#define D3U_OUT_FLUSH_THRESH 0x444EE0UL /* orig 0x444EE0 -- 0xFF00 */
#define D3U_OUT_TOTAL_LEN 0x444EE4UL /* orig 0x444EE4 -- expected out_len */
#define D3U_IN_POS 0x444EE8UL /* orig 0x444EE8 -- input cursor */
#define D3U_OUT_STAGED_LEN 0x444EECUL /* orig 0x444EEC -- staged len */
#define D3U_IN_AVAIL 0x444EF0UL /* orig 0x444EF0 -- buffered avail */
#define D3U_IN_ABS_POS 0x444EF4UL /* orig 0x444EF4 -- absolute input offset */
#define D3U_OUT_TOTAL_POS 0x444EF8UL /* orig 0x444EF8 -- total produced */
#define D3U_RC_SCALE 0x4450F0UL /* orig 0x4450F0 -- range/total */
#define D3U_RC_VALUE 0x4450F4UL /* orig 0x4450F4 -- value/state */
#define D3U_RC_RANGE 0x4450F8UL /* orig 0x4450F8 -- range */
#define D3U_RC_CARRY_BYTE 0x4450FCUL /* orig 0x4450FC -- carry/normalization byte */

/* --- remaining arena cells (generic names; do NOT guess semantics) --- */
#define D3U_TBL_4299CC 0x4299CCUL /* orig 0x4299CC -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_4299D0 0x4299D0UL /* orig 0x4299D0 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_4299D4 0x4299D4UL /* orig 0x4299D4 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_4299D8 0x4299D8UL /* orig 0x4299D8 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_4299DC 0x4299DCUL /* orig 0x4299DC -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_4299E0 0x4299E0UL /* orig 0x4299E0 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_4299E4 0x4299E4UL /* orig 0x4299E4 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_4299E8 0x4299E8UL /* orig 0x4299E8 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_4299EC 0x4299ECUL /* orig 0x4299EC -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_4299F8 0x4299F8UL /* orig 0x4299F8 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_429A00 0x429A00UL /* orig 0x429A00 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_429A04 0x429A04UL /* orig 0x429A04 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_429A08 0x429A08UL /* orig 0x429A08 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_429B98 0x429B98UL /* orig 0x429B98 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_429BA8 0x429BA8UL /* orig 0x429BA8 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_429BB8 0x429BB8UL /* orig 0x429BB8 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_429CB8 0x429CB8UL /* orig 0x429CB8 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_429DB8 0x429DB8UL /* orig 0x429DB8 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_429DE0 0x429DE0UL /* orig 0x429DE0 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_429E08 0x429E08UL /* orig 0x429E08 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_429E30 0x429E30UL /* orig 0x429E30 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_429E3A 0x429E3AUL /* orig 0x429E3A -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_429E44 0x429E44UL /* orig 0x429E44 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_429E54 0x429E54UL /* orig 0x429E54 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_429F54 0x429F54UL /* orig 0x429F54 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_42A054 0x42A054UL /* orig 0x42A054 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_42A06C 0x42A06CUL /* orig 0x42A06C -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_42A86C 0x42A86CUL /* orig 0x42A86C -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_42AC6C 0x42AC6CUL /* orig 0x42AC6C -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_42B06C 0x42B06CUL /* orig 0x42B06C -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_42B46C 0x42B46CUL /* orig 0x42B46C -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_42B86C 0x42B86CUL /* orig 0x42B86C -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_42BC6C 0x42BC6CUL /* orig 0x42BC6C -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_42BC6E 0x42BC6EUL /* orig 0x42BC6E -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_42BDDC 0x42BDDCUL /* orig 0x42BDDC -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_42BF6C 0x42BF6CUL /* orig 0x42BF6C -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_42C438 0x42C438UL /* orig 0x42C438 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_42C492 0x42C492UL /* orig 0x42C492 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_42C6A6 0x42C6A6UL /* orig 0x42C6A6 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_42C9B0 0x42C9B0UL /* orig 0x42C9B0 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_42CBB0 0x42CBB0UL /* orig 0x42CBB0 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_436E40 0x436E40UL /* orig 0x436E40 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_436E44 0x436E44UL /* orig 0x436E44 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_4370B0 0x4370B0UL /* orig 0x4370B0 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43D8B0 0x43D8B0UL /* orig 0x43D8B0 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43D8B4 0x43D8B4UL /* orig 0x43D8B4 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43D8B8 0x43D8B8UL /* orig 0x43D8B8 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43D8BC 0x43D8BCUL /* orig 0x43D8BC -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43D8C0 0x43D8C0UL /* orig 0x43D8C0 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43D8C4 0x43D8C4UL /* orig 0x43D8C4 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43D8C8 0x43D8C8UL /* orig 0x43D8C8 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43D8CC 0x43D8CCUL /* orig 0x43D8CC -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43DACC 0x43DACCUL /* orig 0x43DACC -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43DCCC 0x43DCCCUL /* orig 0x43DCCC -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43DCD0 0x43DCD0UL /* orig 0x43DCD0 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43DCD4 0x43DCD4UL /* orig 0x43DCD4 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43DCD8 0x43DCD8UL /* orig 0x43DCD8 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43DCDC 0x43DCDCUL /* orig 0x43DCDC -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43DCE0 0x43DCE0UL /* orig 0x43DCE0 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43DCE4 0x43DCE4UL /* orig 0x43DCE4 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43E0E4 0x43E0E4UL /* orig 0x43E0E4 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43F4F0 0x43F4F0UL /* orig 0x43F4F0 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43F4F4 0x43F4F4UL /* orig 0x43F4F4 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43F4F8 0x43F4F8UL /* orig 0x43F4F8 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43F4FC 0x43F4FCUL /* orig 0x43F4FC -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43F500 0x43F500UL /* orig 0x43F500 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43F504 0x43F504UL /* orig 0x43F504 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43F508 0x43F508UL /* orig 0x43F508 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43F50C 0x43F50CUL /* orig 0x43F50C -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43F510 0x43F510UL /* orig 0x43F510 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43F514 0x43F514UL /* orig 0x43F514 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43F518 0x43F518UL /* orig 0x43F518 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43F51C 0x43F51CUL /* orig 0x43F51C -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43F520 0x43F520UL /* orig 0x43F520 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43F524 0x43F524UL /* orig 0x43F524 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43F528 0x43F528UL /* orig 0x43F528 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43F6EC 0x43F6ECUL /* orig 0x43F6EC -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_43F6F0 0x43F6F0UL /* orig 0x43F6F0 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_440708 0x440708UL /* orig 0x440708 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_440AF0 0x440AF0UL /* orig 0x440AF0 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_440AF4 0x440AF4UL /* orig 0x440AF4 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_440AF8 0x440AF8UL /* orig 0x440AF8 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_440AFC 0x440AFCUL /* orig 0x440AFC -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_440B00 0x440B00UL /* orig 0x440B00 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_440B04 0x440B04UL /* orig 0x440B04 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_440B08 0x440B08UL /* orig 0x440B08 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_441F08 0x441F08UL /* orig 0x441F08 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_441F18 0x441F18UL /* orig 0x441F18 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_442018 0x442018UL /* orig 0x442018 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_442028 0x442028UL /* orig 0x442028 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_442124 0x442124UL /* orig 0x442124 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_442128 0x442128UL /* orig 0x442128 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_442528 0x442528UL /* orig 0x442528 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_442954 0x442954UL /* orig 0x442954 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_442958 0x442958UL /* orig 0x442958 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_442D58 0x442D58UL /* orig 0x442D58 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_442E54 0x442E54UL /* orig 0x442E54 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_442E58 0x442E58UL /* orig 0x442E58 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_443258 0x443258UL /* orig 0x443258 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_44325C 0x44325CUL /* orig 0x44325C -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_443260 0x443260UL /* orig 0x443260 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_443360 0x443360UL /* orig 0x443360 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_443760 0x443760UL /* orig 0x443760 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_443B60 0x443B60UL /* orig 0x443B60 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_443B64 0x443B64UL /* orig 0x443B64 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_443B68 0x443B68UL /* orig 0x443B68 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_443CE8 0x443CE8UL /* orig 0x443CE8 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_443CEC 0x443CECUL /* orig 0x443CEC -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_443CF0 0x443CF0UL /* orig 0x443CF0 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_443CF4 0x443CF4UL /* orig 0x443CF4 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_443CF8 0x443CF8UL /* orig 0x443CF8 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_443CFC 0x443CFCUL /* orig 0x443CFC -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_443D00 0x443D00UL /* orig 0x443D00 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_443D04 0x443D04UL /* orig 0x443D04 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_443D08 0x443D08UL /* orig 0x443D08 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444108 0x444108UL /* orig 0x444108 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444128 0x444128UL /* orig 0x444128 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444148 0x444148UL /* orig 0x444148 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444188 0x444188UL /* orig 0x444188 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_44418C 0x44418CUL /* orig 0x44418C -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444190 0x444190UL /* orig 0x444190 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444194 0x444194UL /* orig 0x444194 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444198 0x444198UL /* orig 0x444198 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_44419C 0x44419CUL /* orig 0x44419C -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_4441B0 0x4441B0UL /* orig 0x4441B0 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_4441B4 0x4441B4UL /* orig 0x4441B4 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_4441B8 0x4441B8UL /* orig 0x4441B8 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_4441BC 0x4441BCUL /* orig 0x4441BC -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_4441C0 0x4441C0UL /* orig 0x4441C0 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_4441C4 0x4441C4UL /* orig 0x4441C4 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_4441C8 0x4441C8UL /* orig 0x4441C8 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_4441CC 0x4441CCUL /* orig 0x4441CC -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_4441D0 0x4441D0UL /* orig 0x4441D0 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_4441D4 0x4441D4UL /* orig 0x4441D4 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_4441D8 0x4441D8UL /* orig 0x4441D8 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_4441DC 0x4441DCUL /* orig 0x4441DC -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_4441E0 0x4441E0UL /* orig 0x4441E0 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_4441E4 0x4441E4UL /* orig 0x4441E4 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_4441F4 0x4441F4UL /* orig 0x4441F4 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_4441F8 0x4441F8UL /* orig 0x4441F8 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_4441FC 0x4441FCUL /* orig 0x4441FC -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444200 0x444200UL /* orig 0x444200 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444204 0x444204UL /* orig 0x444204 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444208 0x444208UL /* orig 0x444208 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_44420C 0x44420CUL /* orig 0x44420C -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444210 0x444210UL /* orig 0x444210 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444214 0x444214UL /* orig 0x444214 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444218 0x444218UL /* orig 0x444218 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_44421C 0x44421CUL /* orig 0x44421C -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444220 0x444220UL /* orig 0x444220 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444224 0x444224UL /* orig 0x444224 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444228 0x444228UL /* orig 0x444228 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444230 0x444230UL /* orig 0x444230 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444234 0x444234UL /* orig 0x444234 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444238 0x444238UL /* orig 0x444238 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_44423C 0x44423CUL /* orig 0x44423C -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444240 0x444240UL /* orig 0x444240 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444244 0x444244UL /* orig 0x444244 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444248 0x444248UL /* orig 0x444248 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_44424C 0x44424CUL /* orig 0x44424C -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444250 0x444250UL /* orig 0x444250 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444258 0x444258UL /* orig 0x444258 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_44425C 0x44425CUL /* orig 0x44425C -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444260 0x444260UL /* orig 0x444260 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444264 0x444264UL /* orig 0x444264 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444268 0x444268UL /* orig 0x444268 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_44426C 0x44426CUL /* orig 0x44426C -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_44427C 0x44427CUL /* orig 0x44427C -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444280 0x444280UL /* orig 0x444280 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444284 0x444284UL /* orig 0x444284 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444285 0x444285UL /* orig 0x444285 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444286 0x444286UL /* orig 0x444286 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444287 0x444287UL /* orig 0x444287 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444288 0x444288UL /* orig 0x444288 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444289 0x444289UL /* orig 0x444289 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_44428A 0x44428AUL /* orig 0x44428A -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444870 0x444870UL /* orig 0x444870 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444884 0x444884UL /* orig 0x444884 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444C84 0x444C84UL /* orig 0x444C84 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444E4A 0x444E4AUL /* orig 0x444E4A -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444E7E 0x444E7EUL /* orig 0x444E7E -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444E7F 0x444E7FUL /* orig 0x444E7F -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444E80 0x444E80UL /* orig 0x444E80 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444E81 0x444E81UL /* orig 0x444E81 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444E82 0x444E82UL /* orig 0x444E82 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444E99 0x444E99UL /* orig 0x444E99 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444E9A 0x444E9AUL /* orig 0x444E9A -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444E9B 0x444E9BUL /* orig 0x444E9B -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444E9C 0x444E9CUL /* orig 0x444E9C -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444E9D 0x444E9DUL /* orig 0x444E9D -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444E9E 0x444E9EUL /* orig 0x444E9E -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444E9F 0x444E9FUL /* orig 0x444E9F -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444EA0 0x444EA0UL /* orig 0x444EA0 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444EA1 0x444EA1UL /* orig 0x444EA1 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444EA2 0x444EA2UL /* orig 0x444EA2 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444EA3 0x444EA3UL /* orig 0x444EA3 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444EA4 0x444EA4UL /* orig 0x444EA4 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444EA5 0x444EA5UL /* orig 0x444EA5 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444EA6 0x444EA6UL /* orig 0x444EA6 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444EA7 0x444EA7UL /* orig 0x444EA7 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444EA8 0x444EA8UL /* orig 0x444EA8 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444EA9 0x444EA9UL /* orig 0x444EA9 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444EAA 0x444EAAUL /* orig 0x444EAA -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444EAB 0x444EABUL /* orig 0x444EAB -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444EAC 0x444EACUL /* orig 0x444EAC -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444EAD 0x444EADUL /* orig 0x444EAD -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444EAE 0x444EAEUL /* orig 0x444EAE -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444EAF 0x444EAFUL /* orig 0x444EAF -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444EB0 0x444EB0UL /* orig 0x444EB0 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444EB1 0x444EB1UL /* orig 0x444EB1 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444EB2 0x444EB2UL /* orig 0x444EB2 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444EB3 0x444EB3UL /* orig 0x444EB3 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444EB4 0x444EB4UL /* orig 0x444EB4 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444EB5 0x444EB5UL /* orig 0x444EB5 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444EB6 0x444EB6UL /* orig 0x444EB6 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444EB7 0x444EB7UL /* orig 0x444EB7 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444EB8 0x444EB8UL /* orig 0x444EB8 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444EB9 0x444EB9UL /* orig 0x444EB9 -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_FLAG_444EBA 0x444EBAUL /* orig 0x444EBA -- arena cell, see DEC4_ADDRMAP.md */
#define D3U_TBL_444EFC 0x444EFCUL /* orig 0x444EFC -- arena cell, see DEC4_ADDRMAP.md */

#endif /* UHA03_DEC4_ADDR_H */
