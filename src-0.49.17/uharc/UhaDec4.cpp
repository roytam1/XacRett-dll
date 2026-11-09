/* UHARC core 4 (LZP/PPM, methods 22..39). Scaffold: tables +
 * dispatch only. The three version-specific decoders refuse loud
 * (rc=7) until each is ported and verified byte-exact against the
 * oracle corpus; no garbage output is emitted.
 *
 * Method ranges (version-specific; see FORMAT.md):
 *   0.6: 24..39, dict 1 << (((m-24) % 16) + 10)
 *   0.4: 24..39, dict 1 << (((m-24) % 16) + 10)
 *   0.3: 22..35, dict 1 << (((m-22) % 14) + 10)
 */

#include <stdlib.h>
#include <string.h>
#include "UhaDec4.h"

/* 0.6 core 4 (LZP/PPM, methods 24..39). Port of the reference
 * uharc_dec4.c (UNUHARC06). All D4A_ arena addresses below are
 * byte-identical to the reference uharc_dec4_addr.h. */

#define D4A_MASK13_SRC 0x40CDA1UL /* orig 0x40CDA1 -- stack mask src (13B) */
#define D4A_MASK9_SRC 0x40CDADUL /* orig 0x40CDAD -- stack mask src (10B) */
#define D4A_HDR_SIZE 0x4228BEUL /* orig 0x4228BE -- stream header length */
#define D4A_IO_ERR 0x422B90UL /* orig 0x422B90 -- 3=EOF 4=write err */
#define D4A_PPM_PATTERN 0x422BA0UL /* orig 0x422BA0 -- const: Fenwick 512B tiles src */
#define D4A_CNT_OFF_5 0x4236D4UL /* orig 0x4236D4 -- const: cnt off x5 */
#define D4A_CNT_OFF_17 0x4236E8UL /* orig 0x4236E8 -- const: cnt off x17 */
#define D4A_CLS_TREE_SRC 0x42372CUL /* orig 0x42372C -- const: cls tree src */
#define D4A_CLS_CHILD 0x4239BBUL /* orig 0x4239BB -- const: cls child */
#define D4A_CNT_OFF_8 0x4239D0UL /* orig 0x4239D0 -- const: cnt off x8 (dangling) */
#define D4A_BUCKET_LUT 0x4239F0UL /* orig 0x4239F0 -- const: bucket LUT */
#define D4A_BUCKET_LUT2 0x423DF0UL /* orig 0x423DF0 -- const: bucket LUT2 (dangling) */
#define D4A_LEN_LUT_A 0x424270UL /* orig 0x424270 -- const: len LUT A */
#define D4A_LEN_LUT_B 0x424280UL /* orig 0x424280 -- const: len LUT B */
#define D4A_LEN_LUT_C 0x424290UL /* orig 0x424290 -- const: len LUT C */
#define D4A_LEN_NEXT 0x4242A0UL /* orig 0x4242A0 -- const: len next */
#define D4A_FREQ_THRESH1 0x4244A0UL /* orig 0x4244A0 -- const: freq thresh 1 */
#define D4A_FREQ_THRESH2 0x4245A0UL /* orig 0x4245A0 -- const: freq thresh 2 */
#define D4A_TRI2_RANGE 0x4246A0UL /* orig 0x4246A0 -- const: tri2 range */
#define D4A_TRI2_TOTAL 0x4246C8UL /* orig 0x4246C8 -- const: tri2 total */
#define D4A_TRI2_IDX 0x424718UL /* orig 0x424718 -- const: tri2 idx */
#define D4A_SEL_LUT 0x42472CUL /* orig 0x42472C -- const: sel LUT (dangling) */
#define D4A_DELTA_LUT 0x424B2CUL /* orig 0x424B2C -- const: delta LUT */
#define D4A_DELTA_LUT2 0x424B3CUL /* orig 0x424B3C -- const: delta LUT2 */
#define D4A_DELTA_LUT3 0x424C3CUL /* orig 0x424C3C -- const: delta LUT3 */
#define D4A_MASK_LUT 0x424D3CUL /* orig 0x424D3C -- const: mask LUT */
#define D4A_SEL_HIST 0x424D54UL /* orig 0x424D54 -- const: sel hist */
#define D4A_BYTE_CURVE_0 0x425554UL /* orig 0x425554 -- const: byte curve 0 */
#define D4A_BYTE_CURVE_1 0x425954UL /* orig 0x425954 -- const: byte curve 1 */
#define D4A_BYTE_CURVE_2 0x425D54UL /* orig 0x425D54 -- const: byte curve 2 */
#define D4A_BYTE_CURVE_3 0x426154UL /* orig 0x426154 -- const: byte curve 3 */
#define D4A_BYTE_CURVE_4 0x426554UL /* orig 0x426554 -- const: byte curve 4 */
#define D4A_INIT_FLAG 0x426954UL /* orig 0x426954 -- const: init flag (unread) */
#define D4A_SDI_TAB 0x426957UL /* orig 0x426957 -- const: sdi tab */
#define D4A_SDI_PTR 0x426AC3UL /* orig 0x426AC3 -- const: sdi ptr x100 */
#define D4A_SDI_LEN 0x426C53UL /* orig 0x426C53 -- const: sdi len */
#define D4A_DECODE_MAP 0x42711FUL /* orig 0x42711F -- const: decode map */
#define D4A_PENDING_TAB 0x427179UL /* orig 0x427179 -- const: pending tab */
#define D4A_HASH_ENABLE 0x42738DUL /* orig 0x42738D -- const: hash enable */
#define D4A_COST_LUT_SRC 0x429ED2UL /* orig 0x429ED2 -- cost LUT preimage */
#define D4A_COST_LUT 0x429ED4UL /* orig 0x429ED4 -- cost LUT (folded once) */
#define D4A_COST_DIRTY 0x431ED4UL /* orig 0x431ED4 -- fold-once flag */
#define D4A_OUT_TOTAL 0x434514UL /* orig 0x434514 -- total out_len */
#define D4A_OUTBUF_LEN 0x43451CUL /* orig 0x43451C -- outbuf fill / flush size */
#define D4A_IN_CHUNKS 0x434524UL /* orig 0x434524 -- input refill count */
#define D4A_OUT_POS 0x434528UL /* orig 0x434528 -- output cursor */
#define D4A_PERM_TAB 0x43972CUL /* orig 0x43972C -- perm table i/2048-i */
#define D4A_ROW_REGION 0x43F378UL /* orig 0x43F378 -- predictor row region base */
#define D4A_LAST_SEEN 0x457964UL /* orig 0x457964 -- last AE1C per byte */
#define D4A_OUT_COUNT 0x45AE1CUL /* orig 0x45AE1C -- vs byte count */
#define D4A_TRI2_TOTAL_NEXT 0x45AE20UL /* orig 0x45AE20 -- tri2 total next */
#define D4A_TRI2_RANGE_NEXT 0x45AE24UL /* orig 0x45AE24 -- tri2 range next */
#define D4A_MOD_CNT 0x45AE2CUL /* orig 0x45AE2C -- mod counter */
#define D4A_COST_ACCUM 0x45C640UL /* orig 0x45C640 -- smoothed cost */
#define D4A_COST_TMP 0x45C648UL /* orig 0x45C648 -- transient cost sum */
#define D4A_BEST_CNT 0x45C64CUL /* orig 0x45C64C -- dict-offset best count */
#define D4A_COST_GATE 0x45C654UL /* orig 0x45C654 -- cost comparator */
#define D4A_COST_SUM 0x45C658UL /* orig 0x45C658 -- cost sum */
#define D4A_BEST_OFF 0x45C674UL /* orig 0x45C674 -- dict-offset best */
#define D4A_TRI_TOTAL 0x45C67CUL /* orig 0x45C67C -- tri [total] */
#define D4A_TRI_LOW 0x45C680UL /* orig 0x45C680 -- tri [low] = total+4 */
#define D4A_TRI_HIGH 0x45C684UL /* orig 0x45C684 -- tri [high] = total+8 */
#define D4A_PPM_CTX 0x45C690UL /* orig 0x45C690 -- current PPM ctx idx */
#define D4A_COL_IDX 0x45C694UL /* orig 0x45C694 -- column idx */
#define D4A_COL_TOT_A 0x45C698UL /* orig 0x45C698 -- column total threshold A */
#define D4A_ROW_CTX 0x45C69CUL /* orig 0x45C69C -- row ctx = hash */
#define D4A_CTX_COPY 0x45C6A4UL /* orig 0x45C6A4 -- copy of ctx_w0 */
#define D4A_COL_TOT_B 0x45C6A8UL /* orig 0x45C6A8 -- column total threshold B */
#define D4A_CTX_W0 0x45D0F8UL /* orig 0x45D0F8 -- last-4-bytes shift reg (BE) */
#define D4A_CTX_W1 0x45D0FCUL /* orig 0x45D0FC -- previous window */
#define D4A_CTX_W2 0x45D100UL /* orig 0x45D100 -- older window */
#define D4A_FREQ_A 0x45D104UL /* orig 0x45D104 -- adaptive weight A */
#define D4A_DICT_SIZE 0x45D10CUL /* orig 0x45D10C -- dict size 1<<((m-24)%16+10) */
#define D4A_DICT_POS 0x45D110UL /* orig 0x45D110 -- dict insert cursor */
#define D4A_DICT_MASK 0x45D118UL /* orig 0x45D118 -- dict mask size-1 */
#define D4A_FREQ_C 0x45D120UL /* orig 0x45D120 -- adaptive weight C */
#define D4A_FILE_TAB_PTR 0x45D128UL /* orig 0x45D128 -- cum file-end table ptr (=p_prog) */
#define D4A_BLK_END 0x45D12CUL /* orig 0x45D12C -- current block end */
#define D4A_FREQ_B 0x45D138UL /* orig 0x45D138 -- adaptive weight B */
#define D4A_NEXT_BLK 0x45D13CUL /* orig 0x45D13C -- next-block threshold */
#define D4A_HIST_SYMS 0x45DD6EUL /* orig 0x45DD6E -- pending hist symbols */
#define D4A_RUN_CNT 0x45DD85UL /* orig 0x45DD85 -- run counter base */
#define D4A_PRED_FLAG 0x45E38EUL /* orig 0x45E38E -- predictor flag */
#define D4A_MODEL_ID 0x45E390UL /* orig 0x45E390 -- core model selector */
#define D4A_PHASE 0x45E392UL /* orig 0x45E392 -- PPM phase */
#define D4A_ESCAPE_FLAG 0x45E39BUL /* orig 0x45E39B -- pending-escape flag */
#define D4A_HIST_LEN 0x45E39CUL /* orig 0x45E39C -- hist sym count */
#define D4A_COL_SLOT 0x45E3A3UL /* orig 0x45E3A3 -- col scan idx -1..11 */
#define D4A_ROW_SLOT 0x45E3A5UL /* orig 0x45E3A5 -- row scan idx 0..7 */
#define D4A_RC_CODE 0x4749D8UL /* orig 0x4749D8 -- range-coder code */
#define D4A_RC_RANGE 0x4749DCUL /* orig 0x4749DC -- range-coder range */
#define D4A_TBL_42369C 0x42369CUL /* orig 0x42369C -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_4236A0 0x4236A0UL /* orig 0x4236A0 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_4236A4 0x4236A4UL /* orig 0x4236A4 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_4236A8 0x4236A8UL /* orig 0x4236A8 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_4236AC 0x4236ACUL /* orig 0x4236AC -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_4236B0 0x4236B0UL /* orig 0x4236B0 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_4236B4 0x4236B4UL /* orig 0x4236B4 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_4236B8 0x4236B8UL /* orig 0x4236B8 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_42748D 0x42748DUL /* orig 0x42748D -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_4274CD 0x4274CDUL /* orig 0x4274CD -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_42750D 0x42750DUL /* orig 0x42750D -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_42754D 0x42754DUL /* orig 0x42754D -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_42758D 0x42758DUL /* orig 0x42758D -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_4275CD 0x4275CDUL /* orig 0x4275CD -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_42760D 0x42760DUL /* orig 0x42760D -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_42762D 0x42762DUL /* orig 0x42762D -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_42764D 0x42764DUL /* orig 0x42764D -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_43D72C 0x43D72CUL /* orig 0x43D72C -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_43D730 0x43D730UL /* orig 0x43D730 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_43D734 0x43D734UL /* orig 0x43D734 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_43D738 0x43D738UL /* orig 0x43D738 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_43D73C 0x43D73CUL /* orig 0x43D73C -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_43D740 0x43D740UL /* orig 0x43D740 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_43D744 0x43D744UL /* orig 0x43D744 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_43D748 0x43D748UL /* orig 0x43D748 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_43D948 0x43D948UL /* orig 0x43D948 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_43DB48 0x43DB48UL /* orig 0x43DB48 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_43DB4C 0x43DB4CUL /* orig 0x43DB4C -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_43DB50 0x43DB50UL /* orig 0x43DB50 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_43DB54 0x43DB54UL /* orig 0x43DB54 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_43DB58 0x43DB58UL /* orig 0x43DB58 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_43DB5C 0x43DB5CUL /* orig 0x43DB5C -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_43DB60 0x43DB60UL /* orig 0x43DB60 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_43DF60 0x43DF60UL /* orig 0x43DF60 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_43F36C 0x43F36CUL /* orig 0x43F36C -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_43F370 0x43F370UL /* orig 0x43F370 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_43F374 0x43F374UL /* orig 0x43F374 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_43F37C 0x43F37CUL /* orig 0x43F37C -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_43F380 0x43F380UL /* orig 0x43F380 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_43F384 0x43F384UL /* orig 0x43F384 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_43F388 0x43F388UL /* orig 0x43F388 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_43F38C 0x43F38CUL /* orig 0x43F38C -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_43F390 0x43F390UL /* orig 0x43F390 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_43F394 0x43F394UL /* orig 0x43F394 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_43F398 0x43F398UL /* orig 0x43F398 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_43F39C 0x43F39CUL /* orig 0x43F39C -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_43F3A0 0x43F3A0UL /* orig 0x43F3A0 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_43F3A4 0x43F3A4UL /* orig 0x43F3A4 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_43F568 0x43F568UL /* orig 0x43F568 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_44316C 0x44316CUL /* orig 0x44316C -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_444584 0x444584UL /* orig 0x444584 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_444594 0x444594UL /* orig 0x444594 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_4445A0 0x4445A0UL /* orig 0x4445A0 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_4445A4 0x4445A4UL /* orig 0x4445A4 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_4445A8 0x4445A8UL /* orig 0x4445A8 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_444DA4 0x444DA4UL /* orig 0x444DA4 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_4451A0 0x4451A0UL /* orig 0x4451A0 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_4451A4 0x4451A4UL /* orig 0x4451A4 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_4455A4 0x4455A4UL /* orig 0x4455A4 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_4455A8 0x4455A8UL /* orig 0x4455A8 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_4455AC 0x4455ACUL /* orig 0x4455AC -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_445DAC 0x445DACUL /* orig 0x445DAC -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_445DB0 0x445DB0UL /* orig 0x445DB0 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_445DB4 0x445DB4UL /* orig 0x445DB4 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_446044 0x446044UL /* orig 0x446044 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_446048 0x446048UL /* orig 0x446048 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_44604C 0x44604CUL /* orig 0x44604C -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_446050 0x446050UL /* orig 0x446050 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_446054 0x446054UL /* orig 0x446054 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_446058 0x446058UL /* orig 0x446058 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_44605C 0x44605CUL /* orig 0x44605C -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_446060 0x446060UL /* orig 0x446060 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_446064 0x446064UL /* orig 0x446064 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_446460 0x446460UL /* orig 0x446460 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_446464 0x446464UL /* orig 0x446464 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_448460 0x448460UL /* orig 0x448460 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_448464 0x448464UL /* orig 0x448464 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_450464 0x450464UL /* orig 0x450464 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_452460 0x452460UL /* orig 0x452460 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_452464 0x452464UL /* orig 0x452464 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_452860 0x452860UL /* orig 0x452860 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_452864 0x452864UL /* orig 0x452864 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_453064 0x453064UL /* orig 0x453064 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_453460 0x453460UL /* orig 0x453460 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_453464 0x453464UL /* orig 0x453464 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_4534E4 0x4534E4UL /* orig 0x4534E4 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_4554E4 0x4554E4UL /* orig 0x4554E4 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_4558E4 0x4558E4UL /* orig 0x4558E4 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_4578E0 0x4578E0UL /* orig 0x4578E0 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_4578E4 0x4578E4UL /* orig 0x4578E4 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_457D60 0x457D60UL /* orig 0x457D60 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_457D64 0x457D64UL /* orig 0x457D64 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_458564 0x458564UL /* orig 0x458564 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45A564 0x45A564UL /* orig 0x45A564 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45AD64 0x45AD64UL /* orig 0x45AD64 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45ADE4 0x45ADE4UL /* orig 0x45ADE4 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45AE14 0x45AE14UL /* orig 0x45AE14 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45AE18 0x45AE18UL /* orig 0x45AE18 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45AE28 0x45AE28UL /* orig 0x45AE28 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45AE30 0x45AE30UL /* orig 0x45AE30 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45AE34 0x45AE34UL /* orig 0x45AE34 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45AE38 0x45AE38UL /* orig 0x45AE38 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45BA38 0x45BA38UL /* orig 0x45BA38 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45BA3C 0x45BA3CUL /* orig 0x45BA3C -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45C644 0x45C644UL /* orig 0x45C644 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45C650 0x45C650UL /* orig 0x45C650 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45C668 0x45C668UL /* orig 0x45C668 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45C66C 0x45C66CUL /* orig 0x45C66C -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45C670 0x45C670UL /* orig 0x45C670 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45C678 0x45C678UL /* orig 0x45C678 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45C6A0 0x45C6A0UL /* orig 0x45C6A0 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45C6AC 0x45C6ACUL /* orig 0x45C6AC -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45C6B0 0x45C6B0UL /* orig 0x45C6B0 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45C6B4 0x45C6B4UL /* orig 0x45C6B4 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45C6B8 0x45C6B8UL /* orig 0x45C6B8 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45CCD4 0x45CCD4UL /* orig 0x45CCD4 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45D108 0x45D108UL /* orig 0x45D108 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45D114 0x45D114UL /* orig 0x45D114 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45D11C 0x45D11CUL /* orig 0x45D11C -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45D124 0x45D124UL /* orig 0x45D124 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45D130 0x45D130UL /* orig 0x45D130 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45D134 0x45D134UL /* orig 0x45D134 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45D140 0x45D140UL /* orig 0x45D140 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45D144 0x45D144UL /* orig 0x45D144 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45D160 0x45D160UL /* orig 0x45D160 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45D164 0x45D164UL /* orig 0x45D164 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45D166 0x45D166UL /* orig 0x45D166 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45D566 0x45D566UL /* orig 0x45D566 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45D738 0x45D738UL /* orig 0x45D738 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45D768 0x45D768UL /* orig 0x45D768 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45DD5A 0x45DD5AUL /* orig 0x45DD5A -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45DD6A 0x45DD6AUL /* orig 0x45DD6A -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45DD6C 0x45DD6CUL /* orig 0x45DD6C -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45DD84 0x45DD84UL /* orig 0x45DD84 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45E085 0x45E085UL /* orig 0x45E085 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_TBL_45E086 0x45E086UL /* orig 0x45E086 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_FLAG_45E387 0x45E387UL /* orig 0x45E387 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_FLAG_45E388 0x45E388UL /* orig 0x45E388 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_FLAG_45E389 0x45E389UL /* orig 0x45E389 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_FLAG_45E38A 0x45E38AUL /* orig 0x45E38A -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_FLAG_45E38B 0x45E38BUL /* orig 0x45E38B -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_FLAG_45E38C 0x45E38CUL /* orig 0x45E38C -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_FLAG_45E38D 0x45E38DUL /* orig 0x45E38D -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_FLAG_45E38F 0x45E38FUL /* orig 0x45E38F -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_FLAG_45E391 0x45E391UL /* orig 0x45E391 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_FLAG_45E393 0x45E393UL /* orig 0x45E393 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_FLAG_45E394 0x45E394UL /* orig 0x45E394 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_FLAG_45E395 0x45E395UL /* orig 0x45E395 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_FLAG_45E396 0x45E396UL /* orig 0x45E396 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_FLAG_45E397 0x45E397UL /* orig 0x45E397 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_FLAG_45E398 0x45E398UL /* orig 0x45E398 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_FLAG_45E399 0x45E399UL /* orig 0x45E399 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_FLAG_45E39A 0x45E39AUL /* orig 0x45E39A -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_FLAG_45E39D 0x45E39DUL /* orig 0x45E39D -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_FLAG_45E39E 0x45E39EUL /* orig 0x45E39E -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_FLAG_45E39F 0x45E39FUL /* orig 0x45E39F -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_FLAG_45E3A0 0x45E3A0UL /* orig 0x45E3A0 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_FLAG_45E3A1 0x45E3A1UL /* orig 0x45E3A1 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_FLAG_45E3A2 0x45E3A2UL /* orig 0x45E3A2 -- arena cell, see DEC4_ADDRMAP.md */
#define D4A_FLAG_45E3A4 0x45E3A4UL /* orig 0x45E3A4 -- arena cell, see DEC4_ADDRMAP.md */

#if defined(__GNUC__)
/* The arena below is read and written through unsigned char/unsigned
 * short/unsigned int lvalues at overlapping offsets (that is the whole
 * point of the 32-bit address emulation).  GCC's strict aliasing rules
 * do not allow that, and -O2 does miscompile d4_ppm_rescale (the statistics
 * rescale) when they are on, so turn them off for this translation unit. */
#pragma GCC optimize("no-strict-aliasing")
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#if defined(_MSC_VER)
#define UHI64 __int64
#define U64LL(x) x##UI64
#define I64LL(x) x##I64
#else
#define UHI64 long long
#define U64LL(x) x##ULL
#define I64LL(x) x##LL
#endif

#ifndef D4PT
#define D4PT 48019
#endif
#ifndef D4WLO
#define D4WLO 47990
#endif
#ifndef D4WHI
#define D4WHI 48050
#endif

#define D4_ARENA 0x09000000UL

#define A_COL_TAB   0x00610000UL
#define A_ROW_TAB   0x00E10000UL
#define A_HASH1    0x01350000UL
#define A_HASH2    0x01550000UL
#define A_HASH_HEAD   0x01750000UL
#define A_HASH_BYTE   0x01790000UL
#define A_BYTE_HIST   0x017D0000UL
#define A_FENWICK1   0x01810000UL
#define A_FENWICK2   0x01872000UL
#define A_PPM_UNUSED   0x018D4000UL
#define A_PPM_TOT   0x018E0000UL
#define A_PPM_SPARE1   0x018E0700UL
#define A_PPM_SPARE2   0x018E0800UL
#define A_PRED_CACHE  0x018E0900UL
#define A_DICT_BUF 0x01908000UL

static UHI64 d4_cfshl(unsigned UHI64 a, int b) { return (UHI64)(a << b); }
#define __CFSHL__(a,b) d4_cfshl((unsigned UHI64)(a),(b))

/* Hex-Rays bit-field macros (reads; assignments are rewritten) */
#define LOBYTE(x)  ((unsigned char)((unsigned)(x) & 0xFFU))
#define HIBYTE(x)  ((unsigned char)(((unsigned)(x) >> 24) & 0xFFU))
#define BYTE0(x)   ((unsigned char)((unsigned)(x) & 0xFFU))
#define BYTE1(x)   ((unsigned char)(((unsigned)(x) >> 8) & 0xFFU))
#define BYTE2(x)   ((unsigned char)(((unsigned)(x) >> 16) & 0xFFU))
#define BYTE3(x)   ((unsigned char)(((unsigned)(x) >> 24) & 0xFFU))
#define LOWORD(x)  ((unsigned short)((unsigned)(x) & 0xFFFFU))
#define HIWORD(x)  ((unsigned short)(((unsigned)(x) >> 16) & 0xFFFFU))
#define LODWORD(x) ((unsigned int)((unsigned)(x)))
#define HIDWORD(x) ((unsigned int)(((unsigned UHI64)(x)) >> 32))

typedef struct dec4_ctx_s {
    unsigned char *mem;
    Uha4ReadCb rcb;
    void *rctx;
    Uha4WriteCb wcb;
    void *wctx;
    unsigned char inbuf[4096];
    unsigned in_pos, in_avail;
    int in_eof;
    unsigned char outbuf[0x10000];

    unsigned int dict_buf;
    unsigned int col_tab, row_tab, hash1, hash2;
    unsigned int hash_head, hash_byte, byte_hist;
    unsigned int fenwick1, fenwick2, ppm_unused, ppm_tot, ppm_spare1, ppm_spare2;
    unsigned int pred_cache;
    unsigned int file_table;
    unsigned int ctx_byte_ptr, pair_ptr;
    unsigned int hist_ptr_0, hist_ptr_1, hist_ptr_2, hist_ptr_3;
    unsigned int cost_cnt_a, cost_cnt_b;
    unsigned long steps;
    unsigned int p_69C, p_6A0, p_6A4, p_6A8;
    unsigned int p_6AC, p_6B0, p_6B4, p_6B8;
} dec4_ctx;

/* Header flags2 bit6: kennedy.xls special case (1029744-byte file).
 * Set by the container before a 0.6 PPM decode. */
static unsigned char g_dec4_flags2;

void UhaDec4_06SetFlags2(unsigned char flags2)
{
    g_dec4_flags2 = flags2;
}


/* ---------------- prototypes ---------------- */

static int d4_row_rescale(dec4_ctx *c);
static unsigned int d4_col_rescale(dec4_ctx *c);
static int d4_block_reinit(dec4_ctx *c);
static unsigned int d4_build_prob_tables(dec4_ctx *c);
static int d4_block_model_init(dec4_ctx *c);
static unsigned int d4_fenwick_rebuild(dec4_ctx *c, int a1);
static int d4_ppm_rescale(dec4_ctx *c, unsigned int a1);
static unsigned char d4_fenwick_add(dec4_ctx *c, unsigned char a1, int a2);
static int d4_fenwick_add_run(dec4_ctx *c, unsigned char a1, int a2);
static unsigned char d4_cls_update(dec4_ctx *c, unsigned char a1, unsigned char a2);
static unsigned int d4_row_pick_best(dec4_ctx *c);
static int d4_row_cost_update(dec4_ctx *c, char a1);
static unsigned int d4_col_update(dec4_ctx *c, char a1);
static char d4_row_update(dec4_ctx *c, int a1);
static char d4_row_update_cost(dec4_ctx *c, int a1);
static int d4_ctx_feed_byte(dec4_ctx *c, unsigned char a1, char a2);
static int d4_decode_main(dec4_ctx *c, void *a1, int a2);
static char d4_try_match(dec4_ctx *c, int a1);
static int d4_emit_byte(dec4_ctx *c, char a1);
static unsigned char d4_cls_decode(dec4_ctx *c, unsigned char a1);
static int d4_decode_ppm(dec4_ctx *c);
static unsigned char d4_ppm_decode_gated(dec4_ctx *c, unsigned int a1, int a2);
static unsigned char d4_ppm_decode_sym(dec4_ctx *c, int a1);
static void d4_ppm_eval_cost(dec4_ctx *c, unsigned int a1, unsigned char a2);
static int d4_trial_record_cost(dec4_ctx *c, unsigned int a1, int a2, unsigned char a3);
static int d4_cost_accum_update(dec4_ctx *c, unsigned int a1, int a2, unsigned char a3);
static int d4_pred_cache_reset(dec4_ctx *c);
static int d4_pred_next(dec4_ctx *c);
static int d4_pred_learn(dec4_ctx *c, int a1, unsigned char a2);
static int d4_read_header(dec4_ctx *c);


/* ---- static tables, extracted from UNUHARC06.lst DGROUP bytes ---- */

static const unsigned char d4_t422BA0[512] = {
    0,1,1,0,2,0,1,0,4,0,1,0,2,0,1,0,
    8,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,
    16,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,
    8,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,
    32,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,
    8,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,
    16,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,
    8,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,
    64,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,
    8,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,
    16,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,
    8,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,
    32,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,
    8,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,
    16,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,
    8,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,
    128,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,
    8,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,
    16,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,
    8,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,
    32,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,
    8,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,
    16,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,
    8,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,
    64,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,
    8,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,
    16,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,
    8,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,
    32,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,
    8,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,
    16,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0,
    8,0,1,0,2,0,1,0,4,0,1,0,2,0,1,0
};

static const unsigned char d4_t42372C[656] = {
    0,0,1,0,11,0,99,0,0,0,1,0,12,0,99,0,
    0,0,1,0,13,0,99,0,0,0,1,0,14,0,99,0,
    0,0,1,0,14,0,99,0,0,0,1,0,14,0,99,0,
    0,0,1,0,14,0,99,0,0,0,1,0,14,0,99,0,
    0,0,1,0,14,0,99,0,0,0,1,0,14,0,99,0,
    0,0,1,0,11,0,99,0,0,0,1,0,12,0,99,0,
    0,0,1,0,13,0,99,0,0,0,1,0,14,0,99,0,
    0,0,1,0,14,0,99,0,0,0,1,0,14,0,99,0,
    0,0,1,0,14,0,99,0,0,0,1,0,14,0,99,0,
    0,0,1,0,14,0,99,0,0,0,1,0,14,0,99,0,
    0,0,1,0,24,0,99,0,0,0,1,0,24,0,99,0,
    0,0,1,0,24,0,99,0,0,0,1,0,24,0,99,0,
    0,0,1,0,24,0,99,0,0,0,1,0,24,0,99,0,
    0,0,1,0,24,0,99,0,0,0,1,0,24,0,99,0,
    0,0,1,0,24,0,99,0,0,0,1,0,24,0,99,0,
    0,0,1,0,11,0,99,0,0,0,1,0,12,0,99,0,
    0,0,1,0,13,0,99,0,0,0,1,0,14,0,99,0,
    0,0,1,0,14,0,99,0,0,0,1,0,14,0,99,0,
    0,0,1,0,14,0,99,0,0,0,1,0,14,0,99,0,
    0,0,1,0,14,0,99,0,0,0,1,0,14,0,99,0,
    0,0,1,0,6,0,99,0,0,0,1,0,7,0,99,0,
    0,0,1,0,8,0,99,0,0,0,1,0,9,0,99,0,
    0,0,1,0,9,0,99,0,0,0,1,0,9,0,99,0,
    0,0,1,0,9,0,99,0,0,0,1,0,9,0,99,0,
    0,0,1,0,9,0,99,0,0,0,1,0,9,0,99,0,
    0,0,1,0,6,0,99,0,0,0,1,0,7,0,99,0,
    0,0,1,0,8,0,99,0,0,0,1,0,9,0,99,0,
    0,0,1,0,9,0,99,0,0,0,1,0,9,0,99,0,
    0,0,1,0,9,0,99,0,0,0,1,0,9,0,99,0,
    0,0,1,0,9,0,99,0,0,0,1,0,9,0,99,0,
    0,0,1,0,11,0,99,0,0,0,1,0,11,0,99,0,
    0,0,1,0,11,0,99,0,0,0,1,0,11,0,99,0,
    0,0,1,0,11,0,99,0,0,0,1,0,11,0,99,0,
    0,0,1,0,11,0,99,0,0,0,1,0,11,0,99,0,
    0,0,1,0,11,0,99,0,0,0,1,0,11,0,99,0,
    0,0,1,0,6,0,99,0,0,0,1,0,7,0,99,0,
    0,0,1,0,8,0,99,0,0,0,1,0,9,0,99,0,
    0,0,1,0,9,0,99,0,0,0,1,0,9,0,99,0,
    0,0,1,0,9,0,99,0,0,0,1,0,9,0,99,0,
    0,0,1,0,9,0,99,0,0,0,1,0,9,0,99,0,
    0,0,1,0,49,0,99,0,0,0,1,0,79,0,99,0
};

static const unsigned char d4_t4239BB[22] = {
    0,8,0,8,0,8,1,8,2,8,3,8,4,8,5,8,
    5,9,6,9,7,0
};

static const unsigned int d4_t4236D4[5] = {
    0,0,256,258,261
};

static const unsigned int d4_t4236E8[17] = {
    0,0,256,258,261,265,270,276,283,291,
    300,310,321,333,346,360,375
};

static const unsigned int d4_t4239D0[8] = {
    0,0,2,16,16,18,18,18
};

static const unsigned int d4_t4239F0[256] = {
    0,0,0,0,1,1,1,1,2,2,
    2,2,2,2,3,3,3,3,3,3,
    3,3,3,3,4,4,4,4,4,4,
    4,4,4,4,4,4,4,4,4,4,
    5,5,5,5,5,5,5,5,5,5,
    5,5,5,5,5,5,5,5,5,5,
    5,5,5,5,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6
};

static const unsigned int d4_t423DF0[288] = {
    0,0,1,1,2,2,3,3,3,3,
    4,4,4,4,4,4,5,5,5,5,
    5,5,5,5,5,5,5,5,5,5,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,6,6,6
};

static const unsigned char d4_t424270[16] = {
    31,32,33,35,39,47,63,127,127,127,127,127,127,127,127,127
};

static const unsigned char d4_t424280[16] = {
    3,3,3,3,5,7,9,11,11,11,12,12,12,12,12,12
};

static const unsigned char d4_t424290[528] = {
    8,8,8,8,9,10,11,12,12,12,12,12,12,12,12,12,
    0,0,0,0,0,0,0,0,3,5,0,0,4,10,0,0,
    7,9,0,0,8,12,0,0,11,13,0,0,6,62,0,0,
    15,17,0,0,16,20,0,0,19,21,0,0,18,26,0,0,
    23,25,0,0,24,28,0,0,27,29,0,0,22,46,0,0,
    31,33,0,0,32,36,0,0,35,37,0,0,34,42,0,0,
    39,41,0,0,40,44,0,0,43,45,0,0,38,54,0,0,
    47,49,0,0,48,52,0,0,51,53,0,0,50,58,0,0,
    55,57,0,0,56,60,0,0,59,61,0,0,30,254,0,0,
    63,65,0,0,64,68,0,0,67,69,0,0,66,74,0,0,
    71,73,0,0,72,76,0,0,75,77,0,0,70,86,0,0,
    79,81,0,0,80,84,0,0,83,85,0,0,82,90,0,0,
    87,89,0,0,88,92,0,0,91,93,0,0,78,110,0,0,
    95,97,0,0,96,100,0,0,99,101,0,0,98,106,0,0,
    103,105,0,0,104,108,0,0,107,109,0,0,102,118,0,0,
    111,113,0,0,112,116,0,0,115,117,0,0,114,122,0,0,
    119,121,0,0,120,124,0,0,123,125,0,0,94,190,0,0,
    127,129,0,0,128,132,0,0,131,133,0,0,130,138,0,0,
    135,137,0,0,136,140,0,0,139,141,0,0,134,150,0,0,
    143,145,0,0,144,148,0,0,147,149,0,0,146,154,0,0,
    151,153,0,0,152,156,0,0,155,157,0,0,142,174,0,0,
    159,161,0,0,160,164,0,0,163,165,0,0,162,170,0,0,
    167,169,0,0,168,172,0,0,171,173,0,0,166,182,0,0,
    175,177,0,0,176,180,0,0,179,181,0,0,178,186,0,0,
    183,185,0,0,184,188,0,0,187,189,0,0,158,222,0,0,
    191,193,0,0,192,196,0,0,195,197,0,0,194,202,0,0,
    199,201,0,0,200,204,0,0,203,205,0,0,198,214,0,0,
    207,209,0,0,208,212,0,0,211,213,0,0,210,218,0,0,
    215,217,0,0,216,220,0,0,219,221,0,0,206,238,0,0,
    223,225,0,0,224,228,0,0,227,229,0,0,226,234,0,0,
    231,233,0,0,232,236,0,0,235,237,0,0,230,246,0,0,
    239,241,0,0,240,244,0,0,243,245,0,0,242,250,0,0,
    247,249,0,0,248,252,0,0,251,253,0,0,126,0,0,0
};

static const unsigned char d4_t4244A0[256] = {
    253,253,253,253,253,221,157,125,125,125,125,125,125,125,125,125,
    125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,
    125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,
    125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,
    125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,
    125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,
    125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,
    125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,
    125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,
    125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,
    125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,
    125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,
    125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,
    125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,
    125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,
    125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125
};

static const unsigned char d4_t4245A0[256] = {
    247,247,247,247,247,217,155,125,125,125,125,125,125,125,125,125,
    125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,
    125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,
    125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,
    125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,
    125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,
    125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,
    125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,
    125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,
    125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,
    125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,
    125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,
    125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,
    125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,
    125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,
    125,125,125,125,125,125,125,125,125,125,125,125,125,125,125,125
};

static const unsigned int d4_t4246A0[10] = {
    1,1,2,3,4,1,1,2,2,4
};

static const unsigned int d4_t4246C8[20] = {
    513,521,553,585,681,745,749,753,761,764,
    8,32,32,96,64,4,4,8,3,5
};

static const unsigned int d4_t424D3C[6] = {
    127, 127, 127, 127, 7, 3,
};

static const unsigned int d4_t424D54[512] = {
    0, 1, 2, 2, 2, 3, 3, 3,
    3, 3, 4, 4, 4, 4, 4, 4,
    4, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 8,
    8, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12,
    12, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 14, 14, 14, 14, 14, 14,
    14, 14, 14, 14, 14, 14, 14, 14,
    14, 14, 14, 14, 14, 14, 14, 14,
    14, 14, 14, 14, 14, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 16,
    16, 16, 16, 16, 16, 16, 16, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 14, 14, 14, 14,
    14, 14, 14, 14, 14, 14, 14, 14,
    14, 14, 14, 14, 14, 14, 14, 14,
    14, 14, 14, 14, 14, 14, 14, 13,
    13, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13,
    13, 13, 13, 13, 13, 13, 13, 13,
    12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 11,
    11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 9,
    9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9,
    8, 8, 8, 8, 8, 8, 8, 8,
    8, 8, 8, 8, 8, 8, 8, 7,
    7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 5,
    5, 5, 5, 5, 5, 5, 5, 5,
    4, 4, 4, 4, 4, 4, 4, 3,
    3, 3, 3, 3, 2, 2, 2, 1,
};

static const unsigned int d4_t425554[256] = {
    0, 0, 0, 0, 1, 1, 1, 1,
    2, 2, 2, 2, 3, 3, 3, 3,
    4, 4, 4, 4, 5, 5, 5, 5,
    6, 6, 6, 6, 7, 7, 7, 7,
    8, 8, 8, 8, 9, 9, 9, 9,
    10, 10, 10, 10, 11, 11, 11, 11,
    12, 12, 12, 12, 13, 13, 13, 13,
    14, 14, 14, 14, 15, 15, 15, 15,
    16, 16, 16, 16, 17, 17, 17, 17,
    18, 18, 18, 18, 19, 19, 19, 19,
    20, 20, 20, 20, 21, 21, 21, 21,
    22, 22, 22, 22, 23, 23, 23, 23,
    24, 24, 24, 24, 25, 25, 25, 25,
    26, 26, 26, 26, 27, 27, 27, 27,
    28, 28, 28, 28, 29, 29, 29, 29,
    30, 30, 30, 30, 31, 31, 31, 31,
    32, 31, 31, 31, 31, 30, 30, 30,
    30, 29, 29, 29, 29, 28, 28, 28,
    28, 27, 27, 27, 27, 26, 26, 26,
    26, 25, 25, 25, 25, 24, 24, 24,
    24, 23, 23, 23, 23, 22, 22, 22,
    22, 21, 21, 21, 21, 20, 20, 20,
    20, 19, 19, 19, 19, 18, 18, 18,
    18, 17, 17, 17, 17, 16, 16, 16,
    16, 15, 15, 15, 15, 14, 14, 14,
    14, 13, 13, 13, 13, 12, 12, 12,
    12, 11, 11, 11, 11, 10, 10, 10,
    10, 9, 9, 9, 9, 8, 8, 8,
    8, 7, 7, 7, 7, 6, 6, 6,
    6, 5, 5, 5, 5, 4, 4, 4,
    4, 3, 3, 3, 3, 2, 2, 2,
    2, 1, 1, 1, 1, 0, 0, 0,
};

static const unsigned int d4_t426554[256] = {
    0, 1, 2, 3, 4, 5, 5, 6,
    6, 6, 7, 7, 7, 7, 7, 8,
    8, 8, 8, 8, 8, 8, 8, 9,
    9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 9, 9, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10,
    10, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11,
    11, 11, 11, 11, 11, 11, 11, 11,
    10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 10, 10, 10,
    10, 10, 10, 10, 10, 9, 9, 9,
    9, 9, 9, 9, 9, 9, 9, 9,
    9, 9, 8, 8, 8, 8, 8, 8,
    8, 8, 7, 7, 7, 7, 7, 6,
    6, 6, 5, 5, 4, 3, 2, 1,
};

static const unsigned int d4_t425954[256] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 5, 5,
    6, 6, 6, 6, 6, 6, 6, 6,
    7, 7, 7, 7, 7, 7, 7, 7,
    8, 8, 8, 8, 8, 8, 8, 8,
    9, 9, 9, 9, 9, 9, 9, 9,
    10, 10, 10, 10, 10, 10, 10, 10,
    11, 11, 11, 11, 11, 11, 11, 11,
    12, 12, 12, 12, 12, 12, 12, 12,
    13, 13, 13, 13, 13, 13, 13, 13,
    14, 14, 14, 14, 14, 14, 14, 14,
    15, 15, 15, 15, 15, 15, 15, 15,
    16, 15, 15, 15, 15, 15, 15, 15,
    15, 14, 14, 14, 14, 14, 14, 14,
    14, 13, 13, 13, 13, 13, 13, 13,
    13, 12, 12, 12, 12, 12, 12, 12,
    12, 11, 11, 11, 11, 11, 11, 11,
    11, 10, 10, 10, 10, 10, 10, 10,
    10, 9, 9, 9, 9, 9, 9, 9,
    9, 8, 8, 8, 8, 8, 8, 8,
    8, 7, 7, 7, 7, 7, 7, 7,
    7, 6, 6, 6, 6, 6, 6, 6,
    6, 5, 5, 5, 5, 5, 5, 5,
    5, 4, 4, 4, 4, 4, 4, 4,
    4, 3, 3, 3, 3, 3, 3, 3,
    3, 2, 2, 2, 2, 2, 2, 2,
    2, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 0, 0, 0, 0, 0, 0,
};

static const unsigned int d4_t425D54[256] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5,
    6, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6,
    7, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7,
    8, 7, 7, 7, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 7, 7,
    7, 6, 6, 6, 6, 6, 6, 6,
    6, 6, 6, 6, 6, 6, 6, 6,
    6, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5,
    5, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4,
    4, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3,
    3, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2,
    2, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};

static const unsigned int d4_t426154[256] = {
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    2, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
};
static const unsigned char d4_t424718[20] = {
    0,0,8,16,24,0,0,0,0,0,0,1,2,3,4,1,
    1,2,2,4
};

static const unsigned int d4_t42472C[256] = {
    0,1,1,1,2,2,2,2,2,2,
    2,2,2,3,3,3,3,3,3,3,
    3,3,3,3,3,3,3,3,3,3,
    3,3,4,4,4,4,4,4,4,4,
    4,4,4,4,4,4,4,4,4,4,
    4,4,4,4,4,4,4,4,4,4,
    4,4,4,4,4,4,4,4,4,4,
    4,4,4,4,4,4,4,4,4,4,
    4,4,4,4,4,4,4,4,4,4,
    4,4,4,4,4,4,4,4,4,4,
    4,4,4,4,4,4,4,4,4,4,
    4,4,4,4,4,4,4,4,4,4,
    4,4,4,4,4,4,4,4,4,4,
    4,4,4,4,4,4,4,4,4,4,
    4,4,4,4,4,4,4,4,4,4,
    4,4,4,4,4,4,4,4,4,4,
    4,4,4,4,4,4,4,4,4,4,
    4,4,4,4,4,4,4,4,4,4,
    4,4,4,4,4,4,4,4,4,4,
    4,4,4,4,4,4,4,4,4,4,
    4,4,4,4,4,4,4,4,4,4,
    4,4,4,4,4,4,4,4,4,4,
    4,4,4,4,4,5,5,5,5,5,
    5,5,5,5,5,5,5,5,5,5,
    5,5,5,5,6,6,6,6,6,6,
    6,6,6,7,7,7
};

static const unsigned char d4_t424B2C[16] = {
    0,0,0,0,1,1,1,1,1,1,1,1,1,0,0,0
};

static const unsigned char d4_t424B3C[256] = {
    0,1,1,2,2,2,3,3,3,3,3,4,4,4,4,4,
    4,4,4,5,5,5,5,5,5,5,5,5,5,5,5,5,
    6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
    6,6,6,6,6,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
    7,7,7,7,7,7,7,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
    8,8,8,8,8,8,8,8,8,8,9,9,9,9,9,9,
    9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
    9,9,9,9,9,9,9,9,9,9,9,9,10,10,10,10,
    10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,
    10,11,11,11,11,11,11,11,11,11,11,11,11,11,12,12,
    12,12,12,12,12,12,13,13,13,13,13,14,14,14,15,15
};

static const unsigned char d4_t424C3C[256] = {
    0,1,2,3,4,5,6,7,7,8,8,9,9,10,10,10,
    11,11,11,11,11,12,12,12,12,12,12,12,12,13,13,13,
    13,13,13,13,13,13,13,13,13,13,14,14,14,14,14,14,
    14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
    17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,
    17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,
    17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,
    17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,17,
    17,17,18,18,18,18,18,18,18,18,18,18,18,18,18,18,
    18,18,18,18,18,18,18,19,19,19,19,19,19,19,19,19,
    19,19,19,19,20,20,20,20,20,20,20,20,21,21,21,21,
    21,22,22,22,23,23,24,24,25,25,26,27,28,29,30,31
};

static const unsigned char d4_t426C53[100] = {
    32,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,
    4,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    1,2,3,0
};

static const unsigned char d4_t426957[364] = {
    223,223,223,223,223,223,223,223,223,223,223,223,223,223,223,223,
    223,223,223,223,223,223,223,223,223,223,223,223,223,223,223,223,
    139,150,144,145,139,151,154,223,158,145,155,223,150,145,152,223,
    151,158,139,223,153,144,141,223,158,147,147,223,154,145,139,223,
    151,150,140,223,134,144,138,223,139,154,141,223,137,154,141,223,
    150,139,151,223,151,154,141,223,154,140,140,223,156,144,146,223,
    141,154,223,223,150,145,223,223,139,144,223,223,144,145,223,223,
    154,155,223,223,158,145,223,223,144,153,223,223,144,138,223,223,
    158,141,223,223,154,140,223,223,154,145,223,223,158,140,223,223,
    154,141,223,223,150,139,223,223,150,140,223,223,151,154,223,223,
    147,154,223,223,158,139,223,223,144,141,223,223,139,151,223,223,
    156,151,223,223,140,154,223,223,158,147,223,223,150,147,223,223,
    157,154,223,223,140,139,223,223,141,144,223,223,146,154,223,223,
    137,154,223,223,145,144,223,223,158,155,223,223,150,156,223,223,
    150,155,223,223,158,156,223,223,150,146,223,223,145,155,223,223,
    147,134,223,223,136,151,223,223,152,151,223,223,158,134,223,223,
    138,139,223,223,154,147,223,223,140,151,223,223,147,144,223,223,
    158,146,223,223,158,152,223,223,138,145,223,223,150,141,223,223,
    158,157,223,223,144,155,223,223,145,154,223,223,147,155,223,223,
    150,153,223,223,140,138,223,223,140,144,223,223,143,154,223,223,
    156,154,223,223,141,150,223,223,138,141,223,223,148,154,223,223,
    138,140,223,223,144,136,223,223,155,154,223,223,140,158,223,223,
    245,223,223,223,242,245,223,223,192,192,223,223
};

static const unsigned char d4_t42711F[256] = {
    200,200,200,200,200,200,200,200,200,200,200,200,200,200,200,200,
    200,200,200,200,200,200,200,200,200,200,200,200,200,200,200,200,
    200,200,200,200,200,200,200,200,200,200,200,200,200,200,200,200,
    200,200,200,200,200,200,200,200,200,200,200,200,200,200,200,200,
    200,200,200,200,200,200,200,200,200,200,200,200,200,200,200,200,
    200,200,200,200,200,200,200,200,200,200,200,200,200,200,200,200,
    200,200,200,200,200,200,200,200,200,200,200,200,200,200,200,200,
    200,200,200,200,200,200,200,200,200,200,200,200,200,200,200,200,
    0,200,1,2,200,3,4,5,6,7,8,9,10,11,12,13,
    14,15,16,24,200,32,40,48,56,64,72,80,88,17,25,33,
    41,49,57,65,73,81,100,100,100,100,100,100,100,100,100,100,
    200,200,89,200,18,26,34,42,50,58,200,66,74,82,90,19,
    27,35,43,51,200,59,67,75,83,91,20,28,36,200,44,52,
    60,68,76,84,92,21,29,37,45,53,61,200,200,69,77,200,
    85,200,93,22,200,30,38,46,54,62,70,78,86,94,23,31,
    39,47,55,63,71,79,200,87,95,96,97,98,200,99,200,200
};

static const unsigned char d4_t427179[1024] = {
    200,200,200,200,200,200,200,200,200,200,200,200,200,200,200,200,
    200,200,200,200,200,200,200,200,200,200,200,200,200,200,200,200,
    200,200,200,200,200,200,0,200,1,2,200,3,4,5,6,7,
    8,9,10,11,12,13,14,15,16,24,200,32,40,48,56,64,
    72,80,88,17,25,33,41,49,57,65,73,81,100,100,100,100,
    100,100,100,100,100,100,200,200,89,200,18,26,34,42,50,58,
    200,66,74,82,90,19,27,35,43,51,200,59,67,75,83,91,
    20,28,36,200,44,52,60,68,76,84,92,21,29,37,45,53,
    61,200,200,69,77,200,85,200,93,22,200,30,38,46,54,62,
    70,78,86,94,23,31,39,47,55,63,71,79,200,87,95,96,
    97,98,200,99,200,200,128,130,131,133,134,135,136,137,138,139,
    140,141,142,143,144,145,146,147,149,150,151,152,153,154,155,156,
    157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,
    173,174,175,178,180,181,182,183,184,185,187,188,189,190,191,192,
    193,194,195,197,198,199,200,201,202,203,204,206,207,208,209,210,
    211,212,213,214,215,216,217,218,221,222,224,226,227,229,230,231,
    232,233,234,235,236,237,238,239,240,241,242,243,244,245,247,248,
    249,250,251,253,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,
    0,0,0,0,0,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,1,1,1,1,1,1,1,0,0,1,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
    0,0,1,0,1,0,0,1,1,0,0,0,0,0,0,0,
    0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,1,
    0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,
    1,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
    0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,1,1,1,1,1,1,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,10,0,0,0,50,0,0,0,80,0,0,0,
    120,0,0,0,170,0,0,0,230,0,0,0,44,1,0,0,
    124,1,0,0,214,1,0,0,58,2,0,0,168,2,0,0,
    32,3,0,0,232,3,0,0,176,4,0,0,120,5,0,0,
    64,6,0,0,1,0,0,0,4,0,0,0,8,0,0,0,
    12,0,0,0,17,0,0,0,24,0,0,0,33,0,0,0,
    43,0,0,0,53,0,0,0,64,0,0,0,76,0,0,0,
    89,0,0,0,103,0,0,0,118,0,0,0,144,0,0,0,
    199,0,0,0,1,0,0,0,3,0,0,0,6,0,0,0,
    10,0,0,0,16,0,0,0,23,0,0,0,32,0,0,0,
    43,0,0,0,53,0,0,0,64,0,0,0,76,0,0,0,
    89,0,0,0,103,0,0,0,118,0,0,0,144,0,0,0,
    199,0,0,0,0,0,0,0,1,0,0,0,3,0,0,0,
    6,0,0,0,13,0,0,0,22,0,0,0,32,0,0,0,
    43,0,0,0,53,0,0,0,64,0,0,0,76,0,0,0
};

static const unsigned char d4_t42738D[256] = {
    1,1,1,1,1,1,1,0,0,1,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,1,0,0,1,0,
    1,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,
    0,0,1,0,0,0,0,0,0,0,0,1,0,0,0,0,
    0,0,0,0,0,1,0,0,0,0,0,0,1,0,1,1,
    1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,1,0,0,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
    1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1
};

static const unsigned int d4_t42748D[16] = {
    10,50,80,120,170,230,300,380,470,570,
    680,800,1000,1200,1400,1600
};

static const unsigned int d4_t4274CD[16] = {
    1,4,8,12,17,24,33,43,53,64,
    76,89,103,118,144,199
};

static const unsigned int d4_t42750D[16] = {
    1,3,6,10,16,23,32,43,53,64,
    76,89,103,118,144,199
};

static const unsigned int d4_t42754D[16] = {
    0,1,3,6,13,22,32,43,53,64,
    76,89,103,118,144,199
};

static const unsigned int d4_t42758D[16] = {
    5,7,10,14,18,22,26,30,34,37,
    40,42,44,46,47,48
};

static const unsigned int d4_t4275CD[16] = {
    5,7,10,14,18,22,26,30,34,38,
    42,46,49,52,54,56
};

static const unsigned int d4_t42760D[8] = {
    0,1024,2240,3200,3776,3968,4032,4032
};

static const unsigned int d4_t42762D[8] = {
    20,36,45,51,56,60,62,63
};

static const unsigned int d4_t42764D[33] = {
    63,63,62,61,60,59,58,57,53,49,
    46,43,40,36,33,30,30,30,30,26,
    26,22,22,18,18,14,14,10,10,8,
    7,6,5
};

static const unsigned short d4_t429ED4[16384] = {
    0,65520,60856,62798,63594,64029,64305,64495,64635,64740,64825,64892,
    64949,64995,65036,65070,65101,65126,65150,65171,65190,65206,65222,65236,
    65249,65260,65271,65282,65290,65299,65307,65315,65322,65328,65334,65340,
    65346,65351,65356,65361,65365,65369,65373,65378,65380,65385,65387,65391,
    65394,65397,65399,65403,65404,65408,65410,65412,65414,65417,65418,65421,
    65422,65425,65426,65428,65430,65431,65433,65434,65436,65438,65438,65441,
    65441,65443,65444,65446,65446,65448,65449,65450,65451,65452,65453,65454,
    65455,65457,65457,65458,65458,65460,65461,65461,65462,65463,65464,65465,
    65465,65466,65467,65467,65468,65469,65470,65470,65470,65472,65472,65472,
    65474,65473,65475,65475,65475,65476,65477,65477,65477,65478,65479,65479,
    65479,65480,65481,65481,65481,65482,65482,65482,65484,65483,65484,65484,
    65485,65485,65485,65486,65486,65487,65487,65487,65487,65488,65489,65488,
    65489,65490,65489,65490,65490,65491,65491,65491,65491,65492,65492,65492,
    65493,65493,65493,65493,65494,65494,65494,65494,65495,65495,65495,65496,
    65495,65496,65497,65496,65497,65496,65498,65497,65497,65498,65498,65498,
    65499,65498,65499,65499,65499,65500,65499,65500,65500,65500,65501,65500,
    65501,65501,65501,65501,65502,65501,65502,65502,65502,65503,65502,65503,
    65503,65503,65503,65503,65503,65504,65504,65504,65504,65504,65504,65505,
    65505,65504,65505,65505,65506,65505,65506,65505,65506,65506,65506,65506,
    65507,65506,65507,65506,65507,65507,65507,65507,65508,65507,65508,65507,
    65508,65508,65508,65508,65509,65508,65509,65508,65509,65509,65509,65509,
    65509,65509,65509,65510,65510,65509,65510,65510,65510,65510,65510,65510,
    65511,65510,65511,65510,65511,65511,65511,65511,65511,65511,65512,65511,
    65512,65511,65512,65512,65511,65512,65512,65512,65513,65512,65512,65513,
    65512,65513,65513,65512,65513,65513,65513,65513,65513,65513,65514,65513,
    65514,65513,65514,65513,65514,65514,65514,65514,65514,65514,65514,65514,
    65515,65514,65515,65514,65515,65514,65515,65515,65515,65515,65515,65515,
    65515,65515,65515,65516,65515,65516,65515,65516,65515,65516,65516,65516,
    65515,65516,65516,65516,65517,65516,65516,65516,65517,65516,65516,65517,
    65517,65516,65517,65517,65516,65517,65517,65517,65517,65517,65517,65517,
    65518,65517,65517,65518,65517,65517,65518,65518,65517,65518,65518,65517,
    65518,65518,65518,65518,65518,65518,65518,65518,65519,65518,65518,65518,
    65519,65518,65519,65518,65519,65519,65518,65519,65519,65518,65519,65519,
    65519,65519,65519,65519,65519,65519,65520,65519,65519,65519,65520,65519,
    65520,65519,65520,65519,65520,65519,65520,65520,65519,65520,65520,65520,
    65520,65520,65520,65520,65520,65520,65520,65520,65520,65521,65520,65520,
    65521,65520,65520,65521,65520,65521,65520,65521,65521,65520,65521,65521,
    65521,65520,65521,65521,65521,65521,65521,65521,65521,65521,65521,65521,
    65522,65521,65521,65521,65522,65521,65521,65522,65521,65522,65521,65522,
    65521,65522,65521,65522,65522,65521,65522,65522,65522,65522,65521,65522,
    65522,65522,65522,65522,65522,65522,65522,65522,65523,65522,65522,65522,
    65523,65522,65522,65522,65523,65522,65523,65522,65523,65522,65523,65522,
    65523,65522,65523,65523,65522,65523,65523,65523,65523,65522,65523,65523,
    65523,65523,65523,65523,65523,65523,65523,65523,65523,65523,65523,65524,
    65523,65523,65523,65523,65524,65523,65523,65524,65523,65524,65523,65524,
    65523,65524,65523,65524,65523,65524,65523,65524,65524,65523,65524,65524,
    65524,65523,65524,65524,65524,65524,65524,65524,65523,65524,65524,65524,
    65524,65524,65524,65525,65524,65524,65524,65524,65524,65525,65524,65524,
    65524,65525,65524,65524,65525,65524,65524,65525,65524,65525,65524,65525,
    65524,65525,65524,65525,65524,65525,65524,65525,65525,65524,65525,65525,
    65525,65524,65525,65525,65525,65525,65524,65525,65525,65525,65525,65525,
    65525,65525,65525,65525,65525,65525,65525,65525,65525,65525,65525,65526,
    65525,65525,65525,65525,65526,65525,65525,65525,65526,65525,65525,65526,
    65525,65526,65525,65525,65526,65525,65526,65525,65526,65525,65526,65525,
    65526,65526,65525,65526,65525,65526,65526,65525,65526,65526,65526,65525,
    65526,65526,65526,65525,65526,65526,65526,65526,65526,65526,65526,65526,
    65525,65526,65526,65526,65526,65526,65526,65526,65527,65526,65526,65526,
    65526,65526,65526,65526,65527,65526,65526,65526,65526,65527,65526,65526,
    65527,65526,65526,65526,65527,65526,65527,65526,65526,65527,65526,65527,
    65526,65527,65526,65527,65526,65527,65526,65527,65526,65527,65526,65527,
    65527,65526,65527,65527,65526,65527,65527,65526,65527,65527,65526,65527,
    65527,65527,65527,65526,65527,65527,65527,65527,65527,65527,65526,65527,
    65527,65527,65527,65527,65527,65527,65527,65527,65527,65527,65527,65527,
    65527,65527,65527,65527,65528,65527,65527,65527,65527,65527,65527,65528,
    65527,65527,65527,65528,65527,65527,65527,65528,65527,65527,65528,65527,
    65527,65528,65527,65527,65528,65527,65527,65528,65527,65528,65527,65528,
    65527,65528,65527,65528,65527,65528,65527,65528,65527,65528,65527,65528,
    65528,65527,65528,65527,65528,65528,65527,65528,65528,65527,65528,65528,
    65528,65527,65528,65528,65528,65527,65528,65528,65528,65527,65528,65528,
    65528,65528,65528,65528,65527,65528,65528,65528,65528,65528,65528,65528,
    65528,65528,65528,65528,65528,65528,65528,65528,65528,65528,65528,65528,
    65528,65528,65528,65528,65528,65529,65528,65528,65528,65528,65528,65528,
    65529,65528,65528,65528,65528,65529,65528,65528,65528,65529,65528,65528,
    65528,65529,65528,65528,65529,65528,65528,65529,65528,65528,65529,65528,
    65529,65528,65528,65529,65528,65529,65528,65529,65528,65529,65528,65529,
    65528,65529,65528,65529,65528,65529,65528,65529,65528,65529,65528,65529,
    65529,65528,65529,65528,65529,65529,65528,65529,65529,65528,65529,65529,
    65528,65529,65529,65528,65529,65529,65529,65528,65529,65529,65529,65528,
    65529,65529,65529,65529,65528,65529,65529,65529,65529,65529,65528,65529,
    65529,65529,65529,65529,65529,65529,65529,65529,65528,65529,65529,65529,
    65529,65529,65529,65529,65529,65529,65529,65529,65529,65529,65529,65529,
    65529,65529,65529,65530,65529,65529,65529,65529,65529,65529,65529,65529,
    65530,65529,65529,65529,65529,65529,65529,65530,65529,65529,65529,65529,
    65530,65529,65529,65529,65530,65529,65529,65529,65530,65529,65529,65530,
    65529,65529,65529,65530,65529,65529,65530,65529,65529,65530,65529,65530,
    65529,65529,65530,65529,65530,65529,65529,65530,65529,65530,65529,65530,
    65529,65529,65530,65529,65530,65529,65530,65529,65530,65529,65530,65529,
    65530,65529,65530,65530,65529,65530,65529,65530,65529,65530,65530,65529,
    65530,65529,65530,65530,65529,65530,65529,65530,65530,65529,65530,65530,
    65529,65530,65530,65530,65529,65530,65530,65529,65530,65530,65530,65529,
    65530,65530,65530,65529,65530,65530,65530,65529,65530,65530,65530,65530,
    65529,65530,65530,65530,65530,65530,65529,65530,65530,65530,65530,65530,
    65530,65529,65530,65530,65530,65530,65530,65530,65530,65530,65530,65530,
    65530,65530,65530,65529,65530,65530,65530,65530,65530,65530,65530,65530,
    65530,65530,65530,65530,65530,65531,65530,65530,65530,65530,65530,65530,
    65530,65530,65530,65530,65530,65530,65531,65530,65530,65530,65530,65530,
    65530,65530,65531,65530,65530,65530,65530,65530,65531,65530,65530,65530,
    65530,65530,65531,65530,65530,65530,65531,65530,65530,65530,65531,65530,
    65530,65530,65531,65530,65530,65530,65531,65530,65530,65531,65530,65530,
    65530,65531,65530,65530,65531,65530,65530,65531,65530,65531,65530,65530,
    65531,65530,65530,65531,65530,65531,65530,65530,65531,65530,65531,65530,
    65530,65531,65530,65531,65530,65531,65530,65531,65530,65530,65531,65530,
    65531,65530,65531,65530,65531,65530,65531,65530,65531,65530,65531,65531,
    65530,65531,65530,65531,65530,65531,65530,65531,65530,65531,65531,65530,
    65531,65530,65531,65531,65530,65531,65530,65531,65531,65530,65531,65531,
    65530,65531,65530,65531,65531,65530,65531,65531,65530,65531,65531,65531,
    65530,65531,65531,65530,65531,65531,65530,65531,65531,65531,65530,65531,
    65531,65531,65530,65531,65531,65531,65530,65531,65531,65531,65530,65531,
    65531,65531,65531,65530,65531,65531,65531,65531,65530,65531,65531,65531,
    65531,65531,65530,65531,65531,65531,65531,65531,65531,65530,65531,65531,
    65531,65531,65531,65531,65531,65531,65530,65531,65531,65531,65531,65531,
    65531,65531,65531,65531,65531,65531,65531,65531,65531,65531,65531,65530,
    65531,65531,65531,65531,65531,65531,65531,65531,65531,65531,65531,65531,
    65531,65531,65531,65532,65531,65531,65531,65531,65531,65531,65531,65531,
    65531,65531,65531,65531,65531,65531,65531,65531,65532,65531,65531,65531,
    65531,65531,65531,65531,65531,65532,65531,65531,65531,65531,65531,65531,
    65532,65531,65531,65531,65531,65531,65531,65532,65531,65531,65531,65531,
    65532,65531,65531,65531,65531,65532,65531,65531,65531,65531,65532,65531,
    65531,65531,65532,65531,65531,65531,65532,65531,65531,65531,65532,65531,
    65531,65531,65532,65531,65531,65532,65531,65531,65531,65532,65531,65531,
    65532,65531,65531,65532,65531,65531,65532,65531,65531,65532,65531,65531,
    65532,65531,65531,65532,65531,65531,65532,65531,65531,65532,65531,65532,
    65531,65531,65532,65531,65532,65531,65531,65532,65531,65532,65531,65531,
    65532,65531,65532,65531,65532,65531,65532,65531,65531,65532,65531,65532,
    65531,65532,65531,65532,65531,65532,65531,65532,65531,65532,65531,65532,
    65531,65532,65531,65532,65531,65532,65531,65532,65531,65532,65531,65532,
    65531,65532,65531,65532,65531,65532,65531,65532,65532,65531,65532,65531,
    65532,65531,65532,65532,65531,65532,65531,65532,65531,65532,65532,65531,
    65532,65531,65532,65532,65531,65532,65532,65531,65532,65531,65532,65532,
    65531,65532,65532,65531,65532,65532,65531,65532,65532,65531,65532,65532,
    65531,65532,65532,65531,65532,65532,65531,65532,65532,65531,65532,65532,
    65531,65532,65532,65532,65531,65532,65532,65531,65532,65532,65532,65531,
    65532,65532,65532,65531,65532,65532,65532,65531,65532,65532,65532,65531,
    65532,65532,65532,65531,65532,65532,65532,65532,65531,65532,65532,65532,
    65532,65531,65532,65532,65532,65532,65531,65532,65532,65532,65532,65532,
    65531,65532,65532,65532,65532,65532,65531,65532,65532,65532,65532,65532,
    65532,65531,65532,65532,65532,65532,65532,65532,65532,65531,65532,65532,
    65532,65532,65532,65532,65532,65532,65531,65532,65532,65532,65532,65532,
    65532,65532,65532,65532,65532,65532,65532,65532,65531,65532,65532,65532,
    65532,65532,65532,65532,65532,65532,65532,65532,65532,65532,65532,65532,
    65532,65532,65532,65532,65532,65532,65532,65532,65532,65532,65532,65532,
    65532,65532,65532,65532,65532,65532,65532,65532,65532,65532,65532,65532,
    65532,65532,65532,65532,65532,65532,65532,65532,65532,65532,65533,65532,
    65532,65532,65532,65532,65532,65532,65532,65532,65532,65532,65532,65533,
    65532,65532,65532,65532,65532,65532,65532,65532,65532,65533,65532,65532,
    65532,65532,65532,65532,65532,65533,65532,65532,65532,65532,65532,65532,
    65532,65533,65532,65532,65532,65532,65532,65533,65532,65532,65532,65532,
    65532,65533,65532,65532,65532,65532,65532,65533,65532,65532,65532,65532,
    65533,65532,65532,65532,65532,65533,65532,65532,65532,65533,65532,65532,
    65532,65532,65533,65532,65532,65532,65533,65532,65532,65532,65533,65532,
    65532,65532,65533,65532,65532,65532,65533,65532,65532,65532,65533,65532,
    65532,65532,65533,65532,65532,65533,65532,65532,65532,65533,65532,65532,
    65533,65532,65532,65532,65533,65532,65532,65533,65532,65532,65533,65532,
    65532,65533,65532,65532,65533,65532,65532,65533,65532,65532,65533,65532,
    65532,65533,65532,65532,65533,65532,65532,65533,65532,65533,65532,65532,
    65533,65532,65532,65533,65532,65532,65533,65532,65533,65532,65532,65533,
    65532,65533,65532,65532,65533,65532,65533,65532,65532,65533,65532,65533,
    65532,65532,65533,65532,65533,65532,65533,65532,65532,65533,65532,65533,
    65532,65533,65532,65533,65532,65532,65533,65532,65533,65532,65533,65532,
    65533,65532,65533,65532,65532,65533,65532,65533,65532,65533,65532,65533,
    65532,65533,65532,65533,65532,65533,65532,65533,65532,65533,65532,65533,
    65532,65533,65532,65533,65532,65533,65532,65533,65532,65533,65532,65533,
    65532,65533,65532,65533,65532,65533,65533,65532,65533,65532,65533,65532,
    65533,65532,65533,65532,65533,65533,65532,65533,65532,65533,65532,65533,
    65533,65532,65533,65532,65533,65532,65533,65533,65532,65533,65532,65533,
    65532,65533,65533,65532,65533,65532,65533,65533,65532,65533,65532,65533,
    65533,65532,65533,65533,65532,65533,65532,65533,65533,65532,65533,65533,
    65532,65533,65532,65533,65533,65532,65533,65533,65532,65533,65533,65532,
    65533,65532,65533,65533,65532,65533,65533,65532,65533,65533,65532,65533,
    65533,65532,65533,65533,65532,65533,65533,65532,65533,65533,65533,65532,
    65533,65533,65532,65533,65533,65532,65533,65533,65533,65532,65533,65533,
    65532,65533,65533,65532,65533,65533,65533,65532,65533,65533,65533,65532,
    65533,65533,65532,65533,65533,65533,65532,65533,65533,65533,65532,65533,
    65533,65533,65532,65533,65533,65533,65532,65533,65533,65533,65532,65533,
    65533,65533,65532,65533,65533,65533,65533,65532,65533,65533,65533,65532,
    65533,65533,65533,65533,65532,65533,65533,65533,65533,65532,65533,65533,
    65533,65533,65532,65533,65533,65533,65533,65532,65533,65533,65533,65533,
    65532,65533,65533,65533,65533,65533,65532,65533,65533,65533,65533,65533,
    65532,65533,65533,65533,65533,65533,65533,65532,65533,65533,65533,65533,
    65533,65532,65533,65533,65533,65533,65533,65533,65533,65532,65533,65533,
    65533,65533,65533,65533,65533,65532,65533,65533,65533,65533,65533,65533,
    65533,65532,65533,65533,65533,65533,65533,65533,65533,65533,65533,65532,
    65533,65533,65533,65533,65533,65533,65533,65533,65533,65533,65533,65532,
    65533,65533,65533,65533,65533,65533,65533,65533,65533,65533,65533,65533,
    65533,65533,65532,65533,65533,65533,65533,65533,65533,65533,65533,65533,
    65533,65533,65533,65533,65533,65533,65533,65533,65533,65533,65533,65533,
    65533,65533,65533,65533,65533,65533,65532,65533,65533,65533,65533,65533,
    65533,65533,65533,65533,65533,65533,65533,65533,65533,65533,65533,65533,
    65533,65533,65533,65533,65533,65533,65533,65533,65533,65534,65533,65533,
    65533,65533,65533,65533,65533,65533,65533,65533,65533,65533,65533,65533,
    65533,65533,65533,65533,65533,65533,65533,65533,65533,65533,65533,65533,
    65534,65533,65533,65533,65533,65533,65533,65533,65533,65533,65533,65533,
    65533,65533,65533,65533,65534,65533,65533,65533,65533,65533,65533,65533,
    65533,65533,65533,65533,65534,65533,65533,65533,65533,65533,65533,65533,
    65533,65533,65533,65534,65533,65533,65533,65533,65533,65533,65533,65533,
    65534,65533,65533,65533,65533,65533,65533,65533,65534,65533,65533,65533,
    65533,65533,65533,65533,65534,65533,65533,65533,65533,65533,65533,65534,
    65533,65533,65533,65533,65533,65533,65534,65533,65533,65533,65533,65533,
    65533,65534,65533,65533,65533,65533,65533,65534,65533,65533,65533,65533,
    65533,65534,65533,65533,65533,65533,65534,65533,65533,65533,65533,65533,
    65534,65533,65533,65533,65533,65534,65533,65533,65533,65533,65534,65533,
    65533,65533,65533,65534,65533,65533,65533,65533,65534,65533,65533,65533,
    65533,65534,65533,65533,65533,65534,65533,65533,65533,65533,65534,65533,
    65533,65533,65534,65533,65533,65533,65533,65534,65533,65533,65533,65534,
    65533,65533,65533,65534,65533,65533,65533,65534,65533,65533,65533,65534,
    65533,65533,65533,65534,65533,65533,65533,65534,65533,65533,65534,65533,
    65533,65533,65534,65533,65533,65533,65534,65533,65533,65534,65533,65533,
    65533,65534,65533,65533,65533,65534,65533,65533,65534,65533,65533,65534,
    65533,65533,65533,65534,65533,65533,65534,65533,65533,65534,65533,65533,
    65533,65534,65533,65533,65534,65533,65533,65534,65533,65533,65534,65533,
    65533,65534,65533,65533,65533,65534,65533,65533,65534,65533,65533,65534,
    65533,65533,65534,65533,65533,65534,65533,65533,65534,65533,65534,65533,
    65533,65534,65533,65533,65534,65533,65533,65534,65533,65533,65534,65533,
    65533,65534,65533,65534,65533,65533,65534,65533,65533,65534,65533,65533,
    65534,65533,65534,65533,65533,65534,65533,65533,65534,65533,65534,65533,
    65533,65534,65533,65533,65534,65533,65534,65533,65533,65534,65533,65534,
    65533,65533,65534,65533,65534,65533,65533,65534,65533,65534,65533,65533,
    65534,65533,65534,65533,65533,65534,65533,65534,65533,65534,65533,65533,
    65534,65533,65534,65533,65533,65534,65533,65534,65533,65534,65533,65533,
    65534,65533,65534,65533,65534,65533,65533,65534,65533,65534,65533,65534,
    65533,65534,65533,65533,65534,65533,65534,65533,65534,65533,65534,65533,
    65533,65534,65533,65534,65533,65534,65533,65534,65533,65534,65533,65534,
    65533,65533,65534,65533,65534,65533,65534,65533,65534,65533,65534,65533,
    65534,65533,65534,65533,65534,65533,65534,65533,65534,65533,65533,65534,
    65533,65534,65533,65534,65533,65534,65533,65534,65533,65534,65533,65534,
    65533,65534,65533,65534,65533,65534,65533,65534,65533,65534,65533,65534,
    65533,65534,65533,65534,65533,65534,65534,65533,65534,65533,65534,65533,
    65534,65533,65534,65533,65534,65533,65534,65533,65534,65533,65534,65533,
    65534,65533,65534,65534,65533,65534,65533,65534,65533,65534,65533,65534,
    65533,65534,65533,65534,65534,65533,65534,65533,65534,65533,65534,65533,
    65534,65534,65533,65534,65533,65534,65533,65534,65533,65534,65534,65533,
    65534,65533,65534,65533,65534,65534,65533,65534,65533,65534,65533,65534,
    65534,65533,65534,65533,65534,65533,65534,65534,65533,65534,65533,65534,
    65533,65534,65534,65533,65534,65533,65534,65534,65533,65534,65533,65534,
    65534,65533,65534,65533,65534,65534,65533,65534,65533,65534,65534,65533,
    65534,65533,65534,65534,65533,65534,65533,65534,65534,65533,65534,65533,
    65534,65534,65533,65534,65534,65533,65534,65533,65534,65534,65533,65534,
    65534,65533,65534,65533,65534,65534,65533,65534,65534,65533,65534,65533,
    65534,65534,65533,65534,65534,65533,65534,65534,65533,65534,65534,65533,
    65534,65533,65534,65534,65533,65534,65534,65533,65534,65534,65533,65534,
    65534,65533,65534,65534,65533,65534,65534,65533,65534,65534,65533,65534,
    65534,65533,65534,65534,65533,65534,65534,65533,65534,65534,65533,65534,
    65534,65533,65534,65534,65533,65534,65534,65533,65534,65534,65533,65534,
    65534,65533,65534,65534,65533,65534,65534,65534,65533,65534,65534,65533,
    65534,65534,65533,65534,65534,65534,65533,65534,65534,65533,65534,65534,
    65533,65534,65534,65534,65533,65534,65534,65533,65534,65534,65533,65534,
    65534,65534,65533,65534,65534,65533,65534,65534,65534,65533,65534,65534,
    65534,65533,65534,65534,65533,65534,65534,65534,65533,65534,65534,65534,
    65533,65534,65534,65533,65534,65534,65534,65533,65534,65534,65534,65533,
    65534,65534,65534,65533,65534,65534,65534,65533,65534,65534,65534,65533,
    65534,65534,65534,65533,65534,65534,65534,65533,65534,65534,65534,65533,
    65534,65534,65534,65533,65534,65534,65534,65533,65534,65534,65534,65533,
    65534,65534,65534,65534,65533,65534,65534,65534,65533,65534,65534,65534,
    65533,65534,65534,65534,65534,65533,65534,65534,65534,65533,65534,65534,
    65534,65534,65533,65534,65534,65534,65534,65533,65534,65534,65534,65534,
    65533,65534,65534,65534,65533,65534,65534,65534,65534,65533,65534,65534,
    65534,65534,65533,65534,65534,65534,65534,65534,65533,65534,65534,65534,
    65534,65533,65534,65534,65534,65534,65533,65534,65534,65534,65534,65534,
    65533,65534,65534,65534,65534,65533,65534,65534,65534,65534,65534,65533,
    65534,65534,65534,65534,65534,65533,65534,65534,65534,65534,65534,65533,
    65534,65534,65534,65534,65534,65533,65534,65534,65534,65534,65534,65533,
    65534,65534,65534,65534,65534,65534,65533,65534,65534,65534,65534,65534,
    65533,65534,65534,65534,65534,65534,65534,65533,65534,65534,65534,65534,
    65534,65534,65533,65534,65534,65534,65534,65534,65534,65534,65533,65534,
    65534,65534,65534,65534,65534,65533,65534,65534,65534,65534,65534,65534,
    65534,65533,65534,65534,65534,65534,65534,65534,65534,65534,65533,65534,
    65534,65534,65534,65534,65534,65534,65533,65534,65534,65534,65534,65534,
    65534,65534,65534,65533,65534,65534,65534,65534,65534,65534,65534,65534,
    65534,65533,65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,
    65533,65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,65533,
    65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,65533,
    65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,
    65534,65533,65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,
    65534,65534,65534,65534,65534,65533,65534,65534,65534,65534,65534,65534,
    65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,
    65533,65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,
    65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,
    65534,65534,65534,65533,65534,65534,65534,65534,65534,65534,65534,65534,
    65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,
    65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,
    65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,
    65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,
    65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,
    65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,
    65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,
    65534,65534,65534,65534,65534,65534,65534,65535,65534,65534,65534,65534,
    65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,
    65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,65535,65534,
    65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,
    65534,65534,65534,65534,65534,65534,65535,65534,65534,65534,65534,65534,
    65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,65535,65534,
    65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,
    65534,65535,65534,65534,65534,65534,65534,65534,65534,65534,65534,65534,
    65534,65534,65535,65534,65534,65534,65534,65534,65534,65534,65534,65534,
    65534,65534,65535,65534,65534,65534,65534,65534,65534,65534,65534,65534,
    65534,65535,65534,65534,65534,65534,65534,65534,65534,65534,65534,65535,
    65534,65534,65534,65534,65534,65534,65534,65534,65534,65535,65534,65534,
    65534,65534,65534,65534,65534,65534,65535,65534,65534,65534,65534,65534,
    65534,65534,65534,65535,65534,65534,65534,65534,65534,65534,65534,65534,
    65535,65534,65534,65534,65534,65534,65534,65534,65535,65534,65534,65534,
    65534,65534,65534,65534,65535,65534,65534,65534,65534,65534,65534,65534,
    65535,65534,65534,65534,65534,65534,65534,65534,65535,65534,65534,65534,
    65534,65534,65534,65535,65534,65534,65534,65534,65534,65534,65535,65534,
    65534,65534,65534,65534,65534,65535,65534,65534,65534,65534,65534,65534,
    65535,65534,65534,65534,65534,65534,65534,65535,65534,65534,65534,65534,
    65534,65535,65534,65534,65534,65534,65534,65535,65534,65534,65534,65534,
    65534,65534,65535,65534,65534,65534,65534,65534,65535,65534,65534,65534,
    65534,65534,65535,65534,65534,65534,65534,65534,65535,65534,65534,65534,
    65534,65534,65535,65534,65534,65534,65534,65535,65534,65534,65534,65534,
    65534,65535,65534,65534,65534,65534,65535,65534,65534,65534,65534,65534,
    65535,65534,65534,65534,65534,65535,65534,65534,65534,65534,65534,65535,
    65534,65534,65534,65534,65535,65534,65534,65534,65534,65535,65534,65534,
    65534,65534,65535,65534,65534,65534,65534,65535,65534,65534,65534,65534,
    65535,65534,65534,65534,65534,65535,65534,65534,65534,65534,65535,65534,
    65534,65534,65534,65535,65534,65534,65534,65535,65534,65534,65534,65534,
    65535,65534,65534,65534,65534,65535,65534,65534,65534,65535,65534,65534,
    65534,65534,65535,65534,65534,65534,65535,65534,65534,65534,65534,65535,
    65534,65534,65534,65535,65534,65534,65534,65534,65535,65534,65534,65534,
    65535,65534,65534,65534,65535,65534,65534,65534,65534,65535,65534,65534,
    65534,65535,65534,65534,65534,65535,65534,65534,65534,65535,65534,65534,
    65534,65534,65535,65534,65534,65534,65535,65534,65534,65534,65535,65534,
    65534,65534,65535,65534,65534,65534,65535,65534,65534,65534,65535,65534,
    65534,65534,65535,65534,65534,65534,65535,65534,65534,65534,65535,65534,
    65534,65535,65534,65534,65534,65535,65534,65534,65534,65535,65534,65534,
    65534,65535,65534,65534,65534,65535,65534,65534,65534,65535,65534,65534,
    65535,65534,65534,65534,65535,65534,65534,65534,65535,65534,65534,65535,
    65534,65534,65534,65535,65534,65534,65534,65535,65534,65534,65535,65534,
    65534,65534,65535,65534,65534,65535,65534,65534,65534,65535,65534,65534,
    65535,65534,65534,65534,65535,65534,65534,65535,65534,65534,65534,65535,
    65534,65534,65535,65534,65534,65534,65535,65534,65534,65535,65534,65534,
    65534,65535,65534,65534,65535,65534,65534,65535,65534,65534,65534,65535,
    65534,65534,65535,65534,65534,65535,65534,65534,65534,65535,65534,65534,
    65535,65534,65534,65535,65534,65534,65535,65534,65534,65534,65535,65534,
    65534,65535,65534,65534,65535,65534,65534,65535,65534,65534,65535,65534,
    65534,65534,65535,65534,65534,65535,65534,65534,65535,65534,65534,65535,
    65534,65534,65535,65534,65534,65535,65534,65534,65535,65534,65534,65535,
    65534,65534,65534,65535,65534,65534,65535,65534,65534,65535,65534,65534,
    65535,65534,65534,65535,65534,65534,65535,65534,65534,65535,65534,65534,
    65535,65534,65534,65535,65534,65534,65535,65534,65534,65535,65534,65534,
    65535,65534,65534,65535,65534,65535,65534,65534,65535,65534,65534,65535,
    65534,65534,65535,65534,65534,65535,65534,65534,65535,65534,65534,65535,
    65534,65534,65535,65534,65535,65534,65534,65535,65534,65534,65535,65534,
    65534,65535,65534,65534,65535,65534,65534,65535,65534,65535,65534,65534,
    65535,65534,65534,65535,65534,65534,65535,65534,65535,65534,65534,65535,
    65534,65534,65535,65534,65534,65535,65534,65535,65534,65534,65535,65534,
    65534,65535,65534,65534,65535,65534,65535,65534,65534,65535,65534,65534,
    65535,65534,65535,65534,65534,65535,65534,65534,65535,65534,65535,65534,
    65534,65535,65534,65534,65535,65534,65535,65534,65534,65535,65534,65535,
    65534,65534,65535,65534,65534,65535,65534,65535,65534,65534,65535,65534,
    65535,65534,65534,65535,65534,65534,65535,65534,65535,65534,65534,65535,
    65534,65535,65534,65534,65535,65534,65535,65534,65534,65535,65534,65535,
    65534,65534,65535,65534,65535,65534,65534,65535,65534,65535,65534,65534,
    65535,65534,65535,65534,65534,65535,65534,65535,65534,65534,65535,65534,
    65535,65534,65534,65535,65534,65535,65534,65534,65535,65534,65535,65534,
    65534,65535,65534,65535,65534,65535,65534,65534,65535,65534,65535,65534,
    65534,65535,65534,65535,65534,65535,65534,65534,65535,65534,65535,65534,
    65534,65535,65534,65535,65534,65535,65534,65534,65535,65534,65535,65534,
    65535,65534,65534,65535,65534,65535,65534,65534,65535,65534,65535,65534,
    65535,65534,65534,65535,65534,65535,65534,65535,65534,65534,65535,65534,
    65535,65534,65535,65534,65535,65534,65534,65535,65534,65535,65534,65535,
    65534,65534,65535,65534,65535,65534,65535,65534,65535,65534,65534,65535,
    65534,65535,65534,65535,65534,65534,65535,65534,65535,65534,65535,65534,
    65535,65534,65534,65535,65534,65535,65534,65535,65534,65535,65534,65535,
    65534,65534,65535,65534,65535,65534,65535,65534,65535,65534,65534,65535,
    65534,65535,65534,65535,65534,65535,65534,65535,65534,65534,65535,65534,
    65535,65534,65535,65534,65535,65534,65535,65534,65535,65534,65534,65535,
    65534,65535,65534,65535,65534,65535,65534,65535,65534,65535,65534,65535,
    65534,65534,65535,65534,65535,65534,65535,65534,65535,65534,65535,65534,
    65535,65534,65535,65534,65534,65535,65534,65535,65534,65535,65534,65535,
    65534,65535,65534,65535,65534,65535,65534,65535,65534,65535,65534,65535,
    65534,65534,65535,65534,65535,65534,65535,65534,65535,65534,65535,65534,
    65535,65534,65535,65534,65535,65534,65535,65534,65535,65534,65535,65534,
    65535,65534,65535,65534,65535,65534,65535,65534,65535,65534,65535,65534,
    65534,65535,65534,65535,65534,65535,65534,65535,65534,65535,65534,65535,
    65534,65535,65534,65535,65534,65535,65534,65535,65534,65535,65534,65535,
    65534,65535,65534,65535,65534,65535,65534,65535,65534,65535,65534,65535,
    65534,65535,65534,65535,65534,65535,65534,65535,65534,65535,65534,65535,
    65534,65535,65534,65535,65534,65535,65535,65534,65535,65534,65535,65534,
    65535,65534,65535,65534,65535,65534,65535,65534,65535,65534,65535,65534,
    65535,65534,65535,65534,65535,65534,65535,65534,65535,65534,65535,65534,
    65535,65534,65535,65535,65534,65535,65534,65535,65534,65535,65534,65535,
    65534,65535,65534,65535,65534,65535,65534,65535,65534,65535,65534,65535,
    65535,65534,65535,65534,65535,65534,65535,65534,65535,65534,65535,65534,
    65535,65534,65535,65534,65535,65535,65534,65535,65534,65535,65534,65535,
    65534,65535,65534,65535,65534,65535,65534,65535,65535,65534,65535,65534,
    65535,65534,65535,65534,65535,65534,65535,65534,65535,65535,65534,65535,
    65534,65535,65534,65535,65534,65535,65534,65535,65535,65534,65535,65534,
    65535,65534,65535,65534,65535,65534,65535,65535,65534,65535,65534,65535,
    65534,65535,65534,65535,65534,65535,65535,65534,65535,65534,65535,65534,
    65535,65534,65535,65535,65534,65535,65534,65535,65534,65535,65534,65535,
    65535,65534,65535,65534,65535,65534,65535,65534,65535,65535,65534,65535,
    65534,65535,65534,65535,65535,65534,65535,65534,65535,65534,65535,65534,
    65535,65535,65534,65535,65534,65535,65534,65535,65535,65534,65535,65534,
    65535,65534,65535,65535,65534,65535,65534,65535,65534,65535,65535,65534,
    65535,65534,65535,65534,65535,65535,65534,65535,65534,65535,65534,65535,
    65535,65534,65535,65534,65535,65534,65535,65535,65534,65535,65534,65535,
    65534,65535,65535,65534,65535,65534,65535,65535,65534,65535,65534,65535,
    65534,65535,65535,65534,65535,65534,65535,65535,65534,65535,65534,65535,
    65534,65535,65535,65534,65535,65534,65535,65535,65534,65535,65534,65535,
    65535,65534,65535,65534,65535,65534,65535,65535,65534,65535,65534,65535,
    65535,65534,65535,65534,65535,65535,65534,65535,65534,65535,65535,65534,
    65535,65534,65535,65535,65534,65535,65534,65535,65535,65534,65535,65534,
    65535,65535,65534,65535,65534,65535,65535,65534,65535,65534,65535,65535,
    65534,65535,65534,65535,65535,65534,65535,65534,65535,65535,65534,65535,
    65534,65535,65535,65534,65535,65534,65535,65535,65534,65535,65535,65534,
    65535,65534,65535,65535,65534,65535,65534,65535,65535,65534,65535,65534,
    65535,65535,65534,65535,65535,65534,65535,65534,65535,65535,65534,65535,
    65534,65535,65535,65534,65535,65535,65534,65535,65534,65535,65535,65534,
    65535,65535,65534,65535,65534,65535,65535,65534,65535,65535,65534,65535,
    65534,65535,65535,65534,65535,65535,65534,65535,65534,65535,65535,65534,
    65535,65535,65534,65535,65534,65535,65535,65534,65535,65535,65534,65535,
    65534,65535,65535,65534,65535,65535,65534,65535,65535,65534,65535,65534,
    65535,65535,65534,65535,65535,65534,65535,65535,65534,65535,65534,65535,
    65535,65534,65535,65535,65534,65535,65535,65534,65535,65534,65535,65535,
    65534,65535,65535,65534,65535,65535,65534,65535,65534,65535,65535,65534,
    65535,65535,65534,65535,65535,65534,65535,65535,65534,65535,65535,65534,
    65535,65534,65535,65535,65534,65535,65535,65534,65535,65535,65534,65535,
    65535,65534,65535,65535,65534,65535,65535,65534,65535,65534,65535,65535,
    65534,65535,65535,65534,65535,65535,65534,65535,65535,65534,65535,65535,
    65534,65535,65535,65534,65535,65535,65534,65535,65535,65534,65535,65535,
    65534,65535,65535,65534,65535,65535,65534,65535,65534,65535,65535,65534,
    65535,65535,65534,65535,65535,65534,65535,65535,65534,65535,65535,65534,
    65535,65535,65534,65535,65535,65534,65535,65535,65534,65535,65535,65534,
    65535,65535,65534,65535,65535,65535,65534,65535,65535,65534,65535,65535,
    65534,65535,65535,65534,65535,65535,65534,65535,65535,65534,65535,65535,
    65534,65535,65535,65534,65535,65535,65534,65535,65535,65534,65535,65535,
    65534,65535,65535,65534,65535,65535,65535,65534,65535,65535,65534,65535,
    65535,65534,65535,65535,65534,65535,65535,65534,65535,65535,65534,65535,
    65535,65534,65535,65535,65535,65534,65535,65535,65534,65535,65535,65534,
    65535,65535,65534,65535,65535,65534,65535,65535,65535,65534,65535,65535,
    65534,65535,65535,65534,65535,65535,65534,65535,65535,65535,65534,65535,
    65535,65534,65535,65535,65534,65535,65535,65535,65534,65535,65535,65534,
    65535,65535,65534,65535,65535,65534,65535,65535,65535,65534,65535,65535,
    65534,65535,65535,65534,65535,65535,65535,65534,65535,65535,65534,65535,
    65535,65535,65534,65535,65535,65534,65535,65535,65534,65535,65535,65535,
    65534,65535,65535,65534,65535,65535,65535,65534,65535,65535,65534,65535,
    65535,65534,65535,65535,65535,65534,65535,65535,65534,65535,65535,65535,
    65534,65535,65535,65534,65535,65535,65535,65534,65535,65535,65534,65535,
    65535,65535,65534,65535,65535,65534,65535,65535,65535,65534,65535,65535,
    65534,65535,65535,65535,65534,65535,65535,65535,65534,65535,65535,65534,
    65535,65535,65535,65534,65535,65535,65534,65535,65535,65535,65534,65535,
    65535,65535,65534,65535,65535,65534,65535,65535,65535,65534,65535,65535,
    65534,65535,65535,65535,65534,65535,65535,65535,65534,65535,65535,65535,
    65534,65535,65535,65534,65535,65535,65535,65534,65535,65535,65535,65534,
    65535,65535,65534,65535,65535,65535,65534,65535,65535,65535,65534,65535,
    65535,65535,65534,65535,65535,65535,65534,65535,65535,65534,65535,65535,
    65535,65534,65535,65535,65535,65534,65535,65535,65535,65534,65535,65535,
    65535,65534,65535,65535,65535,65534,65535,65535,65534,65535,65535,65535,
    65534,65535,65535,65535,65534,65535,65535,65535,65534,65535,65535,65535,
    65534,65535,65535,65535,65534,65535,65535,65535,65534,65535,65535,65535,
    65534,65535,65535,65535,65534,65535,65535,65535,65534,65535,65535,65535,
    65534,65535,65535,65535,65534,65535,65535,65535,65534,65535,65535,65535,
    65534,65535,65535,65535,65534,65535,65535,65535,65534,65535,65535,65535,
    65535,65534,65535,65535,65535,65534,65535,65535,65535,65534,65535,65535,
    65535,65534,65535,65535,65535,65534,65535,65535,65535,65534,65535,65535,
    65535,65535,65534,65535,65535,65535,65534,65535,65535,65535,65534,65535,
    65535,65535,65534,65535,65535,65535,65535,65534,65535,65535,65535,65534,
    65535,65535,65535,65534,65535,65535,65535,65535,65534,65535,65535,65535,
    65534,65535,65535,65535,65534,65535,65535,65535,65535,65534,65535,65535,
    65535,65534,65535,65535,65535,65535,65534,65535,65535,65535,65534,65535,
    65535,65535,65535,65534,65535,65535,65535,65534,65535,65535,65535,65535,
    65534,65535,65535,65535,65534,65535,65535,65535,65535,65534,65535,65535,
    65535,65534,65535,65535,65535,65535,65534,65535,65535,65535,65535,65534,
    65535,65535,65535,65534,65535,65535,65535,65535,65534,65535,65535,65535,
    65535,65534,65535,65535,65535,65534,65535,65535,65535,65535,65534,65535,
    65535,65535,65535,65534,65535,65535,65535,65535,65534,65535,65535,65535,
    65534,65535,65535,65535,65535,65534,65535,65535,65535,65535,65534,65535,
    65535,65535,65535,65534,65535,65535,65535,65535,65534,65535,65535,65535,
    65535,65534,65535,65535,65535,65535,65534,65535,65535,65535,65535,65534,
    65535,65535,65535,65535,65534,65535,65535,65535,65535,65534,65535,65535,
    65535,65535,65534,65535,65535,65535,65535,65534,65535,65535,65535,65535,
    65534,65535,65535,65535,65535,65534,65535,65535,65535,65535,65534,65535,
    65535,65535,65535,65535,65534,65535,65535,65535,65535,65534,65535,65535,
    65535,65535,65534,65535,65535,65535,65535,65534,65535,65535,65535,65535,
    65535,65534,65535,65535,65535,65535,65534,65535,65535,65535,65535,65535,
    65534,65535,65535,65535,65535,65534,65535,65535,65535,65535,65534,65535,
    65535,65535,65535,65535,65534,65535,65535,65535,65535,65534,65535,65535,
    65535,65535,65535,65534,65535,65535,65535,65535,65535,65534,65535,65535,
    65535,65535,65534,65535,65535,65535,65535,65535,65534,65535,65535,65535,
    65535,65535,65534,65535,65535,65535,65535,65534,65535,65535,65535,65535,
    65535,65534,65535,65535,65535,65535,65535,65534,65535,65535,65535,65535,
    65535,65534,65535,65535,65535,65535,65535,65534,65535,65535,65535,65535,
    65535,65534,65535,65535,65535,65535,65535,65534,65535,65535,65535,65535,
    65535,65534,65535,65535,65535,65535,65535,65534,65535,65535,65535,65535,
    65535,65534,65535,65535,65535,65535,65535,65534,65535,65535,65535,65535,
    65535,65534,65535,65535,65535,65535,65535,65535,65534,65535,65535,65535,
    65535,65535,65534,65535,65535,65535,65535,65535,65534,65535,65535,65535,
    65535,65535,65535,65534,65535,65535,65535,65535,65535,65534,65535,65535,
    65535,65535,65535,65535,65534,65535,65535,65535,65535,65535,65535,65534,
    65535,65535,65535,65535,65535,65534,65535,65535,65535,65535,65535,65535,
    65534,65535,65535,65535,65535,65535,65535,65534,65535,65535,65535,65535,
    65535,65534,65535,65535,65535,65535,65535,65535,65534,65535,65535,65535,
    65535,65535,65535,65534,65535,65535,65535,65535,65535,65535,65534,65535,
    65535,65535,65535,65535,65535,65534,65535,65535,65535,65535,65535,65535,
    65535,65534,65535,65535,65535,65535,65535,65535,65534,65535,65535,65535,
    65535,65535,65535,65534,65535,65535,65535,65535,65535,65535,65535,65534,
    65535,65535,65535,65535,65535,65535,65534,65535,65535,65535,65535,65535,
    65535,65535,65534,65535,65535,65535,65535,65535,65535,65534,65535,65535,
    65535,65535,65535,65535,65535,65534,65535,65535,65535,65535,65535,65535,
    65535,65534,65535,65535,65535,65535,65535,65535,65535,65534,65535,65535,
    65535,65535,65535,65535,65535,65534,65535,65535,65535,65535,65535,65535,
    65535,65534,65535,65535,65535,65535,65535,65535,65535,65534,65535,65535,
    65535,65535,65535,65535,65535,65534,65535,65535,65535,65535,65535,65535,
    65535,65534,65535,65535,65535,65535,65535,65535,65535,65535,65534,65535,
    65535,65535,65535,65535,65535,65535,65535,65534,65535,65535,65535,65535,
    65535,65535,65535,65534,65535,65535,65535,65535,65535,65535,65535,65535,
    65534,65535,65535,65535,65535,65535,65535,65535,65535,65534,65535,65535,
    65535,65535,65535,65535,65535,65535,65534,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65534,65535,65535,65535,65535,65535,65535,65535,
    65535,65534,65535,65535,65535,65535,65535,65535,65535,65535,65535,65534,
    65535,65535,65535,65535,65535,65535,65535,65535,65534,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65534,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65534,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65534,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65534,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65534,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65534,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65534,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65534,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65534,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65534,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65534,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65534,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65534,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65534,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65534,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65534,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65534,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65534,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65534,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65534,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65534,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65534,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65534,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65534,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65534,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65534,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65534,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65534,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65534,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65534,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,0,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,0,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,0,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,0,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,0,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,0,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,0,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,0,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,0,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,0,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    0,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,0,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    0,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,0,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,0,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    0,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,0,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,0,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,0,65535,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,0,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,0,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,0,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,0,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,0,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,0,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,0,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,0,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,0,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,0,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,0,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    65535,0,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    0,65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,0,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,65535,0,65535,
    65535,65535,65535,65535,65535,65535,65535,65535,65535,0,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,0,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,0,65535,65535,65535,65535,65535,65535,
    65535,65535,65535,65535,0,65535,65535,65535,65535,65535,65535,65535,
    65535,65535,0,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    0,65535,65535,65535,65535,65535,65535,65535,65535,0,65535,65535,
    65535,65535,65535,65535,65535,65535,65535,0,65535,65535,65535,65535,
    65535,65535,65535,65535,65535,0,65535,65535,65535,65535,65535,65535,
    65535,65535,0,65535,65535,65535,65535,65535,65535,65535,65535,65535,
    0,65535,65535,65535,65535,65535,65535,65535,65535,0,65535,65535,
    65535,65535,65535,65535,65535,65535,0,65535,65535,65535,65535,65535,
    65535,65535,65535,0,65535,65535,65535,65535,65535,65535,65535,65535,
    0,65535,65535,65535,65535,65535,65535,65535,65535,0,65535,65535,
    65535,65535,65535,65535,65535,65535,0,65535,65535,65535,65535,65535,
    65535,65535,65535,0,65535,65535,65535,65535,65535,65535,65535,65535,
    0,65535,65535,65535,65535,65535,65535,65535,0,65535,65535,65535,
    65535,65535,65535,65535,65535,0,65535,65535,65535,65535,65535,65535,
    65535,0,65535,65535,65535,65535,65535,65535,65535,65535,0,65535,
    65535,65535,65535,65535,65535,65535,0,65535,65535,65535,65535,65535,
    65535,65535,65535,0,65535,65535,65535,65535,65535,65535,65535,0,
    65535,65535,65535,65535,65535,65535,65535,0,65535,65535,65535,65535,
    65535,65535,65535,0,65535,65535,65535,65535,65535,65535,65535,0,
    65535,65535,65535,65535,65535,65535,65535,0,65535,65535,65535,65535,
    65535,65535,65535,0,65535,65535,65535,65535,65535,65535,65535,0,
    65535,65535,65535,65535,65535,65535,65535,0,65535,65535,65535,65535,
    65535,65535,0,65535,65535,65535,65535,65535,65535,65535,0,65535,
    65535,65535,65535,65535,65535,65535,0,65535,65535,65535,65535,65535,
    65535,0,65535,65535,65535,65535,65535,65535,65535,0,65535,65535,
    65535,65535,65535,65535,65535,0,65535,65535,65535,65535,65535,65535,
    0,65535,65535,65535,65535,65535,65535,0,65535,65535,65535,65535,
    65535,65535,65535,0,65535,65535,65535,65535,65535,65535,0,65535,
    65535,65535,65535,65535,65535,0,65535,65535,65535,65535,65535,65535,
    65535,0,65535,65535,65535,65535,65535,65535,0,65535,65535,65535,
    65535,65535,65535,0,65535,65535,65535,65535,65535,65535,0,65535,
    65535,65535,65535,65535,65535,0,65535,65535,65535,65535,65535,65535,
    0,65535,65535,65535,65535,65535,65535,0,65535,65535,65535,65535,
    65535,65535,0,65535,65535,65535,65535,65535,65535,0,65535,65535,
    65535,65535,65535,65535,0,65535,65535,65535,65535,65535,65535,0,
    65535,65535,65535,65535,65535,65535,0,65535,65535,65535,65535,65535,
    65535,0,65535,65535,65535,65535,65535,0,65535,65535,65535,65535,
    65535,65535,0,65535,65535,65535,65535,65535,65535,0,65535,65535,
    65535,65535,65535,0,65535,65535,65535,65535,65535,65535,0,65535,
    65535,65535,65535,65535,65535,0,65535,65535,65535,65535,65535,0,
    65535,65535,65535,65535,65535,65535,0,65535,65535,65535,65535,65535,
    0,65535,65535,65535,65535,65535,65535,0,65535,65535,65535,65535,
    65535,0,65535,65535,65535,65535,65535,65535,0,65535,65535,65535,
    65535,65535,0,65535,65535,65535,65535,65535,0,65535,65535,65535,
    65535,65535,65535,0,65535,65535,65535,65535,65535,0,65535,65535,
    65535,65535,65535,0,65535,65535,65535,65535,65535,65535,0,65535,
    65535,65535,65535,65535,0,65535,65535,65535,65535,65535,0,65535,
    65535,65535,65535,65535,0,65535,65535,65535,65535,65535,0,65535,
    65535,65535,65535,65535,65535,0,65535,65535,65535,65535,65535,0,
    65535,65535,65535,65535,65535,0,65535,65535,65535,65535,65535,0,
    65535,65535,65535,65535,65535,0,65535,65535,65535,65535,65535,0,
    65535,65535,65535,65535,65535,0,65535,65535,65535,65535,65535,0,
    65535,65535,65535,65535,65535,0,65535,65535,65535,65535,65535,0,
    65535,65535,65535,65535,65535,0,65535,65535,65535,65535,65535,0,
    65535,65535,65535,65535,65535,0,65535,65535,65535,65535,0,65535,
    65535,65535,65535,65535,0,65535,65535,65535,65535,65535,0,65535,
    65535,65535,65535,65535,0,65535,65535,65535,65535,65535,0,65535,
    65535,65535,65535,0,65535,65535,65535,65535,65535,0,65535,65535,
    65535,65535,65535,0,65535,65535,65535,65535,0,65535,65535,65535,
    65535,65535,0,65535,65535,65535,65535,65535,0,65535,65535,65535,
    65535,0,65535,65535,65535,65535,65535,0,65535,65535,65535,65535,
    65535,0,65535,65535,65535,65535,0,65535,65535,65535,65535,65535,
    0,65535,65535,65535,65535,0,65535,65535,65535,65535,65535,0,
    65535,65535,65535,65535,0,65535,65535,65535,65535,65535,0,65535,
    65535,65535,65535,0,65535,65535,65535,65535,65535,0,65535,65535,
    65535,65535,0,65535,65535,65535,65535,65535,0,65535,65535,65535,
    65535,0,65535,65535,65535,65535,65535,0,65535,65535,65535,65535,
    0,65535,65535,65535,65535,0,65535,65535,65535,65535,65535,0,
    65535,65535,65535,65535,0,65535,65535,65535,65535,0,65535,65535,
    65535,65535,65535,0,65535,65535,65535,65535,0,65535,65535,65535,
    65535,0,65535,65535,65535,65535,65535,0,65535,65535,65535,65535,
    0,65535,65535,65535,65535,0,65535,65535,65535,65535,0,65535,
    65535,65535,65535,65535,0,65535,65535,65535,65535,0,65535,65535,
    65535,65535,0,65535,65535,65535,65535,0,65535,65535,65535,65535,
    0,65535,65535,65535,65535,65535,0,65535,65535,65535,65535,0,
    65535,65535,65535,65535,0,65535,65535,65535,65535,0,65535,65535,
    65535,65535,0,65535,65535,65535,65535,0,65535,65535,65535,65535,
    0,65535,65535,65535,65535,0,65535,65535,65535,65535,0,65535,
    65535,65535,65535,0,65535,65535,65535,65535,0,65535,65535,65535,
    65535,0,65535,65535,65535,65535,0,65535,65535,65535,65535,0,
    65535,65535,65535,65535,0,65535,65535,65535,65535,0,65535,65535,
    65535,65535,0,65535,65535,65535,65535,0,65535,65535,65535,65535,
    0,65535,65535,65535,65535,0,65535,65535,65535,65535,0,65535,
    65535,65535,65535,0,65535,65535,65535,65535,0,65535,65535,65535,
    65535,0,65535,65535,65535,0,65535,65535,65535,65535,0,65535,
    65535,65535,65535,0,65535,65535,65535,65535,0,65535,65535,65535,
    65535,0,65535,65535,65535,65535,0,65535,65535,65535,0,65535,
    65535,65535,65535,0,65535,65535,65535,65535,0,65535,65535,65535,
    65535,0,65535,65535,65535,65535,0,65535,65535,65535,0,65535,
    65535,65535,65535,0,65535,65535,65535,65535,0,65535,65535,65535,
    0,65535,65535,65535,65535,0,65535,65535,65535,65535,0,65535,
    65535,65535,65535,0,65535,65535,65535,0,65535,65535,65535,65535,
    0,65535,65535,65535,65535,0,65535,65535,65535,0,65535,65535,
    65535,65535,0,65535,65535,65535,65535,0,65535,65535,65535,0,
    65535,65535,65535,65535,0,65535,65535,65535,0,65535,65535,65535,
    65535,0,65535,65535,65535,65535,0,65535,65535,65535,0,65535,
    65535,65535,65535,0,65535,65535,65535,0,65535,65535,65535,65535,
    0,65535,65535,65535,0,65535,65535,65535,65535,0,65535,65535,
    65535,65535,0,65535,65535,65535,0,65535,65535,65535,65535,0,
    65535,65535,65535,0,65535,65535,65535,65535,0,65535,65535,65535,
    0,65535,65535,65535,65535,0,65535,65535,65535,0,65535,65535,
    65535,0,65535,65535,65535,65535,0,65535,65535,65535,0,65535,
    65535,65535,65535,0,65535,65535,65535,0,65535,65535,65535,65535,
    0,65535,65535,65535,0,65535,65535,65535,65535,0,65535,65535,
    65535,0,65535,65535,65535,0,65535,65535,65535,65535,0,65535,
    65535,65535,0,65535,65535,65535,65535,0,65535,65535,65535,0,
    65535,65535,65535,0,65535,65535,65535,65535,0,65535,65535,65535,
    0,65535,65535,65535,0,65535,65535,65535,65535,0,65535,65535,
    65535,0,65535,65535,65535,0,65535,65535,65535,65535,0,65535,
    65535,65535,0,65535,65535,65535,0,65535,65535,65535,0,65535,
    65535,65535,65535,0,65535,65535,65535,0,65535,65535,65535,0,
    65535,65535,65535,65535,0,65535,65535,65535,0,65535,65535,65535,
    0,65535,65535,65535,0,65535,65535,65535,65535,0,65535,65535,
    65535,0,65535,65535,65535,0,65535,65535,65535,0,65535,65535,
    65535,0,65535,65535,65535,65535,0,65535,65535,65535,0,65535,
    65535,65535,0,65535,65535,65535,0,65535,65535,65535,0,65535,
    65535,65535,65535,0,65535,65535,65535,0,65535,65535,65535,0,
    65535,65535,65535,0,65535,65535,65535,0,65535,65535,65535,0,
    65535,65535,65535,0,65535,65535,65535,65535,0,65535,65535,65535,
    0,65535,65535,65535,0,65535,65535,65535,0,65535,65535,65535,
    0,65535,65535,65535,0,65535,65535,65535,0,65535,65535,65535,
    0,65535,65535,65535,0,65535,65535,65535,0,65535,65535,65535,
    65535,0,65535,65535,65535,0,65535,65535,65535,0,65535,65535,
    65535,0,65535,65535,65535,0,65535,65535,65535,0,65535,65535,
    65535,0,65535,65535,65535,0,65535,65535,65535,0,65535,65535,
    65535,0,65535,65535,65535,0,65535,65535,65535,0,65535,65535,
    65535,0,65535,65535,65535,0,65535,65535,65535,0,65535,65535,
    65535,0,65535,65535,65535,0,65535,65535,65535,0,65535,65535,
    65535,0,65535,65535,65535,0,65535,65535,65535,0,65535,65535,
    65535,0,65535,65535,65535,0,65535,65535,65535,0,65535,65535,
    65535,0,65535,65535,65535,0,65535,65535,65535,0,65535,65535,
    0,65535,65535,65535,0,65535,65535,65535,0,65535,65535,65535,
    0,65535,65535,65535,0,65535,65535,65535,0,65535,65535,65535,
    0,65535,65535,65535,0,65535,65535,65535,0,65535,65535,65535,
    0,65535,65535,0,65535,65535,65535,0,65535,65535,65535,0,
    65535,65535,65535,0,65535,65535,65535,0,65535,65535,65535,0,
    65535,65535,65535,0,65535,65535,0,65535,65535,65535,0,65535,
    65535,65535,0,65535,65535,65535,0,65535,65535,65535,0,65535,
    65535,65535,0,65535,65535,0,65535,65535,65535,0,65535,65535,
    65535,0,65535,65535,65535,0,65535,65535,65535,0,65535,65535,
    0,65535,65535,65535,0,65535,65535,65535,0,65535,65535,65535,
    0,65535,65535,65535,0,65535,65535,0,65535,65535,65535,0,
    65535,65535,65535,0,65535,65535,65535,0,65535,65535,0,65535,
    65535,65535,0,65535,65535,65535,0,65535,65535,65535,0,65535,
    65535,0,65535,65535,65535,0,65535,65535,65535,0,65535,65535,
    65535,0,65535,65535,0,65535,65535,65535,0,65535,65535,65535,
    0,65535,65535,65535,0,65535,65535,0,65535,65535,65535,0,
    65535,65535,65535,0,65535,65535,0,65535,65535,65535,0,65535,
    65535,65535,0,65535,65535,0,65535,65535,65535,0,65535,65535,
    65535,0,65535,65535,0,65535,65535,65535,0,65535,65535,65535,
    0,65535,65535,0,65535,65535,65535,0,65535,65535,65535,0,
    65535,65535,0,65535,65535,65535,0,65535,65535,65535,0,65535,
    65535,0,65535,65535,65535,0,65535,65535,65535,0,65535,65535,
    0,65535,65535,65535,0,65535,65535,0,65535,65535,65535,0,
    65535,65535,65535,0,65535,65535,0,65535,65535,65535,0,65535,
    65535,0,65535,65535,65535,0,65535,65535,65535,0,65535,65535,
    0,65535,65535,65535,0,65535,65535,0,65535,65535,65535,0,
    65535,65535,65535,0,65535,65535,0,65535,65535,65535,0,65535,
    65535,0,65535,65535,65535,0,65535,65535,0,65535,65535,65535,
    0,65535,65535,65535,0,65535,65535,0,65535,65535,65535,0,
    65535,65535,0,65535,65535,65535,0,65535,65535,0,65535,65535,
    65535,0,65535,65535,0,65535,65535,65535,0,65535,65535,0,
    65535,65535,65535,0,65535,65535,0,65535,65535,65535,0,65535,
    65535,0,65535,65535,65535,0,65535,65535,0,65535,65535,65535,
    0,65535,65535,0,65535,65535,65535,0,65535,65535,0,65535,
    65535,65535,0,65535,65535,0,65535,65535,65535,0,65535,65535,
    0,65535,65535,65535,0,65535,65535,0,65535,65535,65535,0,
    65535,65535,0,65535,65535,65535,0,65535,65535,0,65535,65535,
    65535,0,65535,65535,0,65535,65535,0,65535,65535,65535,0,
    65535,65535,0,65535,65535,65535,0,65535,65535,0,65535,65535,
    65535,0,65535,65535,0,65535,65535,65535,0,65535,65535,0,
    65535,65535,0,65535,65535,65535,0,65535,65535,0,65535,65535,
    65535,0,65535,65535,0,65535,65535,0,65535,65535,65535,0,
    65535,65535,0,65535,65535,65535,0,65535,65535,0,65535,65535,
    0,65535,65535,65535,0,65535,65535,0,65535,65535,65535,0,
    65535,65535,0,65535,65535,0,65535,65535,65535,0,65535,65535,
    0,65535,65535,0,65535,65535,65535,0,65535,65535,0,65535,
    65535,65535,0,65535,65535,0,65535,65535,0,65535,65535,65535,
    0,65535,65535,0,65535,65535,0,65535,65535,65535,0,65535,
    65535,0,65535,65535,0,65535,65535,65535,0,65535,65535,0,
    65535,65535,0,65535,65535,65535,0,65535,65535,0,65535,65535,
    0,65535,65535,65535,0,65535,65535,0,65535,65535,0,65535,
    65535,65535,0,65535,65535,0,65535,65535,0,65535,65535,0,
    65535,65535,65535,0,65535,65535,0,65535,65535,0,65535,65535,
    65535,0,65535,65535,0,65535,65535,0,65535,65535,65535,0,
    65535,65535,0,65535,65535,0,65535,65535,0,65535,65535,65535,
    0,65535,65535,0,65535,65535,0,65535,65535,0,65535,65535,
    65535,0,65535,65535,0,65535,65535,0,65535,65535,65535,0,
    65535,65535,0,65535,65535,0,65535,65535,0,65535,65535,65535,
    0,65535,65535,0,65535,65535,0,65535,65535,0,65535,65535,
    65535,0,65535,65535,0,65535,65535,0,65535,65535,0,65535,
    65535,0,65535,65535,65535,0,65535,65535,0,65535,65535,0,
    65535,65535,0,65535,65535,65535,0,65535,65535,0,65535,65535,
    0,65535,65535,0,65535,65535,0,65535,65535,65535,0,65535,
    65535,0,65535,65535,0,65535,65535,0,65535,65535,0,65535,
    65535,65535,0,65535,65535,0,65535,65535,0,65535,65535,0,
    65535,65535,0,65535,65535,65535,0,65535,65535,0,65535,65535,
    0,65535,65535,0,65535,65535,0,65535,65535,0,65535,65535,
    65535,0,65535,65535,0,65535,65535,0,65535,65535,0,65535,
    65535,0,65535,65535,0,65535,65535,65535,0,65535,65535,0,
    65535,65535,0,65535,65535,0,65535,65535,0,65535,65535,0,
    65535,65535,0,65535,65535,65535,0,65535,65535,0,65535,65535,
    0,65535,65535,0,65535,65535,0,65535,65535,0,65535,65535,
    0,65535,65535,65535,0,65535,65535,0,65535,65535,0,65535,
    65535,0,65535,65535,0,65535,65535,0,65535,65535,0,65535,
    65535,0,65535,65535,65535,0,65535,65535,0,65535,65535,0,
    65535,65535,0,65535,65535,0,65535,65535,0,65535,65535,0,
    65535,65535,0,65535,65535,0,65535,65535,0,65535,65535,65535,
    0,65535,65535,0,65535,65535,0,65535,65535,0,65535,65535,
    0,65535,65535,0,65535,65535,0,65535,65535,0,65535,65535,
    0,65535,65535,0,65535,65535,0,65535,65535,0,65535,65535,
    0,65535,65535,65535,0,65535,65535,0,65535,65535,0,65535,
    65535,0,65535,65535,0,65535,65535,0,65535,65535,0,65535,
    65535,0,65535,65535,0,65535,65535,0,65535,65535,0,65535,
    65535,0,65535,65535,0,65535,65535,0,65535,65535,0,65535,
    65535,0,65535,65535,0,65535,65535,0,65535,65535,0,65535,
    65535,0,65535,65535,0,65535,65535,0,65535,65535,0,65535,
    65535,0,65535,65535,65535,0,65535,65535,0,65535,65535,0,
    65535,65535,0,65535,65535,0,65535,65535,0,65535,65535,0,
    65535,65535,0,65535,65535,0,65535,65535,0,65535,65535,0,
    65535,65535,0,65535,65535,0,65535,65535,0,65535,65535,0,
    65535,65535,0,65535,65535,0,65535,65535,0,65535,65535,0,
    65535,65535,0,65535,65535,0,65535,65535,0,65535,0,65535,
    65535,0,65535,65535,0,65535,65535,0,65535,65535,0,65535,
    65535,0,65535,65535,0,65535,65535,0,65535,65535,0,65535,
    65535,0,65535,65535,0,65535,65535,0,65535,65535,0,65535,
    65535,0,65535,65535,0,65535,65535,0,65535,65535,0,65535,
    65535,0,65535,65535,0,65535,65535,0,65535,65535,0,65535,
    65535,0,65535,65535,0,65535,65535,0,65535,65535,0,65535,
    0,65535,65535,0,65535,65535,0,65535,65535,0,65535,65535,
    0,65535,65535,0,65535,65535,0,65535,65535,0,65535,65535,
    0,65535,65535,0,65535,65535,0,65535,65535,0,65535,65535,
    0,65535,65535,0,65535,0,65535,65535,0,65535,65535,0,
    65535,65535,0,65535,65535,0,65535,65535,0,65535,65535,0,
    65535,65535,0,65535,65535,0,65535,65535,0,65535,65535,0,
    65535,0,65535,65535,0,65535,65535,0,65535,65535,0,65535,
    65535,0,65535,65535,0,65535,65535,0,65535,65535,0,65535,
    65535,0,65535,0,65535,65535,0,65535,65535,0,65535,65535,
    0,65535,65535,0,65535,65535,0,65535,65535,0,65535,65535,
    0,65535,0,65535,65535,0,65535,65535,0,65535,65535,0,
    65535,65535,0,65535,65535,0,65535,65535,0,65535,65535,0,
    65535,0,65535,65535,0,65535,65535,0,65535,65535,0,65535,
    65535,0,65535,65535,0,65535,65535,0,65535,0,65535,65535,
    0,65535,65535,0,65535,65535,0,65535,65535,0,65535,65535,
    0,65535,0,65535,65535,0,65535,65535,0,65535,65535,0,
    65535,65535,0,65535,65535,0,65535,0,65535,65535,0,65535,
    65535,0,65535,65535,0,65535,65535,0,65535,65535,0,65535,
    0,65535,65535,0,65535,65535,0,65535,65535,0,65535,65535,
    0,65535,65535,0,65535,0,65535,65535,0,65535,65535,0,
    65535,65535,0,65535,65535,0,65535,0,65535,65535,0,65535,
    65535,0,65535,65535,0,65535,65535,0,65535,0,65535,65535,
    0,65535,65535,0,65535,65535,0,65535,65535,0,65535,0,
    65535,65535,0,65535,65535,0,65535,65535,0,65535,65535,0,
    65535,0,65535,65535,0,65535,65535,0,65535,65535,0,65535,
    65535,0,65535,0,65535,65535,0,65535,65535,0,65535,65535,
    0,65535,0,65535,65535,0,65535,65535,0,65535,65535,0,
    65535,65535,0,65535,0,65535,65535,0,65535,65535,0,65535,
    65535,0,65535,0,65535,65535,0,65535,65535,0,65535,65535,
    0,65535,0,65535,65535,0,65535,65535,0,65535,65535,0,
    65535,65535,0,65535,0,65535,65535,0,65535,65535,0,65535,
    65535,0,65535,0,65535,65535,0,65535,65535,0,65535,65535,
    0,65535,0,65535,65535,0,65535,65535,0,65535,0,65535,
    65535,0,65535,65535,0,65535,65535,0,65535,0,65535,65535,
    0,65535,65535,0,65535,65535,0,65535,0,65535,65535,0,
    65535,65535,0,65535,65535,0,65535,0,65535,65535,0,65535,
    65535,0,65535,0,65535,65535,0,65535,65535,0,65535,65535,
    0,65535,0,65535,65535,0,65535,65535,0,65535,0,65535,
    65535,0,65535,65535,0,65535,65535,0,65535,0,65535,65535,
    0,65535,65535,0,65535,0,65535,65535,0,65535,65535,0,
    65535,65535,0,65535,0,65535,65535,0,65535,65535,0,65535,
    0,65535,65535,0,65535,65535,0,65535,0,65535,65535,0,
    65535,65535,0,65535,65535,0,65535,0,65535,65535,0,65535,
    65535,0,65535,0,65535,65535,0,65535,65535,0,65535,0,
    65535,65535,0,65535,65535,0,65535,0,65535,65535,0,65535,
    65535,0,65535,0,65535,65535,0,65535,65535,0,65535,0,
    65535,65535,0,65535,65535,0,65535,0,65535,65535,0,65535,
    65535,0,65535,0,65535,65535,0,65535,65535,0,65535,0,
    65535,65535,0,65535,65535,0,65535,0,65535,65535,0,65535,
    65535,0,65535,0,65535,65535,0,65535,65535,0,65535,0,
    65535,65535,0,65535,65535,0,65535,0,65535,65535,0,65535,
    65535,0,65535,0,65535,65535,0,65535,65535,0,65535,0,
    65535,65535,0,65535,65535,0,65535,0,65535,65535,0,65535,
    0,65535,65535,0,65535,65535,0,65535,0,65535,65535,0,
    65535,65535,0,65535,0,65535,65535,0,65535,65535,0,65535,
    0,65535,65535,0,65535,0,65535,65535,0,65535,65535,0,
    65535,0,65535,65535,0,65535,65535,0,65535,0,65535,65535,
    0,65535,0,65535,65535,0,65535,65535,0,65535,0,65535,
    65535,0,65535,0,65535,65535,0,65535,65535,0,65535,0,
    65535,65535,0,65535,65535,0,65535,0,65535,65535,0,65535,
    0,65535,65535,0,65535,65535,0,65535,0,65535,65535,0,
    65535,0,65535,65535,0,65535,65535,0,65535,0,65535,65535,
    0,65535,0,65535,65535,0,65535,65535,0,65535,0,65535,
    65535,0,65535,0,65535,65535,0,65535,65535,0,65535,0,
    65535,65535,0,65535,0,65535,65535,0,65535,0,65535,65535,
    0,65535,65535,0,65535,0,65535,65535,0,65535,0,65535,
    65535,0,65535,65535,0,65535,0,65535,65535,0,65535,0,
    65535,65535,0,65535,0,65535,65535,0,65535,65535,0,65535,
    0,65535,65535,0,65535,0,65535,65535,0,65535,0,65535,
    65535,0,65535,65535,0,65535,0,65535,65535,0,65535,0,
    65535,65535,0,65535,0,65535,65535,0,65535,0,65535,65535,
    0,65535,65535,0,65535,0,65535,65535,0,65535,0,65535,
    65535,0,65535,0,65535,65535,0,65535,0,65535,65535,0,
    65535,65535,0,65535,0,65535,65535,0,65535,0,65535,65535,
    0,65535,0,65535,65535,0,65535,0,65535,65535,0,65535,
    0,65535,65535,0,65535,65535,0,65535,0,65535,65535,0,
    65535,0,65535,65535,0,65535,0,65535,65535,0,65535,0,
    65535,65535,0,65535,0,65535,65535,0,65535,0,65535,65535,
    0,65535,0,65535,65535,0,65535,0,65535,65535,0,65535,
    65535,0,65535,0,65535,65535,0,65535,0,65535,65535,0,
    65535,0,65535,65535,0,65535,0,65535,65535,0,65535,0,
    65535,65535,0,65535,0,65535,65535,0,65535,0,65535,65535,
    0,65535,0,65535,65535,0,65535,0,65535,65535,0,65535,
    0,65535,65535,0,65535,0,65535,65535,0,65535,0,65535,
    65535,0,65535,0,65535,65535,0,65535,0,65535,65535,0,
    65535,0,65535,65535,0,65535,0,65535,65535,0,65535,0,
    65535,65535,0,65535,0,65535,65535,0,65535,0,65535,65535,
    0,65535,0,65535,65535,0,65535,0,65535,65535,0,65535,
    0,65535,65535,0,65535,0,65535,65535,0,65535,0,65535,
    65535,0,65535,0,65535,65535,0,65535,0,65535,65535,0,
    65535,0,65535,65535,0,65535,0,65535,65535,0,65535,0,
    65535,0,65535,65535,0,65535,0,65535,65535,0,65535,0,
    65535,65535,0,65535,0,65535,65535,0,65535,0,65535,65535,
    0,65535,0,65535,65535,0,65535,0,65535,65535,0,65535,
    0,65535,65535,0,65535,0,65535,0,65535,65535,0,65535,
    0,65535,65535,0,65535,0,65535,65535,0,65535,0,65535,
    65535,0,65535,0,65535,65535,0,65535,0,65535,0,65535,
    65535,0,65535,0,65535,65535,0,65535,0,65535,65535,0,
    65535,0,65535,65535,0,65535,0,65535,65535,0,65535,0,
    65535,0,65535,65535,0,65535,0,65535,65535,0,65535,0,
    65535,65535,0,65535,0,65535,65535,0,65535,0,65535,0,
    65535,65535,0,65535,0,65535,65535,0,65535,0,65535,65535,
    0,65535,0,65535,0,65535,65535,0,65535,0,65535,65535,
    0,65535,0,65535,65535,0,65535,0,65535,0,65535,65535,
    0,65535,0,65535,65535,0,65535,0,65535,65535,0,65535,
    0,65535,0,65535,65535,0,65535,0,65535,65535,0,65535,
    0,65535,65535,0,65535,0,65535,0,65535,65535,0,65535,
    0,65535,65535,0,65535,0,65535,0,65535,65535,0,65535,
    0,65535,65535,0,65535,0,65535,0,65535,65535,0,65535,
    0,65535,65535,0,65535,0,65535,65535,0,65535,0,65535,
    0,65535,65535,0,65535,0,65535,65535,0,65535,0,65535,
    0,65535,65535,0,65535,0,65535,65535,0,65535,0,65535,
    0,65535,65535,0,65535,0,65535,65535,0,65535,0,65535,
    0,65535,65535,0,65535,0,65535,65535,0,65535,0,65535,
    0,65535,65535,0,65535,0,65535,0,65535,65535,0,65535,
    0,65535,65535,0,65535,0,65535,0,65535,65535,0,65535,
    0,65535,65535,0,65535,0,65535,0,65535,65535,0,65535,
    0,65535,65535,0,65535,0,65535,0,65535,65535,0,65535,
    0,65535,0,65535,65535,0,65535,0,65535,65535,0,65535,
    0,65535,0,65535,65535,0,65535,0,65535,0,65535,65535,
    0,65535,0,65535,65535,0,65535,0,65535,0,65535,65535,
    0,65535,0,65535,0,65535,65535,0,65535,0,65535,0,
    65535,65535,0,65535,0,65535,65535,0,65535,0,65535,0,
    65535,65535,0,65535,0,65535,0,65535,65535,0,65535,0,
    65535,0,65535,65535,0,65535,0,65535,0,65535,65535,0,
    65535,0,65535,65535,0,65535,0,65535,0,65535,65535,0,
    65535,0,65535,0,65535,65535,0,65535,0,65535,0,65535,
    65535,0,65535,0,65535,0,65535,65535,0,65535,0,65535,
    0,65535,65535,0,65535,0,65535,0,65535,65535,0,65535,
    0,65535,65535,0,65535,0,65535,0,65535,65535,0,65535,
    0,65535,0,65535,65535,0,65535,0,65535,0,65535,65535,
    0,65535,0,65535,0,65535,65535,0,65535,0,65535,0,
    65535,65535,0,65535,0,65535,0,65535,65535,0,65535,0,
    65535,0,65535,65535,0,65535,0,65535,0,65535,65535,0,
    65535,0,65535,0,65535,0,65535,65535,0,65535,0,65535,
    0,65535,65535,0,65535,0,65535,0,65535,65535,0,65535,
    0,65535,0,65535,65535,0,65535,0,65535,0,65535,65535,
    0,65535,0,65535,0,65535,65535,0,65535,0,65535,0,
    65535,65535,0,65535,0,65535,0,65535,0,65535,65535,0,
    65535,0,65535,0,65535,65535,0,65535,0,65535,0,65535,
    65535,0,65535,0,65535,0,65535,65535,0,65535,0,65535,
    0,65535,0,65535,65535,0,65535,0,65535,0,65535,65535,
    0,65535,0,65535,0,65535,65535,0,65535,0,65535,0,
    65535,0,65535,65535,0,65535,0,65535,0,65535,65535,0,
    65535,0,65535,0,65535,65535,0,65535,0,65535,0,65535,
    0,65535,65535,0,65535,0,65535,0,65535,65535,0,65535,
    0,65535,0,65535,0,65535,65535,0,65535,0,65535,0,
    65535,65535,0,65535,0,65535,0,65535,0,65535,65535,0,
    65535,0,65535,0,65535,65535,0,65535,0,65535,0,65535,
    0,65535,65535,0,65535,0,65535,0,65535,65535,0,65535,
    0,65535,0,65535,0,65535,65535,0,65535,0,65535,0,
    65535,65535,0,65535,0,65535,0,65535,0,65535,65535,0,
    65535,0,65535,0,65535,0,65535,65535,0,65535,0,65535,
    0,65535,65535,0,65535,0,65535,0,65535,0,65535,65535,
    0,65535,0,65535,0,65535,0,65535,65535,0,65535,0,
    65535,0,65535,0,65535,65535,0,65535,0,65535,0,65535,
    0,65535,65535,0,65535,0,65535,0,65535,65535,0,65535,
    0,65535,0,65535,0,65535,65535,0,65535,0,65535,0,
    65535,0,65535,65535,0,65535,0,65535,0,65535,0,65535,
    65535,0,65535,0,65535,0,65535,0,65535,65535,0,65535,
    0,65535,0,65535,0,65535,65535,0,65535,0,65535,0,
    65535,0,65535,65535,0,65535,0,65535,0,65535,0,65535,
    65535,0,65535,0,65535,0,65535,0,65535,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,65535,0,65535,0,
    65535,0,65535,0,65535,65535,0,65535,0,65535,0,65535,
    0,65535,65535,0,65535,0,65535,0,65535,0,65535,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,65535,0,
    65535,0,65535,0,65535,0,65535,65535,0,65535,0,65535,
    0,65535,0,65535,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,65535,0,65535,0,65535,0,65535,0,65535,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,65535,
    0,65535,0,65535,0,65535,0,65535,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,65535,0,65535,0,65535,0,
    65535,0,65535,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    0,65535,0,65535,0,65535,0,65535,0,65535,0,0,
    65535,0,65535,0,65535,0,65535,0,65535,0,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,0,65535,0,65535,
    0,65535,0,65535,0,65535,0,0,65535,0,65535,0,
    65535,0,65535,0,65535,0,0,65535,0,65535,0,65535,
    0,65535,0,0,65535,0,65535,0,65535,0,65535,0,
    65535,0,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,0,65535,0,65535,0,65535,0,65535,0,0,65535,
    0,65535,0,65535,0,65535,0,65535,0,0,65535,0,
    65535,0,65535,0,65535,0,65535,0,0,65535,0,65535,
    0,65535,0,65535,0,0,65535,0,65535,0,65535,0,
    65535,0,65535,0,0,65535,0,65535,0,65535,0,65535,
    0,0,65535,0,65535,0,65535,0,65535,0,65535,0,
    0,65535,0,65535,0,65535,0,65535,0,0,65535,0,
    65535,0,65535,0,65535,0,0,65535,0,65535,0,65535,
    0,65535,0,65535,0,0,65535,0,65535,0,65535,0,
    65535,0,0,65535,0,65535,0,65535,0,65535,0,65535,
    0,0,65535,0,65535,0,65535,0,65535,0,0,65535,
    0,65535,0,65535,0,65535,0,0,65535,0,65535,0,
    65535,0,65535,0,0,65535,0,65535,0,65535,0,65535,
    0,65535,0,0,65535,0,65535,0,65535,0,65535,0,
    0,65535,0,65535,0,65535,0,65535,0,0,65535,0,
    65535,0,65535,0,65535,0,0,65535,0,65535,0,65535,
    0,65535,0,0,65535,0,65535,0,65535,0,65535,0,
    0,65535,0,65535,0,65535,0,65535,0,0,65535,0,
    65535,0,65535,0,65535,0,0,65535,0,65535,0,65535,
    0,65535,0,0,65535,0,65535,0,65535,0,65535,0,
    0,65535,0,65535,0,65535,0,65535,0,0,65535,0,
    65535,0,65535,0,65535,0,0,65535,0,65535,0,65535,
    0,65535,0,0,65535,0,65535,0,65535,0,65535,0,
    0,65535,0,65535,0,65535,0,65535,0,0,65535,0,
    65535,0,65535,0,65535,0,0,65535,0,65535,0,65535,
    0,65535,0,0,65535,0,65535,0,65535,0,65535,0,
    0,65535,0,65535,0,65535,0,65535,0,0,65535,0,
    65535,0,65535,0,0,65535,0,65535,0,65535,0,65535,
    0,0,65535,0,65535,0,65535,0,65535,0,0,65535,
    0,65535,0,65535,0,65535,0,0,65535,0,65535,0,
    65535,0,0,65535,0,65535,0,65535,0,65535,0,0,
    65535,0,65535,0,65535,0,65535,0,0,65535,0,65535,
    0,65535,0,65535,0,0,65535,0,65535,0,65535,0,
    0,65535,0,65535,0,65535,0,65535,0,0,65535,0,
    65535,0,65535,0,65535,0,0,65535,0,65535,0,65535,
    0,0,65535,0,65535,0,65535,0,65535,0,0,65535,
    0,65535,0,65535,0,0,65535,0,65535,0,65535,0,
    65535,0,0,65535,0,65535,0,65535,0,0,65535,0,
    65535,0,65535,0,65535,0,0,65535,0,65535,0,65535,
    0,65535,0,0,65535,0,65535,0,65535,0,0,65535,
    0,65535,0,65535,0,65535,0,0,65535,0,65535,0,
    65535,0,0,65535,0,65535,0,65535,0,65535,0,0,
    65535,0,65535,0,65535,0,0,65535,0,65535,0,65535,
    0,0,65535,0,65535,0,65535,0,65535,0,0,65535,
    0,65535,0,65535,0,0,65535,0,65535,0,65535,0,
    65535,0,0,65535,0,65535,0,65535,0,0,65535,0,
    65535,0,65535,0,0,65535,0,65535,0,65535,0,65535,
    0,0,65535,0,65535,0,65535,0,0,65535,0,65535,
    0,65535,0,0,65535,0,65535,0,65535,0,65535,0,
    0,65535,0,65535,0,65535,0,0,65535,0,65535,0,
    65535,0,0,65535,0,65535,0,65535,0,65535,0,0,
    65535,0,65535,0,65535,0,0,65535,0,65535,0,65535,
    0,0,65535,0,65535,0,65535,0,65535,0,0,65535,
    0,65535,0,65535,0,0,65535,0,65535,0,65535,0,
    0,65535,0,65535,0,65535,0,0,65535,0,65535,0,
    65535,0,0,65535,0,65535,0,65535,0,65535,0,0,
    65535,0,65535,0,65535,0,0,65535,0,65535,0,65535,
    0,0,65535,0,65535,0,65535,0,0,65535,0,65535,
    0,65535,0,0,65535,0,65535,0,65535,0,0,65535,
    0,65535,0,65535,0,65535,0,0,65535,0,65535,0,
    65535,0,0,65535,0,65535,0,65535,0,0,65535,0,
    65535,0,65535,0,0,65535,0,65535,0,65535,0,0,
    65535,0,65535,0,65535,0,0,65535,0,65535,0,65535,
    0,0,65535,0,65535,0,65535,0,0,65535,0,65535,
    0,65535,0,0,65535,0,65535,0,65535,0,0,65535,
    0,65535,0,65535,0,0,65535,0,65535,0,65535,0,
    0,65535,0,65535,0,65535,0,0,65535,0,65535,0,
    65535,0,0,65535,0,65535,0,65535,0,0,65535,0,
    65535,0,65535,0,0,65535,0,65535,0,65535,0,0,
    65535,0,65535,0,65535,0,0,65535,0,65535,0,65535,
    0,0,65535,0,65535,0,65535,0,0,65535,0,65535,
    0,65535,0,0,65535,0,65535,0,65535,0,0,65535,
    0,65535,0,65535,0,0,65535,0,65535,0,65535,0,
    0,65535,0,65535,0,65535,0,0,65535,0,65535,0,
    65535,0,0,65535,0,65535,0,0,65535,0,65535,0,
    65535,0,0,65535,0,65535,0,65535,0,0,65535,0,
    65535,0,65535,0,0,65535,0,65535,0,65535,0,0,
    65535,0,65535,0,65535,0,0,65535,0,65535,0,65535,
    0,0,65535,0,65535,0,0,65535,0,65535,0,65535,
    0,0,65535,0,65535,0,65535,0,0,65535,0,65535,
    0,65535,0,0,65535,0,65535,0,65535,0,0,65535,
    0,65535,0,0,65535,0,65535,0,65535,0,0,65535,
    0,65535,0,65535,0,0,65535,0,65535,0,65535,0,
    0,65535,0,65535,0,65535,0,0,65535,0,65535,0,
    0,65535,0,65535,0,65535,0,0,65535,0,65535,0,
    65535,0,0,65535,0,65535,0,65535,0,0,65535,0,
    65535,0,0,65535,0,65535,0,65535,0,0,65535,0,
    65535,0,65535,0,0,65535,0,65535,0,0,65535,0,
    65535,0,65535,0,0,65535,0,65535,0,65535,0,0,
    65535,0,65535,0,0,65535,0,65535,0,65535,0,0,
    65535,0,65535,0,65535,0,0,65535,0,65535,0,0,
    65535,0,65535,0,65535,0,0,65535,0,65535,0,65535,
    0,0,65535,0,65535,0,0,65535,0,65535,0,65535,
    0,0,65535,0,65535,0,65535,0,0,65535,0,65535,
    0,0,65535,0,65535,0,65535,0,0,65535,0,65535,
    0,65535,0,0,65535,0,65535,0,0,65535,0,65535,
    0,65535,0,0,65535,0,65535,0,0,65535,0,65535,
    0,65535,0,0,65535,0,65535,0,0,65535,0,65535,
    0,65535,0,0,65535,0,65535,0,65535,0,0,65535,
    0,65535,0,0,65535,0,65535,0,65535,0,0,65535,
    0,65535,0,0,65535,0,65535,0,65535,0,0,65535,
    0,65535,0,0,65535,0,65535,0,65535,0,0,65535,
    0,65535,0,0,65535,0,65535,0,65535,0,0,65535,
    0,65535,0,0,65535,0,65535,0,65535,0,0,65535,
    0,65535,0,0,65535,0,65535,0,65535,0,0,65535,
    0,65535,0,0,65535,0,65535,0,65535,0,0,65535,
    0,65535,0,0,65535,0,65535,0,65535,0,0,65535,
    0,65535,0,0,65535,0,65535,0,65535,0,0,65535,
    0,65535,0,0,65535,0,65535,0,65535,0,0,65535,
    0,65535,0,0,65535,0,65535,0,65535,0,0,65535,
    0,65535,0,0,65535,0,65535,0,65535,0,0,65535,
    0,65535,0,0,65535,0,65535,0,0,65535,0,65535,
    0,65535,0,0,65535,0,65535,0,0,65535,0,65535,
    0,65535,0,0,65535,0,65535,0,0,65535,0,65535,
    0,0,65535,0,65535,0,65535,0,0,65535,0,65535,
    0,0,65535,0,65535,0,65535,0,0,65535,0,65535,
    0,0,65535,0,65535,0,0,65535,0,65535,0,65535,
    0,0,65535,0,65535,0,0,65535,0,65535,0,0,
    65535,0,65535,0,65535,0,0,65535,0,65535,0,0,
    65535,0,65535,0
};

static const unsigned short d4_sdicoff[100] = {
    0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,32,36,40,44,
    48,52,56,60,64,68,72,76,80,84,
    88,92,96,100,104,108,112,116,120,124,
    128,132,136,140,144,148,152,156,160,164,
    168,172,176,180,184,188,192,196,200,204,
    208,212,216,220,224,228,232,236,240,244,
    248,252,256,260,264,268,272,276,280,284,
    288,292,296,300,304,308,312,316,320,324,
    328,332,336,340,344,348,352,356,356,360
};
static const unsigned char d4_sdiclen[100] = {
    32,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,4,3,3,3,
    3,3,3,3,3,3,3,3,3,3,3,3,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1,2,3,0
};


/* ---------------- accessors ---------------- */
/* The arena stands in for the original 32-bit DGROUP + heap.  A wrong model
 * state (corrupt / unsupported stream) can produce an out-of-range index, so
 * clamp every address into the arena's last 8 bytes instead of trapping. */
static unsigned int d4_o(unsigned int o)
{
    if (o > (unsigned)(D4_ARENA - 8)) o = (unsigned)(D4_ARENA - 8);
    return o;
}
#define D4O(o)  d4_o((unsigned)(o))
#define G8(a)   (*(unsigned char  *)(void *)(c->mem + D4O(a)))
#define G16(a)  (*(unsigned short *)(void *)(c->mem + D4O(a)))
#define G32(a)  (*(unsigned int   *)(void *)(c->mem + D4O(a)))
#define M8(o)   (*(unsigned char  *)(void *)(c->mem + D4O(o)))
#define M16(o)  (*(unsigned short *)(void *)(c->mem + D4O(o)))
#define M32(o)  (*(unsigned int   *)(void *)(c->mem + D4O(o)))
#define L8(p,i)   (*(unsigned char  *)(void *)((p) + (i)))
#define L16(p,i)  (*(unsigned short *)(void *)((p) + (i)))
#define L32(p,i)  (*(unsigned int   *)(void *)((p) + (i)))
#define G8P(a)  ((unsigned char  *)(void *)(c->mem + (unsigned)(a)))
#define G16P(a) ((unsigned short *)(void *)(c->mem + (unsigned)(a)))
#define G32P(a) ((unsigned int   *)(void *)(c->mem + (unsigned)(a)))

#include <stdio.h>
#include <stdarg.h>
void D4TRACE(const char *format, ...) {
#ifdef DEBUG
    va_list ap;
    va_start(ap, format);
    vfprintf(stderr, format, ap);
    va_end(ap);
#else
    (void)format;
#endif
}

/* ---------------- hand written primitives ---------------- */

/* Division guard: a zero divisor means the model state is inconsistent
 * (corrupt or unsupported stream).  Return 0 instead of trapping. */
static unsigned long d4_div(unsigned long a, unsigned long b)
{
    if (!b) return 0;
    return a / b;
}

static int d4_get_byte(dec4_ctx *c) /* input byte (orig d4_40B093) */
{
    int n;
    if (c->in_pos < c->in_avail) return (int)c->inbuf[c->in_pos++];
    if (c->in_eof) { G32(D4A_IO_ERR) = 3; return 0; }
    n = c->rcb(c->rctx, c->inbuf, (unsigned)sizeof(c->inbuf));
    if (n <= 0) { c->in_eof = 1; G32(D4A_IO_ERR) = 3; return 0; }
    c->in_avail = (unsigned)n;
    c->in_pos = 1;
    G32(D4A_IN_CHUNKS) += 1;
    return (int)c->inbuf[0];
}

static int d4_put_byte(dec4_ctx *c, char a1)
{
    c->outbuf[G32(D4A_OUTBUF_LEN)] = (unsigned char)a1;
    G32(D4A_OUTBUF_LEN) += 1;
    G32(D4A_OUT_POS) += 1;
#ifdef D4EMITDBG
    if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
      fprintf(stderr, "EMIT out=%u byte=%u\n",
          G32(D4A_OUT_POS), (unsigned)(unsigned char)a1);
#endif
#ifdef CRASHTICKDBG
    if ((G32(D4A_OUT_POS) & 0x3F) == 0)
        fprintf(stderr, "TICK out=%u\n", G32(D4A_OUT_POS));
#endif
#ifdef FLUSHDBG
    if (G32(D4A_OUTBUF_LEN) >= 0x1000U) {
#else
    if (G32(D4A_OUTBUF_LEN) >= 0x10000U) {
#endif
        unsigned n = G32(D4A_OUTBUF_LEN);
        if (c->wcb(c->wctx, c->outbuf, n) != 0) G32(D4A_IO_ERR) = 4;
        G32(D4A_OUTBUF_LEN) = 0;
    }
    return 0;
}

static int d4_progress_tick(dec4_ctx *c) { (void)c; return 0; }

static void *d4_stub_free(dec4_ctx *c, void *p) { (void)c; (void)p; return 0; }

static unsigned char *d4_stub_alloc(dec4_ctx *c, unsigned n) { (void)c; (void)n; return 0; }

static unsigned int d4_get_file_table(dec4_ctx *c) { return c->file_table; }

static void d4_fill8(dec4_ctx *c, unsigned int dst, int val, unsigned count)
{
    memset(c->mem + dst, (unsigned char)val, count);
}

static void d4_fill16(dec4_ctx *c, int pat, unsigned int dst, unsigned words)
{
    unsigned i;
    unsigned char *p;
    p = c->mem + dst;
    for (i = 0; i < words; i++) {
        p[2*i] = (unsigned char)(pat & 0xFF);
        p[2*i+1] = (unsigned char)((pat >> 8) & 0xFF);
    }
}

static void d4_rc_init(dec4_ctx *c) /* range init (orig d4_419A4D) */
{
    unsigned i;
    G32(D4A_RC_CODE) = 0;
    G32(D4A_RC_RANGE) = 0xFFFFFFFFU;
    for (i = 0; i < 5; i++)
        G32(D4A_RC_CODE) = (G32(D4A_RC_CODE) << 8) | (unsigned)d4_get_byte(c);
}


/* Range renormalize: while *prange and *prange < 0x1000000, shift in one
 * input byte. Callers keep code/range in locals and store back to
 * D4A_RC_CODE/D4A_RC_RANGE after. (17 sites, incl. the 8136 variant
 * which spells the shift without (unsigned char) casts -- identical
 * for byte values 0..255.) */
static void d4_rcrenorm(dec4_ctx *c, unsigned int *pcode, unsigned int *prange)
{
    while (*prange && *prange < 0x1000000U) {
        *pcode = (*pcode << 8) | (unsigned char)d4_get_byte(c);
        *prange = (*prange << 8) & 0xFFFFFFFFU;
    }
}

static void d4_cost_fold(dec4_ctx *c) /* fold cost LUT once (orig d4_419AC0) */
{
    unsigned i;
    if (!G8(D4A_COST_DIRTY)) return;
    for (i = 1; i < 0x4000U; i++) {
        unsigned v = (unsigned)G16(D4A_COST_LUT + 2*i) + (unsigned)G16(D4A_COST_LUT_SRC + 2*i);
        G16(D4A_COST_LUT + 2*i) = (unsigned short)(v & 0xFFFFU);
    }
    G8(D4A_COST_DIRTY) = 0;
}


static const unsigned char d4_t40CDA1[13] = {1,1,1,1,1,1,1,1,1,1,1,1,0};
static const unsigned char d4_t40CDAD[10] = {0,1,1,1,1,1,1,1,1,0};

/* ---------------- table image loader ---------------- */

#define CP(dst, src, n) memcpy(c->mem + (dst), (src), (n))

static void d4_load_tables(dec4_ctx *c)
{
    unsigned i;
    CP(D4A_PPM_PATTERN, d4_t422BA0, sizeof d4_t422BA0);
    CP(D4A_CLS_TREE_SRC, d4_t42372C, sizeof d4_t42372C);
    CP(D4A_CLS_CHILD, d4_t4239BB, sizeof d4_t4239BB);
    for (i = 0; i < 5; i++) G32(D4A_CNT_OFF_5 + 4*i) = d4_t4236D4[i];
    for (i = 0; i < 17; i++) G32(D4A_CNT_OFF_17 + 4*i) = d4_t4236E8[i];
    for (i = 0; i < 8; i++) G32(D4A_CNT_OFF_8 + 4*i) = d4_t4239D0[i];
    for (i = 0; i < 256; i++) G32(D4A_BUCKET_LUT + 4*i) = d4_t4239F0[i];
    for (i = 0; i < 288; i++) G32(D4A_BUCKET_LUT2 + 4*i) = d4_t423DF0[i];
    CP(D4A_LEN_LUT_A, d4_t424270, sizeof d4_t424270);
    CP(D4A_LEN_LUT_B, d4_t424280, sizeof d4_t424280);
    CP(D4A_LEN_LUT_C, d4_t424290, sizeof d4_t424290);
    CP(D4A_FREQ_THRESH1, d4_t4244A0, sizeof d4_t4244A0);
    CP(D4A_FREQ_THRESH2, d4_t4245A0, sizeof d4_t4245A0);
    for (i = 0; i < 10; i++) G32(D4A_TRI2_RANGE + 4*i) = d4_t4246A0[i];
    for (i = 0; i < 20; i++) G32(D4A_TRI2_TOTAL + 4*i) = d4_t4246C8[i];
    for (i = 0; i < 6; i++) G32(D4A_MASK_LUT + 4*i) = d4_t424D3C[i];
    for (i = 0; i < 512; i++) G32(D4A_SEL_HIST + 4*i) = d4_t424D54[i];
    for (i = 0; i < 256; i++) G32(D4A_BYTE_CURVE_0 + 4*i) = d4_t425554[i];
    for (i = 0; i < 256; i++) G32(D4A_BYTE_CURVE_4 + 4*i) = d4_t426554[i];
    for (i = 0; i < 256; i++) G32(D4A_BYTE_CURVE_1 + 4*i) = d4_t425954[i];
    for (i = 0; i < 256; i++) G32(D4A_BYTE_CURVE_2 + 4*i) = d4_t425D54[i];
    for (i = 0; i < 256; i++) G32(D4A_BYTE_CURVE_3 + 4*i) = d4_t426154[i];
    CP(D4A_TRI2_IDX, d4_t424718, sizeof d4_t424718);
    for (i = 0; i < 256; i++) G32(D4A_SEL_LUT + 4*i) = d4_t42472C[i];
    CP(D4A_DELTA_LUT, d4_t424B2C, sizeof d4_t424B2C);
    CP(D4A_DELTA_LUT2, d4_t424B3C, sizeof d4_t424B3C);
    CP(D4A_DELTA_LUT3, d4_t424C3C, sizeof d4_t424C3C);
    CP(D4A_SDI_LEN, d4_t426C53, sizeof d4_t426C53);
    CP(D4A_SDI_TAB, d4_t426957, sizeof d4_t426957);
    CP(D4A_DECODE_MAP, d4_t42711F, sizeof d4_t42711F);
    CP(D4A_PENDING_TAB, d4_t427179, sizeof d4_t427179);
    CP(D4A_HASH_ENABLE, d4_t42738D, sizeof d4_t42738D);
    for (i = 0; i < 16; i++) G32(D4A_TBL_42748D + 4*i) = d4_t42748D[i];
    for (i = 0; i < 16; i++) G32(D4A_TBL_4274CD + 4*i) = d4_t4274CD[i];
    for (i = 0; i < 16; i++) G32(D4A_TBL_42750D + 4*i) = d4_t42750D[i];
    for (i = 0; i < 16; i++) G32(D4A_TBL_42754D + 4*i) = d4_t42754D[i];
    for (i = 0; i < 16; i++) G32(D4A_TBL_42758D + 4*i) = d4_t42758D[i];
    for (i = 0; i < 16; i++) G32(D4A_TBL_4275CD + 4*i) = d4_t4275CD[i];
    for (i = 0; i < 8; i++) G32(D4A_TBL_42760D + 4*i) = d4_t42760D[i];
    for (i = 0; i < 8; i++) G32(D4A_TBL_42762D + 4*i) = d4_t42762D[i];
    for (i = 0; i < 33; i++) G32(D4A_TBL_42764D + 4*i) = d4_t42764D[i];
    for (i = 0; i < 16384U; i++) G16(D4A_COST_LUT + 2*i) = d4_t429ED4[i];
    for (i = 0; i < 100U; i++) G32(D4A_SDI_PTR + 4*i) = D4A_SDI_TAB + d4_sdicoff[i];
    CP(D4A_MASK13_SRC, d4_t40CDA1, sizeof d4_t40CDA1);
    CP(D4A_MASK9_SRC, d4_t40CDAD, sizeof d4_t40CDAD);
    G8(D4A_COST_DIRTY) = 1;
    d4_cost_fold(c);
    d4_cost_fold(c);   /* second call no-ops (flag cleared); keeps parity */
#ifdef D4TABDBG
    for (i = 0; i < 16384U; i++) {
      unsigned a = (unsigned)G16(D4A_COST_LUT + 2*i);
      if (a >= 6000 && a <= 6040)
        fprintf(stderr, "TAB429 idx=%u val=%u\n", i, a);
    }
#endif
}

/* ---------------- init (sub_40CDD1) ---------------- */

static int d4_decoder_init(dec4_ctx *c, int a1, int a2)
{
    unsigned dict, i;
    if (!(a2 >= 24 && a2 <= 39)) return 1;
    G8(D4A_INIT_FLAG) = 1;
    G8(D4A_FLAG_45E387) = (unsigned char)a1;
    dict = 1U << (((a2 - 24) % 16) + 10);
    G32(D4A_DICT_SIZE) = dict;
    G32(D4A_DICT_POS) = 0;
    G32(D4A_DICT_MASK) = dict - 1;
    G32(D4A_TBL_45D140) = 0;
    c->dict_buf = A_DICT_BUF;
    c->col_tab = A_COL_TAB;
    c->row_tab = A_ROW_TAB;
    c->hash1 = A_HASH1;
    c->hash2 = A_HASH2;
    c->hash_head = A_HASH_HEAD;
    c->hash_byte = A_HASH_BYTE;
    c->byte_hist = A_BYTE_HIST;
    c->fenwick1 = A_FENWICK1;
    c->fenwick2 = A_FENWICK2;
    c->ppm_unused = A_PPM_UNUSED;
    c->ppm_tot = A_PPM_TOT;
    c->ppm_spare1 = A_PPM_SPARE1;
    c->ppm_spare2 = A_PPM_SPARE2;
    c->pred_cache = A_PRED_CACHE;
    M32(D4A_TBL_42369C)   = c->row_tab + 1310720;
    M32(D4A_TBL_4236A0)   = c->row_tab + 1572864;
    M32(D4A_TBL_4236A4)   = c->row_tab + 0x200000;
    M32(D4A_TBL_4236A8)   = c->row_tab + 2621440;
    M32(D4A_TBL_4236AC)   = c->row_tab + 2883584;
    M32(D4A_TBL_4236B0)   = c->row_tab + 3145728;
    M32(D4A_TBL_4236B8)   = c->row_tab + 5242880;
    M32(D4A_TBL_4236B4)   = c->row_tab + 3407872;
    c->hist_ptr_0   = c->byte_hist;
    c->hist_ptr_1   = c->byte_hist + 0x10000;
    c->hist_ptr_2   = c->byte_hist + 0x20000;
    c->hist_ptr_3   = c->byte_hist + 0x30000;
    memset(c->mem + c->dict_buf, 0, dict + 256);
    memset(c->mem + c->col_tab, 0, 0x100000);
    memset(c->mem + c->row_tab, 0, 0x280000);
    memset(c->mem + c->hash1, 0xFF, 0x200000);
    memset(c->mem + c->hash2, 0xFF, 0x200000);
    memset(c->mem + c->hash_head, 0, 0x40000);
    memset(c->mem + c->hash_byte, 0, 0x40000);
    memset(c->mem + c->byte_hist, 0, 0x40000);
    memset(c->mem + D4A_LAST_SEEN, 0, 0x400);
    memset(c->mem + c->fenwick1 + 266752, 0, 0x1F000);
    memset(c->mem + c->fenwick2 + 266752, 0, 0x1F000);
    d4_fill16(c, 1, c->ppm_tot + 1042, 248);
    d4_fill16(c, 32, D4A_TBL_45D768, 769);
    d4_fill16(c, 16, D4A_TBL_45D166, 769);
    d4_fill16(c, 10, D4A_TBL_45DD5A, 8);
    d4_fill16(c, 8, D4A_TBL_45D738, 24);
    G16(D4A_TBL_45D566) = 6;
    /* asm CDD1 0x40D1E0/0x40D1F6: bound is eax<=0xC04, i.e. indices
     * 1..769 (0xC04/4). A <=768 bound misses index 769 = AE38[768],
     * which then stays 0 instead of 0x4000; its later decay below 0
     * spuriously triggers E4FF(768) (gmud06x: first extra call at out
     * 742923, cascading to the dll fork at 743877). */
    for (i = 1; i <= 769; i++) {
        G32(D4A_TBL_45BA38 + 4*i) = 1;
        G32(D4A_TBL_45AE34 + 4*i) = 0x4000;
    }
    for (i = 1; i <= 769; i++) {
        G8(D4A_TBL_45E085 + i) = 0xFF;
        G8(D4A_TBL_45DD84 + i) = 0;
    }
    d4_build_prob_tables(c);
    G32(D4A_NEXT_BLK) = 0xFFFFFFFFU;
    G32(D4A_TBL_45D124) = 0xFFFFFFFFU;
    G32(D4A_TBL_45D130) = 0xFFFFFFFFU;
    G32(D4A_TBL_45D134) = 0;
    G32(D4A_TBL_45D114) = 0;
    G32(D4A_TBL_45D11C) = 0;
    c->file_table = 0;
    G32(D4A_BLK_END) = 0;
    G32(D4A_TBL_45D144) = 0;
    G32(D4A_FREQ_B) = 1;
    G32(D4A_FREQ_C) = 1;
    G32(D4A_FREQ_A) = 1;
    G32(D4A_CTX_W0) = 0;
    G32(D4A_CTX_W1) = 0;
    G32(D4A_CTX_W2) = 0;
    G32(D4A_TBL_45D108) = 0;
    G8(D4A_FLAG_45E3A2) = 0;
    G8(D4A_PHASE) = 0;
    G8(D4A_ESCAPE_FLAG) = 0;
    G32(D4A_OUT_COUNT) = 0;
    G32(D4A_TBL_45AE30) = 0;
    G32(D4A_MOD_CNT) = 0;
    G8(D4A_FLAG_45E38D) = 0; G8(D4A_FLAG_45E391) = 0; G8(D4A_PRED_FLAG) = 0;
    G8(D4A_MODEL_ID) = 0; G8(D4A_FLAG_45E38B) = 0; G8(D4A_FLAG_45E38C) = 0;
    G8(D4A_FLAG_45E38F) = 0; G8(D4A_FLAG_45E389) = 0; G8(D4A_FLAG_45E38A) = 0;
    G32(D4A_TBL_45D160) = 0;
    G16(D4A_TBL_45D164) = 0;
    for (i = 0; i < 7; i++) G32(D4A_TBL_44316C + 4*i) = 0;
    G32(D4A_TBL_45AE18) = 0;
    G8(D4A_FLAG_45E388) = 0;
    G32(D4A_TBL_45AE14) = 0;
    G32(D4A_TBL_45ADE4) = 0;
    G16(D4A_TBL_45DD6A) = 0;
    G16(D4A_TBL_45DD6C) = 0;
    G32(D4A_FILE_TAB_PTR) = 0;
    G8(D4A_FLAG_45E3A0) = 0;
    G32(D4A_TBL_45AE28) = G32(D4A_OUT_TOTAL);
    /* asm 0x40D3DF..0x40D40E: TWO loops - eax=-2048..-1 stores -eax at
     * [eax&0xFFF] (upper half 2048..1), then eax=0..2047 stores eax at
     * [eax] (low half identity). */
    for (i = 0; i < 2048U; i++) {
        G32(D4A_PERM_TAB + 4*i) = i;
        G32(D4A_PERM_TAB + 4*(i + 2048U)) = 2048U - i;
    }
    G8(D4A_FLAG_45E395) = (G32(D4A_TBL_45D124) != 0) ? 0 : 10;
    return 0;
}

/* ---------------- translated functions ---------------- */


static int d4_row_rescale(dec4_ctx *c)
{
  unsigned int v0;
  int v1;
  signed int result;
  int v3;
  char v4;
  char v5;
  char v6;
  char v7;
  char v8;
  int v9;

  v0 = 0;
  v1 = 0;
  while ( 1 )
  {
    result = 0x20000 >> G8(D4A_PHASE);
    if ( v0 >= 0x20000 >> G8(D4A_PHASE) )
      break;
    v3 = v1 + c->row_tab;
    if ( M8(v1 + c->row_tab) )
    {
      v4 = 8;
      v5 = 0;
      do
      {
        M8((unsigned char)--v4 + v3 + 1) >>= 2;
        v5 += M8((unsigned char)v4 + v3 + 1);
      }
      while ( v4 );
      if ( v5 )
        v6 = ((signed int)M8(v3) >> 2) + 1;
      else
        v6 = 0;
      M8(v3) = v6;
      M8(v3 + 17) >>= 1;
      v7 = M8(v3);
      M8(v3 + 19) &= 0xFu;
      v8 = v7 + v5;
      v9 = M8(v3 + 19);
      M8(v3 + 18) = v8;
      if ( !M8(v9 + v3 + 1) )
        M8(v3 + 19) = 0;
    }
    v1 += 20;
    ++v0;
  }
  return result;
}

static unsigned int d4_col_rescale(dec4_ctx *c)
{
  unsigned int result;
  unsigned int v1;
  unsigned int v2;
  int v3;
  char v4;
  char v5;
  int v6;
  int v7;
  int v8;

  result = 0;
  do
  {
    do
    {
      v3 = c->col_tab + 32 * result;
      if ( !M8(v3) )
        break;
      v4 = 12;
      v5 = 0;
      do
      {
        M8((unsigned char)--v4 + v3 + 1) >>= 2;
        v5 += M8((unsigned char)v4 + v3 + 1);
      }
      while ( v4 );
      if ( v5 )
      {
        M8(v3) = ((signed int)M8(v3) >> 2) + 1;
        v6 = M8(v3 + 25) & 0x3F;
        M8(v3 + 26) &= 0xFu;
        M8(v3 + 25) = v6 >> 1;
        v7 = M8(v3 + 26);
        M8(v3 + 27) = 0;
        if ( !M8(v7 + v3 + 1) )
          M8(v3 + 26) = 0;
      }
      else
      {
        v1 = result;
        v8 = 32 * result;
        while ( 1 )
        {
          v1 += 0x8000;
          v8 += 0x100000;   /* HIWORD += 16 */
          if ( v1 >= 0x40000 )
            break;
          v2 = v8 + c->col_tab;
          if ( !M8(v8 + c->col_tab) )
            break;
          memcpy(c->mem + (v3), c->mem + (v2), 0x20u);
          v3 = (int)v2;
        }
        result -= 0x8000;
        M8(v3) = 0;
      }
      result += 0x8000;
    }
    while ( result < 0x40000 );
    result = (result & 0x7FFF) + 1;
  }
  while ( result < 0x8000 );
  return result;
}

static int d4_block_reinit(dec4_ctx *c)
{
  int v0;
  int v1;
  unsigned int v2;
  unsigned int v3;
  int v4;
  int v5;
  unsigned int v6;
  char v7;
  int v8;
  char v9;
  char v10;
  int v11;
  int v12;
  unsigned int v13;
  char v14;
  unsigned int v15;
  char v16;
  int v17;
  unsigned int v18;
  int v19;
  signed int v20;
  unsigned int v21;
  unsigned int v22;
  int v23;
  int v24;
  unsigned int v25;
  unsigned int v26;
  unsigned int v27;
  int v28;
  int v29;
  int v30;
  unsigned int v31;
  unsigned int v32;
  int v33;
  int v34;
  int v35;
  unsigned int v36;
  int v37;
  int v38;
  int v39;
  int result;
  int v41;
  int v42;
  unsigned int v43;
  int v44;
  int v45;
  int v46;
  int v47;
  unsigned char savedregs[256];

  v45 = G32(D4A_FREQ_B);
  v0 = G32(D4A_DICT_POS);
  v42 = G32(D4A_FREQ_C);
  v1 = c->dict_buf;
  v46 = G32(D4A_FREQ_A);
  v2 = 0;
  do
  {
    savedregs[v2] = M8(v1 + v0);
    v0 = G32(D4A_DICT_MASK) & (v0 + 1);
    ++v2;
  }
  while ( v2 < 0x100 );
  v3 = (unsigned int)(G32(D4A_DICT_SIZE) - 320) >> 1;
  v4 = G32(D4A_DICT_MASK) & (G32(D4A_DICT_POS) + 320);
  v5 = G32(D4A_DICT_MASK) & (v4 + v3);
  if ( v3 > G32(D4A_OUT_COUNT) )
    v5 = 0;
  G8(D4A_ESCAPE_FLAG) = 0;
  v44 = G32(D4A_DICT_MASK) & (G32(D4A_DICT_POS) + 320);
  while ( v5 != G32(D4A_DICT_POS) )
  {
    v6 = (c->dict_buf + v4);
    v7 = ~M8(c->dict_buf + v5);
    v8 = (v4 + 1) & G32(D4A_DICT_MASK);
    if ( G8(D4A_ESCAPE_FLAG) == 1 )
    {
      /* asm 0x40D793: movzx ebx,bl - same unsigned-index fix as 40FFFD. */
      v9 = ~*(&M8(D4A_PENDING_TAB + (11 * ((unsigned char)G8(D4A_FLAG_45E39E) - 166))) + (unsigned char)v7);
      G8(D4A_ESCAPE_FLAG) = 0;
LABEL_23:
      M8(v6) = v9;
      v4 = v8;
      goto LABEL_24;
    }
    if ( (unsigned char)v7 < 0x80u && !G8(D4A_ESCAPE_FLAG) )
    {
      v9 = M8(c->dict_buf + v5);
      goto LABEL_23;
    }
    v10 = M8(D4A_DECODE_MAP + ((unsigned char)v7));
    v11 = G32(D4A_DICT_MASK) & (v4 + 1);
    if ( (unsigned char)v10 >= 0xC8u )
    {
      v16 = v7 - G8(D4A_ESCAPE_FLAG);
      G8(D4A_ESCAPE_FLAG) = 0;
      M8(c->dict_buf + v4) = ~v16;
      v4 = v11;
    }
    else if ( v10 == 100 )
    {
      G8(D4A_ESCAPE_FLAG) = 1;
      G8(D4A_FLAG_45E39E) = v7;
    }
    else if ( v10 == 99 )
    {
      G8(D4A_ESCAPE_FLAG) = 32;
    }
    else
    {
      v12 = (unsigned char)v10;
      v13 = M32(D4A_SDI_PTR + (v12) * 4);
      v14 = M8(D4A_SDI_LEN + (v12));
      if ( G8(D4A_ESCAPE_FLAG) )
      {
        v15 = v13;
        G8(D4A_ESCAPE_FLAG) = 0;
        ++v13;
        M8(c->dict_buf + v4) = M8(v15) + 32;
        v4 = v11;
        --v14;
      }
      do
      {
        M8(c->dict_buf + v4) = M8(v13);
        v4 = G32(D4A_DICT_MASK) & (v4 + 1);
        ++v13;
        if ( v5 == v4 )
          break;
        --v14;
      }
      while ( v14 );
    }
LABEL_24:
    if ( v5 == v4 )
      break;
    v5 = G32(D4A_DICT_MASK) & (v5 + 1);
  }
  G8(D4A_ESCAPE_FLAG) = 0;
  v17 = G32(D4A_DICT_MASK) & (v4 + G32(D4A_TBL_45D140) - G32(D4A_DICT_POS));
  G32(D4A_DICT_POS) = v4;
  G32(D4A_TBL_45D140) = v17;
  v18 = 0;
  do
  {
    M32(D4A_TBL_446064 + (((unsigned char)v4) * 4)) = 1;
    M8(c->dict_buf + v4) = savedregs[v18++];
    v4 = G32(D4A_DICT_MASK) & (v4 + 1);
  }
  while ( v18 < 0x100 );
  v19 = v44;
  v20 = 64;
  do
  {
    v21 = c->dict_buf;
    v19 = G32(D4A_DICT_MASK) & (v19 - 1);
    M8(c->dict_buf + v19) = 0;
    --v20;
  }
  while ( v20 );
  v22 = (v21 + (G32(D4A_DICT_SIZE)));
  v23 = -(char)((unsigned char)v21 + G32(D4A_DICT_SIZE)) & 3;
  v24 = 256 - v23;
  if ( v23 < 256 )
  {
    memcpy(c->mem + (v22), c->mem + (v21), v23);
    v26 = (v21 + (v23));
    v25 = (v22 + (v23));
    v24 = -(char)v23 & 3;
    v27 = (unsigned int)(256 - v23) >> 2;
    memcpy(c->mem + (v25), c->mem + (v26), 4 * v27);
    v21 = (v26 + (4 * v27));
    v22 = (v25 + (4 * v27));
    v23 = 0;
  }
  memcpy(c->mem + (v22), c->mem + (v21), v24 + v23);
  d4_col_rescale(c);
  d4_fill8(c, c->row_tab, 0, 0x140000u);
  d4_fill8(c, c->hash1, -1, 0x200000u);
  d4_fill8(c, c->hash2, -1, 0x200000u);
  d4_fill8(c, c->hash_head, 0, 0x40000u);
  d4_fill8(c, c->hash_byte, 0, 0x40000u);
  G32(D4A_CTX_W0) = 0;
  G32(D4A_CTX_W1) = 0;
  v29 = v44;
  G32(D4A_CTX_W2) = 0;
  G8(D4A_FLAG_45E394) = 0;
  G8(D4A_FLAG_45E397) = 0;
  if ( (G32(D4A_DICT_MASK) & (unsigned int)(G32(D4A_DICT_SIZE) + G32(D4A_DICT_POS) - v44)) <= 0x8000 )
    v30 = -1;
  else
    v30 = G32(D4A_DICT_MASK) & (G32(D4A_DICT_POS) - 0x8000);
  while ( 2 )
  {
    if ( v29 != G32(D4A_DICT_POS) )
    {
      v31 = c->hash1 + 8 * ((G32(D4A_CTX_W0) ^ ((unsigned int)G32(D4A_CTX_W0) >> 13)) & 0x3FFFF);
      v32 = (c->hash2
                     + 8
                     * ((G32(D4A_CTX_W2) ^ ((unsigned int)G32(D4A_CTX_W2) >> 15) ^ 16
                                                                           * (G32(D4A_CTX_W0) ^ ((unsigned int)G32(D4A_CTX_W0) >> 15)) ^ ((G32(D4A_CTX_W1) ^ ((unsigned int)G32(D4A_CTX_W1) >> 15)) << 6)) & 0x3FFFF)
                     + 4);
      /* asm 0x40DA63: ebx=[eax-4]; [eax]=ebx; [eax-4]=edx  (8-byte entries,
       * eax = htab2 + 8*h + 4, so the two slots are -4 apart) */
      M32(v32) = M32(v32 - 4);
      M32(v32 - 4) = v29;
      v33 = G32(D4A_CTX_W0);
      M32(v31) = v29;
      M32(v31 + 4) = v33;
      if ( v29 == v30 )
      {
        v30 = -1;
LABEL_39:
        G32(D4A_CTX_COPY) = G32(D4A_CTX_W0);
        G32(D4A_COL_IDX) = ((unsigned short)G32(D4A_CTX_W0) ^ (unsigned short)((unsigned int)G32(D4A_CTX_W0) >> 13)) & 0x7FFF;
        v34 = 32 * (((unsigned short)G32(D4A_CTX_W0) ^ (unsigned short)((unsigned int)G32(D4A_CTX_W0) >> 13)) & 0x7FFF);
        while ( 1 )
        {
          v35 = v34 + c->col_tab;
          if ( !M8(v34 + c->col_tab) )
          {
            G32(D4A_COL_IDX) += 0x40000;   /* HIWORD += 4 */
            goto LABEL_48;
          }
          if ( M32(v35 + 28) == G32(D4A_CTX_COPY) )
            break;
          G32(D4A_COL_IDX) += 0x8000;
          v34 += 0x100000;
          if ( (unsigned int)G32(D4A_COL_IDX) >= 0x40000 )
            goto LABEL_48;
        }
        v47 = M8(v35);
        v36 = v34 + c->col_tab;
        v37 = v35 + 12;
        G8(D4A_COL_SLOT) = -1;
        v38 = M8(v37 - 11);
        v43 = v37;
        do
        {
          ++v36;
          v47 += v38;
          if ( v36 >= v43 )
            break;
          v38 = (v38 & ~0x000000FFU) | ((M8(v36 + 1)) & 0x000000FFU);
        }
        while ( (unsigned char)v38 );
        G32(D4A_TBL_45C678) = v47;
LABEL_48:
        d4_col_update(c, M8(v29 + c->dict_buf));
      }
      else if ( v30 == -1 )
      {
        goto LABEL_39;
      }
      G32(D4A_CTX_W2) = ((unsigned int)G32(D4A_CTX_W1) >> 24) + (G32(D4A_CTX_W2) << 8);
      G32(D4A_CTX_W1) = ((unsigned int)G32(D4A_CTX_W0) >> 24) + (G32(D4A_CTX_W1) << 8);
      v39 = M8(v29 + c->dict_buf);
      v29 = G32(D4A_DICT_MASK) & (v29 + 1);
      G32(D4A_CTX_W0) = v39 + (G32(D4A_CTX_W0) << 8);
      continue;
    }
    break;
  }
  G32(D4A_FREQ_B) = v45;
  G32(D4A_FREQ_C) = v42;
  G32(D4A_FREQ_A) = v46;
  d4_build_prob_tables(c);
  G32(D4A_NEXT_BLK) = -1;
  result = G32(D4A_OUT_POS);
  G8(D4A_PHASE) = 1;
  G32(D4A_BLK_END) = G32(D4A_OUT_POS);
  return result;
}

static unsigned int d4_build_prob_tables(dec4_ctx *c)
{
  unsigned int v0;
  int v2;
  int v3;
  int v4;
  int v5;
  int v6;
  unsigned int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  unsigned int result;
  unsigned int v13;
  unsigned int v14;
  int v15;
  int v16;
  unsigned int v17;
  int v18;
  int v19;
  unsigned int v20;
  int v21;
  unsigned int v22;
  int v23;
  signed int v24;
  int v25;
  int v26;
  int v27;
  int v28;
  int v29;
  int v30;
  unsigned int v31;
  unsigned int v32;
  unsigned int v33;
  int v34;

  /* NOTE (MSVC6): the original nested do/while form of this table setup
   * is miscompiled by MSVC6 /O2 (cell 0x457D68 stays 0 instead of 0x1680,
   * breaking D3X.UHA at out byte 694). Plain for-loops generate correct
   * code there and identical behaviour everywhere else. */
  v2 = 0;
  for (v0 = 0; v0 < 0x20; v0++)
  {
    for (v4 = 0, v5 = v0 * 64; v4 < 16; v4++)
    {
      v5 += 4;
      v6 = ((8 * (v0 >= 0x10) + M32(D4A_TBL_42758D + ((v4) * 4)) + (v0 & 4)) << 7) & 0x3FFFFFF;
      M32((D4A_TBL_457D60) + v5) = v6;
      M32((D4A_TBL_452860) + v5) = v6;
    }
    ++v2;
    v3 = M32(D4A_TBL_42764D + ((v2) * 4));
    M32(D4A_TBL_4578E0 + ((v2) * 4)) = 4096;
    M32(D4A_TBL_453460 + ((v2) * 4)) = v3 << 7;
  }
  v7 = 0;
  v29 = 0;
  do
  {
    v8 = v29;
    v9 = 0;
    do
    {
      v23 = 4 * (v7 >= 8);
      v25 = v8;
      v11 = 4 * (v7 >= 0x40);
      if ( v7 < 0x10 )
        M32(D4A_TBL_453064 + ((v8) * 4)) = ((v23 + M32(D4A_TBL_4275CD + ((v9) * 4))) << 7) & 0x3FFFFFF;
      if ( v7 < 0x20 )
        M32(D4A_TBL_45A564 + ((v8) * 4)) = ((4 * (v7 >= 0x10) + M32(D4A_TBL_4275CD + ((v9) * 4))) << 7) & 0x3FFFFFF;
      if ( v7 < 0x10 )
        M32(D4A_TBL_4554E4 + ((v8) * 4)) = ((v23 + M32(D4A_TBL_4275CD + ((v9) * 4))) << 7) & 0x3FFFFFF;
      M32(D4A_TBL_4558E4 + ((v8) * 4)) = ((v11 + M32(D4A_TBL_4275CD + ((v9) * 4))) << 7) & 0x3FFFFFF;
      M32(D4A_TBL_4534E4 + ((v8) * 4)) = ((v11 + M32(D4A_TBL_4275CD + ((v9) * 4)) + (v7 >= 0x10)) << 7) & 0x3FFFFFF;
      v10 = M32(D4A_TBL_4275CD + ((v9) * 4)) + (v7 >= 0x10);
      ++v8;
      ++v9;
      M32(D4A_TBL_458564 + ((v25) * 4)) = ((v10 + v11) << 7) & 0x3FFFFFF;
    }
    while ( v9 != 16 );
    ++v7;
    v29 += 16;
  }
  while ( v7 < 0x80 );
  result = 0;
  v24 = 1024;
  v34 = 0;
  v26 = 0;
  v27 = 0;
  do
  {
    v13 = 0;
    v28 = v34;
    v32 = result << 10;
    v31 = result << 10;
    v30 = -v34;
    do
    {
      v33 = 0;
      v14 = v31;
      do
      {
        v15 = v32 + 4 * v33;
        M32((D4A_TBL_450464) + v15) = 4096;
        if ( result > v13 )
          M32((D4A_TBL_450464) + v15) -= M32((D4A_TBL_42760D) + v28);
        if ( v13 > result )
          M32((D4A_TBL_450464) + v14) += M32((D4A_TBL_42760D) + v30);
        ++v33;
        v14 += 4;
      }
      while ( v33 < 0x20 );
      v32 += 128;
      v28 -= 4;
      v30 += 4;
      ++v13;
      v31 += 128;
    }
    while ( v13 < 8 );
    v16 = v26;
    v17 = 0;
    do
    {
      v16 += 4;
      v18 = (M32(D4A_TBL_42762D + ((result + (v17++ & 1)) * 4)) << 7) & 0x3FFFFFF;
      M32((D4A_TBL_452460) + v16) = v18;
    }
    while ( v17 < 0x20 );
    v19 = v27;
    v20 = 0;
    do
    {
      v19 += 4;
      v21 = (M32(D4A_TBL_42762D + ((result + (v20++ & 1)) * 4)) << 7) & 0x3FFFFFF;
      M32((D4A_TBL_448460) + v19) = v21;
    }
    while ( v20 < 0x400 );
    v22 = result << 10;
    do
    {
      v22 += 4;
      M32((D4A_TBL_446460) + v22) = 2048;
    }
    while ( v22 != v24 );
    v34 += 4;
    v26 += 128;
    v27 += 4096;
    ++result;
    v24 += 1024;
  }
  while ( result < 8 );
  return result;
}

static int d4_block_model_init(dec4_ctx *c)
{
  int v0;
  int v1;
  int v2;
  unsigned int v3;
  unsigned int v4;
  signed int v5;
  unsigned int v6;
  unsigned int v7;
  int v8;
  int v9;
  unsigned int v10;
  unsigned int v11;
  unsigned int v12;
  int v13;
  unsigned int v14;
  unsigned int v15;
  int v16;
  int v17;
  int v18;
  unsigned char v19;
  int v20;
  unsigned int v21;
  unsigned int v22;
  unsigned int v23;
  unsigned int v24;
  int v25;
  int v26;
  unsigned int v27;
  int v28;
  int v29;
  int v30;
  int v31;
  unsigned int v32;
  unsigned int v33;
  unsigned int v34;
  unsigned char v35;   /* compared/stored as a byte (e390); signed char would
                          spuriously re-enter the model-switch block */
  int v36;
  int v37;
  int v38;
  int result;
  unsigned int v40;
  unsigned int i;
  int v42;
  int v43;
  int v44;

  D4TRACE("DEB6 out=%u blk=%u next=%u prog=%u idx=%u phase=%u\n", G32(D4A_OUT_POS), G32(D4A_BLK_END), G32(D4A_NEXT_BLK), c->file_table, G32(D4A_TBL_45D144), G8(D4A_PHASE));
#ifdef D4DEB6DBG
  fprintf(stderr, "DEB6B out=%u dac1=%u c14=%u\n",
      G32(D4A_OUT_POS), M32(D4A_TBL_445DAC + 4), M32(D4A_TBL_4455A4 + 4));
#endif

  v0 = (unsigned char)G8(D4A_PHASE);
  v1 = 0;
  do
  {
    ++v1;
    M32(D4A_TBL_45BA38 + ((v1) * 4)) = v0;
  }
  while ( v1 != 521 );
  v2 = 0;
  do
  {
    ++v2;
    M32(D4A_TBL_45AE34 + ((v2) * 4)) = 0x4000;
  }
  while ( v2 != 521 );
  v3 = 0;
  do
    M8(D4A_TBL_45E085 + (++v3)) = -1;
  while ( v3 < 0x209 );
  v4 = 0;
  do
    M8(D4A_TBL_45DD84 + (++v4)) = 0;
  while ( v4 < 0x209 );
  if ( G8(D4A_PHASE) )
  {
    d4_fill8(c, D4A_TBL_45AD64, 1, 0x80u);
    d4_fill8(c, M32(D4A_TBL_42369C), 0, 0x400000u);
  }
  d4_fill8(c, c->fenwick1, 0, 0x41200u);
  d4_fill8(c, c->fenwick2, 0, 0x41200u);
  v5 = 393728;
  do
  {
    v6 = (D4A_PPM_PATTERN);
    v7 = (v5 + c->fenwick1);
    v5 += 512;
    v8 = -(char)v7 & 3;
    v9 = 512 - v8;
    if ( v8 < 512 )
    {
      memcpy(c->mem + (v7), c->mem + ((D4A_PPM_PATTERN)), v8);
      v11 = (D4A_PPM_PATTERN) + v8;
      v10 = (v7 + (v8));
      v9 = -(char)v8 & 3;
      v12 = (unsigned int)(512 - v8) >> 2;
      memcpy(c->mem + (v10), c->mem + (v11), 4 * v12);
      v6 = (v11 + (4 * v12));
      v7 = (v10 + (4 * v12));
      v8 = 0;
    }
    memcpy(c->mem + (v7), c->mem + (v6), v9 + v8);
  }
  while ( v5 != 400896 );
  v13 = 0;
  do
  {
    v13 += 2;
    M16(v13 + c->fenwick1 + 393726) *= 8;
  }
  while ( v13 != 7168 );
  d4_fill16(c, 8, c->fenwick2 + 393728, 3584);
  d4_fill16(c, 1, c->ppm_tot, 521);
  /* Hex-Rays lost the source of v16 in the 656-byte copy below (the
   * original is just memcpy(unk_445DB4, dword_42372C, 656)). */
  memcpy(c->mem + D4A_TBL_445DB4, c->mem + D4A_CLS_TREE_SRC, 656);
  v24 = 521;
  do
  {
    d4_ppm_rescale(c, v24);
    ++v24;
  }
  while ( v24 < 0x301 );
  d4_row_rescale(c);
  G32(D4A_TBL_45C6B0) = 0;
  G32(D4A_TBL_45C6AC) = 0;
  G32(D4A_TBL_45C6A0) = 0;
  G32(D4A_TBL_45C6B4) = 511;
  c->cost_cnt_b = D4A_TBL_45CCD4;
  d4_fill8(c, D4A_TBL_45CCD4, 0, 0x424u);
  c->cost_cnt_a = D4A_TBL_45C6B8;
  d4_fill8(c, D4A_TBL_45C6B8, 0, 0x61Cu);
  d4_fill8(c, D4A_TBL_444DA4, 0, 0x400u);
  d4_fill8(c, D4A_TBL_4455AC, (int)0x80000000u, 0x400u);
  d4_fill8(c, D4A_TBL_4445A4, 0, 0x400u);
  G32(D4A_TBL_45C66C) = 0;
  G32(D4A_BEST_OFF) = 1;
  G32(D4A_BEST_CNT) = 1;
  G32(D4A_TBL_45C670) = 0;
  v25 = 0;
  do
  {
    ++v25;
    M32(D4A_TBL_4451A0 + ((v25) * 4)) = 37440;
  }
  while ( v25 != 256 );
  G32(D4A_TBL_45C650) = 255;
  G32(D4A_TBL_45C644) = 9547200;
  M32(D4A_TBL_4455A4 + ((0) * 4)) = 0;
  G32(D4A_TBL_4455A8) = 0;
  M32(D4A_TBL_445DAC + ((0) * 4)) = 0;
  G32(D4A_TBL_445DB0) = 0;
  G32(D4A_COST_ACCUM) = 0;
  G32(D4A_COST_GATE) = 0;
  if ( c->file_table )
  {
    G32(D4A_BLK_END) += 0x40000 >> 2 * G8(D4A_PHASE);
    if ( (unsigned int)G32(D4A_BLK_END) > G32(D4A_OUT_TOTAL) )
      G32(D4A_BLK_END) = G32(D4A_OUT_TOTAL);
    v26 = 4 * G32(D4A_TBL_45D144);
    do
    {
      if ( M32(c->file_table + v26) >= (unsigned int)G32(D4A_BLK_END) )
        break;
      v26 += 4;
      v27 = M32(c->file_table + v26);
      ++G32(D4A_TBL_45D144);
      if ( v27 >= G32(D4A_NEXT_BLK) )
        break;
    }
    while ( v27 < G32(D4A_TBL_45D124) );
    v28 = M32(c->file_table + 4 * G32(D4A_TBL_45D144));
  }
  else
  {
    v28 = G32(D4A_OUT_TOTAL);
  }
  G32(D4A_BLK_END) = v28;
  if ( G32(D4A_TBL_45D144) )
  {
    v29 = c->file_table + 4 * G32(D4A_TBL_45D144);
    v30 = M32(v29 - 4);
    v31 = G32(D4A_BLK_END) - v30;
    if ( (unsigned int)G32(D4A_TBL_45D144) > 1 )
      v30 -= M32(v29 - 8);
    if ( G8(D4A_FLAG_45E387) == 1 )
      v32 = G32(D4A_TBL_45D108);
    else
      v32 = G32(D4A_OUT_POS);
    if ( v30 == 586960 && v31 == 2988578 )
    {
      d4_col_rescale(c);
      d4_col_rescale(c);
    }
    if ( v30 == 481861 && v31 == 426754 )
      d4_col_rescale(c);
    if ( v30 == 1566200 && v31 == 2934336 || v30 == 2934336 && v31 == 438144 )
    {
      d4_col_rescale(c);
      d4_row_rescale(c);
    }
    if ( v30 == 3706306 && v31 == 1498414
      || v30 == 513216 && v31 == 246814
      || v30 != 1029744 && v31 == 1029744 && v32 >= G32(D4A_TBL_45D124) )
    {
      d4_fill8(c, c->col_tab, 0, 0x100000u);
      d4_fill8(c, c->row_tab, 0, 0x140000u);
      if ( v32 >= G32(D4A_TBL_45D124) )
        G8(D4A_FLAG_45E395) = 10;
    }
  }
  G32(D4A_TRI_TOTAL) = 10;
  v33 = G32(D4A_RC_RANGE) / 0xAu;
  G32(D4A_RC_RANGE) = v33;
  v34 = (unsigned)d4_div(G32(D4A_RC_CODE), v33);
  v35 = v34;
  v36 = (unsigned char)v34;
  G32(D4A_TRI_LOW) = (unsigned char)v34;
  G32(D4A_TRI_HIGH) = (unsigned char)v34 + 1;
  v37 = M32(D4A_TRI_TOTAL + (1) * 4);
  v38 = M32(D4A_TRI_TOTAL + (2) * 4) - v37;
  result = G32(D4A_RC_RANGE) * v37;
  v40 = G32(D4A_RC_RANGE) * v38;
  i = (unsigned long)(G32(D4A_RC_CODE) - result);
  d4_rcrenorm(c, &i, &v40);
  G32(D4A_RC_CODE) = (int)i;
  G32(D4A_RC_RANGE) = v40;
  if ( v35 != G8(D4A_MODEL_ID) )
  {
    G8(D4A_MODEL_ID) = v35;
    result = M32(D4A_TRI2_TOTAL + ((v36) * 4));
    v44 = M32(D4A_TRI2_RANGE + ((v36) * 4));
    G32(D4A_TRI2_TOTAL_NEXT) = M32(D4A_TRI2_TOTAL + ((v36) * 4));
    result = (result & ~0x000000FFU) | ((M8(D4A_TRI2_IDX + (v36))) & 0x000000FFU);
    G32(D4A_TRI2_RANGE_NEXT) = v44;
    G8(D4A_FLAG_45E38C) = result;
    result = (result & ~0x000000FFU) | ((v35 == 5 || v35 == 6) & 0x000000FFU);
    G8(D4A_PRED_FLAG) = 0;
    G8(D4A_FLAG_45E38B) = 0;
    G32(D4A_MOD_CNT) = 0;
    G8(D4A_FLAG_45E38D) = result;
    if ( (unsigned char)G8(D4A_MODEL_ID) > 4u )
      result = d4_pred_cache_reset(c);
  }
  return result;
}

static unsigned int d4_fenwick_rebuild(dec4_ctx *c, int a1)
{
#ifdef D4RSDBG
  fprintf(stderr, "RS out=%u a1=%d\n", G32(D4A_OUT_POS), a1);
#endif
  int v1;
  int v2;
  int v3;
  int v4;
  unsigned int v5;
  unsigned int v6;
  int v7;
  int v8;
  int v9;
  unsigned int v10;
  unsigned int v11;
  int v12;
  unsigned int v13;
  unsigned int v14;
  int v15;
  int v16;
  unsigned int v17;
  unsigned int v18;
  unsigned int v19;
  int v20;
  unsigned int v21;
  int v22;
  unsigned int result;
  int v24;

  v1 = a1;
  v2 = (a1 << 9) + c->fenwick2;
  v3 = 0;
  v4 = v1 << 8;
  v24 = v2 + 512;
  do
  {
    v5 = ((M32(v2 + 4) + 65537) & 0xFFFEFFFE) >> 1;
    v6 = ((M32(v2) + 65537) & 0xFFFEFFFE) >> 1;
    M32(v2 + 4) = v5;
    v7 = v6 + v5;
    v8 = M32(v2 + 12);
    M32(v2) = v6;
    v9 = M32(v2 + 8);
    v2 += 16;
    v10 = ((v8 + 65537) & 0xFFFEFFFE) >> 1;
    v11 = ((v9 + 65537) & 0xFFFEFFFE) >> 1;
    M32(v2 - 4) = v10;
    v3 += v11 + v10 + v7;
    M32(v2 - 8) = v11;
  }
  while ( v2 != v24 );
  v12 = 2 * v4;
  v13 = (c->fenwick1 + 2 * v4 + 2);
  v14 = (v12 + c->fenwick2);
  v15 = (-2 - (unsigned char)v13 + 2) & 3;
  v16 = 510 - v15;
  if ( v15 < 510 )
  {
    memcpy(c->mem + (v13), c->mem + (v14), v15);
    v18 = (v14 + (v15));
    v17 = (v13 + (v15));
    v16 &= 3u;
    v19 = (unsigned int)(510 - v15) >> 2;
    memcpy(c->mem + (v17), c->mem + (v18), 4 * v19);
    v14 = (v18 + (4 * v19));
    v13 = (v17 + (4 * v19));
    v15 = 0;
  }
  memcpy(c->mem + (v13), c->mem + (v14), v16 + v15);
  v20 = v12 + c->fenwick1;
  M16(v12 + c->fenwick1) = HIWORD(v3) + v3;
  v21 = 1;
  do
  {
    v22 = 4 * v21;
    v21 *= 2;
    result = (v20 + 512 - v22);
    do
    {
      /* asm 0x40E4E4: the read is at result - v21 (v21 already doubled);
       * (-v21)<<1 reads 2x too far back and corrupts the rebuilt tree.
       * Same class as the fixed d4_ppm_rescale read (see its 0x40E5C0 note). */
      M16(result) += M16(result - v21);
      result -= v22;
    }
    while ( result != v20 );
  }
  while ( v21 < 0x80 );
  return result;
}

static int d4_ppm_rescale(dec4_ctx *c, unsigned int a1)
{
  unsigned int v1;
  short v2;
  unsigned int v3;
  int v4;
  unsigned int v5;
  unsigned int v6;
  int v7;
  int v8;
  unsigned int v9;
  unsigned int v10;
  unsigned int v11;
  unsigned int v12;
  int v13;
  unsigned int v14;
  signed int result;
  unsigned int v16;
  int v17;
  unsigned int v18;
  int v19;
  unsigned int v20;
  unsigned int v21;
  int v22;
  unsigned int v23;

  v1 = a1;
  v23 = a1 << 8;
  v2 = 0;
  v3 = (c->fenwick2 + (a1 << 9));
  v4 = (int)(v3 + 0x200);   /* asm 0x40e520: esi = start + 0x200, stride 2 */
  do
  {
    if ( M16(v3) )
    {
      if ( !G8(D4A_PHASE) )
      {
        ++M16(v3);
LABEL_3:
        M16(v3) >>= 1;
        v2 += M16(v3);
        goto LABEL_4;
      }
      if ( M16(v3) > 0x10u )
        goto LABEL_3;
      M16(v3) = 0;
    }
LABEL_4:
    v3 += 2;
  }
  while ( v3 != v4 );
  v22 = 2 * v23;
  v5 = (2 * v23 + c->fenwick2);
  v6 = (c->fenwick1 + 2 * v23 + 2);
  v7 = (-2 - (unsigned char)v6 + 2) & 3;
  v8 = 510 - v7;
  if ( v7 < 510 )
  {
    memcpy(c->mem + (v6), c->mem + (v5), v7);
    v10 = (v5 + (v7));
    v9 = (v6 + (v7));
    v8 &= 3u;
    v11 = (unsigned int)(510 - v7) >> 2;
    memcpy(c->mem + (v9), c->mem + (v10), 4 * v11);
    v5 = (v10 + (4 * v11));
    v6 = (v9 + (4 * v11));
    v7 = 0;
  }
  memcpy(c->mem + (v6), c->mem + (v5), v8 + v7);
  v21 = v22 + c->fenwick1;
  M16(v22 + c->fenwick1) = v2;
  v12 = 1;
  do
  {
    v13 = 4 * v12;
    v12 *= 2;
    v14 = (v21 + 512 - v13);
    do
    {
      /* asm 0x40e5c0: the read is at v14 - v12 (not 2*v12 = v13). */
      M16(v14) += M16(v14 - v12);
      v14 -= v13;
    }
    while ( v14 != v21 );
  }
  while ( v12 < 0x80 );
  result = c->fenwick1;
  v16 = (c->fenwick1 + 2 * v23);
  if ( M16(v16) )
  {
    v17 = c->ppm_tot;
    v18 = v1;
    ++M16(v17 + 2 * v18);
    M16(v17 + 2 * v18) >>= 1;
    v19 = M16(v16) + M16(v17 + 2 * v1);
    v20 = v1;
    v18 = (v18 & ~0x000000FFU) | ((G8(D4A_PHASE)) & 0x000000FFU);
    M32(D4A_TBL_45AE38 + ((v1) * 4)) = v19;
    result = 512 >> v18;
    if ( v1 < 512 >> v18 )
    {
      result = 96 * M32(D4A_TBL_45AE38 + (v20) * 4);
      M32(D4A_TBL_45AE38 + ((v20) * 4)) = result;
    }
    else
    {
      M32(D4A_TBL_45AE38 + ((v1) * 4)) <<= 8;
    }
    if ( G8(D4A_MODEL_ID) == 3 )
    {
      result = 8 * M16(c->ppm_tot + 2 * v1);
      if ( M16(c->fenwick1 + (v1 << 9)) > result )
        M32(D4A_TBL_45AE38 + ((v1) * 4)) *= 16;
    }
  }
  return result;
}

static unsigned char d4_fenwick_add(dec4_ctx *c, unsigned char a1, int a2)
{
#ifdef D4E672DBG
  if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
    fprintf(stderr, "E672 out=%u a1=%u a2=%d c690=%u v4=%u\n",
        G32(D4A_OUT_POS), (unsigned)a1, a2, G32(D4A_PPM_CTX),
        (unsigned)M16(D4A_TBL_45D166 + ((a2) * 2)));
#endif
  int v2;
  unsigned int v3;
  short v4;
  int v5;
  unsigned char result;
  char v7;

  v2 = a2;
  v3 = (c->fenwick1 + (G32(D4A_PPM_CTX) << 9));
  v4 = M16(D4A_TBL_45D166 + ((v2) * 2));
  M16(c->fenwick2 + 2 * (a1 + (G32(D4A_PPM_CTX) << 8))) += v4;
  result = a1 + 1;
  /* asm 0x40E6A8/0x40E6AA: al is a byte - a1=255 wraps to 0 and skips the
   * Fenwick walk; as an int, result=256 runs once and writes index 256
   * (the NEXT plane's root). */
  v5 = ((unsigned char)result) == 0;
  while ( !v5 )
  {
    v7 = result & -result;
    M16(v3 + ((result) * 2)) += v4;
    v5 = (unsigned char)(v7 + result) == 0;
    result += v7;
  }
  M16(v3) += v4;
  if ( M16(v3) >= 0x4000u )
    result = (unsigned int)d4_fenwick_rebuild(c, G32(D4A_PPM_CTX));
  return result;
}

static int d4_fenwick_add_run(dec4_ctx *c, unsigned char a1, int a2)
{
  unsigned int v2;
  unsigned int v3;
  short v4;
  int v5;
  int v6;
  unsigned int v7;
  int v8;
  char v9;
  int v10;
  signed int result;
  int v12;
  unsigned char v13;
  unsigned char v14;

  v13 = a1;
  v2 = a2;
  v3 = (2 * ((a2 << 8) + a1) + c->fenwick2);
  v4 = M16(D4A_TBL_45D768 + ((a2) * 2));
  v5 = M16(v3) + M16(c->ppm_tot + 2 * a2);
  if ( M8(D4A_TBL_45E086 + (a2)) == v13 )
  {
    v5 *= 32;
    if ( (unsigned char)M8(D4A_RUN_CNT + (a2)) < 0xFu )
      ++M8(D4A_RUN_CNT + (a2));
  }
  else
  {
    M8(D4A_RUN_CNT + (a2)) = 0;
    M8(D4A_TBL_45E086 + (a2)) = v13;
  }
  v6 = a2;
  M32(D4A_TBL_45AE38 + ((v6) * 4)) -= v5;
  v12 = 2 * a2;
  if ( M16(v3) )
  {
    if ( M16(c->fenwick1 + (a2 << 9)) <= (unsigned int)M16(v12 + c->ppm_tot) * M32(D4A_TBL_45BA3C + (v6) * 4) )
      d4_fenwick_add(c, v13, a2);
    M32(D4A_TBL_45BA3C + ((v2) * 4)) = (unsigned int)M32(D4A_TBL_45BA3C + ((v2) * 4)) >> 4;
  }
  else
  {
    d4_fenwick_add(c, v13, a2);
    M16(v12 + c->ppm_tot) += v4;
    M32(D4A_TBL_45BA3C + ((v6) * 4)) += (unsigned char)G8(D4A_PHASE);
  }
  M16(v3) += v4;
  v7 = (c->fenwick1 + (v2 << 9));
  v8 = v13 == 255;
  v14 = v13 + 1;
  while ( !v8 )
  {
    v9 = v14 & -v14;
    M16(v7 + ((v14) * 2)) += v4;
    v8 = (unsigned char)(v9 + v14) == 0;
    v14 += v9;
  }
  v10 = c->ppm_tot;
  M16(v7) += v4;
  result = M16(v7) + M16(v10 + 2 * v2);
  if ( result >= 0x4000 || (int)M32(D4A_TBL_45AE38 + ((v2) * 4)) < 0 )
    result = d4_ppm_rescale(c, v2);   /* asm 0x40e7f9: cmpl $0,... = signed */
  return result;
}

static unsigned char d4_cls_update(dec4_ctx *c, unsigned char a1, unsigned char a2)
{
  unsigned char v2;
  unsigned int v3;
  int v4;
  int v5;
  int v6;
  int v7;
  unsigned char result;

  /* 8-byte entry: +0 pad, +2/+4/+6 cumulative u16 (the class tree). */
  v2 = a2;
  v3 = D4A_TBL_445DB4 + 8 * a1;
  if ( a2 == 1 )
    M16(v3 + 4) += 32;
  M16(v3 + 6) += 32;
  if ( M16(v3 + 6) >= 0x4000u )
  {
    v4 = (M16(v3 + 6) - M16(v3 + 4) + 1) >> 1;
    v5 = (M16(v3 + 6) - M16(v3 + 4) + 1) >> 1;
    v4 = (v4 & ~0x0000FFFFU) | (M16(v3 + 2) & 0x0000FFFFU);
    v6 = v4 + ((M16(v3 + 4) - M16(v3 + 2) + 1) >> 1);
    M16(v3 + 4) = v6;
    M16(v3 + 6) = v6 + v5;
  }
  v7 = 2 * (unsigned char)G8(D4A_FLAG_45E3A2);
  result = M8(D4A_CLS_CHILD + v7 + v2);
  G8(D4A_FLAG_45E3A2) = M8(D4A_CLS_CHILD + v7 + v2);
  return result;
}

static unsigned int d4_row_pick_best(dec4_ctx *c)
{
  char v0;
  int v1;
  signed int v2;
  char v3;
  int v4;
  char v5;
  unsigned int v6;
  int v7;
  unsigned int v8;
  int result;
  unsigned int v10;
  char v11;

  v10 = 0;
  v0 = 8;
  c->pair_ptr = 0;
  v1 = ((unsigned)HIWORD(G32(D4A_CTX_W0))) << 16;
  v2 = 0;
  v1 = (v1 & ~0x0000FFFFU) | ((0) & 0x0000FFFFU);
  v3 = 0;
  if ( v1 == -65536 )
  {
    v2 = 0x10000;
  }
  else if ( (unsigned short)G32(D4A_CTX_W0) == 0xFFFF )
  {
    v2 = 0x20000;
  }
  else if ( (G32(D4A_CTX_W0) & 0xFF000000) == -16777216 )
  {
    v2 = 196608;
  }
  else if ( (G32(D4A_CTX_W0) & 0xFF0000) == 16711680 )
  {
    v2 = 0x40000;
  }
  else if ( (G32(D4A_CTX_W0) & 0xFF00) == 65280 )
  {
    v2 = 327680;
  }
  else if ( (unsigned char)G32(D4A_CTX_W0) == 255 )
  {
    v2 = 393216;
  }
  v4 = HIWORD(G32(D4A_CTX_W0)) & 0xFF00;
  G32(D4A_TBL_44604C) = (unsigned int)G32(D4A_CTX_W0) >> 16;
  M32(D4A_TBL_446044 + ((0) * 4)) = v4 + ((unsigned int)G32(D4A_CTX_W1) >> 24);
  G32(D4A_TBL_446048) = (unsigned short)((unsigned int)G32(D4A_CTX_W0) >> 8);
  G32(D4A_TBL_446050) = (unsigned char)G32(D4A_CTX_W0) + (((unsigned int)G32(D4A_CTX_W0) >> 8) & 0xFF00);
  G32(D4A_TBL_446054) = v4 + ((unsigned short)G32(D4A_CTX_W0) >> 8);
  G32(D4A_TBL_446058) = (unsigned char)G32(D4A_CTX_W0) + v4;
  G32(D4A_TBL_44605C) = v2 + (unsigned short)G32(D4A_CTX_W1);
  if ( (unsigned char)G32(D4A_CTX_W0) == 255 )
  {
    G32(D4A_TBL_446048) += 0x10000;
    G32(D4A_TBL_44604C) += 0x10000;
  }
  v5 = M8(c->dict_buf + (G32(D4A_DICT_MASK) & (G32(D4A_DICT_POS) - G32(D4A_BEST_CNT))));
  v11 = M8(c->dict_buf + (G32(D4A_DICT_MASK) & (G32(D4A_DICT_POS) - 2 * G32(D4A_BEST_CNT))));
  if ( (unsigned char)G8(D4A_FLAG_45E395) == G32(D4A_BEST_CNT) )
  {
    if ( (unsigned char)G32(D4A_CTX_W0) == 1 )
      v3 = 8;
    v6 = (unsigned int)G32(D4A_CTX_W0) >> 16 == 63231;
  }
  else
  {
    v6 = G32(D4A_OUT_COUNT) % (unsigned int)G32(D4A_BEST_CNT);
  }
  G32(D4A_TBL_446060) = (unsigned char)(v5 - v11) + (v6 << 8);
  M8(M32(D4A_TBL_4236B8) + 4 * G32(D4A_TBL_446060) + 2) = 2 * v5 - v11;
  while ( (unsigned char)v3 < 8u )
  {
    v7 = 4 * M32(D4A_TBL_446044 + ((unsigned char)v3) * 4) + M32(D4A_TBL_42369C + ((unsigned char)v3) * 4);
    if ( M8(v7) && M8(v7 + 3) < 0x80u )
    {
      v8 = (M8(v7 + 1) << 8) * M8(v7 + 3) / M8(v7);
      if ( v8 > v10 )
      {
        v10 = v8;
        v0 = v3;
      }
    }
    ++v3;
  }
  if ( (unsigned char)v0 >= 8u
    || (result = 4 * M32(D4A_TBL_446044 + ((unsigned char)v0) * 4) + M32(D4A_TBL_42369C + ((unsigned char)v0) * 4),
        M8(result + 1) <= M8(result))
    && (M8(result + 3) & 0x7Fu) <= 1 )
  {
    if ( (unsigned int)G32(D4A_BEST_OFF) > 1 )
      M32(c->cost_cnt_b) >>= 1;
    result = 0;
  }
  else
  {
    if ( (unsigned int)G32(D4A_BEST_CNT) > 1 )
      G8(D4A_FLAG_45E398) = v0 == 7;
    if ( M8(result) == 2 && (unsigned char)(M8(result + 1) - 3) < 4u )
      c->pair_ptr = D4A_TBL_45AD64 + 2 * ((unsigned char)v0 + 8 * M8(result + 1) - 24);
    if ( M8(result) == 3 && (unsigned char)(M8(result + 1) - 4) < 4u )
      c->pair_ptr = D4A_TBL_45AD64 + 2 * ((unsigned char)v0 + 8 * M8(result + 1) - 32) + 64;
    G8(D4A_FLAG_45E396) = v0;
  }
#ifdef D4E885DBG
  if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI) {
    unsigned q5;
    fprintf(stderr, "E885 out=%u ret=%d v0=%d v10=%u d0f8=%u d0fc=%u c64c=%u e395=%u ae1c=%u v5=%u v11=%u a060=%u e39c=%u h0=%u h1=%u h2=%u h3=%u\n",
          G32(D4A_OUT_POS), 
        result, (int)v0, v10, G32(D4A_CTX_W0), G32(D4A_CTX_W1), G32(D4A_BEST_CNT), G8(D4A_FLAG_45E395),
        G32(D4A_OUT_COUNT), (unsigned)(unsigned char)v5, (unsigned)(unsigned char)v11, G32(D4A_TBL_446060),
        G8(D4A_HIST_LEN), (unsigned)M8(D4A_HIST_SYMS), (unsigned)M8(D4A_HIST_SYMS + 1),
        (unsigned)M8(D4A_HIST_SYMS + 2), (unsigned)M8(D4A_HIST_SYMS + 3));
    for (q5 = 0; q5 < 8; q5++) {
      int row = 4 * M32(D4A_TBL_446044 + q5 * 4) + M32(D4A_TBL_42369C + q5 * 4);
      fprintf(stderr, "E885ROW %u: b0=%u b1=%u b2=%u b3=%u\n", q5,
          (unsigned)M8(row), (unsigned)M8(row+1), (unsigned)M8(row+2), (unsigned)M8(row+3));
    }
  }
#endif
#ifdef D4E2DBG
  fprintf(stderr, "E2 out=%u ret=%d e396=%u e39c=%u\n",
      G32(D4A_OUT_POS), result, G8(D4A_FLAG_45E396), G8(D4A_HIST_LEN));
#endif
  return result;
}

static int d4_row_cost_update(dec4_ctx *c, char a1)
{
  unsigned char v1;   /* byte value: compared with M8() (unsigned) */
  unsigned char v2;
  int v3;
  signed int result;
  int v5;

  v1 = a1;
  v2 = 0;
  do
  {
    result = M32(D4A_TBL_42369C + (v2) * 4);
    v5 = result + 4 * M32(D4A_TBL_446044 + (v2) * 4);
    if ( M8(v5) )
    {
      if ( v2 == G8(D4A_FLAG_45E396) )
      {
        if ( G8(D4A_FLAG_45E399) )
        {
          if ( v1 == M8(v5 + 2) )
          {
            G8(D4A_FLAG_45E39F) = 0;
            v3 = M8(v5 + 1);
          }
          else
          {
            v3 = M8(v5);
          }
          result = (unsigned short)M16(D4A_COST_LUT + (((v3 << 14) / (M8(v5 + 1) + (unsigned int)M8(v5) ? M8(v5 + 1) + (unsigned int)M8(v5) : 1)) * 2));
          G32(D4A_COST_SUM) += result;
        }
        if ( c->pair_ptr )
        {
          result = c->pair_ptr;
          ++M8((v1 == M8(v5 + 2)) + result);
          if ( M8(result + 1) >= 0xFEu || M8(result) >= 0xFEu )
          {
            result = c->pair_ptr;
            ++M8(result);
            ++M8(result + 1);
            M8(result) >>= 1;
            M8(result + 1) >>= 1;
          }
        }
      }
      M8(v5 + 3) &= 0x7Fu;
      if ( v1 == M8(v5 + 2) )
      {
        if ( M8(v5 + 1) >= 0xFEu )
        {
          ++M8(v5);
          ++M8(v5 + 1);
          M8(v5) >>= 1;
          M8(v5 + 1) >>= 1;
        }
        ++M8(v5 + 1);
        if ( M8(v5 + 3) < 0x7Fu )
          ++M8(v5 + 3);
      }
      else
      {
        if ( M8(v5) >= 0xFEu )
        {
          ++M8(v5);
          ++M8(v5 + 1);
          M8(v5) >>= 1;
          M8(v5 + 1) >>= 1;
        }
        ++M8(v5);
        if ( M8(v5 + 3) >= 2u )
        {
          result = (signed int)M8(v5 + 3) >> 1;
          M8(v5 + 3) = result;
        }
        else
        {
          M8(v5 + 3) = 0;
          M8(v5 + 2) = v1;
        }
        M8(v5 + 3) += -128;
      }
    }
    else
    {
      M16(v5) = 514;
      M8(v5 + 3) = 0;
      M8(v5 + 2) = v1;
    }
    ++v2;
  }
  while ( v2 < 8u );
  return result;
}

static unsigned int d4_col_update(dec4_ctx *c, char a1)
{
  int v1;
  int v2;
  int v3;
  unsigned char v4;
  char v5;
  int v6;
  char v7;
  int v8;
  int v9;
  unsigned char v10;
  char v11;
  char v12;
  unsigned int v13;
  int v14;
  unsigned int v15;
  int v16;
  char v17;
  int result;
  char v19;

  v19 = a1;
  if ( (unsigned int)G32(D4A_COL_IDX) >= 0x40000 )
  {
    G32(D4A_COL_IDX) &= 0x7FFFu;
    v13 = 0;
    v14 = c->col_tab + 32 * G32(D4A_COL_IDX);
    v15 = 0;
    v16 = c->col_tab + 32 * G32(D4A_COL_IDX);
    while ( M8(v16) )
    {
      if ( (M8(v16 + 25) & 0x3F) < (M8(v14 + 32 * v13 + 25) & 0x3F)
                                         + (M8(v14 + 32 * v13 + 1) > M8(v16 + 1) + 1) )
        v13 = v15;
      v15 += 0x8000;
      v16 += 0x100000;
      if ( v15 >= 0x40000 )
        goto LABEL_54;
    }
    v13 = v15;
LABEL_54:
    if ( v15 < 0x38000 )
      M8(v14 + 32 * v15 + 0x100000) = 0;
    d4_fill8(c, 32 * v13 + v14, 0, 0x20u);
    /* asm 0x40F006: ecx += 32*edi, the freshly cleared bucket entry */
    v1 = 32 * v13 + v14;
    M8(v1) = 1;
    v17 = G8(D4A_FLAG_45E3A4);
    M8(v1 + 26) = 32;
    M8(v1 + 1) = v17 + 1;
    M8(v1 + 13) = v19;
    M32(v1 + 28) = G32(D4A_CTX_COPY);
  }
  else
  {
    v1 = c->col_tab + 32 * G32(D4A_COL_IDX);
    if ( G8(D4A_COL_SLOT) == 0xFF )
    {
      G8(D4A_COL_SLOT) = 0;
      do
      {
        /* asm 0x40ED0E compares bytes (unsigned); a1 is a signed char */
        if ( M8(v1 + (unsigned char)G8(D4A_COL_SLOT) + 13) == (unsigned char)a1 )
          break;
        ++G8(D4A_COL_SLOT);
      }
      while ( (unsigned char)G8(D4A_COL_SLOT) < 0xCu );
    }
    if ( G8(D4A_FLAG_45E394)
      && (unsigned char)G8(D4A_COL_SLOT) < 0xCu
      && (signed int)M8(v1 + (unsigned char)G8(D4A_COL_SLOT) + 1) >> 6 > M8(v1) )
    {
      G32(D4A_FREQ_A) += 4;
    }
    if ( G8(D4A_FLAG_45E397) )
    {
      if ( (unsigned char)G8(D4A_COL_SLOT) >= 0xCu )
      {
        v2 = M8(v1);
      }
      else
      {
        G8(D4A_FLAG_45E39A) = 0;
        G8(D4A_FLAG_45E399) = 0;
        G8(D4A_FLAG_45E39F) = 0;
        v2 = M8(v1 + (unsigned char)G8(D4A_COL_SLOT) + 1);
      }
      G32(D4A_COST_SUM) += (unsigned short)M16(D4A_COST_LUT + (((v2 << 14) / (unsigned int)G32(D4A_COL_TOT_A)) * 2));
    }
    v3 = (unsigned char)M8(D4A_FREQ_THRESH1 + (M8(v1))) < (unsigned int)G32(D4A_TBL_45C678);
    if ( (unsigned char)G8(D4A_COL_SLOT) < 0xCu )
    {
      v4 = M8(v1 + 25) & 0xC0;
      if ( M8(v1 + (unsigned char)G8(D4A_COL_SLOT) + 1) + 1 >= (signed int)M8(v1 + 1) >> 1 )
      {
        v5 = ((signed int)v4 >> 1) & 0xC0;
      }
      else
      {
        v5 = v4 + 64;
        if ( !v5 )
          v3 = 1;
      }
      M8(v1 + 25) = v5 | M8(v1 + 25) & 0x3F;
      if ( G8(D4A_COL_SLOT) )
      {
        if ( (signed int)M8(v1 + 26) >> 4 >= 4 && (M8(v1 + 26) & 0xF) == G8(D4A_COL_SLOT) )
          goto LABEL_60;
      }
    }
    if ( v3 )
    {
LABEL_60:
      v6 = v1 + 12;
      do
      {
        ++M8(--v6 + 1);
        M8(v6 + 1) >>= 1;
      }
      while ( v6 != v1 - 1 );
    }
    if ( (unsigned char)G8(D4A_COL_SLOT) >= 0xCu )
    {
      M8(v1) += 2;
      G8(D4A_COL_SLOT) = 1;
      M8(v1 + 27) >>= 1;
      do
      {
        if ( !M8(v1 + (unsigned char)G8(D4A_COL_SLOT) + 1) )
          break;
        ++G8(D4A_COL_SLOT);
      }
      while ( G8(D4A_COL_SLOT) != 11 );
      v8 = v1 + (unsigned char)G8(D4A_COL_SLOT);
      if ( M8(v8 + 1) < 6u )
      {
        M8(v8 + 1) = G8(D4A_FLAG_45E3A4) + 2;
        M8(v8 + 13) = v19;
        M8(v1 + 26) = G8(D4A_COL_SLOT);
      }
      else
      {
        G8(D4A_COL_SLOT) = 0;
      }
      M8(v1 + 25) = ((M8(v1 + 25) & 0x3F) >> 1) + (M8(v1 + 25) & 0xC0);
    }
    else
    {
      M8(v1 + (unsigned char)G8(D4A_COL_SLOT) + 1) += 2;
      if ( (M8(v1 + 25) & 0x3Fu) < 0x3F )
        ++M8(v1 + 25);
      if ( (M8(v1 + 26) & 0xF) == G8(D4A_COL_SLOT) )
      {
        if ( M8(v1 + 26) < 0xF0u )
          M8(v1 + 26) += 16;
        if ( M8(v1 + 27) < 0xF8u )
          M8(v1 + 27) += 8;
      }
      else
      {
        v7 = G8(D4A_COL_SLOT);
        M8(v1 + 27) >>= 1;
        M8(v1 + 26) = v7;
      }
      G32(D4A_FREQ_C) += 5;
    }
    while ( G8(D4A_COL_SLOT) )
    {
      v9 = v1 + (unsigned char)G8(D4A_COL_SLOT);
      v10 = M8(v9 + 1);
      if ( v10 < M8(v9) )
        break;
      v11 = M8(v9 + 12);
      v12 = M8(v9);
      M8(v9) = v10;
      M8(v9 + 1) = v12;
      M8(v9 + 12) = M8(v9 + 13);
      M8(v9 + 13) = v11;
      --G8(D4A_COL_SLOT);
      --M8(v1 + 26);
    }
    G32(D4A_FREQ_B) += 2;
    if ( (unsigned int)G32(D4A_FREQ_B) > 0x800 )
    {
      G32(D4A_FREQ_B) = (unsigned int)G32(D4A_FREQ_B) >> 1;
      G32(D4A_FREQ_C) = (unsigned int)G32(D4A_FREQ_C) >> 1;
      G32(D4A_FREQ_A) = (unsigned int)G32(D4A_FREQ_A) >> 1;
    }
  }
  result = c->ctx_byte_ptr;
  M8(c->ctx_byte_ptr) = M8(v1 + 26);
  return result;
}

static char d4_row_update(dec4_ctx *c, int a1)
{
  int v1;
  unsigned int v2;
  char v3;
  unsigned char v4;
  char v5;
  char v6;
  int v7;
  char v9;

  v9 = a1;
  v1 = c->row_tab + 20 * G32(D4A_ROW_CTX);
  if ( !M8(v1) )
  {
    d4_fill8(c, c->row_tab + 20 * G32(D4A_ROW_CTX), 0, 0x14u);
    /* asm 0x40F186: memset returns the destination (Hex-Rays lost it as v7);
     * the head of the new 20-byte context entry gets 0x0202, +18 = 4 and
     * +9 = the first candidate byte. */
    M16(v1) = 514;
    M8(v1 + 18) = 4;
    M8(v1 + 9) = a1;
    return a1;
  }
  G8(D4A_ROW_SLOT) = 0;
  do
  {
    if ( M8(v1 + (unsigned char)G8(D4A_ROW_SLOT) + 9) == (unsigned char)a1 )
      break;
    ++G8(D4A_ROW_SLOT);
  }
  while ( (unsigned char)G8(D4A_ROW_SLOT) < 8u );
  if ( M8(v1 + 18) > (unsigned char)M8(D4A_FREQ_THRESH2 + (M8(v1))) )
  {
    v2 = (v1 + 8);
    M8(v1 + 18) = 0;
    do
    {
      ++M8(v2);
      M8(v2) >>= 1;
      v3 = M8(v2);
      --v2;
      M8(v1 + 18) += v3;
    }
    while ( v2 != (v1 - 1) );
  }
  ++M8(v1 + 18);
  if ( (unsigned char)G8(D4A_ROW_SLOT) < 8u )
  {
    ++M8(v1 + (unsigned char)G8(D4A_ROW_SLOT) + 1);
    a1 = (a1 & ~0x000000FFU) | ((M8(v1 + 19) & 0xF) & 0x000000FFU);
    if ( (unsigned char)a1 == G8(D4A_ROW_SLOT) )
    {
      if ( M8(v1 + 19) < 0x40u )
        M8(v1 + 19) += 16;
      goto LABEL_19;
    }
    goto LABEL_18;
  }
  ++M8(v1 + 17);
  G8(D4A_ROW_SLOT) = 1;
  ++M8(v1);
  do
  {
    if ( !M8(v1 + (unsigned char)G8(D4A_ROW_SLOT) + 1) )
      break;
    ++G8(D4A_ROW_SLOT);
  }
  while ( G8(D4A_ROW_SLOT) != 7 );
  a1 = v1 + (unsigned char)G8(D4A_ROW_SLOT);
  if ( M8(a1 + 1) < 0xCu )
  {
    M8(v1 + 18) += 1 - M8(a1 + 1);
    M8(a1 + 1) = 1;
    M8(a1 + 9) = v9;
LABEL_18:
    a1 = (a1 & ~0x000000FFU) | ((G8(D4A_ROW_SLOT)) & 0x000000FFU);
    M8(v1 + 19) = G8(D4A_ROW_SLOT);
    goto LABEL_19;
  }
  G8(D4A_ROW_SLOT) = 0;
LABEL_19:
  while ( G8(D4A_ROW_SLOT) )
  {
    a1 = v1 + (unsigned char)G8(D4A_ROW_SLOT);
    v4 = M8(a1 + 1);
    if ( v4 < M8(a1) )
      break;
    v5 = M8(a1);
    M8(a1) = v4;
    v6 = M8(a1 + 8);
    M8(a1 + 8) = M8(a1 + 9);
    M8(a1 + 1) = v5;
    M8(a1 + 9) = v6;
    --G8(D4A_ROW_SLOT);
    --M8(v1 + 19);
  }
  return a1;
}

static char d4_row_update_cost(dec4_ctx *c, int a1)
{
  int v1;
  int v2;
  unsigned int v3;
  char v4;
  unsigned char v5;
  char v6;
  char v7;
  int v8;
  char v10;

  v10 = a1;
  v1 = c->row_tab + 20 * G32(D4A_ROW_CTX);
  if ( M8(v1) )
  {
    if ( G8(D4A_ROW_SLOT) == 0xFF )
    {
      G8(D4A_ROW_SLOT) = 0;
      do
      {
        if ( M8(v1 + (unsigned char)G8(D4A_ROW_SLOT) + 9) == (unsigned char)a1 )
          break;
        ++G8(D4A_ROW_SLOT);
      }
      while ( (unsigned char)G8(D4A_ROW_SLOT) < 8u );
    }
    if ( G8(D4A_FLAG_45E39A) )
    {
      if ( (unsigned char)G8(D4A_ROW_SLOT) >= 8u )
      {
        v2 = M8(v1);
      }
      else
      {
        G8(D4A_FLAG_45E399) = 0;
        G8(D4A_FLAG_45E39F) = 0;
        v2 = M8(v1 + (unsigned char)G8(D4A_ROW_SLOT) + 1);
      }
      G32(D4A_COST_SUM) += (unsigned short)M16(D4A_COST_LUT + (((v2 << 14) / (unsigned int)G32(D4A_COL_TOT_B)) * 2));
    }
    if ( M8(v1 + 18) > (unsigned char)M8(D4A_FREQ_THRESH2 + (M8(v1))) )
    {
      v3 = (v1 + 8);
      M8(v1 + 18) = 0;
      do
      {
        ++M8(v3);
        M8(v3) >>= 1;
        v4 = M8(v3);
      --v3;   /* asm 0x40F252: the pointer walks down */
        M8(v1 + 18) += v4;
      }
      while ( v3 != (v1 - 1) );
    }
    M8(v1 + 18) += 4;
    if ( (unsigned char)G8(D4A_ROW_SLOT) >= 8u )
    {
      G8(D4A_ROW_SLOT) = 1;
      ++M8(v1 + 17);
      ++G32(D4A_TBL_45C6AC);
      M8(v1) += 4;
      do
      {
        if ( !M8(v1 + (unsigned char)G8(D4A_ROW_SLOT) + 1) )
          break;
        ++G8(D4A_ROW_SLOT);
      }
      while ( G8(D4A_ROW_SLOT) != 7 );
      a1 = v1 + (unsigned char)G8(D4A_ROW_SLOT);
      if ( M8(a1 + 1) < 0xCu )
      {
        M8(v1 + 18) += 4 - M8(a1 + 1);
        M8(a1 + 1) = 4;
        M8(a1 + 9) = v10;
        a1 = (a1 & ~0x000000FFU) | ((G8(D4A_ROW_SLOT)) & 0x000000FFU);
        M8(v1 + 19) = G8(D4A_ROW_SLOT);
      }
      else
      {
        G8(D4A_ROW_SLOT) = 0;
      }
    }
    else
    {
      G32(D4A_TBL_45C6AC) = (unsigned int)(3 * G32(D4A_TBL_45C6AC)) >> 2;
      M8(v1 + 17) = (signed int)M8(v1 + 17) >> 1;
      M8(v1 + (unsigned char)G8(D4A_ROW_SLOT) + 1) += 4;
      a1 = (a1 & ~0x000000FFU) | ((M8(v1 + 19) & 0xF) & 0x000000FFU);
      if ( (unsigned char)a1 == G8(D4A_ROW_SLOT) )
      {
        if ( M8(v1 + 19) < 0xF0u )
          M8(v1 + 19) += 16;
      }
      else
      {
        a1 = (a1 & ~0x000000FFU) | ((G8(D4A_ROW_SLOT)) & 0x000000FFU);
        M8(v1 + 19) = G8(D4A_ROW_SLOT);
      }
      G32(D4A_FREQ_C) += 5;
    }
    while ( G8(D4A_ROW_SLOT) )
    {
      a1 = v1 + (unsigned char)G8(D4A_ROW_SLOT);
      v5 = M8(a1 + 1);
      if ( v5 < M8(a1) )
        break;
      v6 = M8(a1);
      M8(a1) = v5;
      v7 = M8(a1 + 8);
      M8(a1 + 8) = M8(a1 + 9);
      M8(a1 + 1) = v6;
      M8(a1 + 9) = v7;
      --G8(D4A_ROW_SLOT);
      --M8(v1 + 19);
    }
    G32(D4A_FREQ_B) += 2;
    if ( (unsigned int)G32(D4A_FREQ_B) > 0x800 )
    {
      G32(D4A_FREQ_B) = (unsigned int)G32(D4A_FREQ_B) >> 1;
      G32(D4A_FREQ_C) = (unsigned int)G32(D4A_FREQ_C) >> 1;
      G32(D4A_FREQ_A) = (unsigned int)G32(D4A_FREQ_A) >> 1;
    }
  }
  else
  {
    d4_fill8(c, c->row_tab + 20 * G32(D4A_ROW_CTX), 0, 0x14u);
    /* asm 0x40F386: the memset destination (v1) gets 0x0303 at +0, 6 at +18
     * and the decoded byte at +9 (Hex-Rays lost it as the unused v8). */
    M16(v1) = 771;
    M8(v1 + 18) = 6;
    M8(v1 + 9) = a1;
  }
  return a1;
}

static int d4_ctx_feed_byte(dec4_ctx *c, unsigned char a1, char a2)
{
  unsigned char v2;
  char v3;
  int v4;
  unsigned int v5;
  char v6;
  unsigned int v7;
  char v8;
  unsigned int v9;
  int v10;
  int v11;
  unsigned int v12 = 0;
  int v13;
  int v14;
  int v15;
  int v16;
  unsigned int v17;
  int v18;
  int v19;
  char v20;
  int v21;
  int result;

  v2 = a1;
  M8(c->hist_ptr_0) = a1;
  M8(c->hist_ptr_1) = a1;
  M8(c->hist_ptr_2) = a1;
  M8(c->hist_ptr_3) = a1;
  if ( a2 )
  {
    v3 = G32(D4A_OUT_COUNT) & 1;
    v4 = G32(D4A_OUT_COUNT) & 1;
    v5 = D4A_TBL_45DD6A + v4;
    v6 = 2 * M8(D4A_TBL_45DD6A + v4) & 0xF;
    v7 = D4A_TBL_45DD6C + v4;
    M8(v5) = v6;
    v8 = 2 * M8(v7) & 0xF;
    G8(D4A_FLAG_45E3A0) = 0;
    M8(v7) = v8;
    if ( v2 >= 0x80u )
    {
      /* asm 0x40F3F9 / 0x40F455 compare the byte with 0xff (v2 is unsigned) */
      if ( v2 == 0xFF )
      {
        ++M8(v7);
        if ( M8(D4A_TBL_45DD6A + ((v3 == 0))) == 15 && M8(v7) == 15 )
          G8(D4A_FLAG_45E3A0) = 2;
      }
      else if ( v2 <= 0xDFu || v2 >= 0xF2u && v2 <= 0xF6u )
      {
        ++M8(v5);
        if ( M8(D4A_TBL_45DD6C + ((v3 == 0))) == 15 && M8(v5) == 15 )
          G8(D4A_FLAG_45E3A0) = 1;
      }
    }
  }
  if ( v2 && v2 != 0xFF )
  {
    v9 = G32(D4A_DICT_MASK) & (G32(D4A_DICT_POS) - M32(D4A_TBL_4455AC + ((v2) * 4)));
    if ( v9 >= 0x100 )
    {
      v9 = G32(D4A_DICT_MASK) & (G32(D4A_DICT_POS) - M32(D4A_TBL_4455AC + (((unsigned char)(v2 - 1)) * 4)));
      if ( v9 >= 0x100 )
        v9 = (G32(D4A_DICT_POS) - M32(D4A_TBL_4455AC + (((unsigned char)(v2 + 1)) * 4))) & G32(D4A_DICT_MASK);
    }
    if ( v9 )
    {
      if ( v9 < 0x100 )
      {
        ++M32(D4A_TBL_4445A4 + ((v9) * 4));
        if ( v9 != G32(D4A_BEST_OFF) )
        {
          if ( v2 == M8(c->dict_buf + (G32(D4A_DICT_MASK) & (G32(D4A_DICT_POS) - G32(D4A_BEST_OFF)))) )
            ++M32(D4A_TBL_4445A4 + ((G32(D4A_BEST_OFF)) * 4));
          else
            G32(D4A_TBL_45C66C) += 2;
          if ( v9 == G32(D4A_TBL_45C670) )
          {
            if ( M32(D4A_TBL_4445A4 + ((v9) * 4)) >= (unsigned int)M32(D4A_TBL_4445A4 + ((G32(D4A_BEST_OFF)) * 4)) )
            {
              G32(D4A_TBL_45C670) = G32(D4A_BEST_OFF);
              G32(D4A_BEST_OFF) = v9;
              if ( v9 > 1 )
                G32(D4A_BEST_CNT) = v9;
            }
          }
          else
          {
            G32(D4A_TBL_45C670) = v9;
          }
        }
      }
    }
    if ( (unsigned int)++G32(D4A_TBL_45C66C) >= 0x28 )
    {
      v10 = 0;
      do
      {
        ++v10;
        M32(D4A_TBL_4445A0 + ((v10) * 4)) = (unsigned int)M32(D4A_TBL_4445A0 + ((v10) * 4)) >> 1;
      }
      while ( v10 != 256 );
      v11 = G32(D4A_BEST_OFF);
      G32(D4A_TBL_45C66C) = 0;
      ++M32(D4A_TBL_4445A4 + ((v11) * 4));
      if ( v11 == 1 && (unsigned int)G32(D4A_TBL_4445A8) < 4 )
        ++G32(D4A_TBL_4445A8);
    }
    M32(D4A_TBL_4455AC + ((v2) * 4)) = G32(D4A_DICT_POS);
  }
  if ( G8(D4A_PHASE) )
    M32(D4A_TBL_444DA4 + ((v2) * 4)) = G32(D4A_DICT_POS) + 64;
  if ( G8(D4A_MODEL_ID) )
  {
    v12 = (v12 & ~0x000000FFU) | ((v2 - G8(D4A_FLAG_45E38B)) & 0x000000FFU);
    if ( G8(D4A_PRED_FLAG) )
      v12 = (v12 & ~0x000000FFU) | ((-(char)v12) & 0x000000FFU);
    if ( (unsigned char)v12 >= 0x80u )
      G8(D4A_PRED_FLAG) = G8(D4A_PRED_FLAG) == 0;
    if ( G8(D4A_MODEL_ID) == 1 )
    {
      G32(D4A_TRI2_TOTAL_NEXT) = (unsigned char)M8(D4A_DELTA_LUT2 + ((unsigned char)v12))
                   + 16 * (unsigned char)M8(D4A_DELTA_LUT + (((unsigned char)G32(D4A_TRI2_TOTAL_NEXT) - 9) & 0xF))
                   + 521;
    }
    else
    {
      v13 = 16 * G32(D4A_MOD_CNT);
      if ( G8(D4A_MODEL_ID) == 2 )
      {
        v14 = v13 + (unsigned char)M8(D4A_DELTA_LUT2 + ((unsigned char)v12)) + 553;
      }
      else if ( G8(D4A_MODEL_ID) == 3 )
      {
        if ( G8(D4A_FLAG_45E389) )
        {
          v12 = v2 - ((unsigned int)G32(D4A_CTX_W0) >> 16);
          G8(D4A_PRED_FLAG) = G8(D4A_FLAG_45E38F);
          if ( G8(D4A_FLAG_45E38F) )
            v12 = (v12 & ~0x000000FFU) | ((-(char)v12) & 0x000000FFU);
          if ( (unsigned char)v12 >= 0x80u )
            G8(D4A_PRED_FLAG) = G8(D4A_PRED_FLAG) == 0;
        }
        v15 = (unsigned char)G8(D4A_FLAG_45E38A);
        if ( v2 == M8(D4A_TBL_45D160 + ((unsigned char)G8(D4A_FLAG_45E38A))) )
          ++M32(D4A_TBL_44316C + ((v15) * 4));
        else
          M32(D4A_TBL_44316C + ((v15) * 4)) = (unsigned int)M32(D4A_TBL_44316C + ((v15) * 4)) >> 1;
        v14 = (unsigned char)M8(D4A_DELTA_LUT3 + ((unsigned char)v12)) + 32 * G32(D4A_MOD_CNT) + 585;
      }
      else
      {
        if ( G8(D4A_MODEL_ID) != 4 )
        {
          d4_pred_learn(c, v2, v12);
          goto LABEL_61;
        }
        v14 = v13 + (unsigned char)M8(D4A_DELTA_LUT2 + ((unsigned char)v12)) + 681;
      }
      G32(D4A_TRI2_TOTAL_NEXT) = v14;
    }
  }
LABEL_61:
  ++G32(D4A_OUT_COUNT);
  ++G32(D4A_TBL_45C6B0);
  if ( G8(D4A_PHASE) || v2 < 4u || v2 > 6u )
  {
    G32(D4A_CTX_W2) = ((unsigned int)G32(D4A_CTX_W1) >> 24) + (G32(D4A_CTX_W2) << 8);
    G32(D4A_CTX_W1) = ((unsigned int)G32(D4A_CTX_W0) >> 24) + (G32(D4A_CTX_W1) << 8);
    G32(D4A_CTX_W0) = v2 + (G32(D4A_CTX_W0) << 8);
  }
  else if ( (unsigned char)G32(D4A_CTX_W0) != 0xDF )   /* asm 0x40F733: cmp byte [45D0F8],0xdf / je skip */
  {
    G32(D4A_CTX_W2) = ((unsigned int)G32(D4A_CTX_W1) >> 24) + (G32(D4A_CTX_W2) << 8);
    G32(D4A_CTX_W1) = ((unsigned int)G32(D4A_CTX_W0) >> 24) + (G32(D4A_CTX_W1) << 8);
    G32(D4A_CTX_W0) = (G32(D4A_CTX_W0) << 8) + 223;
  }
  if ( G8(D4A_MODEL_ID) )
  {
    v16 = G32(D4A_MOD_CNT);
    G32(D4A_MOD_CNT) = G32(D4A_OUT_COUNT) % (unsigned int)G32(D4A_TRI2_RANGE_NEXT);
    G8(D4A_FLAG_45E38B) = (unsigned int)G32(D4A_CTX_W0) >> G8(D4A_FLAG_45E38C);
    if ( G8(D4A_MODEL_ID) == 3 )
    {
      G8(D4A_FLAG_45E38A) = G32(D4A_OUT_COUNT) % 6u;
      v17 = G32(D4A_OUT_COUNT) % 6u;
      v18 = (unsigned int)M32(D4A_TBL_44316C + ((v17) * 4)) > 8;
      M8(D4A_TBL_45D160 + (v17)) = G8(D4A_FLAG_45E38B) + G32(D4A_CTX_W0) - BYTE3(G32(D4A_CTX_W0));
      v19 = v18 && (unsigned int)G32(D4A_TBL_45AE18) < 5;
      G8(D4A_FLAG_45E389) = v19;
      if ( v19 )
      {
        G8(D4A_FLAG_45E38F) = G8(D4A_PRED_FLAG);
        G32(D4A_TRI2_TOTAL_NEXT) = 32 * v16 + 601;
        v20 = M8(D4A_TBL_45D160 + ((unsigned char)G8(D4A_FLAG_45E38A)));
        G8(D4A_PRED_FLAG) = 0;
        G8(D4A_FLAG_45E38B) = v20;
      }
      if ( v18 )
        ++G32(D4A_TBL_45AE18);
      else
        G32(D4A_TBL_45AE18) = 0;
    }
    else if ( (unsigned char)G8(D4A_MODEL_ID) >= 5u )
    {
      d4_pred_next(c);
    }
  }
  c->hist_ptr_0 = c->byte_hist + (((unsigned int)G32(D4A_CTX_W0) >> 16) & 0xFF00) + ((unsigned int)G32(D4A_CTX_W1) >> 24);
  c->hist_ptr_2 = c->byte_hist + (((unsigned int)G32(D4A_CTX_W0) >> 8) & 0xFF00) + 0x20000 + (unsigned char)G32(D4A_CTX_W0);
  v21 = (unsigned char)G32(D4A_CTX_W1) + (((unsigned short)G32(D4A_CTX_W0) << 8) & 0xFF00) + 196608;
  result = v21 + c->byte_hist;
  c->hist_ptr_1 = c->byte_hist + (unsigned short)((unsigned int)G32(D4A_CTX_W0) >> 8) + 0x10000;
  c->hist_ptr_3 = v21 + c->byte_hist;
  return result;
}

static int d4_decode_main(dec4_ctx *c, void *a1, int a2)
{
  int result;

  d4_rc_init(c);
  d4_read_header(c);
  D4TRACE("HDR phase=%u 4228BE=%u 45D13C=%u 45D12C=%u total=%u prog=%u\n", G8(D4A_PHASE), G32(D4A_HDR_SIZE), G32(D4A_NEXT_BLK), G32(D4A_BLK_END), G32(D4A_OUT_TOTAL), c->file_table);
  do
  {
    if ( G32(D4A_OUT_POS) >= (unsigned int)G32(D4A_NEXT_BLK) )
      d4_block_reinit(c);
    if ( G32(D4A_OUT_POS) >= (unsigned int)G32(D4A_BLK_END) )
      d4_block_model_init(c);
    if ( !d4_try_match(c, a2) )
      d4_decode_ppm(c);
    if ( !(G32(D4A_OUT_POS) & 0xF) )
      d4_progress_tick(c);
    result = G32(D4A_OUT_POS);
  }
  while ( (unsigned int)G32(D4A_OUT_POS) < G32(D4A_OUT_TOTAL) && !G32(D4A_IO_ERR) );
  return result;
}

static char d4_try_match(dec4_ctx *c, int a1)
{
  unsigned int v1;
  unsigned int v2;
  unsigned int v3;
  char v4;
  unsigned int v5;
  int v6;
  unsigned char v7;   /* asm 0x40FB42: byte compare (unsigned); a signed char
                         breaks the backward match early on bytes >= 0x80 */
  int v8;
  signed int v9;
  int v10;
  int v11;
  int v12;
  int v13;
  int v14;
  char v15;
  unsigned int v16;
  int v17;
  char v18;
  int v19;
  unsigned int v20;
  unsigned int v21;
  int v22;
  unsigned int v23;
  unsigned int i;
  unsigned char v25;
  int v26;
  int v27;
  unsigned char v28;
  char v29;
  int v30;
  unsigned int v31;
  unsigned int v33;
  unsigned int v34;
  int v35;
  int v36;
  int v37;
  unsigned int v38;
  unsigned char v39;
  unsigned char v40;
  char v41;
  unsigned char v42;
  char v43;
  unsigned char v44;
  unsigned char v45;

  v38 = (G32(D4A_CTX_W0) ^ ((unsigned int)G32(D4A_CTX_W0) >> 13)) & 0x3FFFF;
  v36 = 8 * v38 + c->hash1;
  v1 = (8
                      * ((((G32(D4A_CTX_W1) ^ ((unsigned int)G32(D4A_CTX_W1) >> 15)) << 6) ^ 16
                                                                                     * (G32(D4A_CTX_W0) ^ ((unsigned int)G32(D4A_CTX_W0) >> 15)) ^ G32(D4A_CTX_W2) ^ ((unsigned int)G32(D4A_CTX_W2) >> 15)) & 0x3FFFF)
                      + 4
                      + c->hash2);
  v42 = 0;
  v41 = 0;
  v39 = 0;
  v43 = 2;
  v40 = M8(c->hash_byte + v38);
  c->ctx_byte_ptr = c->hash_byte + v38;
  v35 = G32(D4A_DICT_POS);
  if ( (unsigned int)G32(D4A_DICT_POS) < 0x40 )
    v35 = G32(D4A_DICT_SIZE) + G32(D4A_DICT_POS);
  while ( 1 )
  {
    v2 = M32(v1);
    --v43;
    if ( M32(v1) != -1 && (G32(D4A_DICT_MASK) & (v2 - G32(D4A_DICT_POS))) >= 0x140 )
    {
      if ( v2 < 0x40 )
        v2 += G32(D4A_DICT_SIZE);
      if ( M8(v35 + c->dict_buf - 12) == M8(c->dict_buf + v2 - 12) )
      {
        v3 = (c->dict_buf + v2 - 1);
        v4 = 64;
        v5 = (c->dict_buf + v35 - 1);
        while ( (unsigned char)v4 >= 8u )
        {
          if ( M8(v5) == M8(v3) )
          {
            --v4;
            if ( M8(v5 - 1) == M8(v3 - 1) )
            {
              --v4;
              if ( M8(v5 - 2) == M8(v3 - 2) )
              {
                --v4;
                if ( M8(v5 - 3) == M8(v3 - 3) )
                {
                  --v4;
                  if ( M8(v5 - 4) == M8(v3 - 4) )
                  {
                    --v4;
                    if ( M8(v5 - 5) == M8(v3 - 5) )
                    {
                      --v4;
                      if ( M8(v5 - 6) == M8(v3 - 6) )
                      {
                        --v4;
                        if ( M8(v5 - 7) == M8(v3 - 7) )
                        {
                          v5 -= 8;
                          v3 -= 8;
                          if ( --v4 )
                            continue;
                        }
                      }
                    }
                  }
                }
              }
            }
          }
          goto LABEL_23;
        }
        v6 = v3 - v5;
        v7 = M8(v5);
        do
        {
          if ( v7 != M8(v5 + (v6)) )
            break;
          v7 = M8(v5-- - 1);
          --v4;
        }
        while ( v4 );
LABEL_23:
        if ( (unsigned char)(64 - v4) >= v39 )
        {
          v39 = 64 - v4;
          a1 = M32(v1);
        }
      }
    }
    if ( !v43 )
      break;
    /* asm 0x40FB63: [edx] = [edx-4]; edx -= 4 (the h2 entry's two dwords) */
    M32(v1) = M32(v1 - 4);
    v1 -= 4;
  }
  v8 = G32(D4A_DICT_POS);
  M32(v1) = G32(D4A_DICT_POS);
  if ( G32(D4A_TBL_45D114) )
  {
    a1 = (v8 - G32(D4A_TBL_45D130)) & G32(D4A_DICT_MASK);
    v41 = 1;
    v42 = ((unsigned int)G32(D4A_TBL_45D114) > 2) + 80;
  }
  else
  {
    if ( (unsigned int)G32(D4A_TBL_45D134) >= 0x10 )
      v9 = 12;
    else
      v9 = 48;
    if ( v39 >= v9 )
    {
      v42 = 21;
      goto LABEL_46;
    }
  }
  if ( !v42 )
  {
    a1 = (G32(D4A_DICT_POS) - G32(D4A_TBL_45D130)) & G32(D4A_DICT_MASK);
    if ( (G32(D4A_DICT_MASK) & (unsigned int)(a1 - G32(D4A_DICT_POS))) < 0x104
      || ((v10 = (int)((G32(D4A_DICT_MASK) & (a1 - 4)) + c->dict_buf),
           v37 = ((unsigned)M8(v10) << 24) | ((unsigned)M8(v10 + 1) << 16)
               | ((unsigned)M8(v10 + 2) << 8) | (unsigned)M8(v10 + 3),
           (v37 & 0xFFFFU) != (G32(D4A_CTX_W0) & 0xFFFFU))
          && ((v37 & 0xFFFF00FFU) != (G32(D4A_CTX_W0) & 0xFFFF00FFU))
          && ((v37 & 0xFFFFFF00U) != (G32(D4A_CTX_W0) & 0xFFFFFF00U)) ) )
    {
      if ( v40 < 0x40u || M8(v38 + c->hash_head) )
      {
        if ( M32(v36 + 4) == G32(D4A_CTX_W0) )
        {
          a1 = M32(v36);
          v42 = M32(v36) != -1;
        }
        else if ( a1 != -1 )
        {
          v42 = 31;
        }
      }
    }
    else
    {
      v42 = 11;
      v41 = 1;
    }
  }
LABEL_46:
  M32(v36) = G32(D4A_DICT_POS);
  M32(v36 + 4) = G32(D4A_CTX_W0);
  if ( !v42 || (G32(D4A_DICT_MASK) & (unsigned int)(a1 - G32(D4A_DICT_POS))) < 0x100 )
    goto LABEL_88;
  if ( !G32(D4A_TBL_45D114) )
  {
    if ( M8(v38 + c->hash_head) )
      v15 = -1;
    else
      v15 = 39;
    v42 += v15 + G8(D4A_FLAG_45E3A2);
  }
  if ( G32(D4A_TBL_45D11C) && M8(G32(D4A_TBL_45D11C)) == M8(a1 + c->dict_buf) )
  {
    d4_cls_update(c, v42, 2u);
    goto LABEL_87;
  }
  if ( d4_cls_decode(c, v42) != 1 )
  {
    ++G32(D4A_TBL_45D134);
LABEL_87:
    M8(v38 + c->hash_head) = 0;
LABEL_88:
    G32(D4A_TBL_45D114) = 0;
    return 0;
  }
  v34 = 14;
  v16 = 4 * G32(D4A_FREQ_C);
  v17 = G32(D4A_DICT_MASK) & (G32(D4A_DICT_POS) - a1);
  if ( v41 )
  {
    v18 = M8(D4A_LEN_LUT_B + (v16 / G32(D4A_FREQ_B)));
  }
  else
  {
    if ( G32(D4A_FREQ_A) <= (unsigned int)G32(D4A_FREQ_B) )
    {
      v44 = M8(D4A_LEN_LUT_C + (v16 / G32(D4A_FREQ_B))) - G8(D4A_PHASE);
      if ( G8(D4A_FLAG_45E3A0) )
        v44 *= 2;
      goto LABEL_63;
    }
    v18 = M8(D4A_LEN_LUT_A + (4 * G32(D4A_FREQ_A) / (unsigned int)G32(D4A_FREQ_B)));
  }
  v44 = v18;
LABEL_63:
  if ( v41 )
    v19 = (G32(D4A_TBL_45D114) != 0) + 3 + ((unsigned int)G32(D4A_TBL_45D114) > 2);
  else
    v19 = (M8(v38 + c->hash_head) > 0xEu) + (M8(v38 + c->hash_head) > 0x3Eu);
  v33 = (D4A_TBL_446464) + 1024 * v19;
  do
  {
    v45 = v34 + 1;
    if ( v44 < (unsigned char)(v34 + 1) )
    {
      G32(D4A_TRI_TOTAL) = 4096;
      G32(D4A_RC_RANGE) = (unsigned)d4_div(G32(D4A_RC_RANGE), 0x1000u);
      v20 = v33 + (4 * v34);
      if ( (unsigned short)((unsigned)d4_div(G32(D4A_RC_CODE), G32(D4A_RC_RANGE))) >= (unsigned int)M32(v20) )
      {
        G32(D4A_TRI_LOW) = M32(v20);
        G32(D4A_TRI_HIGH) = 4096;
        M32(v20) -= (unsigned int)G32(D4A_TRI_LOW) >> 4;
      }
      else
      {
        v45 = v34;
        G32(D4A_TRI_HIGH) = M32(v20);
        v21 = (unsigned int)(4096 - M32(v20)) >> 4;
        G32(D4A_TRI_LOW) = 0;
        M32(v20) += v21;
      }
      v22 = M32(D4A_TRI_TOTAL + (1) * 4);
      v23 = G32(D4A_RC_RANGE) * (M32(D4A_TRI_TOTAL + (2) * 4) - v22);
      i = (unsigned long)(G32(D4A_RC_CODE) - G32(D4A_RC_RANGE) * v22);
  d4_rcrenorm(c, &i, &v23);
      G32(D4A_RC_CODE) = (int)i;
      G32(D4A_RC_RANGE) = v23;
    }
    v34 = *(&M8(D4A_LEN_NEXT + (2 * v34)) + (v45 > v34));
  }
  while ( v34 );
  if ( v17 == G32(D4A_TBL_45D130) )
  {
    G32(D4A_TBL_45D134) = (unsigned int)G32(D4A_TBL_45D134) >> 1;
  }
  else
  {
    G32(D4A_TBL_45D130) = v17;
    G32(D4A_TBL_45D134) += v45;
  }
  M8(v38 + c->hash_head) = v45;
  if ( v45 == 0xFF )   /* asm 0x40FF2E: cmp dl,0xff (v45 is unsigned char) */
  {
    G32(D4A_TBL_45D11C) = 0;
    ++G32(D4A_TBL_45D114);
  }
  else
  {
    G32(D4A_TBL_45D114) = 0;
    G32(D4A_TBL_45D11C) = (G32(D4A_DICT_MASK) & (a1 + v45)) + c->dict_buf;
  }
#ifdef D4MATCHDBG
  if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
    fprintf(stderr, "MATCH out=%u a1=%u len=%u\n",
        G32(D4A_OUT_POS), (unsigned)a1, (unsigned)(unsigned char)v45);
#endif
  do
  {
    v28 = M8(c->dict_buf + a1);
    d4_emit_byte(c, M8(c->dict_buf + a1));
    d4_ctx_feed_byte(c, v28, v45 <= 8u);
    v30 = c->dict_buf;
    v31 = G32(D4A_DICT_POS);
    /* asm 0x40FF97/0x40FFA7: both writes store the byte just copied (cl),
     * which Hex-Rays left as the never assigned v29. */
    M8(c->dict_buf + G32(D4A_DICT_POS)) = v28;
    if ( v31 < 0x100 )
      M8(v30 + G32(D4A_DICT_SIZE) + v31) = v28;
    a1 = G32(D4A_DICT_MASK) & (a1 + 1);
    G32(D4A_DICT_POS) = G32(D4A_DICT_MASK) & (G32(D4A_DICT_POS) + 1);
    --v45;
  }
  while ( v45 );
  return 1;
}

static int d4_emit_byte(dec4_ctx *c, char a1)
{
  char v1;
  char v2;
  int result;
  char v4;
  int v5;
  unsigned int v6;
  char v7;

  v1 = ~a1;
#ifdef D4QDBG
  if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI) {
    unsigned r6 = 4 * M32(D4A_TBL_446044 + 6 * 4) + M32(D4A_TBL_42369C + 6 * 4);
    fprintf(stderr, "Q|out=%u|c640=%u|c654=%u|c648=%u|e39c=%u|r6=%u %u %u %u|h=%u %u %u %u\n",
        G32(D4A_OUT_POS), M32(D4A_COST_ACCUM), G32(D4A_COST_GATE), G32(D4A_COST_TMP), G8(D4A_HIST_LEN),
        (unsigned)M8(r6), (unsigned)M8(r6+1), (unsigned)M8(r6+2), (unsigned)M8(r6+3),
        (unsigned)M8(D4A_HIST_SYMS), (unsigned)M8(D4A_HIST_SYMS + 1),
        (unsigned)M8(D4A_HIST_SYMS + 2), (unsigned)M8(D4A_HIST_SYMS + 3));
  }
#endif
#ifdef D4FFFDDBG
  if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
    fprintf(stderr, "CR out=%u code=%u range=%u\n",
        G32(D4A_OUT_POS), G32(D4A_RC_CODE), G32(D4A_RC_RANGE));
#endif
#ifdef D4TRIDBG
  fprintf(stderr, "TRI out=%u c67c=%u c680=%u c684=%u\n",
      G32(D4A_OUT_POS), G32(D4A_TRI_TOTAL), G32(D4A_TRI_LOW), G32(D4A_TRI_HIGH));
#endif
#ifdef D4COSTDBG
  fprintf(stderr, "COST out=%u c640=%u c654=%u c648=%u\n",
      G32(D4A_OUT_POS), M32(D4A_COST_ACCUM), G32(D4A_COST_GATE), G32(D4A_COST_TMP));
#endif
#ifdef D4MDLDBG
  {
    unsigned mv2 = (unsigned)(c->row_tab + 20 * G32(D4A_ROW_CTX));
    fprintf(stderr, "MDL out=%u c69c=%u m0=%u m2=%u m9=%u m17=%u m18=%u\n",
        G32(D4A_OUT_POS), G32(D4A_ROW_CTX),
        (unsigned)M8(mv2), (unsigned)M8(mv2 + 2), (unsigned)M8(mv2 + 9),
        (unsigned)M8(mv2 + 17), (unsigned)M8(mv2 + 18));
  }
#endif
#ifdef D4W16DBG
  fprintf(stderr, "W16 out=%u w=%u\n", G32(D4A_OUT_POS),
      (unsigned)M16(c->fenwick2 + 781 * 512 + 144 * 2));
#endif
#ifdef D4CTXDBG
  fprintf(stderr, "CTX out=%u d0f8=%u d0fc=%u e3a0=%u c6a4=%u c69c=%u\n",
      G32(D4A_OUT_POS), G32(D4A_CTX_W0), G32(D4A_CTX_W1), G8(D4A_FLAG_45E3A0),
      G32(D4A_CTX_COPY), G32(D4A_ROW_CTX));
#endif
#ifdef D4FFFDDBG
  if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
    fprintf(stderr, "FFFD out=%u a1=%d v1u=%u v4=%u e39b=%u\n",
        G32(D4A_OUT_POS), (int)a1, (unsigned)(unsigned char)v1,
        (unsigned)(unsigned char)M8(D4A_DECODE_MAP + ((unsigned char)v1)), G8(D4A_ESCAPE_FLAG));
#endif
  if ( G8(D4A_ESCAPE_FLAG) == 1 )
  {
    G8(D4A_ESCAPE_FLAG) = 0;
    if ( G32(D4A_OUT_POS) < (unsigned int)G32(D4A_TBL_45D124) )
    {
      /* asm 0x410068: movzx edx,dl - the pending byte is a table index
       * (unsigned); a signed char v1 walks backwards into 42711F. */
      v2 = *(&M8(D4A_PENDING_TAB + (11 * ((unsigned char)G8(D4A_FLAG_45E39E) - 166))) + (unsigned char)v1);
      return d4_put_byte(c, v2);
    }
    if ( v1 == -2 )
    {
      v2 = -2;
      return d4_put_byte(c, v2);
    }
    d4_put_byte(c, 0);
    d4_put_byte(c, 0);
    d4_put_byte(c, 3);
    d4_put_byte(c, 3);
LABEL_7:
    v2 = v1;
    return d4_put_byte(c, v2);
  }
  result = G32(D4A_OUT_POS);
  if ( G32(D4A_OUT_POS) < (unsigned int)G32(D4A_TBL_45D124) || v1 != -2 )
  {
    result = G32(D4A_OUT_POS);
    if ( G32(D4A_OUT_POS) >= (unsigned int)G32(D4A_HDR_SIZE) || (unsigned char)v1 < 0x80u && !G8(D4A_ESCAPE_FLAG) )
      goto LABEL_7;
    v4 = M8(D4A_DECODE_MAP + ((unsigned char)v1));
    if ( (unsigned char)v4 >= 0xC8u )
    {
      result = d4_put_byte(c, v1 - G8(D4A_ESCAPE_FLAG));
      G8(D4A_ESCAPE_FLAG) = 0;
    }
    else if ( v4 == 100 )
    {
      G8(D4A_ESCAPE_FLAG) = 1;
      G8(D4A_FLAG_45E39E) = v1;
    }
    else if ( v4 == 99 )
    {
      G8(D4A_ESCAPE_FLAG) = 32;
    }
    else
    {
      v5 = (unsigned char)v4;
      v6 = M32(D4A_SDI_PTR + ((unsigned char)v4) * 4);
      v7 = M8(D4A_SDI_LEN + (v5));
      if ( G8(D4A_ESCAPE_FLAG) )
      {
        --v7;
        d4_put_byte(c, ~M8(v6) - 32);
        ++v6;
        G8(D4A_ESCAPE_FLAG) = 0;
      }
      do
      {
        result = d4_put_byte(c, ~M8(v6));
        ++v6;
        --v7;
      }
      while ( v7 );
    }
  }
  else
  {
    G8(D4A_ESCAPE_FLAG) = 1;
  }
  return result;
}

static unsigned char d4_cls_decode(dec4_ctx *c, unsigned char a1)
{
  unsigned int v1;
  int v2;
  unsigned int v3;
  unsigned int i;
  unsigned char v5;
  int v6;
  int v7;
  unsigned int v8;
  int v9;
  int v10;
  unsigned int v11;
  unsigned int j;
  unsigned char v13;
  int v14;
  int v15;
  int v16;
  char v17;
  int v18;
  unsigned char v20;
  unsigned char v21;
  int guard = 0;

  while ( 1 )
  {
    if (++guard > 8) { G32(D4A_IO_ERR) = 3; return 0; }
    v21 = a1;
    v1 = (D4A_TBL_445DB4) + 8 * a1 + 6;
    v20 = 3;
    G32(D4A_TRI_TOTAL) = M16(v1);
    G32(D4A_RC_RANGE) = (unsigned)d4_div(G32(D4A_RC_RANGE), (unsigned int)G32(D4A_TRI_TOTAL));
    do
    {
      v1 -= 2;
      --v20;
    }
    while ( (unsigned short)((unsigned)d4_div(G32(D4A_RC_CODE), G32(D4A_RC_RANGE))) < M16(v1) && v20 );
    G32(D4A_TRI_LOW) = M16(v1);
    G32(D4A_TRI_HIGH) = M16(v1 + 2);
    v2 = M32(D4A_TRI_TOTAL + (1) * 4);
    v3 = G32(D4A_RC_RANGE) * (M32(D4A_TRI_TOTAL + (2) * 4) - v2);
    i = (unsigned long)(G32(D4A_RC_CODE) - G32(D4A_RC_RANGE) * v2);
  d4_rcrenorm(c, &i, &v3);
    G32(D4A_RC_CODE) = (int)i;
    G32(D4A_RC_RANGE) = v3;
    if ( v20 )
      break;
    G32(D4A_TRI_TOTAL) = 10;
    G32(D4A_RC_RANGE) = (unsigned)d4_div(G32(D4A_RC_RANGE), 0xAu);
    v8 = (unsigned)d4_div(G32(D4A_RC_CODE), G32(D4A_RC_RANGE));
    v9 = (unsigned char)((unsigned)d4_div(G32(D4A_RC_CODE), G32(D4A_RC_RANGE)));
    G32(D4A_TRI_LOW) = (unsigned char)((unsigned)d4_div(G32(D4A_RC_CODE), G32(D4A_RC_RANGE)));
    G32(D4A_TRI_HIGH) = v9 + 1;
    v10 = M32(D4A_TRI_TOTAL + (1) * 4);
    v11 = G32(D4A_RC_RANGE) * (M32(D4A_TRI_TOTAL + (2) * 4) - v10);
    j = (unsigned long)(G32(D4A_RC_CODE) - G32(D4A_RC_RANGE) * v10);
  d4_rcrenorm(c, &j, &v11);
    G32(D4A_RC_CODE) = (int)j;
    G32(D4A_RC_RANGE) = v11;
    if ( (unsigned char)v8 != G8(D4A_MODEL_ID) )
    {
      G8(D4A_MODEL_ID) = v8;
      v16 = M32(D4A_TRI2_RANGE + ((v9) * 4));
      G32(D4A_TRI2_TOTAL_NEXT) = M32(D4A_TRI2_TOTAL + ((v9) * 4));
      v17 = M8(D4A_TRI2_IDX + (v9));
      G32(D4A_TRI2_RANGE_NEXT) = v16;
      G8(D4A_FLAG_45E38C) = v17;
      v18 = (unsigned char)v8 == 5 || (unsigned char)v8 == 6;
      G8(D4A_PRED_FLAG) = 0;
      G8(D4A_FLAG_45E38B) = 0;
      G32(D4A_MOD_CNT) = 0;
      G8(D4A_FLAG_45E38D) = v18;
      if ( (unsigned char)G8(D4A_MODEL_ID) > 4u )
        d4_pred_cache_reset(c);
    }
    a1 = v21;
  }
  d4_cls_update(c, v21, v20);
  return v20;
}

/* -O2 miscompiles this function without zero-initialised autos (D3X file-1
 * byte 70, 06_mx random.bin): some Hex-Rays locals are read before they are
 * written on rarely-taken paths, and GCC's -O2 exploits the garbage. Zeroing
 * is proven safe here (06_mx fully exact, full suite green); the alternative
 * would be auditing every local against the asm. Same for d4_ppm_decode_gated and
 * d4_ppm_decode_sym below. */
#pragma GCC push_options
#if defined(__GNUC__) && __GNUC__ >= 12
#pragma GCC optimize ("-ftrivial-auto-var-init=zero")
#endif
static int d4_decode_ppm(dec4_ctx *c)
{
  signed int v0;
  signed int v1;
  int v2;
  char v3;
  unsigned int v4;
  unsigned int v5;
  int v6;
  int v7;
  int v8;
  int v9;
  int v10;
  int v11;
  int v12;
  unsigned int v13;
  int v14;
  int v15;
  signed int v16;
  unsigned char v17;
  int v18;
  unsigned int v19;
  int v20;
  int v21;
  unsigned int v22;
  unsigned int v23;
  signed int v24;
  unsigned int v25;
  int v26;
  unsigned char v27;
  int v28;
  char v29;
  char v30;
  int v31;
  unsigned int v32;
  unsigned int j;
  unsigned char v34;
  int v35;
  int v36;
  unsigned int v37;
  unsigned int v38;
  unsigned int v39;
  int v40;
  int v41;
  unsigned int v42;
  unsigned int v43;
  unsigned int v44;
  unsigned int v45;
  unsigned int v46;
  unsigned int v47;
  unsigned int v48;
  unsigned int v49;
  unsigned int v50;
  unsigned int v51;
  unsigned int v52;
  unsigned int v53;
  int v54;
  unsigned char v55;
  char v56;
  int v57;
  unsigned int v58;
  unsigned int i;
  unsigned char v60;
  int v61;
  int v62;
  int v63;
  int v64;
  int v65;
  unsigned int v66;
  int v67;
  unsigned char v68;   /* byte values: compared with M8() (unsigned) */
  unsigned char v69;
  unsigned int v70;
  unsigned int v71;
  unsigned int v72;
  signed int v73;
  unsigned int v74;
  unsigned char v75;
  int v76;
  char v77;
  char v78;
  int v79;
  unsigned int v80;
  unsigned int k;
  unsigned char v82;
  int v83;
  int v84;
  unsigned int v85;
  int v86;
  unsigned int v87;
  signed int v88;
  int v89;
  unsigned int v90;
  unsigned int v91;
  unsigned int v92;
  unsigned int v93;
  unsigned int v94;
  unsigned int v95;
  unsigned int v96;
  unsigned int v97;
  int v98;
  unsigned char v99;
  char v100;
  int v101;
  unsigned int v102;
  unsigned int l;
  unsigned char v104;
  int v105;
  int v106;
  char v107;
  unsigned int v108;
  unsigned char v109;   /* asm 0x4119BB: byte compare (unsigned); a signed
                           char misses history bytes >= 0x80 */
  int v110;
  int v111;
  unsigned char v112;
  int v113;
  int v114;
  unsigned int v115;
  unsigned int m;
  unsigned char v117;
  int v118;
  int v119;
  unsigned int v120;
  int v122;
  unsigned int v123;
  unsigned char v124;
  unsigned int v125;
  unsigned char v126;
  unsigned int v127;
  unsigned char v128;
  unsigned int v129;
  unsigned char v130;
  unsigned int v131;
  unsigned char v132;
  int v133;
  unsigned int v134;
  unsigned char v135;
  unsigned int v136;
  unsigned char v137;
  unsigned int v138;
  int v139;
  unsigned char v140;
  int v141;
  int v142;
  char v143;
  int v144;
  unsigned int v145;
  unsigned char v146;
  int result;
  /* asm keeps these as contiguous byte arrays on the stack: 0x40CDA1
   * (13 bytes) and 0x40CDAE (9 bytes).  They were modelled as separate
   * int/char locals, which GCC is free not to place adjacently, so the
   * byte view read other locals (observed: mask bytes 4..7 were
   * 90 01 db 00 instead of 01 01 01 01).  Use real arrays. */
  /* asm: 13- and 9-byte stack masks, but indexed by 4-bit nibbles
   * (v177/v86: 0..15).  The original reads up to index 15, i.e. 3/7 bytes
   * past the end - zero stack padding on fresh pages.  Keep 16 bytes,
   * zero-filled, so those reads are defined and stay 0. */
  unsigned char vmask13[16];
  unsigned char vmask9[16];
  unsigned int mi;
  unsigned int v155;
  int v156;
  unsigned int v157;
  int v158;
  unsigned int v159;
  int v160;
  int v161;
  int v162;
  int v163;
  unsigned int v164;
  unsigned int v165;
  unsigned int v166;
  unsigned int v167;
  UHI64 v168 = 0;
  int v169;
  unsigned int v170;
  unsigned int v171;
  unsigned int v172;
  unsigned int v173;
  unsigned int v174;
  unsigned int v175;
  int v176;
  int v177;
  unsigned int v178;
  int v179;
  unsigned int v180;
  unsigned int v181;
  int v182;
  unsigned int v183;
  int v184;
  unsigned int v185;
  int v186;
  unsigned int v187;
  unsigned int v188;
  int v189;
  unsigned int v190;
  unsigned int v191;
  unsigned int v192;
  unsigned int v193;
  int v194;
  unsigned int v195;
  unsigned int v196;
  unsigned int v197;
  unsigned int v198 = 0;
  unsigned int v199;
  unsigned int v200;
  unsigned int v201 = 0;
  int v202 = 0;
  unsigned int v203;
  char v204;
  unsigned char v205;
  int v206;
  int v207;
  char v208;
  char v209;
  unsigned char v210;
  char v211;
  unsigned char v212;
  char v213;
  char v214;
  unsigned char v215 = 0;
  int v216;
  char v217;
  unsigned char v218 = 0;

  v208 = 1;
  v211 = 1;
  v209 = G8(D4A_PHASE);
  if ( G8(D4A_FLAG_45E3A0) == 2 )
    G32(D4A_CTX_COPY) = (G32(D4A_CTX_W1) & 0xFF000000)
                 + ((G32(D4A_CTX_W1) << 8) & 0xFF0000)
                 + (((unsigned int)G32(D4A_CTX_W0) >> 16) & 0xFF00)
                 + ((unsigned short)G32(D4A_CTX_W0) >> 8);
  else
    G32(D4A_CTX_COPY) = G32(D4A_CTX_W0);
  if ( (G32(D4A_CTX_COPY) & 0xC0) == 192 )
    v0 = 16;
  else
    v0 = 0;
  v163 = v0;
  G32(D4A_COL_IDX) = ((unsigned short)G32(D4A_CTX_COPY) ^ (unsigned short)((unsigned int)G32(D4A_CTX_COPY) >> 13)) & 0x7FFF;
  if ( G8(D4A_PHASE) || !M8(D4A_HASH_ENABLE + ((unsigned char)((unsigned int)G32(D4A_CTX_COPY) >> 16))) )
    v1 = 0;
  else
    v1 = 0x10000;
  G32(D4A_ROW_CTX) = v1 + (unsigned short)G32(D4A_CTX_COPY);
  G8(D4A_FLAG_45E394) = 0;
  G8(D4A_FLAG_45E397) = 0;
  G8(D4A_FLAG_45E39A) = 0;
  G8(D4A_FLAG_45E399) = 0;
  G8(D4A_FLAG_45E39F) = 0;
  G8(D4A_FLAG_45E39D) = 0;
  G8(D4A_FLAG_45E393) = 0;
  G8(D4A_FLAG_45E398) = 0;
  G8(D4A_HIST_LEN) = 0;
  G32(D4A_COST_SUM) = 0;
  G32(D4A_TBL_45C668) = 0;
  G8(D4A_FLAG_45E3A1) = 0;
  G8(D4A_FLAG_45E3A4) = 0;
  v2 = c->row_tab + 20 * G32(D4A_ROW_CTX);
  if ( G32(D4A_TBL_45D11C) )
  {
    G8(D4A_HIST_LEN) = 1;
    v3 = M8(G32(D4A_TBL_45D11C));
    G32(D4A_TBL_45D11C) = 0;
    M8(D4A_HIST_SYMS + (0)) = v3;
  }
  if ( (unsigned int)G32(D4A_BEST_OFF) > 1 || (unsigned char)(G32(D4A_BEST_CNT) - 2) <= 2u )
  {
    v4 = G32(D4A_BEST_CNT);
    v5 = G32(D4A_BEST_CNT);
    if ( (unsigned int)G32(D4A_BEST_CNT) <= 4 )
      v5 = G32(D4A_OUT_COUNT) % (unsigned int)G32(D4A_BEST_CNT) + M32(D4A_CNT_OFF_5 + ((G32(D4A_BEST_CNT)) * 4));
    if ( (unsigned int)G32(D4A_BEST_CNT) <= 0x10 )
      v4 = G32(D4A_OUT_COUNT) % (unsigned int)G32(D4A_BEST_CNT) + M32(D4A_CNT_OFF_17 + ((G32(D4A_BEST_CNT)) * 4));
    c->cost_cnt_b = D4A_TBL_45CCD4 + 4 * v5;
    c->cost_cnt_a = D4A_TBL_45C6B8 + 4 * v4;
  }
  if ( v209 )
  {
    if ( G8(D4A_FLAG_45E38D) )
      v6 = 31 * M32(D4A_TBL_445DAC + (0) * 4) < (unsigned int)(32 * G32(D4A_TBL_445DB0));
    else
      v6 = M32(D4A_TBL_445DAC + ((0) * 4)) < (unsigned int)G32(D4A_TBL_445DB0);
    v209 = v6;
  }
  v7 = G8(D4A_MODEL_ID) && ((unsigned int)G32(D4A_BEST_OFF) <= 4 || M32(c->cost_cnt_b) <= 0x3FEu);
  v216 = v7;
  v8 = v209 && v7;
  v207 = v8;
  /* asm 0x41055A..0x410571: v206 = v207 && (M32(0x45C640) < G32(0x45C654)).
   * (The previous form `v8 &(0x45C640) < X` was a precedence trap: it is
   * `v8 & (0x45C640 < X)`, i.e. always v8, which made the cost gate
   * always true.) */
  v9 = v8 && (M32(D4A_COST_ACCUM) < (unsigned int)G32(D4A_COST_GATE));
  v206 = v9;
#ifdef D4GATEDBG
  if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
    fprintf(stderr, "GATE out=%u v209=%d v7=%d v8=%d v206=%d c640=%u c654=%u c674=%u dac0=%u db0=%u e38d=%u e390=%u\n",
          G32(D4A_OUT_POS), 
        v209, v7, v8, v9, M32(D4A_COST_ACCUM), G32(D4A_COST_GATE), G32(D4A_BEST_OFF),
        M32(D4A_TBL_445DAC), G32(D4A_TBL_445DB0), G8(D4A_FLAG_45E38D), G8(D4A_MODEL_ID));
#endif
  v159 = (unsigned char)G32(D4A_CTX_COPY);
  if ( !G8(D4A_PHASE) && M8(D4A_HASH_ENABLE + (BYTE1(G32(D4A_CTX_COPY)))) )
    v159 += 256;
  v161 = M32(D4A_TBL_42750D + (((signed int)M8(v2 + 19) >> 4) * 4));
  v162 = M32(D4A_TBL_4274CD + (((unsigned char)M8(D4A_RUN_CNT + (v159))) * 4));
  v210 = M8(v2 + (M8(v2 + 19) & 0xF) + 9);
  v10 = M8(c->hist_ptr_0);
  v205 = M8(D4A_TBL_45E086 + (v159));
  v11 = G32(D4A_OUT_COUNT);
  M32(D4A_LAST_SEEN + ((v10) * 4)) = G32(D4A_OUT_COUNT);
  M32(D4A_LAST_SEEN + ((M8(c->hist_ptr_1)) * 4)) = v11;
  M32(D4A_LAST_SEEN + ((M8(c->hist_ptr_2)) * 4)) = v11;
  M32(D4A_LAST_SEEN + ((M8(c->hist_ptr_3)) * 4)) = v11;
  v160 = 0;
  v12 = 32 * G32(D4A_COL_IDX);
  while ( 1 )
  {
    v158 = v12 + c->col_tab;
    if ( !M8(v12 + c->col_tab) )
    {
      G32(D4A_COL_IDX) += 0x40000;   /* HIWORD += 4 */
      goto LABEL_201;
    }
    if ( M32(v12 + c->col_tab + 28) == G32(D4A_CTX_COPY) )
      break;
    G32(D4A_COL_IDX) += 0x8000;
    v12 += 0x100000;
    if ( (unsigned int)G32(D4A_COL_IDX) >= 0x40000 )
      goto LABEL_201;
  }
  v166 = 12;
  v167 = 12;
  G8(D4A_COL_SLOT) = -1;
  v13 = 0;
  v168 = (v168 & ~U64LL(0x00000000FFFFFFFF)) | (((unsigned UHI64)(0)) & U64LL(0x00000000FFFFFFFF));
  v14 = M8(v158);
  v168 = (v168 & ~U64LL(0x00000000FFFFFFFF)) | (((unsigned UHI64)(M8(v158 + 1))) & U64LL(0x00000000FFFFFFFF));
  v15 = v158;
  do
  {
    if ( M8(v15 + 13) == v205 )
      v166 = v13;
    if ( M8(v15 + 13) == v210 )
      v167 = v13;
    v14 += v168;
    ++v13;
    ++v15;
    if ( v13 >= 0xC )
      break;
    v168 = (v168 & ~0x000000FFU) | ((M8(v15 + 1)) & 0x000000FFU);
  }
  while ( (unsigned char)v168 );
  v16 = M8(v158 + 26);
  v160 = M32(D4A_TBL_42754D + ((v16 >> 4) * 4));
  v17 = M8(v158 + (M8(v158 + 26) & 0xF) + 13);
  G32(D4A_TBL_45C678) = v14;
  v215 = v17;
  if ( (G32(D4A_TBL_45C6A0)
     || (unsigned int)G32(D4A_BEST_OFF) <= 1
     || G32(D4A_FREQ_A) > (unsigned int)G32(D4A_FREQ_B)
     || M32(c->cost_cnt_b) <= 0x3FEu
     || ((signed int)(v16 - (__CFSHL__((unsigned UHI64)v16 >> 32, 4) + 16 * ((unsigned UHI64)v16 >> 32))) >> 4)
      * ((unsigned int)M8(v158 + 27) << 6) >= M32(c->cost_cnt_a))
    && (!M8(v2)
     || !G8(D4A_PHASE) && M8(v2 + 19) < 0x30u
     || (M8(v158 + 25) & 0x38
      || (M8(v158 + 1) + (M8(v158 + 25) & 4) + ((signed int)M8(v158 + 26) >> 4))
       * (unsigned int)M8(v2 + 18) >= G32(D4A_TBL_45C678)
                                            * (((signed int)M8(v2 + 19) >> 4) + (unsigned int)M8(v2 + 1)))
     && (M8(v2 + 19) < 0x60u || (signed int)M8(v2 + 19) >> 2 <= M8(v158 + 26))) )
  {
    G8(D4A_FLAG_45E394) = 1;
    ++G32(D4A_TBL_45C6B0);
    G32(D4A_COL_TOT_A) = G32(D4A_TBL_45C678);
    for (mi = 0; mi < 16u; mi++) vmask13[mi] = 0;
  for (mi = 0; mi < 13u; mi++) vmask13[mi] = M8(D4A_MASK13_SRC + mi);



    if ( G8(D4A_HIST_LEN) )
    {
      v18 = G32(D4A_TBL_45C678);
      v19 = v158;
      v20 = 0;
      v203 = v158 + 12;
      while ( M8(D4A_HIST_SYMS + (0)) != M8(v19 + 13) )
      {
        ++v19;
        ++v20;
        if ( !M8(v19 + 1) || v19 >= v203 )
          goto LABEL_69;
      }
      v21 = M8(v19 + 1);
      vmask13[v20] = 0;
      v18 -= v21;
LABEL_69:
      G32(D4A_COL_TOT_A) = v18;
    }
    v22 = M8(v158);
    if ( v22 >= G32(D4A_COL_TOT_A) )
      goto LABEL_201;
    G8(D4A_FLAG_45E397) = v207;
    G8(D4A_FLAG_45E394) = v206 == 0;
    if ( v206 )
      goto LABEL_201;
    v23 = 0;
    if ( M8(v158 + 2) )
    {
      v170 = 0;
      v172 = 0;
      v175 = 0;
      v37 = G32(D4A_COL_TOT_A) - v22;
      v38 = 0;
      if ( G32(D4A_FREQ_A) >= (unsigned int)G32(D4A_FREQ_B) )
        v177 = 12;
      else
        v177 = M8(v158 + 26) & 0xF;
      v176 = v177;
      G32(D4A_TRI_TOTAL) = G32(D4A_COL_TOT_A);
      if ( v37 < 0x80 )
      {
        if ( G8(D4A_PHASE) )
        {
          v197 = BYTE1(G32(D4A_CTX_COPY)) & 0xE0;
          v39 = (G32(D4A_CTX_COPY) & 0xC0u) >> 5;
          v40 = (unsigned char)(G32(D4A_CTX_COPY) & 0xE0) == v197;
        }
        else
        {
          v197 = 2 * (unsigned char)M8(D4A_HASH_ENABLE + (BYTE1(G32(D4A_CTX_COPY))));
          v40 = v197 + 4 * (unsigned char)M8(D4A_HASH_ENABLE + ((unsigned char)G32(D4A_CTX_COPY)));
          v39 = (unsigned char)G32(D4A_CTX_COPY) == 0xDF;   /* asm 0x410B06: cmp al,0xdf */
        }
        /* asm 0x410B3E: esi = offset dword_450464; esi += 1024*d4239F0[..];
         * edx = 128*d4239F0[..]; lea esi,[edx+esi+4*eax]  (address, not value) */
        v23 = D4A_TBL_450464 + 4 * (256 * M32(D4A_BUCKET_LUT + (M8(v158)) * 4))
            + 4 * (32 * M32(D4A_BUCKET_LUT + (v37) * 4))
            + 4 * (16 * ((unsigned int)G32(D4A_TBL_45C6B0) >= 6)
            + 8 * (M8(v2 + 7) != 0)
            + v40
            + v39);
        G32(D4A_TRI_TOTAL) = (v37 << 13) / M32(v23) + 1;
      }
  #ifdef D4BOUNDSDBG
    if (v176 >= 13 || v166 >= 13 || v167 >= 13)
      fprintf(stderr, "BOUNDS13 out=%u v176=%u v166=%u v167=%u\n",
          G32(D4A_OUT_POS), v176, v166, v167);
#endif
    if ( vmask13[v176] )
      {
        v174 = M8(v176 + v158 + 1);
        v179 = 0;
        v178 = G32(D4A_COL_TOT_A) - v174;
        v41 = v160 + v174 - (v174 == 2) + ((signed int)M8(v158 + 27) >> 4);
        if ( v215 == v210 )
        {
          v179 = 64;
          v41 += v161;
        }
        if ( v215 == v205 )
        {
          v179 = (v179 & ~0x000000FFU) | ((v179 | 0x80) & 0x000000FFU);
          v41 += v162;
        }
        v197 = v41 + v178;
        v42 = 16 * v41 / (v41 + v178);
        if ( v176 )
          v42 |= 0x100;   /* BYTE1(v42) |= 1 */
        if ( (v215 & 0xC0) == 0xC0 )
          v42 = (v42 & ~0x000000FFU) | ((v42 | 0x20) & 0x000000FFU);
        if ( G32(D4A_DICT_POS) <= (unsigned int)M32(D4A_TBL_444DA4 + ((v215) * 4)) )
          v42 |= 0x200;   /* BYTE1(v42) |= 2 */
        if ( M32(D4A_LAST_SEEN + ((v215) * 4)) == G32(D4A_OUT_COUNT) )
          v42 |= 0x400;   /* BYTE1(v42) |= 4 */
        v43 = M32(D4A_TBL_458564 + ((v179 | v42 | v163) * 4)) * v178;
        v175 = D4A_TBL_458564 + ((v179 | v42 | v163) * 4);
        v197 = 0x2000 - M32(v175);
        v44 = v43 / v197 + 1;
        if ( v44 > v174 )
        {
          if ( v44 >= 0x100 )
            v44 = 255;
          G32(D4A_TRI_TOTAL) += v44 - v174;
          M8(v176 + v158 + 1) = v44;
        }
      }
      if ( vmask13[v166] && v176 != v166 )
      {
        v171 = M8(v166 + v158 + 1);
        v180 = G32(D4A_COL_TOT_A) - v171;
        v197 = v162 + v171 + G32(D4A_COL_TOT_A) - v171;
        v45 = 16 * (v162 + v171) / v197;
        if ( (v205 & 0xC0) == 0xC0 )
          v45 = (v45 & ~0x000000FFU) | ((v45 | 0x20) & 0x000000FFU);
        if ( v205 == v210 )
          v45 = (v45 & ~0x000000FFU) | ((v45 | 0x40) & 0x000000FFU);
        if ( G32(D4A_DICT_POS) <= (unsigned int)M32(D4A_TBL_444DA4 + ((v205) * 4)) )
          v45 = (v45 & ~0x000000FFU) | ((v45 | 0x80) & 0x000000FFU);
        if ( M32(D4A_LAST_SEEN + ((v205) * 4)) == G32(D4A_OUT_COUNT) )
          v45 |= 0x100;   /* BYTE1(v45) |= 1 */
        v172 = D4A_TBL_45A564 + ((v163 | v45) * 4);
        v46 = M32(v172) * v180;
        v197 = 0x2000 - M32(v172);
        v47 = v46 / v197 + 1;
        if ( v47 > v171 )
        {
          if ( v47 >= 0x100 )
            v47 = 255;
          G32(D4A_TRI_TOTAL) += v47 - v171;
          M8(v166 + v158 + 1) = v47;
        }
      }
      if ( vmask13[v167] && v176 != v167 && v166 != v167 )
      {
        v173 = M8(v167 + v158 + 1);
        v181 = G32(D4A_COL_TOT_A) - v173;
        v48 = 16 * (v161 + v173) / (G32(D4A_COL_TOT_A) - v173 + v161 + v173);
        if ( (v210 & 0xC0) == 0xC0 )
          v48 = (v48 & ~0x000000FFU) | ((v48 | 0x20) & 0x000000FFU);
        if ( G32(D4A_DICT_POS) <= (unsigned int)M32(D4A_TBL_444DA4 + ((v210) * 4)) )
          v48 = (v48 & ~0x000000FFU) | ((v48 | 0x40) & 0x000000FFU);
        if ( M32(D4A_LAST_SEEN + ((v210) * 4)) == G32(D4A_OUT_COUNT) )
          v48 = (v48 & ~0x000000FFU) | ((v48 | 0x80) & 0x000000FFU);
        v38 = D4A_TBL_4554E4 + ((v163 | v48) * 4);
        v49 = M32(v38) * v181 / (0x2000 - M32(v38)) + 1;
        if ( v49 > v173 )
        {
          if ( v49 >= 0x100 )
            v49 = 255;
          G32(D4A_TRI_TOTAL) += v49 - v173;
          M8(v167 + v158 + 1) = v49;
        }
      }
      if ( vmask13[0] && v176 && v166 && v167 )
      {
        v169 = M8(v158 + 1);
        v50 = 16 * v169 / (unsigned int)G32(D4A_COL_TOT_A);
        if ( (M8(v158 + 13) & 0xC0) == 0xC0 )
          v50 = (v50 & ~0x000000FFU) | ((v50 | 0x20) & 0x000000FFU);
        if ( G32(D4A_DICT_POS) <= (unsigned int)M32(D4A_TBL_444DA4 + ((M8(v158 + 13)) * 4)) )
          v50 = (v50 & ~0x000000FFU) | ((v50 | 0x80) & 0x000000FFU);
        if ( M32(D4A_LAST_SEEN + ((M8(v158 + 13)) * 4)) == G32(D4A_OUT_COUNT) )
          v50 |= 0x100;   /* BYTE1(v50) |= 1 */
        v51 = (D4A_TBL_452864) + 4 * (v163 | v50);
        v52 = ((unsigned int)(M32(v51) * G32(D4A_COL_TOT_A)) >> 13) + 1;
        v170 = v51;
        if ( v52 >= 0x100 )
          v52 = 255;
        G32(D4A_TRI_TOTAL) += v52 - v169;
        M8(v158 + 1) = v52;
      }
      v182 = 0;
      v53 = G32(D4A_RC_RANGE) / (unsigned int)G32(D4A_TRI_TOTAL);
      G32(D4A_RC_RANGE) = v53;
      v183 = 0;
      v165 = (unsigned short)((unsigned)d4_div(G32(D4A_RC_CODE), v53));
      v54 = v158;
      while ( 1 )
      {
        if ( vmask13[v183] )
        {
          if ( !M8(v54 + 1) )
          {
            v183 = 12;
            goto LABEL_175;
          }
          v168 = (v168 & ~U64LL(0xFFFFFFFF00000000)) | ((((unsigned UHI64)(M8(v54 + 1) + v182)) << 32) & U64LL(0xFFFFFFFF00000000));
          if ( v165 < HIDWORD(v168) )
          {
            v55 = M8(v54 + 13);
            v208 = 0;
            v212 = v55;
#ifdef D4PATHDBG
    if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
      fprintf(stderr, "PATH P5707 out=%u v212=%u\n", G32(D4A_OUT_POS), (unsigned)v212);
#endif
LABEL_175:
            G32(D4A_TRI_LOW) = v182;
            G32(D4A_TRI_HIGH) = HIDWORD(v168);
            G8(D4A_COL_SLOT) = v183;
            if ( (unsigned char)v183 == 12 )
              G32(D4A_TRI_HIGH) = G32(D4A_TRI_TOTAL);
            v57 = M32(D4A_TRI_TOTAL + (1) * 4);
            v58 = G32(D4A_RC_RANGE) * (M32(D4A_TRI_TOTAL + (2) * 4) - v57);
            i = (unsigned long)(G32(D4A_RC_CODE) - G32(D4A_RC_RANGE) * v57);
  d4_rcrenorm(c, &i, &v58);
            G32(D4A_RC_CODE) = (int)i;
            G32(D4A_RC_RANGE) = v58;
            if ( v170 )
            {
              M8(v158 + 1) = v169;
              if ( G8(D4A_COL_SLOT) )
                M32(v170) -= M32(v170) >> 6;
              else
                M32(v170) += (0x2000 - M32(v170)) >> 6;
            }
            if ( v172 )
            {
              M8(v166 + v158 + 1) = v171;
              if ( (unsigned char)G8(D4A_COL_SLOT) == v166 )
                M32(v172) += (unsigned int)(0x2000 - M32(v172)) >> 6;
              else
                M32(v172) -= (unsigned int)M32(v172) >> 6;
            }
            if ( v38 )
            {
              M8(v167 + v158 + 1) = v173;
              if ( (unsigned char)G8(D4A_COL_SLOT) == v167 )
                M32(v38) += (unsigned int)(0x2000 - M32(v38)) >> 6;
              else
                M32(v38) -= (unsigned int)M32(v38) >> 6;
            }
            if ( v175 )
            {
              M8(v176 + v158 + 1) = v174;
              if ( (unsigned char)G8(D4A_COL_SLOT) == v176 )
                M32(v175) += (unsigned int)(0x2000 - M32(v175)) >> 6;
              else
                M32(v175) -= (unsigned int)M32(v175) >> 6;
            }
LABEL_195:
            if ( v23 )
            {
              if ( v208 )
                M32(v23) -= (unsigned int)M32(v23) >> 6;
              else
                M32(v23) += (unsigned int)(0x2000 - M32(v23)) >> 6;
            }
            goto LABEL_201;
          }
          v56 = G8(D4A_HIST_LEN);
          M8(D4A_HIST_SYMS + ((unsigned char)G8(D4A_HIST_LEN))) = M8(v54 + 13);
          v182 = HIDWORD(v168);
          G8(D4A_HIST_LEN) = v56 + 1;
        }
        ++v183;
        ++v54;
        if ( v183 >= 0xC )
          goto LABEL_175;
      }
    }
    v24 = 0;
    v25 = M8(v158 + 1);
    if ( M8(v158 + 13) == v210 )
    {
      v24 = 1;
      v25 += (signed int)M8(v2 + 19) >> 4;
    }
    if ( M8(v158 + 13) == v205 )
    {
      v24 = (v24 & ~0x000000FFU) | ((v24 | 1) & 0x000000FFU);
      v25 += (unsigned char)M8(D4A_RUN_CNT + (v159));
    }
    if ( M8(v2 + 7) && (unsigned int)G32(D4A_TBL_45C6B0) < 6 )
      v24 = (v24 & ~0x000000FFU) | ((v24 | 0x10) & 0x000000FFU);
    if ( G8(D4A_PHASE) )
    {
      if ( G32(D4A_DICT_POS) <= (unsigned int)M32(D4A_TBL_444DA4 + ((M8(v158 + 13)) * 4)) )
        v24 |= 0x100;   /* BYTE1(v24) |= 1 */
      if ( M8(v158 + 13) != (unsigned char)G32(D4A_CTX_COPY) )
        goto LABEL_87;
    }
    else if ( !M8(D4A_HASH_ENABLE + (M8(v158 + 13))) )
    {
LABEL_87:
      if ( v25 < 0x1E )
      {
        v26 = G32(D4A_CTX_COPY) & 0xE0;
        v157 = (unsigned int)G32(D4A_CTX_COPY) >> 8;
        v24 |= v26;
        if ( G8(D4A_PHASE) )
        {
          if ( v26 == (((unsigned int)G32(D4A_CTX_COPY) >> 8) & 0xE0) )
            v24 = (v24 & ~0x000000FFU) | ((v24 | 8) & 0x000000FFU);
          if ( v26 == (M8(v158 + 13) & 0xE0) )
            v24 = (v24 & ~0x000000FFU) | ((v24 | 4) & 0x000000FFU);
        }
        else
        {
          v24 |= 4 * (unsigned char)M8(D4A_HASH_ENABLE + ((unsigned char)v157))
               + 8 * (unsigned char)M8(D4A_HASH_ENABLE + ((unsigned char)G32(D4A_CTX_COPY)));
        }
      }
      if ( M32(D4A_LAST_SEEN + ((M8(v158 + 13)) * 4)) == G32(D4A_OUT_COUNT) )
        v24 |= 0x200;   /* BYTE1(v24) |= 2 */
      v23 = ((D4A_TBL_448464) + 4096 * M32(D4A_BUCKET_LUT2 + (v25) * 4) + 4 * v24);
      G32(D4A_RC_RANGE) = (unsigned)d4_div(G32(D4A_RC_RANGE), 0x2000u);
      if ( (unsigned short)((unsigned)d4_div(G32(D4A_RC_CODE), G32(D4A_RC_RANGE))) >= (unsigned int)M32(v23) )
      {
        v29 = G8(D4A_HIST_LEN);
        v30 = M8(v158 + 13);
        G8(D4A_COL_SLOT) = 12;
        M8(D4A_HIST_SYMS + ((unsigned char)G8(D4A_HIST_LEN))) = v30;
        G32(D4A_TRI_HIGH) = 0x2000;
        G32(D4A_TRI_LOW) = M32(v23);
        G8(D4A_HIST_LEN) = v29 + 1;
      }
      else
      {
        v208 = 0;
        v27 = M8(v158 + 13);
#ifdef D4PATHDBG
    if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
      fprintf(stderr, "PATH P5835 out=%u v212=%u\n", G32(D4A_OUT_POS), (unsigned)(unsigned char)v27);
#endif
        G8(D4A_COL_SLOT) = 0;
        v212 = v27;
        v28 = M32(v23);
        G32(D4A_TRI_LOW) = 0;
        G32(D4A_TRI_HIGH) = v28;
      }
      v31 = M32(D4A_TRI_TOTAL + (1) * 4);
      v32 = G32(D4A_RC_RANGE) * (M32(D4A_TRI_TOTAL + (2) * 4) - v31);
      j = (unsigned long)(G32(D4A_RC_CODE) - G32(D4A_RC_RANGE) * v31);
  d4_rcrenorm(c, &j, &v32);
      G32(D4A_RC_CODE) = (int)j;
      G32(D4A_RC_RANGE) = v32;
      goto LABEL_195;
    }
    v24 = (v24 & ~0x000000FFU) | ((v24 | 2) & 0x000000FFU);
    goto LABEL_87;
  }
LABEL_201:
  if ( !v208 )
  {
    if ( M8(v158 + (unsigned char)G8(D4A_COL_SLOT) + 1) < 7u )
      d4_row_update(c, v212);
    goto LABEL_408;
  }
  if ( G8(D4A_PHASE) )
    v63 = d4_row_pick_best(c);
  else
    v63 = 0;
  v204 = 1;
  v214 = 0;
  G32(D4A_TBL_45C6B0) = (unsigned int)G32(D4A_TBL_45C6B0) >> 3;
  v184 = v63;
#ifdef D4GATEDBG
  if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
    fprintf(stderr, "GATE2 out=%u v63=%u v107=%d v184=%u\n",
          G32(D4A_OUT_POS), 
        (unsigned)v63, v107, (unsigned)v184);
#endif
  if ( v63 )
  {
    G32(D4A_TBL_45C6A0) += 2;
    v64 = M8(v2 + 19) >= 0x90u
       || 5 * M8(v63) > 3 * M8(v63 + 1) && (M8(v63 + 3) & 0x7Fu) <= 2;
    G8(D4A_FLAG_45E39D) = v64;
  }
  else
  {
    G8(D4A_FLAG_45E39D) = 1;
  }
  v213 = v205 & 0xC0;
  v217 = v210 & 0xC0;
  do
  {
    if ( !G8(D4A_FLAG_45E39D) && !v214 )
      goto LABEL_336;
    v204 = 0;
    v65 = !G32(D4A_TBL_45C6AC)
       || !M8(v2 + 17)
       || (unsigned int)G32(D4A_TBL_45C6B4) < 0xFFFF && M32(c->cost_cnt_b) <= 0x3FEu;
#ifdef D4V65DBG
    if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
      fprintf(stderr, "V65 out=%u v65=%d c6ac=%u m2_17=%u c6b4=%u p68c=%u m2_0=%u m2_2=%u m2_9=%u m2_18=%u v2=%u c69c=%u\n",
          G32(D4A_OUT_POS), v65, G32(D4A_TBL_45C6AC), (unsigned)M8(v2 + 17),
          G32(D4A_TBL_45C6B4), M32(c->cost_cnt_b), (unsigned)M8(v2),
          (unsigned)M8(v2 + 2), (unsigned)M8(v2 + 9), (unsigned)M8(v2 + 18),
          (unsigned)v2, G32(D4A_ROW_CTX));
#endif
    G8(D4A_FLAG_45E39D) = v65;
    if ( !v65 || !M8(v2) )
      goto LABEL_336;
    v185 = 0;
    for (mi = 0; mi < 16u; mi++) vmask9[mi] = 0;
  for (mi = 0; mi < 9u; mi++) vmask9[mi] = M8(D4A_MASK9_SRC + 1 + mi);


    v164 = 8;
    v66 = 0;
    v186 = M8(v2 + 18);
    v67 = v2;
    do
    {
      v187 = 0;
      v68 = M8(v67 + 9);
      ++v185;
      if ( v68 == v205 )
        v164 = v66;
      while ( (unsigned char)G8(D4A_HIST_LEN) > v187 )
      {
        v69 = M8(D4A_HIST_SYMS + (v187++));
        if ( v69 == M8(v67 + 9) )
        {
          v186 -= M8(v67 + 1);
          vmask9[v66] = 0;
          break;
        }
      }
      ++v67;
      ++v66;
    }
    while ( M8(v67 + 1) && v66 < 8 );
    G32(D4A_COL_TOT_B) = v186;
    v70 = 5 * M8(v2);
    G8(D4A_ROW_SLOT) = -1;
    if ( 4 * v186 <= v70 )
      goto LABEL_336;
    if ( G32(D4A_TBL_45C6AC) + 4 * (unsigned int)M8(v2 + 17) >= 0x10 )
      goto LABEL_336;
    G8(D4A_FLAG_45E39A) = v207;
    if ( v206 )
      goto LABEL_336;
    v71 = 0;
    if ( !M8(v2 + 2) )
    {
      v72 = (signed int)M8(v2 + 1) >> 1;
      v73 = 0;
      if ( M8(v2 + 9) == v205 )
      {
        v73 = 1;
        v72 += (unsigned char)M8(D4A_RUN_CNT + (v159));
      }
      if ( v72 < 0x10 )
      {
        if ( G8(D4A_PHASE) )
        {
          v74 = (G32(D4A_CTX_COPY) & 0xC0u) >> 5;
        }
        else
        {
          v197 = (unsigned int)G32(D4A_CTX_COPY) >> 8;
          v74 = 2 * (unsigned char)M8(D4A_HASH_ENABLE + (BYTE1(G32(D4A_CTX_COPY))))
              + 4 * (unsigned char)M8(D4A_HASH_ENABLE + ((unsigned char)G32(D4A_CTX_COPY)));
        }
        v73 |= v74;
      }
      if ( G8(D4A_PHASE) && (G32(D4A_DICT_POS) <= (unsigned int)M32(D4A_TBL_444DA4 + ((M8(v2 + 9)) * 4))) )
        v73 = (v73 & ~0x000000FFU) | ((v73 | 8) & 0x000000FFU);
      if ( M32(D4A_LAST_SEEN + ((M8(v2 + 9)) * 4)) == G32(D4A_OUT_COUNT) )
        v73 = (v73 & ~0x000000FFU) | ((v73 | 0x10) & 0x000000FFU);
      v71 = (D4A_TBL_452464) + 128 * M32(D4A_BUCKET_LUT2 + (v72) * 4) + 4 * v73;
      G32(D4A_RC_RANGE) = (unsigned)d4_div(G32(D4A_RC_RANGE), 0x2000u);
#ifdef D4V65DBG
      if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
        fprintf(stderr, "D5982 out=%u div16=%u m71=%u v72=%u v73=%u\n",
            G32(D4A_OUT_POS), (unsigned)(unsigned short)((unsigned)d4_div(G32(D4A_RC_CODE), G32(D4A_RC_RANGE))),
            M32(v71), v72, v73);
#endif
      if ( (unsigned int)(unsigned short)((unsigned)d4_div(G32(D4A_RC_CODE), G32(D4A_RC_RANGE))) >= M32(v71) )
      {
#ifdef D4V65DBG
        if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
          fprintf(stderr, "IF5982 out=%u\n", G32(D4A_OUT_POS));
#endif
        v77 = G8(D4A_HIST_LEN);
        v78 = M8(v2 + 9);
        G8(D4A_ROW_SLOT) = 8;
        M8(D4A_HIST_SYMS + ((unsigned char)G8(D4A_HIST_LEN))) = v78;
        G32(D4A_TRI_HIGH) = 0x2000;
        G32(D4A_TRI_LOW) = M32(v71);
        G8(D4A_HIST_LEN) = v77 + 1;
      }
      else
      {
        v208 = 0;
        G8(D4A_ROW_SLOT) = 0;
        v75 = M8(v2 + 9);
#ifdef D4PATHDBG
    if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
      fprintf(stderr, "PATH P5982 out=%u v212=%u\n", G32(D4A_OUT_POS), (unsigned)(unsigned char)v75);
#endif
        G8(D4A_FLAG_45E3A4) = 1;
        v212 = v75;
        v76 = M32(v71);
        G32(D4A_TRI_LOW) = 0;
        G32(D4A_TRI_HIGH) = v76;
      }
      v79 = M32(D4A_TRI_TOTAL + (1) * 4);
      v80 = G32(D4A_RC_RANGE) * (M32(D4A_TRI_TOTAL + (2) * 4) - v79);
      k = (unsigned long)(G32(D4A_RC_CODE) - G32(D4A_RC_RANGE) * v79);
  d4_rcrenorm(c, &k, &v80);
      G32(D4A_RC_CODE) = (int)k;
      G32(D4A_RC_RANGE) = v80;
      goto LABEL_332;
    }
    v190 = 0;
    v192 = 0;
    v85 = 0;
    if ( G32(D4A_FREQ_A) >= (unsigned int)G32(D4A_FREQ_B) )
      v86 = 8;
    else
      v86 = M8(v2 + 19) & 0xF;
    v194 = v86;
    G32(D4A_TRI_TOTAL) = G32(D4A_COL_TOT_B);
    if ( v185 < 8 )
    {
      v87 = G32(D4A_COL_TOT_B) - M8(v2);
      if ( v87 < 0x10 )
      {
        v71 = (D4A_TBL_4578E4) + 4 * ((M8(v2 + 18) == G32(D4A_COL_TOT_B)) + (v87 & 0xC) + M32(D4A_CNT_OFF_8 + ((v185) * 4)));
        G32(D4A_TRI_TOTAL) = ((signed int)M8(v2) >> 1)
                     + ((G32(D4A_COL_TOT_B) - (unsigned int)M8(v2)) << 13) / M32(v71)
                     + 1;
      }
    }
#ifdef D4BOUNDSDBG
    if (v194 >= 9)
      fprintf(stderr, "BOUNDS9 out=%u v194=%u val=%u\n",
          G32(D4A_OUT_POS), v194, (unsigned)vmask9[v194]);
#endif
    if ( vmask9[v194] )
    {
      v193 = M8(v2 + v194 + 1);
      v88 = 0;
      v195 = G32(D4A_COL_TOT_B) - v193;
      v89 = v161 + v193;
      if ( v210 == v205 )
      {
        v88 = 128;
        v89 += v162;
      }
      v197 = v89 + v195;
      v90 = 16 * v89 / (v89 + v195);
      if ( v194 )
        v90 |= 0x100;   /* BYTE1(v90) |= 1 */
      if ( v217 == -64 )
        v90 = (v90 & ~0x000000FFU) | ((v90 | 0x20) & 0x000000FFU);
      if ( v184 && v210 == M8(v184 + 2) )
        v90 = (v90 & ~0x000000FFU) | ((v90 | 0x40) & 0x000000FFU);
      if ( G32(D4A_DICT_POS) <= (unsigned int)M32(D4A_TBL_444DA4 + ((v210) * 4)) )
        v90 |= 0x200;   /* BYTE1(v90) |= 2 */
      if ( M32(D4A_LAST_SEEN + ((v210) * 4)) == G32(D4A_OUT_COUNT) )
        v90 |= 0x400;   /* BYTE1(v90) |= 4 */
      v85 = D4A_TBL_4534E4 + ((v163 | v88 | v90) * 4);
      v91 = M32(v85) * v195 / (0x2000 - M32(v85)) + 1;
      if ( v91 > v193 )
      {
        if ( v91 >= 0x100 )
          v91 = 255;
        G32(D4A_TRI_TOTAL) += v91 - v193;
        M8(v2 + v194 + 1) = v91;
      }
    }
    if ( vmask9[v164] && v194 != v164 )
    {
      v191 = M8(v2 + v164 + 1);
      v196 = G32(D4A_COL_TOT_B) - v191;
      v197 = v162 + v191 + G32(D4A_COL_TOT_B) - v191;
      v92 = 16 * (v162 + v191) / v197;
      if ( v213 == -64 )
        v92 = (v92 & ~0x000000FFU) | ((v92 | 0x20) & 0x000000FFU);
      if ( G32(D4A_DICT_POS) <= (unsigned int)M32(D4A_TBL_444DA4 + ((v205) * 4)) )
        v92 = (v92 & ~0x000000FFU) | ((v92 | 0x40) & 0x000000FFU);
      if ( M32(D4A_LAST_SEEN + ((v205) * 4)) == G32(D4A_OUT_COUNT) )
        v92 = (v92 & ~0x000000FFU) | ((v92 | 0x80) & 0x000000FFU);
      v192 = D4A_TBL_453064 + ((v163 | v92) * 4);
      v93 = M32(v192) * v196 / (0x2000 - M32(v192)) + 1;
      if ( v93 > v191 )
      {
        if ( v93 >= 0x100 )
          v93 = 255;
        G32(D4A_TRI_TOTAL) += v93 - v191;
        M8(v2 + v164 + 1) = v93;
      }
    }
    if ( vmask9[0] && v194 && v164 )
    {
      v189 = M8(v2 + 1);
      v94 = 16 * v189 / (unsigned int)G32(D4A_COL_TOT_B);
      if ( (M8(v2 + 9) & 0xC0) == 0xC0 )
        v94 = (v94 & ~0x000000FFU) | ((v94 | 0x20) & 0x000000FFU);
      if ( v184 && M8(v2 + 9) == M8(v184 + 2) )
        v94 = (v94 & ~0x000000FFU) | ((v94 | 0x40) & 0x000000FFU);
      if ( G32(D4A_DICT_POS) <= (unsigned int)M32(D4A_TBL_444DA4 + ((M8(v2 + 9)) * 4)) )
        v94 = (v94 & ~0x000000FFU) | ((v94 | 0x80) & 0x000000FFU);
      if ( M32(D4A_LAST_SEEN + ((M8(v2 + 9)) * 4)) == G32(D4A_OUT_COUNT) )
        v94 |= 0x100;   /* BYTE1(v94) |= 1 */
      v95 = (D4A_TBL_457D64) + 4 * (v163 | v94);
      v96 = ((unsigned int)(M32(v95) * G32(D4A_COL_TOT_B)) >> 13) + 1;
      v190 = v95;
      if ( v96 >= 0x100 )
        v96 = 255;
      G32(D4A_TRI_TOTAL) += v96 - v189;
      M8(v2 + 1) = v96;
    }
    v199 = 0;
    v97 = G32(D4A_RC_RANGE) / (unsigned int)G32(D4A_TRI_TOTAL);
    G32(D4A_RC_RANGE) = v97;
    v200 = 0;
    v188 = (unsigned short)((unsigned)d4_div(G32(D4A_RC_CODE), v97));
    v98 = v2;
    while ( !vmask9[v200] )
    {
LABEL_312:
      ++v200;
      ++v98;
      if ( v200 >= 8 )
        goto LABEL_313;
    }
    if ( !M8(v98 + 1) )
    {
      v200 = 8;
      goto LABEL_313;
    }
    v198 = M8(v98 + 1) + v199;
    if ( v188 >= v198 )
    {
      v100 = G8(D4A_HIST_LEN);
      M8(D4A_HIST_SYMS + ((unsigned char)G8(D4A_HIST_LEN))) = M8(v98 + 9);
      v199 = v198;
      G8(D4A_HIST_LEN) = v100 + 1;
      goto LABEL_312;
    }
    v99 = M8(v98 + 9);
    v208 = 0;
    v212 = v99;
#ifdef D4PATHDBG
    if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
      fprintf(stderr, "PATH P6126 out=%u v212=%u\n", G32(D4A_OUT_POS), (unsigned)v212);
#endif
LABEL_313:
    G32(D4A_TRI_LOW) = v199;
    G32(D4A_TRI_HIGH) = v198;
    G8(D4A_ROW_SLOT) = v200;
    if ( (unsigned char)v200 == 8 )
      G32(D4A_TRI_HIGH) = G32(D4A_TRI_TOTAL);
    v101 = M32(D4A_TRI_TOTAL + (1) * 4);
    v102 = G32(D4A_RC_RANGE) * (M32(D4A_TRI_TOTAL + (2) * 4) - v101);
    l = (unsigned long)(G32(D4A_RC_CODE) - G32(D4A_RC_RANGE) * v101);
  d4_rcrenorm(c, &l, &v102);
    G32(D4A_RC_CODE) = (int)l;
    G32(D4A_RC_RANGE) = v102;
    if ( v190 )
    {
      M8(v2 + 1) = v189;
      if ( G8(D4A_ROW_SLOT) )
        M32(v190) -= M32(v190) >> 6;
      else
        M32(v190) += (0x2000 - M32(v190)) >> 6;
    }
    if ( v192 )
    {
      M8(v2 + v164 + 1) = v191;
      if ( (unsigned char)G8(D4A_ROW_SLOT) == v164 )
        M32(v192) += (unsigned int)(0x2000 - M32(v192)) >> 6;
      else
        M32(v192) -= (unsigned int)M32(v192) >> 6;
    }
    if ( v85 )
    {
      M8(v2 + v194 + 1) = v193;
      if ( (unsigned char)G8(D4A_ROW_SLOT) == v194 )
        M32(v85) += (unsigned int)(0x2000 - M32(v85)) >> 6;
      else
        M32(v85) -= (unsigned int)M32(v85) >> 6;
    }
    if ( !v208 && 4 * (unsigned int)M8(v2 + (unsigned char)G8(D4A_ROW_SLOT) + 1) >= G32(D4A_COL_TOT_B) )
      G8(D4A_FLAG_45E3A4) = ((unsigned int)G32(D4A_COL_TOT_B) > 0x10) + 1;
LABEL_332:
    if ( v71 )
    {
      if ( v208 )
        M32(v71) -= M32(v71) >> 6;
      else
        M32(v71) += (unsigned int)(0x2000 - M32(v71)) >> 6;
    }
LABEL_336:
    if ( v214 )
      break;
    G8(D4A_FLAG_45E393) = v208;
    if ( v208 )
    {
      v214 = 1;
      v107 = 1;
      if ( !G8(D4A_PHASE) )
        G8(D4A_FLAG_45E393) = 0;
      if ( v184 )
      {
        G32(D4A_TBL_45C6A0) = (unsigned int)G32(D4A_TBL_45C6A0) >> 2;
        v108 = 0;
#ifdef D4SCANDBG
        if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
          fprintf(stderr, "SCAN out=%u e39c=%u h0=%u rowb2=%u\n",
              G32(D4A_OUT_POS), G8(D4A_HIST_LEN),
              (unsigned)M8(D4A_HIST_SYMS), (unsigned)M8(v184 + 2));
#endif
        while ( v108 < (unsigned char)G8(D4A_HIST_LEN) )
        {
          v109 = M8(D4A_HIST_SYMS + (v108++));
          if ( v109 == M8(v184 + 2) )
          {
            v107 = 0;
#ifdef D4V65DBG
            if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
              fprintf(stderr, "SCAN-HIT out=%u\n", G32(D4A_OUT_POS));
#endif
            if ( M8(v184) > 3u || (signed int)M8(v184 + 1) >> 6 > M8(v184) )
              G8(D4A_FLAG_45E393) = 0;
            break;
          }
        }
        if ( v107 )
        {
          G8(D4A_FLAG_45E399) = v207;
          if ( !v206 )
          {
            if ( c->pair_ptr )
            {
              v155 = M8(c->pair_ptr);
              v110 = M8(c->pair_ptr + 1);
            }
            else
            {
              v155 = M8(v184);
              v110 = M8(v184 + 1);
            }
            /* asm 0x411A3E..0x411A92: the table value is added into v110 in
             * place and the three conditional adds may target v110 as well;
             * rewrite the Hex-Rays (*(&v155 + cond)) trick explicitly. */
            v156 = M32(D4A_TBL_4274CD + ((((signed int)M8(v184 + 3) >> 3) & 0xF) * 4));
            v110 += v156;
            /* asm 0x411A53/0x411A6C/0x411A85: pair[M8(v184+2)==cmp] += value,
             * pair[0]=v155, pair[1]=v110 - the byte always lands in one. */
            if ( M8(v184 + 2) == v215 )
              v110 += v160;
            else
              v155 += v160;
            if ( M8(v184 + 2) == v210 )
              v110 += v161;
            else
              v155 += v161;
            if ( M8(v184 + 2) == v205 )
              v110 += v162;
            else
              v155 += v162;
            G32(D4A_TRI_TOTAL) = v155 + v110;
            G32(D4A_RC_RANGE) = (unsigned)d4_div(G32(D4A_RC_RANGE), v155 + v110);
#ifdef D4208DBG
            if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
              fprintf(stderr, "V208LP out=%u v184=%u v155=%u v110=%u div=%u\n",
          G32(D4A_OUT_POS), 
                  (unsigned)v184, (unsigned)v155, (unsigned)v110,
                  (unsigned)d4_div(G32(D4A_RC_CODE), G32(D4A_RC_RANGE)));
#endif
            if ( (unsigned short)((unsigned)d4_div(G32(D4A_RC_CODE), G32(D4A_RC_RANGE))) < v155 )
            {
              if ( G8(D4A_FLAG_45E398) )
              {
                M32(c->cost_cnt_b) >>= 1;
                M32(c->cost_cnt_a) >>= 1;
                G32(D4A_TBL_45C6B4) = ((unsigned int)G32(D4A_TBL_45C6B4) >> 1) + 255;
              }
              G32(D4A_TRI_HIGH) = v155;
              v113 = (unsigned char)G8(D4A_HIST_LEN);
              M8(D4A_HIST_SYMS + (v113)) = M8(v184 + 2);
              G32(D4A_TRI_LOW) = 0;
              G8(D4A_HIST_LEN) = v113 + 1;
            }
            else
            {
              if ( G8(D4A_FLAG_45E398) )
              {
                v111 = G32(D4A_TBL_45C6B4);
                M32(c->cost_cnt_b) += G32(D4A_TBL_45C6B4);
                G32(D4A_TBL_45C6B4) *= 2;
                M32(c->cost_cnt_a) += v111;
                if ( (unsigned int)G32(D4A_TBL_45C6B4) > 0xFFFF )
                  G32(D4A_TBL_45C6B4) = 0xFFFF;
              }
              G32(D4A_TRI_LOW) = v155;
              G32(D4A_TRI_HIGH) = G32(D4A_TRI_TOTAL);
              v112 = M8(v184 + 2);
              v208 = 0;
              v212 = v112;
#ifdef D4212DBG
              if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
                fprintf(stderr, "FAST out=%u v212=%u code=%u range=%u e392=%u\n",
                    G32(D4A_OUT_POS), (unsigned)v212, G32(D4A_RC_CODE), G32(D4A_RC_RANGE), G8(D4A_PHASE));
#endif
            }
            v114 = M32(D4A_TRI_TOTAL + (1) * 4);
            v115 = G32(D4A_RC_RANGE) * (M32(D4A_TRI_TOTAL + (2) * 4) - v114);
            m = (unsigned long)(G32(D4A_RC_CODE) - G32(D4A_RC_RANGE) * v114);
  d4_rcrenorm(c, &m, &v115);
            G32(D4A_RC_CODE) = (int)m;
            G32(D4A_RC_RANGE) = v115;
          }
        }
      }
    }
  }
  while ( v208 && v204 );
  /* asm 0x411BD6/0x411BDA: when the decode loop exits with v208==0 the
   * literal was already produced by a fast path (which stored it in v212)
   * - skip the v120/v201 setup and the 411F8B decode and go straight to
   * the model-update/output tail.  Falling through here re-decodes a
   * second literal, consuming range bits the original never reads. */
  if ( !v208 )
    goto LABEL_408;
  if ( v208 )
  {
    v120 = v159;
    v211 = 0;
    G8(D4A_FLAG_45E39F) = v207;
    if ( G8(D4A_PHASE) )
    {
      v218 = M8(c->dict_buf + (G32(D4A_DICT_MASK) & (G32(D4A_DICT_POS) - G32(D4A_BEST_OFF))));
      if ( (unsigned int)G32(D4A_BEST_OFF) > 1 && M32(c->cost_cnt_b) )
      {
        v211 = 1;
        v120 = v218 + 256;
      }
      if ( v216 )
      {
        /* asm 0x411c40..0x411c51: v216 = (v216 && byte_45E38E), not a
         * bitwise AND with the address (Hex-Rays dropped the dereference). */
        v216 = v216 && G8(D4A_PRED_FLAG);
        v218 = G8(D4A_FLAG_45E38B);
        v201 = G32(D4A_TRI2_TOTAL_NEXT);
        v202 = (unsigned char)G8(D4A_MODEL_ID) + 772;
      }
      else
      {
        v122 = M32(D4A_SEL_LUT + (((unsigned char)(v218
                                            - M8(c->dict_buf
                                                       + (G32(D4A_DICT_MASK) & (G32(D4A_DICT_POS) - 2 * G32(D4A_BEST_OFF)))))) * 4));
        v202 = 772;
        v201 = v122 + 513;
      }
    }
    if ( v209 )
    {
      v123 = 0;
      if ( v216 )
      {
        while ( v123 < (unsigned char)G8(D4A_HIST_LEN) )
        {
          ++v123;
          M8(v123 + 4578669) = v218 - M8(v123 + 4578669);
        }
      }
      else
      {
        v124 = v218;
        while ( v123 < (unsigned char)G8(D4A_HIST_LEN) )
        {
          v125 = v123++;
          M8(D4A_HIST_SYMS + (v125)) -= v124;
        }
      }
      v126 = d4_ppm_decode_gated(c, v201, v202);
      if ( v216 )
        v128 = v218 - v126;
      else
        v128 = v126 + v218;
      v212 = v128;
#ifdef D4209DBG
      if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
        fprintf(stderr, "V209 out=%u v216=%d v218=%u v201=%u v202=%d v126=%u v128=%u e38e=%u e390=%u\n",
            G32(D4A_OUT_POS), v216, (unsigned)v218, v201, v202, (unsigned)v126, (unsigned)v128, G8(D4A_PRED_FLAG), G8(D4A_MODEL_ID));
#endif
#ifdef D4209DBG
      if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
        fprintf(stderr, "V209 out=%u v216=%d v218=%u v201=%u v202=%d v126=%u v128=%u e38e=%u\n",
            G32(D4A_OUT_POS), v216, (unsigned)v218, v201, v202, (unsigned)v126, (unsigned)v128, G8(D4A_PRED_FLAG));
#endif
      v129 = 0;
      if ( v216 )
      {
        while ( v129 < (unsigned char)G8(D4A_HIST_LEN) )
        {
          ++v129;
          M8(v129 + 4578669) = v218 - M8(v129 + 4578669);
        }
      }
      else
      {
        v130 = v218;
        while ( v129 < (unsigned char)G8(D4A_HIST_LEN) )
        {
          v131 = v129++;
          M8(D4A_HIST_SYMS + (v131)) += v130;
        }
      }
      v132 = v212;
      v133 = 769;
      /* asm 0x411d61: eax = ecx, and ecx holds v120 (byte_45C6A4 context). */
      v134 = v120;
      goto LABEL_394;
    }
    v135 = d4_ppm_decode_gated(c, v120, 769);
    v212 = v135;
#ifdef D4V135DBG
    if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
      fprintf(stderr, "V135 out=%u v120=%u v135=%u\n",
          G32(D4A_OUT_POS), (unsigned)v120, (unsigned)v135);
#endif
    if ( G8(D4A_PHASE) )
    {
      v136 = 0;
      if ( v216 )
        v132 = v218 - v135;
      else
        v132 = v135 - v218;   /* asm 0x411d9c: sub bl,[v218] (path B) */
      if ( v216 )
      {
        while ( v136 < (unsigned char)G8(D4A_HIST_LEN) )
        {
          ++v136;
          M8(v136 + 4578669) = v218 - M8(v136 + 4578669);
        }
      }
      else
      {
        v137 = v218;
        while ( v136 < (unsigned char)G8(D4A_HIST_LEN) )
        {
          v138 = v136++;
          M8(D4A_HIST_SYMS + (v138)) -= v137;
        }
      }
      v133 = v202;
      v134 = v201;
LABEL_394:
      d4_trial_record_cost(c, v134, v133, v132);
    }
  }
LABEL_408:
  d4_col_update(c, v212);
  if ( G8(D4A_FLAG_45E39D) )
    d4_row_update_cost(c, v212);   /* asm 0x411E16: eax = edx = the decoded byte (was a
                           * Hex-Rays leftover v139, never assigned) */
  if ( G8(D4A_FLAG_45E393) )
    d4_row_cost_update(c, v212);
  if ( v207 )
  {
    v140 = v212 - G8(D4A_FLAG_45E38B);
    if ( G8(D4A_PRED_FLAG) )
      v140 = -v140;
    G8(D4A_HIST_LEN) = 0;
    d4_cost_accum_update(c, G32(D4A_TRI2_TOTAL_NEXT), (unsigned char)G8(D4A_MODEL_ID) + 772, v140);
    if ( G8(D4A_FLAG_45E39F) )
      G32(D4A_COST_SUM) += G32(D4A_TBL_45C668);
    G32(D4A_COST_GATE) = G32(D4A_COST_SUM) + ((unsigned int)(31 * G32(D4A_COST_GATE)) >> 5);
  }
  while ( (G8(D4A_FLAG_45E3A1) = (unsigned char)(G8(D4A_FLAG_45E3A1) - 1)) != 0xFF )
  {
    v141 = (unsigned char)G8(D4A_FLAG_45E3A1);
    G32(D4A_PPM_CTX) = M32(D4A_TBL_444584 + ((v141) * 4));
    if ( M32(D4A_TBL_444594 + ((v141) * 4)) == 512 )
      d4_fenwick_add(c, M8(D4A_TBL_45AE34 + (unsigned char)G8(D4A_FLAG_45E3A1)), M32(D4A_TBL_444594 + ((v141) * 4)));
    else
      d4_fenwick_add_run(c, M8(D4A_TBL_45AE34 + (unsigned char)G8(D4A_FLAG_45E3A1)), M32(D4A_TBL_444594 + ((v141) * 4)));
  }
  d4_emit_byte(c, v212);
  if ( v211 )
  {
    if ( M8(D4A_TBL_45E086 + (v159)) == v212 )
    {
      if ( (unsigned char)M8(D4A_RUN_CNT + (v159)) < 0xFu )
        ++M8(D4A_RUN_CNT + (v159));
    }
    else
    {
      v142 = v159;
      v143 = v212;
      M8(D4A_RUN_CNT + (v159)) = 0;
      M8(D4A_TBL_45E086 + (v142)) = v143;
    }
  }
  d4_ctx_feed_byte(c, v212, 1);
  v144 = c->dict_buf;
  v145 = G32(D4A_DICT_POS);
  v146 = v212;
  M8(c->dict_buf + G32(D4A_DICT_POS)) = v212;
  if ( v145 < 0x100 )
    M8(v144 + G32(D4A_DICT_SIZE) + v145) = v146;
  result = G32(D4A_DICT_MASK);
  G32(D4A_DICT_POS) = G32(D4A_DICT_MASK) & (G32(D4A_DICT_POS) + 1);
  return result;
}

#pragma GCC pop_options

/* See the note above d4_decode_ppm: same -O2 miscompile workaround. */
#pragma GCC push_options
#if defined(__GNUC__) && __GNUC__ >= 12
#pragma GCC optimize ("-ftrivial-auto-var-init=zero")
#endif
static unsigned char d4_ppm_decode_gated(dec4_ctx *c, unsigned int a1, int a2)
{
#ifdef D4F8BDBG
  fprintf(stderr, "F8B out=%u a1=%u a2=%d\n", G32(D4A_OUT_POS), a1, a2);
#endif
  unsigned int v2;
  int v3;
  int v4;
  int v5;
  int v6;
  int v7;
  unsigned int v8;
  unsigned int i;
  unsigned char v10;
  int v11;
  int v12;
  int v13;
  unsigned int v14;
  int v15;
  int v16;
  int v17;
  unsigned char v18;
  int v19;
  int v20;
  unsigned int v21;
  int v22;
  int v23;
  int v25;
  unsigned char v26;
  int v27;
  char v28;

  v2 = a1;
  v3 = a2;
  v28 = 1;
  G32(D4A_COST_TMP) = 0;
  #ifdef D4C648DBG
      if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
        fprintf(stderr, "C648R out=%u\n", G32(D4A_OUT_POS));
  #endif
  v4 = (unsigned char)(G32(D4A_TBL_45C650) + 1);
  v5 = a2 == 769;
  G32(D4A_TBL_45C650) = v4;
  v6 = M32(D4A_TBL_4451A4 + ((v4) * 4));
  G32(D4A_PPM_CTX) = v3;
  G32(D4A_TBL_45C644) -= v6;
  v27 = v5;
#ifdef D411F8BDBG
  if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
    fprintf(stderr, "1F8B out=%u a1=%u a2=%d c644=%u code=%u range=%u\n",
        G32(D4A_OUT_POS), a1, a2, G32(D4A_TBL_45C644), G32(D4A_RC_CODE), G32(D4A_RC_RANGE));
#endif
  if ( (unsigned int)G32(D4A_TBL_45C644) < 0x6D4250 || (unsigned int)G32(D4A_TBL_45C644) >= 0xFEFF01 )
  {
    v26 = d4_ppm_decode_sym(c, v2);
    if ( G8(D4A_FLAG_45E39F) )
      G32(D4A_TBL_45C668) += G32(D4A_COST_TMP);
  }
  else if ( (unsigned int)G32(D4A_TBL_45C644) < 0x91ADC0 )
  {
    v13 = v5;
    v14 = M32(D4A_TBL_4455A4 + ((v5) * 4));
#ifdef D411F8BDBG
    if ((G32(D4A_OUT_POS) >= 407840 && G32(D4A_OUT_POS) <= 407860) || (G32(D4A_OUT_POS) >= 760195 && G32(D4A_OUT_POS) <= 760215) || G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
      fprintf(stderr, "1F8BB out=%u v14=%u dac=%u e39c=%u\n",
          G32(D4A_OUT_POS), M32(D4A_TBL_4455A4 + 4), M32(D4A_TBL_445DAC + 4), G8(D4A_HIST_LEN));
#endif
    v15 = v5 + 770;
    if ( v14 < M32(D4A_TBL_445DAC + ((v13) * 4)) )
    {
      G32(D4A_PPM_CTX) = v15;
      v18 = d4_ppm_decode_sym(c, 512);
      v19 = G32(D4A_COST_TMP);
      v26 = v18;
      G32(D4A_PPM_CTX) = v3;
      G32(D4A_COST_TMP) = 0;
      #ifdef D4C648DBG
          if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
            fprintf(stderr, "C648R out=%u\n", G32(D4A_OUT_POS));
      #endif
      v25 = v19;
      d4_ppm_eval_cost(c, v2, v18);
      /* asm 0x41211c/0x41219e: this branch also carries the cost collected
       * by d4_ppm_decode_sym into the common tail's ecx (= v19); Hex-Rays only kept
       * the v17 definition in the other branch. */
      v17 = v19;
    }
    else
    {
      v26 = d4_ppm_decode_sym(c, v2);
      v17 = G32(D4A_COST_TMP);      /* cost saved across the reset */
      G32(D4A_PPM_CTX) = v15;      /* Hex-Rays had an undefined v16 here */
      G32(D4A_COST_TMP) = 0;
      #ifdef D4C648DBG
          if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
            fprintf(stderr, "C648R out=%u\n", G32(D4A_OUT_POS));
      #endif
      d4_ppm_eval_cost(c, 0x200u, v26);
      v25 = G32(D4A_COST_TMP);
      G32(D4A_COST_TMP) = v17;
    }
    if ( G8(D4A_FLAG_45E39F) )
      G32(D4A_TBL_45C668) += v17;
    v20 = (unsigned char)G8(D4A_FLAG_45E3A1);
    M8(D4A_TBL_45AE34 + v20) = v26;
    M32(D4A_TBL_4451A4 + ((G32(D4A_TBL_45C650)) * 4)) = v17;
    M32(D4A_TBL_444594 + ((v20) * 4)) = 512;
    M32(D4A_TBL_444584 + ((v20) * 4)) = v27 + 770;
    v28 = 0;
    v21 = (unsigned int)(31 * M32(D4A_TBL_4455A4 + (v27) * 4)) >> 5;
    G32(D4A_TBL_45C644) += v17;
    ++G8(D4A_FLAG_45E3A1);
    M32(D4A_TBL_4455A4 + ((v27) * 4)) = v25 + v21;
  }
  else
  {
    G32(D4A_TRI_TOTAL) = 256;
    G32(D4A_RC_RANGE) = (unsigned)d4_div(G32(D4A_RC_RANGE), 0x100u);
    v26 = (unsigned)d4_div(G32(D4A_RC_CODE), G32(D4A_RC_RANGE));
    G32(D4A_TRI_LOW) = v26;
    G32(D4A_TRI_HIGH) = v26 + 1;
    v7 = M32(D4A_TRI_TOTAL + (1) * 4);
    v8 = G32(D4A_RC_RANGE) * (M32(D4A_TRI_TOTAL + (2) * 4) - v7);
    i = (unsigned long)(G32(D4A_RC_CODE) - G32(D4A_RC_RANGE) * v7);
  d4_rcrenorm(c, &i, &v8);
    G32(D4A_RC_CODE) = (int)i;
    G32(D4A_RC_RANGE) = v8;
    if ( G8(D4A_FLAG_45E39F) )
      G32(D4A_TBL_45C668) += 37440;
    d4_ppm_eval_cost(c, v2, v26);
  }
  v22 = (unsigned char)G8(D4A_FLAG_45E3A1)++;
  M32(D4A_TBL_444584 + ((v22) * 4)) = v3;
  M32(D4A_TBL_444594 + ((v22) * 4)) = v2;
  M8(D4A_TBL_45AE34 + v22) = v26;
  if ( v28 )
  {
    v23 = G32(D4A_COST_TMP);
    M32(D4A_TBL_4451A4 + ((G32(D4A_TBL_45C650)) * 4)) = G32(D4A_COST_TMP);
    G32(D4A_TBL_45C644) += v23;
    M32(D4A_TBL_4455A4 + ((v27) * 4)) = G32(D4A_COST_TMP) + ((unsigned int)(31 * M32(D4A_TBL_4455A4 + (v27) * 4)) >> 5);
  }
  M32(D4A_TBL_445DAC + ((v27) * 4)) = G32(D4A_COST_TMP) + ((unsigned int)(31 * M32(D4A_TBL_445DAC + (v27) * 4)) >> 5);
  return v26;
}

#pragma GCC pop_options

/* See the note above d4_decode_ppm: same -O2 miscompile workaround. */
#pragma GCC push_options
#if defined(__GNUC__) && __GNUC__ >= 12
#pragma GCC optimize ("-ftrivial-auto-var-init=zero")
#endif
static unsigned char d4_ppm_decode_sym(dec4_ctx *c, int a1)
{
  unsigned int v1;
  int v2;
  unsigned int v3;
  unsigned int v4;
  unsigned int v5;
  unsigned int j;
  signed int v7;
  int v8;
  unsigned int v9;
  int v10;
  int v11;
  int v12;
  unsigned int v13;
  unsigned int k;
  unsigned char v15;
  int v16;
  int v17;
  unsigned int v19;
  int v20;
  unsigned int v21;
  int v22;
  unsigned int v23;
  unsigned int l;
  unsigned char v25;
  int v26;
  int v27;
  unsigned short v28;
  unsigned int v29;
  unsigned int v30;
  unsigned int v31;
  signed int v32;
  unsigned int v33;
  unsigned short v34;
  unsigned int v35;
  int v36;
  unsigned int v37;
  unsigned int m;
  unsigned char v39;
  int v40;
  int v41;
  int v42;
  int v43;
  int v44;
  int v45 = 0;
  int v46;
  int v47;
  unsigned int v48;
  int v49 = 0;
  unsigned int v50;
  int v51;
  signed int v52;
  unsigned int v53;
  unsigned char v54;
  unsigned int v55;
  int v56;
  int v57;
  unsigned int v58;
  unsigned int i;
  unsigned char v60;
  int v61;
  int v62;
  unsigned int v63;
  char v64[350];
  char _104[350];
  char _160[350];
  int v67;
  unsigned int v68;
  unsigned int v69;
  unsigned int v70;
  unsigned int v71;
  unsigned int v72;
  unsigned int v73;
  unsigned int v74;
  unsigned int v75;
  int v76;
  int v77;
  unsigned int v78;
  int v79;
  unsigned int v80;
  int v81;
  unsigned int v82;
  unsigned int v83;
  unsigned char v84;

  /* Hex-Rays lost the definition of v63: it is the incoming context
   * argument (the asm loads dword_45C690, which the caller sets to a2). */
  v63 = a1;
  v70 = (unsigned char)G8(D4A_HIST_LEN);
  v80 = (c->fenwick1 + (v63 << 9));
  a1 = (a1 & ~0x0000FFFFU) | ((M16(v80)) & 0x0000FFFFU);
  v67 = M16(c->ppm_tot + 2 * v63);
#ifdef D42257DBG
  if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI) {
    unsigned q2;
    fprintf(stderr, "2257 out=%u ctx=%u tot=%u v67=%u e39c=%u code=%u range=%u\n",
        G32(D4A_OUT_POS), v63, (unsigned)(unsigned short)a1, (unsigned)v67,
        v70, G32(D4A_RC_CODE), G32(D4A_RC_RANGE));
    fprintf(stderr, "PBC:");
    for (q2 = 0; q2 < 256; q2++) fprintf(stderr, "%s%u", (q2 % 16) ? "," : "\n", (unsigned)M16(v80 + 2 * q2));
    fprintf(stderr, "\n");
  }
#endif
  v1 = 0;
  if ( (unsigned short)a1 <= (unsigned int)(M32(D4A_TBL_45BA3C + ((v63) * 4)) * v67) )
  {
    v2 = v70;
    v80 = ((G32(D4A_PPM_CTX) << 9) + c->fenwick1);
    v79 = (G32(D4A_PPM_CTX) << 9) + c->fenwick2;
    a1 = (a1 & ~0x0000FFFFU) | ((M16(v80)) & 0x0000FFFFU);
    while ( v2 )
    {
      v3 = (v79 + 2 * M8(v2-- + 4578669));
      a1 = (a1 & ~0x0000FFFFU) | ((a1 - M16(v3)) & 0x0000FFFFU);
    }
LABEL_5:
    G32(D4A_TRI_TOTAL) = (unsigned short)a1;
    G32(D4A_RC_RANGE) = (unsigned)d4_div(G32(D4A_RC_RANGE), (unsigned int)(unsigned short)a1);
    v83 = (unsigned)d4_div(G32(D4A_RC_CODE), G32(D4A_RC_RANGE));
LABEL_73:
    if ( v70 )
    {
      v42 = v70;
      v43 = 2 * v70;
      v72 = 4 * v70;
      do
      {
        v44 = 2 * M8(v42 + 4578669);
        v72 -= 4;
        v67 = v44;
        L32(_104, v72) = v80 + v44;
        v43 -= 2;
        --v42;
        L16(_160, v43) = M16(v67 + v79);
      }
      while ( v42 );
      v68 = 128;
      v45 = a1 - v83;
      v69 = v80;
      do
      {
        v81 = 0;
        v73 = 0;
        v46 = 0;
        v47 = 0;
        v48 = (v80 + (2 * v68));
        while ( 1 )
        {
          v49 = (v49 & ~0xFFFF0000U) | (((HIWORD(v73)) << 16) & 0xFFFF0000U);
          if ( v73 >= v70 )
            break;
          if ( (unsigned int)v48 > L32(_104, v46) )
          {
            v49 = (v49 & ~0x0000FFFFU) | ((L16(_160, v47)) & 0x0000FFFFU);
            v81 += v49;
          }
          ++v73;
          v46 += 4;
          v47 += 2;
        }
        if ( M16(v48) < (unsigned short)v81 + (unsigned short)v45 )
        {
          v74 = 2 * v70;
          v50 = 0;
          v75 = 4 * v70;
          while ( v50 < v70 )
          {
            if ( (unsigned int)v48 > L32(_104, 4 * v50) )
            {
              v75 -= 4;
              v51 = L32(_104, v75);
              v74 -= 2;
              L32(_104, 4 * v50--) = v51;
              v51 = (v51 & ~0x0000FFFFU) | ((L16(_160, v74)) & 0x0000FFFFU);
              --v70;
              L16(_160, 2 * v50 + 2) = v51;
            }
            ++v50;
          }
          v80 = v48;
          v48 = (unsigned long)(M16(v48));
          v45 -= (signed int)(v48 + (-v81));
        }
        v68 >>= 1;
      }
      while ( v68 );
      v52 = v80 - v69;
    }
    else
    {
      v53 = v80;
      v54 = 128;
      v45 = (v45 & ~0x0000FFFFU) | ((a1 - v83) & 0x0000FFFFU);
      do
      {
        v55 = (v80 + (2 * v54));
        if ( (unsigned short)v45 > M16(v55) )
        {
          v80 += 2 * v54;
          v45 = (v45 & ~0x0000FFFFU) | ((v45 - M16(v55)) & 0x0000FFFFU);
        }
        v54 >>= 1;
      }
      while ( v54 );
      v52 = v80 - v53;
    }
    v70 = v52 >> 1;
    G32(D4A_TRI_HIGH) = (unsigned short)(v83 + v45);
    v56 = G32(D4A_TRI_HIGH) - M16(v79 + 2 * (v52 >> 1));
    G32(D4A_TRI_LOW) = v56;
    G32(D4A_COST_TMP) += (unsigned short)M16(D4A_COST_LUT + ((((G32(D4A_TRI_HIGH) - v56) << 14) / (unsigned int)G32(D4A_TRI_TOTAL)) * 2));
#ifdef D4C648DBG
    if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
      fprintf(stderr, "C648 S1 c684=%u v56=%d c67c=%u add=%u\n", G32(D4A_TRI_HIGH), v56, G32(D4A_TRI_TOTAL), (unsigned)(unsigned short)M16(D4A_COST_LUT + ((((G32(D4A_TRI_HIGH) - v56) << 14) / (unsigned int)G32(D4A_TRI_TOTAL)) * 2)));
#endif
    v57 = M32(D4A_TRI_TOTAL + (1) * 4);
    v58 = G32(D4A_RC_RANGE) * (M32(D4A_TRI_TOTAL + (2) * 4) - v57);
    i = (unsigned long)(G32(D4A_RC_CODE) - G32(D4A_RC_RANGE) * v57);
  d4_rcrenorm(c, &i, &v58);
    G32(D4A_RC_CODE) = (int)i;
    G32(D4A_RC_RANGE) = v58;
    if ( v1 )
      M32(v1) += (unsigned int)(0x2000 - M32(v1)) >> 6;
    return v70;
  }
  v4 = 0;
  v79 = (v63 << 9) + c->fenwick2;
  while ( v4 < v70 )
  {
    v5 = (v79 + 2 * (unsigned char)M8(D4A_HIST_SYMS + (v4++)));
    a1 = (a1 & ~0x0000FFFFU) | ((a1 - M16(v5)) & 0x0000FFFFU);
  }
  if ( 512 >> G8(D4A_PHASE) > v63 )
  {
    v84 = M8(D4A_TBL_45E086 + (v63));
    for ( j = 0; j < v70 && M8(D4A_HIST_SYMS + (j)) != v84; ++j )
      ;
    if ( j == v70 )
    {
      v78 = M16(v79 + 2 * v84);
      v77 = M32(D4A_TBL_42748D + (((unsigned char)M8(D4A_RUN_CNT + (v63))) * 4));
      v77 = 16 * (v77 + v78) / ((unsigned short)a1 + (unsigned int)M16(c->ppm_tot + 2 * v63) + v77);
      if ( v78 <= 0x600 )
      {
        if ( v78 > 0x100 )
        {
          v7 = 48;
        }
        else if ( v78 > 0x40 )
        {
          v7 = 32;
        }
        else
        {
          v7 = 16;
        }
        v77 |= v7;
      }
      if ( (v63 & 0xC0) == 192 )
        v77 = (v77 & ~0x000000FFU) | ((v77 | 0x40) & 0x000000FFU);
      if ( (v84 & 0xC0) == 0xC0 )   /* asm 0x412410: and 0xc0 / cmp 0xc0 */
        v77 |= 128 << (v84 == 0xFF);
      v8 = v77 | 0x200;   /* asm: or ch,2 (Hex-Rays wrote a HIWORD form) */
      if ( G8(D4A_PHASE) )
      {
        if ( (unsigned int)(G32(D4A_DICT_POS) - M32(D4A_TBL_4455AC + ((v84) * 4))) > 0x40 )
          goto LABEL_31;
      }
      else if ( !M8(D4A_HASH_ENABLE + (v84)) )
      {
        goto LABEL_31;
      }
      v77 = v8;
LABEL_31:
      if ( M32(D4A_LAST_SEEN + ((v84) * 4)) == G32(D4A_OUT_COUNT) )
        v77 |= 0x400;   /* asm: or byte [var_20+1],4 */
      G32(D4A_RC_RANGE) = (unsigned)d4_div(G32(D4A_RC_RANGE), 0x2000u);
      v9 = D4A_TBL_4558E4 + ((v77) * 4);   /* Hex-Rays took the value; the asm uses the address */
      v77 = (unsigned short)((unsigned)d4_div(G32(D4A_RC_CODE), G32(D4A_RC_RANGE))) < (unsigned int)M32(v9);
      if ( v77 )
      {
        v10 = M32(v9);
        G32(D4A_TRI_LOW) = 0;
        G32(D4A_TRI_HIGH) = v10;
      }
      else
      {
        G32(D4A_TRI_HIGH) = 0x2000;
        M8(D4A_HIST_SYMS + (v70)) = v84;
        a1 -= v78;
        v11 = M32(v9);
        ++v70;
        G32(D4A_TRI_LOW) = v11;
      }
      G32(D4A_COST_TMP) += (unsigned short)M16(D4A_COST_LUT + ((2 * (G32(D4A_TRI_HIGH) - G32(D4A_TRI_LOW))) * 2));
#ifdef D4C648DBG
    if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
      fprintf(stderr, "C648 S2 c684=%u c680=%u add=%u\n", G32(D4A_TRI_HIGH), G32(D4A_TRI_LOW), (unsigned)(unsigned short)M16(D4A_COST_LUT + ((2 * (G32(D4A_TRI_HIGH) - G32(D4A_TRI_LOW))) * 2)));
#endif
      v12 = M32(D4A_TRI_TOTAL + (1) * 4);
      v13 = G32(D4A_RC_RANGE) * (M32(D4A_TRI_TOTAL + (2) * 4) - v12);
      k = (unsigned long)(G32(D4A_RC_CODE) - G32(D4A_RC_RANGE) * v12);
  d4_rcrenorm(c, &k, &v13);
      G32(D4A_RC_CODE) = (int)k;
      G32(D4A_RC_RANGE) = v13;
      if ( v77 )
        M32(v9) += (unsigned int)(0x2000 - M32(v9)) >> 6;
      else
        M32(v9) -= (unsigned int)M32(v9) >> 6;
      if ( v77 )
        return v84;
      goto LABEL_43;
    }
  }
LABEL_43:
  if ( !(unsigned short)a1 )
  {
    v19 = ((G32(D4A_PPM_CTX) << 9) + c->fenwick1);
    v80 = v19;
    v79 = (G32(D4A_PPM_CTX) << 9) + c->fenwick2;
    a1 = (a1 & ~0x0000FFFFU) | ((M16(v19)) & 0x0000FFFFU);
    v20 = v70;
    while ( v20 )
    {
      v21 = (v79 + 2 * M8(v20-- + 4578669));
      a1 = (a1 & ~0x0000FFFFU) | ((a1 - M16(v21)) & 0x0000FFFFU);
    }
    goto LABEL_5;
  }
  if ( v63 >= 0x200 || (unsigned short)a1 >= 0x2000u )
  {
    G32(D4A_TRI_TOTAL) = M16(c->ppm_tot + 2 * v63) + (unsigned short)a1;
  }
  else
  {
    v1 = (D4A_TBL_453464)
       + 4 * (unsigned)d4_div(32u * M16(c->ppm_tot + 2 * v63),
                              (unsigned short)a1 + M16(c->ppm_tot + 2 * v63));
    G32(D4A_TRI_TOTAL) = (unsigned)d4_div((unsigned int)(unsigned short)a1 << 13,
                                     M32(v1)) + 1;
    if ( (unsigned int)G32(D4A_TRI_TOTAL) >= 0x4000 )
      G32(D4A_TRI_TOTAL) = 0x3FFF;
  }
  G32(D4A_RC_RANGE) = (unsigned)d4_div(G32(D4A_RC_RANGE), (unsigned int)G32(D4A_TRI_TOTAL));
  v83 = (unsigned)d4_div(G32(D4A_RC_CODE), G32(D4A_RC_RANGE));
  if ( (unsigned short)((unsigned)d4_div(G32(D4A_RC_CODE), G32(D4A_RC_RANGE))) < (unsigned short)a1 )
    goto LABEL_73;
  memset(v64, 0, 0x100u);
  while ( v70 )
    v64[(unsigned char)M8(D4A_HIST_SYMS + (--v70))] = 1;
  G32(D4A_TRI_LOW) = (unsigned short)a1;
  G32(D4A_TRI_HIGH) = G32(D4A_TRI_TOTAL);
  G32(D4A_COST_TMP) += (unsigned short)M16(D4A_COST_LUT + ((((G32(D4A_TRI_TOTAL) - (unsigned int)(unsigned short)a1) << 14)
                                              / G32(D4A_TRI_TOTAL)) * 2));
#ifdef D4C648DBG
    if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
      fprintf(stderr, "C648 S3 c67c=%u a1lo=%u add=%u\n", G32(D4A_TRI_TOTAL), (unsigned)(unsigned short)a1, (unsigned)(unsigned short)M16(D4A_COST_LUT + ((((G32(D4A_TRI_TOTAL) - (unsigned int)(unsigned short)a1) << 14) / G32(D4A_TRI_TOTAL)) * 2)));
#endif
  v22 = M32(D4A_TRI_TOTAL + (1) * 4);
  v23 = G32(D4A_RC_RANGE) * (M32(D4A_TRI_TOTAL + (2) * 4) - v22);
  l = (unsigned long)(G32(D4A_RC_CODE) - G32(D4A_RC_RANGE) * v22);
  d4_rcrenorm(c, &l, &v23);
  G32(D4A_RC_CODE) = (int)l;
  G32(D4A_RC_RANGE) = v23;
  v28 = 0;
  v71 = c->fenwick2 + (v63 << 9);
  v76 = (G32(D4A_PPM_CTX) << 9) + c->fenwick2;
  v29 = (c->fenwick2 + (v63 << 9));
  v30 = ((G32(D4A_PPM_CTX) << 9) + c->fenwick2);
  v31 = 0;
  do
  {
    if ( M16(v29) || v64[v31] )
      v28 += M16(v30);
    /* asm 0x41274e/0x412751: both tables advance one u16 entry (+2 bytes) */
    v29 += 2;
    ++v31;
    v30 += 2;
  }
  while ( v31 < 0x100 );
  v32 = 256;
  G32(D4A_TRI_TOTAL) = M16(c->fenwick1 + (G32(D4A_PPM_CTX) << 9)) - v28;
#ifdef D4ZDIVDBG
  if (G32(D4A_TRI_TOTAL) == 0) {
    unsigned qz;
    fprintf(stderr, "ZDIV out=%u v63=%u c690=%u base=%u v28=%u e39c=%u\n",
        G32(D4A_OUT_POS), v63, G32(D4A_PPM_CTX),
        (unsigned)M16(c->fenwick1 + (G32(D4A_PPM_CTX) << 9)), v28, G8(D4A_HIST_LEN));
    fprintf(stderr, "ZDIVH:");
    for (qz = 0; qz < 16; qz++) fprintf(stderr, " %u", (unsigned)M8(D4A_HIST_SYMS + qz));
    fprintf(stderr, "\n");
  }
#endif
  v33 = G32(D4A_RC_RANGE) / (unsigned int)G32(D4A_TRI_TOTAL);
  G32(D4A_RC_RANGE) = v33;
  v82 = (unsigned)d4_div(G32(D4A_RC_CODE), v33);
  v34 = 0;
  v35 = (v71 + 512);
  do
  {
    --v32;
    v35 -= 2;   /* asm 0x4127a4/0x4127a5: u16 table, two byte steps */
    if ( !M16(v35) && !v64[v32] )
      v34 += M16(v76 + 2 * v32);
  }
  while ( v34 <= (unsigned short)v82 );
  G32(D4A_TRI_HIGH) = v34;
  G32(D4A_TRI_LOW) = v34 - M16(v76 + 2 * v32);
  G32(D4A_COST_TMP) += (unsigned short)M16(D4A_COST_LUT + (((((unsigned int)v34 - G32(D4A_TRI_LOW)) << 14) / G32(D4A_TRI_TOTAL)) * 2));
#ifdef D4C648DBG
    if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
      fprintf(stderr, "C648 S4 v34=%u c680=%u c67c=%u add=%u\n", v34, G32(D4A_TRI_LOW), G32(D4A_TRI_TOTAL), (unsigned)(unsigned short)M16(D4A_COST_LUT + (((((unsigned int)v34 - G32(D4A_TRI_LOW)) << 14) / G32(D4A_TRI_TOTAL)) * 2)));
#endif
  v36 = M32(D4A_TRI_TOTAL + (1) * 4);
  v37 = G32(D4A_RC_RANGE) * (M32(D4A_TRI_TOTAL + (2) * 4) - v36);
  m = (unsigned long)(G32(D4A_RC_CODE) - G32(D4A_RC_RANGE) * v36);
  d4_rcrenorm(c, &m, &v37);
  G32(D4A_RC_CODE) = (int)m;
  G32(D4A_RC_RANGE) = v37;
  if ( v1 )
    M32(v1) -= M32(v1) >> 6;
  return v32;
}

#pragma GCC pop_options

static void d4_ppm_eval_cost(dec4_ctx *c, unsigned int a1, unsigned char a2)
{
#ifdef D4C648DBG
  if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
    fprintf(stderr, "C648CALL out=%u a1=%u a2=%u ra=%p\n", G32(D4A_OUT_POS), a1, (unsigned)a2,
        __builtin_return_address(0));
#endif
  unsigned int v2;
  unsigned int v3;
  unsigned int v4;
  unsigned int v5;
  int v6;
  unsigned int v7;
  int v8;
  unsigned int v9;
  signed int v10;
  unsigned int v11 = 0;
  unsigned int result;
  int v13;
  int v14;
  int v15;
  unsigned int v16;
  unsigned int v17;
  unsigned int v18;
  unsigned short v19;
  unsigned int v20;
  unsigned int v21 = 0;
  unsigned int v22;
  unsigned int v23;
  int v24;
  unsigned int v25;
  unsigned int v26;
  unsigned int v27;
  unsigned int v28;
  char v29[256];
  unsigned int v30;
  unsigned int v31;
  unsigned int v32;
  int v33;
  unsigned char v34;
  unsigned char v35;

  v2 = a1;
  v35 = a2;
  v3 = M16(c->ppm_tot + 2 * a1) * M32(D4A_TBL_45BA3C + (a1) * 4);
  v32 = 0;
  v4 = (unsigned char)G8(D4A_HIST_LEN);
  if ( M16((a1 << 9) + c->fenwick1) <= v3 )
  {
    v2 = G32(D4A_PPM_CTX);
    v31 = ((G32(D4A_PPM_CTX) << 9) + c->fenwick2);
LABEL_50:
    v19 = M16(c->fenwick1 + (v2 << 9));
    if ( v2 >= 0x301 )
    {
      if ( v4 )
      {
        if ( v32 )
        {
          memset(v29, 0, 0x100u);
          do
          {
            v24 = M8(v4-- + 4578669);
            v29[v24] = 1;
          }
          while ( v4 );
          v25 = v32;
          v26 = 0;
          /* asm 0x412e6c..0x412e72: base is v32 + ((v31-v32)>>1)*2 and the
           * table walk advances 2 bytes per step (256 words, 0x200 bytes). */
          v27 = (unsigned int)(v32 + ((((int)(v31 - v32)) >> 1) * 2));
          do
          {
            /* asm 0x412E7B+: the bitmask lives in stack v29[256] (var_118),
             * not at arena address v26. */
            if ( M16(v25) || v29[v26] )
              v19 -= M16(v27);
            v27 += 2;
            v25 += 2;
            ++v26;
          }
          while ( v25 < v32 + 0x200 );
        }
        else
        {
          do
          {
            v28 = (v31 + ((M8(v4-- + 4578669)) << 1));
            v19 -= M16(v28);
          }
          while ( v4 );
        }
      }
      else if ( v32 )
      {
        v22 = v32;
        /* asm 0x412e0c..0x412e1b: v32 + ((v31-v32)>>1)*2, 2-byte stride. */
        v23 = (unsigned int)(v32 + ((((int)(v31 - v32)) >> 1) * 2));
        do
        {
          if ( M16(v22) )
            v19 -= M16(v23);
          v22 += 2;
          v23 += 2;
        }
        while ( v22 < v32 + 0x200 );
      }
    }
    else
    {
      while ( v4 )
      {
        v20 = (v31 + ((M8(v4-- + 4578669)) << 1));
        v19 -= M16(v20);
      }
      if ( v2 >= 0x200 || v19 >= 0x2000u )
      {
        v21 = (v21 & ~0x0000FFFFU) | ((v19 + M16(c->ppm_tot + 2 * v2)) & 0x0000FFFFU);
      }
      else
      {
        v21 = ((unsigned int)v19 << 13)
            / (M32(D4A_TBL_453464 + (((32 * M16(c->ppm_tot + 2 * v2) / (v19 + M16(c->ppm_tot + 2 * v2) ? v19 + M16(c->ppm_tot + 2 * v2) : 1)) & 31) * 4)) ? M32(D4A_TBL_453464 + (((32 * M16(c->ppm_tot + 2 * v2) / (v19 + M16(c->ppm_tot + 2 * v2) ? v19 + M16(c->ppm_tot + 2 * v2) : 1)) & 31) * 4)) : 1)
            + 1;
        if ( v21 >= 0x4000 )
          v21 = (v21 & ~0x0000FFFFU) | ((0x3FFF) & 0x0000FFFFU);
      }
      v19 = v21;
    }
    result = (unsigned short)M16(D4A_COST_LUT + ((((unsigned int)M16(v31 + ((v35) * 2)) << 14) / v19) * 2));
    G32(D4A_COST_TMP) += (int)result;
#ifdef D4C648DBG
    if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
      fprintf(stderr, "C648 S5 a1=%u a2=%u v2=%u c690=%u v31=%u v19=%u m16=%u m16a=%u idx=%u result=%d e39c=%u h0=%u h1=%u init=%u dec=%u\n", a1, (unsigned)v35, v2, G32(D4A_PPM_CTX), v31, (unsigned)v19, (unsigned)M16(v31 + ((v35) * 2)), (unsigned)(v31 + ((v35) * 2)), ((unsigned)(((unsigned int)M16(v31 + ((v35) * 2)) << 14) / v19)), result, G8(D4A_HIST_LEN), (unsigned)M8(D4A_HIST_SYMS), (unsigned)M8(D4A_HIST_SYMS + 1), (unsigned)M16(c->fenwick1 + (v2 << 9)), (unsigned)M16(v31 + (((unsigned char)M8(D4A_HIST_SYMS)) << 1)));
#endif
  return;
  }
  v31 = ((a1 << 9) + c->fenwick2);
  v33 = 0xFFFF;
  if ( a1 >= 512 >> G8(D4A_PHASE) )
    goto LABEL_37;
  v5 = 0;
  v34 = M8(D4A_TBL_45E086 + (a1));
  while ( v5 < (unsigned char)G8(D4A_HIST_LEN) && M8(D4A_HIST_SYMS + (v5)) != v34 )
    ++v5;
  if ( v5 != (unsigned char)G8(D4A_HIST_LEN) )
  {
LABEL_37:
    if ( !M16(v31 + ((v35) * 2)) )
    {
      if ( (unsigned short)v33 == 0xFFFF )   /* asm 0x412c93: cmpw $0xffff */
      {
        v15 = v2 << 9;
        v15 = (v15 & ~0x0000FFFFU) | ((M16(c->fenwick1 + (v2 << 9))) & 0x0000FFFFU);
        v16 = 0;
        v33 = v15;
        while ( v16 < v4 )
        {
          v17 = (v31 + (((unsigned char)M8(D4A_HIST_SYMS + (v16))) << 1));
          v17 = (unsigned long)(M16(v17));
          ++v16;
          v33 -= (signed int)v17;
        }
      }
      if ( (unsigned short)v33 )
      {
        if ( v2 >= 0x200 || (unsigned short)v33 >= 0x2000u )
        {
          v18 = (unsigned short)v33 + M16(c->ppm_tot + 2 * v2);
        }
        else
        {
          v18 = ((unsigned int)(unsigned short)v33 << 13)
              / M32(D4A_TBL_453464 + ((32
                           * M16(c->ppm_tot + 2 * v2)
                           / ((unsigned short)v33 + M16(c->ppm_tot + 2 * v2))) * 4))
              + 1;
          if ( v18 >= 0x4000 )
            v18 = 0x3FFF;
        }
        G32(D4A_COST_TMP) += (unsigned short)M16(D4A_COST_LUT + ((((v18 - (unsigned short)v33) << 14) / v18) * 2));
#ifdef D4C648DBG
    if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
      fprintf(stderr, "C648 S6 v18=%u v33=%u idx=%u add=%u\n", v18, (unsigned)(unsigned short)v33, ((unsigned)((((v18 - (unsigned short)v33) << 14) / v18))), (unsigned)(unsigned short)M16(D4A_COST_LUT + ((((v18 - (unsigned short)v33) << 14) / v18) * 2)));
#endif
        v32 = v31;
      }
      v2 = G32(D4A_PPM_CTX);
      v31 = ((G32(D4A_PPM_CTX) << 9) + c->fenwick2);
    }
    goto LABEL_50;
  }
  v30 = M16(v31 + ((v34) * 2));
  v6 = a1 << 9;
  v6 = (v6 & ~0x0000FFFFU) | ((M16(c->fenwick1 + (v2 << 9))) & 0x0000FFFFU);
  v33 = v6;
  while ( v5 )
  {
    v7 = (v31 + ((M8(v5 + 4578669)) << 1));
    v7 = (unsigned long)(M16(v7));
    --v5;
    v33 -= (signed int)v7;
  }
  v8 = M32(D4A_TBL_42748D + (((unsigned char)M8(D4A_RUN_CNT + (v2))) * 4));
  v9 = 16 * (v8 + v30) / ((unsigned short)v33 + (unsigned int)M16(c->ppm_tot + 2 * v2) + v8);
  if ( v30 <= 0x600 )
  {
    if ( v30 > 0x100 )
    {
      v10 = 48;
    }
    else if ( v30 > 0x40 )
    {
      v10 = 32;
    }
    else
    {
      v10 = 16;
    }
    v9 |= v10;
  }
  if ( (v2 & 0xC0) == 192 )
    v9 = (v9 & ~0x000000FFU) | ((v9 | 0x40) & 0x000000FFU);
  if ( (v34 & 0xC0) == 0xC0 )   /* asm 0x412BA2: and 0xc0 / cmp 0xc0 */
    v9 |= 128 << (v34 == 0xFF);
  v11 = (v11 & ~0xFFFF0000U) | (((HIWORD(v9)) << 16) & 0xFFFF0000U);
  if ( G8(D4A_PHASE) )
  {
    if ( (unsigned int)(G32(D4A_DICT_POS) - M32(D4A_TBL_4455AC + ((v34) * 4))) > 0x40 )
      goto LABEL_28;
  }
  else if ( !M8(D4A_HASH_ENABLE + (v34)) )
  {
    goto LABEL_28;
  }
  v11 = (v11 & ~0x0000FFFFU) | ((v9 | 0x200) & 0x0000FFFFU);
  v9 = v11;
LABEL_28:
  if ( M32(D4A_LAST_SEEN + ((v34) * 4)) == G32(D4A_OUT_COUNT) )
    v9 |= 0x400;   /* BYTE1(v9) |= 4 */
  result = D4A_TBL_4558E4 + ((v9) * 4);
  v13 = v34 == v35;
  if ( v34 == v35 )
  {
    v14 = M32(result);
  }
  else
  {
    M8(D4A_HIST_SYMS + ((unsigned char)G8(D4A_HIST_LEN))) = v34;
    v33 -= v30;
    v14 = 0x2000 - M32(result);
    ++v4;
  }
  v30 = v14;
  G32(D4A_COST_TMP) += (unsigned short)M16(D4A_COST_LUT + ((2 * v14) * 2));
#ifdef D4C648DBG
    if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
      fprintf(stderr, "C648 S7 v14=%d add=%u\n", v14, (unsigned)(unsigned short)M16(D4A_COST_LUT + ((2 * v14) * 2)));
#endif
  if ( v13 )
    M32(result) += (unsigned int)(0x2000 - M32(result)) >> 6;
  else
    M32(result) -= (unsigned int)M32(result) >> 6;
  if ( !v13 )
    goto LABEL_37;
  return;
}

static int d4_trial_record_cost(dec4_ctx *c, unsigned int a1, int a2, unsigned char a3)
{
#ifdef D4ED2DBG
  fprintf(stderr, "ED2 out=%u a1=%u a2=%d a3=%u\n",
      G32(D4A_OUT_POS), a1, a2, (unsigned)a3);
#endif
  int v3;
  int v4;
  int v5;
  int result;

  v3 = a1;
  v5 = a2;
  G32(D4A_PPM_CTX) = a2;
  G32(D4A_COST_TMP) = 0;
  #ifdef D4C648DBG
      if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
        fprintf(stderr, "C648R out=%u\n", G32(D4A_OUT_POS));
  #endif
  #ifdef D4C648DBG
    if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
      fprintf(stderr, "C648SITE D-412ED2 out=%u\n", G32(D4A_OUT_POS));
  #endif
  d4_ppm_eval_cost(c, a1, a3);
  v4 = (unsigned char)G8(D4A_FLAG_45E3A1)++;
  M32(D4A_TBL_444584 + ((v4) * 4)) = G32(D4A_PPM_CTX);
  M32(D4A_TBL_444594 + ((v4) * 4)) = v3;
  M8(D4A_TBL_45AE34 + v4) = a3;
  M32(D4A_TBL_4455A4 + ((v5 == 769) * 4)) = ((unsigned int)(31 * M32(D4A_TBL_4455A4 + (v5 == 769) * 4)) >> 5) + G32(D4A_COST_TMP);
  result = v5 == 769;
  M32(D4A_TBL_445DAC + ((result) * 4)) = ((unsigned int)(31 * M32(D4A_TBL_445DAC + (result) * 4)) >> 5) + G32(D4A_COST_TMP);
  return result;
}

static int d4_cost_accum_update(dec4_ctx *c, unsigned int a1, int a2, unsigned char a3)
{
  int result;

  G32(D4A_PPM_CTX) = a2;
  G32(D4A_COST_TMP) = 0;
  #ifdef D4C648DBG
      if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
        fprintf(stderr, "C648R out=%u\n", G32(D4A_OUT_POS));
  #endif
  #ifdef D4C648DBG
    if (G32(D4A_OUT_POS) >= D4WLO && G32(D4A_OUT_POS) < D4WHI)
      fprintf(stderr, "C648SITE E-412F80 out=%u\n", G32(D4A_OUT_POS));
  #endif
  d4_ppm_eval_cost(c, a1, a3);
  result = (int)G32(D4A_COST_TMP);
  M32(D4A_COST_ACCUM) = G32(D4A_COST_TMP) + ((31 * M32(D4A_COST_ACCUM)) >> 5);
  return result;
}

static int d4_pred_cache_reset(dec4_ctx *c)
{
  int result;

  d4_fill8(c, D4A_TBL_43D72C, 0, 0x1E40u);
  G32(D4A_TBL_43D734) = 2;
  result = 0;
  do
  {
    result += 31;
    M32(D4A_TBL_43F36C + ((result) * 4)) = -1;
    M32(D4A_TBL_43F370 + ((result) * 4)) = -1;
    M32(D4A_TBL_43F374 + ((result) * 4)) = -1;
  }
  while ( result != 124 );
  return result * 4;
}

static int d4_pred_next(dec4_ctx *c)
{
  int v0;
  UHI64 v1;
  int v2;
  unsigned int v3;
  UHI64 v4;
  char v5;
  signed int v6;
  signed int v7;
  int result;
  char v9;
  int v10;

  if ( G8(D4A_FLAG_45E391) || G32(D4A_TBL_45AE14) >= (unsigned int)G32(D4A_TBL_45ADE4) )
  {
    if ( (unsigned char)G8(D4A_MODEL_ID) >= 7u )
    {
      v3 = D4A_ROW_REGION + ((31 * G32(D4A_MOD_CNT)) * 4);
      M32(v3 + ((3) * 4)) = M32(v3 + ((2) * 4));
      M32(v3 + ((2) * 4)) = M32(v3 + ((1) * 4));
      M32(v3 + ((1) * 4)) = M32(v3 + ((11) * 4)) - M32(v3);
      M32(v3) = M32(v3 + ((11) * 4));
      v4 = M32(v3 + ((3) * 4)) * M32(v3 + ((8) * 4)) + M32(v3 + ((2) * 4)) * M32(v3 + ((7) * 4)) + M32(v3 + ((1) * 4)) * M32(v3 + ((6) * 4)) + M32(v3) * M32(v3 + ((5) * 4)) + 16 * M32(v3 + ((10) * 4)) + G32(D4A_TBL_43D740) * M32(v3 + ((9) * 4));
      /* asm 0x4131E1..0x4131E7 truncates toward zero (see 43DB54 note). */
      M32(D4A_TBL_43DB48 + ((0) * 4)) = ((int)(unsigned int)v4) / 16;
      if ( M32(v3 + ((23) * 4)) <= (unsigned int)M32(v3 + ((24) * 4)) )
        v5 = M32(D4A_TBL_43DB48 + ((0) * 4));
      else
        v5 = M8(v3 + 0x28);   /* asm 0x4131F7: byte [ecx+28h] (dword 10),
                               * not dword [40] */
      G8(D4A_FLAG_45E38B) = v5;
      if ( G8(D4A_MODEL_ID) == 7 )
      {
        G8(D4A_FLAG_45E38B) += -128;
        v2 = G32(D4A_MOD_CNT) + 753;
        if ( M32(v3 + ((28) * 4)) )
        {
          if ( M32(v3 + ((29) * 4)) )
          {
            if ( !M32(v3 + ((30) * 4)) )
              v2 = G32(D4A_MOD_CNT) + 759;
          }
          else
          {
            v2 = G32(D4A_MOD_CNT) + 757;
          }
        }
        else
        {
          v2 = G32(D4A_MOD_CNT) + 755;
        }
      }
      else if ( G8(D4A_MODEL_ID) == 8 )
      {
        /* REVERTED 26-Oct inversion experiment: crashed inside Gmud.dll.
         * Restored original polarity pending asm re-proof. */
        if ( M32(v3 + ((28) * 4)) )
          v6 = G32(D4A_MOD_CNT);
        else
          v6 = 2;
        v2 = v6 + 761;
      }
      else
      {
        if ( M32(v3 + ((28) * 4)) )
          v7 = G32(D4A_MOD_CNT);
        else
          v7 = 4;
        v2 = v7 + 764;
      }
      G8(D4A_PRED_FLAG) = M8(D4A_TBL_43F568 + (G32(D4A_MOD_CNT)));
    }
    else
    {
      M32(D4A_TBL_43DB48 + ((0) * 4)) = 0;
      G32(D4A_TBL_43DB4C) = G32(D4A_TBL_43D738);
      v0 = ((unsigned char)G32(D4A_OUT_COUNT) - (unsigned char)G32(D4A_TBL_43D730)) & 0x7F;
      G32(D4A_TBL_43DB50) = 2 * G32(D4A_TBL_43D738) - G32(D4A_TBL_43D73C);
      G32(D4A_TBL_43F388) = M32(D4A_TBL_43D948 + ((v0) * 4));
      G32(D4A_TBL_43F384) = G32(D4A_TBL_43F380);
      G32(D4A_TBL_43F380) = G32(D4A_TBL_43F37C);
      G32(D4A_TBL_43F37C) = G32(D4A_TBL_43F3A4) - M32(D4A_ROW_REGION + ((0) * 4));
      M32(D4A_ROW_REGION + ((0) * 4)) = G32(D4A_TBL_43F3A4);
      v1 = G32(D4A_TBL_43F384) * G32(D4A_TBL_43F398)
         + G32(D4A_TBL_43F380) * G32(D4A_TBL_43F394)
         + G32(D4A_TBL_43F37C) * G32(D4A_TBL_43F390)
         /* asm 0x413093: imul eax(43F38C), 43F378[0] - not 43F3A4. */
         + M32(D4A_ROW_REGION + ((0) * 4)) * G32(D4A_TBL_43F38C)
         + 16 * G32(D4A_TBL_43F3A0)
         + G32(D4A_TBL_43F388) * G32(D4A_TBL_43F39C);
      /* asm 0x4130E1..0x4130E7 (cdq/shl/sbb/sar) truncates toward zero;
       * an arithmetic >> floors negatives (off by one). v1 is mod 2^32. */
      G32(D4A_TBL_43DB54) = ((int)(unsigned int)v1) / 16;
      G32(D4A_TBL_43DB58) = M32(D4A_TBL_43D948 + ((v0) * 4));
      G32(D4A_TBL_43DB5C) = M32(D4A_TBL_43D948 + ((((unsigned char)G32(D4A_OUT_COUNT) - (unsigned char)G32(D4A_TBL_43D734)) & 0x7F) * 4));
      G8(D4A_FLAG_45E38B) = M32(D4A_TBL_43DB48 + ((G32(D4A_TBL_43D72C)) * 4));
      if ( (unsigned int)G32(D4A_TBL_43D72C) >= 4 )
        G8(D4A_FLAG_45E38B) += G32(D4A_TBL_43D738);
      if ( G8(D4A_MODEL_ID) == 5 )
      {
        v2 = 745;
        G8(D4A_FLAG_45E38B) += -128;
      }
      else
      {
        v2 = 749;
      }
      if ( M32(D4A_ROW_REGION + ((28) * 4)) )
      {
        if ( M32(D4A_ROW_REGION + ((29) * 4)) )
        {
          if ( !M32(D4A_ROW_REGION + ((30) * 4)) )
            v2 += 3;
        }
        else
        {
          v2 += 2;
        }
      }
      else
      {
        ++v2;
      }
    }
    result = G32(D4A_TBL_45AE14);
    if ( G32(D4A_TBL_45AE14) < (unsigned int)G32(D4A_TBL_45ADE4) )
    {
      ++G32(D4A_TBL_45AE14);
      result = c->pred_cache + 8 * result;
      v9 = G8(D4A_PRED_FLAG);
      M32(result) = v2;
      M8(result + 4) = v9;
      M8(result + 5) = G8(D4A_FLAG_45E38B);
    }
  }
  else
  {
    v10 = 8 * G32(D4A_TBL_45AE14)++;
    result = c->pred_cache + v10;
    v2 = M32(c->pred_cache + v10);
    result = (result & ~0x000000FFU) | ((M8(c->pred_cache + v10 + 5)) & 0x000000FFU);
    G8(D4A_PRED_FLAG) = M8(c->pred_cache + v10 + 4);
    G8(D4A_FLAG_45E38B) = result;
  }
  G32(D4A_TRI2_TOTAL_NEXT) = v2;
  return result;
}

static int d4_pred_learn(dec4_ctx *c, int a1, unsigned char a2)
{
  int result = a1;
  unsigned int v2;
  int v3;
  int v4;
  int v5;
  int v6;
  signed int v7;
  signed int v8;
  unsigned int v9;
  unsigned int v10;
  signed int v11;
  int v12;
  unsigned int v13;
  int v14;
  int v15;
  int v16;
  int v17;
  int v18;
  int v19;
  int v20;
  int v21;
  int v22;
  int v23;
  signed int v24;
  int v25;
  signed int v26;
  unsigned int v27;
  int v28;
  int v29;
  int v30;
  /* asm addresses v31..v36 as one 6-dword record ((char*)&v31 + v42);
   * separate locals are not adjacent under GCC -O2 (same class as the
   * vmask13/vmask9 fix), so keep them a real array. */
  int v31_6[6];
  int v37;
  unsigned int v38;
  int v39;
  unsigned int v40;
  unsigned int v41;
  int v42;
  int v43;
  unsigned int v44;
  int v45;
  int v46;
  unsigned char v47;

  v47 = a2;
  if ( G8(D4A_FLAG_45E391) || G32(D4A_TBL_45AE14) >= (unsigned int)G32(D4A_TBL_45ADE4) )
  {
    if ( (unsigned char)G8(D4A_MODEL_ID) < 7u )
    {
      v2 = G32(D4A_ROW_REGION);
      if ( G8(D4A_MODEL_ID) == 6 )
        v3 = (char)result;
      else
        v3 = (unsigned char)result - 128;
      v43 = v3 - G32(D4A_TBL_43D738);
      v4 = 16 * (char)(G32(D4A_TBL_43DB54) - v3);
      M32(D4A_ROW_REGION + ((12) * 4)) += M32(D4A_PERM_TAB + ((16 * (char)(G32(D4A_TBL_43DB54) - v3) & 0xFFF) * 4));
      M32(D4A_ROW_REGION + ((13) * 4)) += M32(D4A_PERM_TAB + (((v4 - M32(D4A_ROW_REGION + (0) * 4)) & 0xFFF) * 4));
      M32(D4A_ROW_REGION + ((14) * 4)) += M32(D4A_PERM_TAB + ((((unsigned short)v4 + LOWORD(M32(D4A_ROW_REGION + (0) * 4))) & 0xFFF) * 4));
      M32(D4A_ROW_REGION + ((15) * 4)) += M32(D4A_PERM_TAB + (((v4 - M32(D4A_ROW_REGION + (1) * 4)) & 0xFFF) * 4));
      M32(D4A_ROW_REGION + ((16) * 4)) += M32(D4A_PERM_TAB + ((((unsigned short)v4 + (unsigned short)M32(D4A_ROW_REGION + (1) * 4)) & 0xFFF) * 4));
      M32(D4A_ROW_REGION + ((17) * 4)) += M32(D4A_PERM_TAB + (((v4 - M32(D4A_ROW_REGION + (2) * 4)) & 0xFFF) * 4));
      M32(D4A_ROW_REGION + ((18) * 4)) += M32(D4A_PERM_TAB + ((((unsigned short)v4 + (unsigned short)M32(D4A_ROW_REGION + (2) * 4)) & 0xFFF) * 4));
      M32(D4A_ROW_REGION + ((19) * 4)) += M32(D4A_PERM_TAB + (((v4 - M32(D4A_ROW_REGION + (3) * 4)) & 0xFFF) * 4));
      M32(D4A_ROW_REGION + ((20) * 4)) += M32(D4A_PERM_TAB + ((((unsigned short)v4 + (unsigned short)M32(D4A_ROW_REGION + (3) * 4)) & 0xFFF) * 4));
      v5 = M32(D4A_PERM_TAB + (((v4 - M32(D4A_ROW_REGION + (4) * 4)) & 0xFFF) * 4));
      v6 = (M32(D4A_ROW_REGION + ((4) * 4)) + v4) & 0xFFF;
      M32(D4A_ROW_REGION + ((21) * 4)) += v5;
      M32(D4A_ROW_REGION + ((22) * 4)) += M32(D4A_PERM_TAB + ((v6) * 4));
      v6 = (v6 & ~0x000000FFU) | ((v3 - LOBYTE(M32(D4A_ROW_REGION + ((10) * 4)))) & 0x000000FFU);
      ++M32(D4A_ROW_REGION + ((27) * 4));
      M32(D4A_ROW_REGION + ((10) * 4)) = v3;
      M32(D4A_ROW_REGION + ((11) * 4)) = (char)v6;
      if ( !(M32(D4A_ROW_REGION + ((27) * 4)) & 0xF) )
      {
        v7 = 1;
        v8 = 0;
        v41 = M32(D4A_ROW_REGION + ((12) * 4));
        M32(D4A_ROW_REGION + ((12) * 4)) = 0;
#ifdef D4SWDBG
        if (G32(D4A_OUT_POS) == 80)
          fprintf(stderr, "SWPRE out=80 e390=%u s13=%d s14=%d s15=%d s16=%d s17=%d s18=%d s19=%d s20=%d s21=%d s22=%d\n",
              G8(D4A_MODEL_ID),
              (int)M32(D4A_ROW_REGION+52), (int)M32(D4A_ROW_REGION+56), (int)M32(D4A_ROW_REGION+60),
              (int)M32(D4A_ROW_REGION+64), (int)M32(D4A_ROW_REGION+68), (int)M32(D4A_ROW_REGION+72),
              (int)M32(D4A_ROW_REGION+76), (int)M32(D4A_ROW_REGION+80), (int)M32(D4A_ROW_REGION+84),
              (int)M32(D4A_ROW_REGION+88));
#endif
        v9 = D4A_ROW_REGION + ((1) * 4);
        do
        {
          if ( M32(v9 + ((12) * 4)) < v41 )
          {
            v41 = M32(v9 + ((12) * 4));
            v8 = v7;
          }
          /* asm 0x4134BD: add eax,4 - dword steps; ++v9 walks bytes and
           * corrupts neighbours with the unaligned zero below. */
          v9 += 4;
          ++v7;
          M32(v9 + ((11) * 4)) = 0;
        }
        while ( v7 < 11 );
#ifdef D4SWDBG
        if (G32(D4A_OUT_POS) == 80)
          fprintf(stderr, "SW out=80 v8=%d\n", v8);
#endif
        switch ( v8 )
        {
          /* asm 004134E8 jumptable: signed compares (jg/jl); M32 is
           * unsigned, so (signed int) casts are required (else > -32
           * never fires and < 32 misfires on negatives). */
          case 1:
            if ( (signed int)M32(D4A_ROW_REGION + ((5) * 4)) > -32 )
              --M32(D4A_ROW_REGION + ((5) * 4));
            break;
          case 2:
            if ( (signed int)M32(D4A_ROW_REGION + ((5) * 4)) < 32 )
              ++M32(D4A_ROW_REGION + ((5) * 4));
            break;
          case 3:
            if ( (signed int)M32(D4A_ROW_REGION + ((6) * 4)) > -32 )
              --M32(D4A_ROW_REGION + ((6) * 4));
            break;
          case 4:
            if ( (signed int)M32(D4A_ROW_REGION + ((6) * 4)) < 32 )
              ++M32(D4A_ROW_REGION + ((6) * 4));
            break;
          case 5:
            if ( (signed int)M32(D4A_ROW_REGION + ((7) * 4)) > -32 )
              --M32(D4A_ROW_REGION + ((7) * 4));
            break;
          case 6:
            if ( (signed int)M32(D4A_ROW_REGION + ((7) * 4)) < 32 )
              ++M32(D4A_ROW_REGION + ((7) * 4));
            break;
          case 7:
            if ( (signed int)M32(D4A_ROW_REGION + ((8) * 4)) > -32 )
            {
              --M32(D4A_ROW_REGION + ((8) * 4));
            }
            break;
          case 8:
            if ( (signed int)M32(D4A_ROW_REGION + ((8) * 4)) < 32 )
              ++M32(D4A_ROW_REGION + ((8) * 4));
            break;
          case 9:
            if ( (signed int)M32(D4A_ROW_REGION + ((8) * 4)) > -32 )
              --M32(D4A_ROW_REGION + ((9) * 4));
            break;
          case 10:
            if ( (signed int)M32(D4A_ROW_REGION + ((8) * 4)) < 32 )
              ++M32(D4A_ROW_REGION + ((9) * 4));
            break;
          default:
            break;
        }
      }
      /* asm 0x413578/0x4135DF/0x4135EC: single-stride 43D948[eax*4]; the
       * 2* factor addressed the wrong slots and starved 412FFA's reads. */
      M32(D4A_TBL_43D948 + ((G32(D4A_OUT_COUNT) & 0x7F)) * 4) = v43;
      if ( !(G32(D4A_OUT_COUNT) & M32(D4A_MASK_LUT + ((G32(D4A_TBL_43D72C)) * 4))) )
      {
        v40 = 0xFFFF;
        G32(D4A_TBL_43D730) = 2;
        G32(D4A_TBL_43D734) = 2;
        v10 = 2;
        v11 = 2;
        do
        {
          v37 = G32(D4A_OUT_COUNT);
          v46 = M32(D4A_TBL_43D948 + ((((unsigned char)G32(D4A_OUT_COUNT) - 1) & 0x7F)) * 4)
              - M32(D4A_TBL_43D948 + ((((unsigned char)G32(D4A_OUT_COUNT) - 1 - (unsigned char)v10) & 0x7F)) * 4);
          v38 = 4 * (v46 & 0xFFF);
          v39 = 4 * (((unsigned char)G32(D4A_OUT_COUNT) - (unsigned char)v10) & 0x7F);
          v46 = M32(D4A_PERM_TAB + ((v38 / 4) * 4))
              + M32(D4A_PERM_TAB + (((M32(D4A_TBL_43D948 + (((G32(D4A_OUT_COUNT) & 0x7F))) * 4)
                            - M32(D4A_TBL_43D948 + ((((unsigned char)G32(D4A_OUT_COUNT) - (unsigned char)v10) & 0x7F)) * 4)) & 0xFFF) * 4));
          v39 = v46;
          v46 = M32(D4A_TBL_43D948 + ((((unsigned char)G32(D4A_OUT_COUNT) - 2) & 0x7F)) * 4)
              - M32(D4A_TBL_43D948 + ((((unsigned char)G32(D4A_OUT_COUNT) - 2 - (unsigned char)v10) & 0x7F)) * 4);
          v12 = M32(D4A_PERM_TAB + ((v46 & 0xFFF) * 4)) + v39;
          M32(D4A_TBL_43D748 + ((v11) * 4)) = v12 + ((unsigned int)(13 * M32(D4A_TBL_43D748 + (v11) * 4)) >> 4);
          v44 = v12;
          if ( M32(D4A_TBL_43D748 + ((v11) * 4)) < (unsigned int)M32(D4A_TBL_43D748 + ((G32(D4A_TBL_43D730)) * 4)) )
            G32(D4A_TBL_43D730) = v10;
          if ( v44 < v40 )
          {
            v40 = v44;
            G32(D4A_TBL_43D734) = v10;
          }
          ++v10;
          ++v11;
        }
        while ( v10 < 0x7C );
      }
      v31_6[0] = v3 - M32(D4A_TBL_43DB48 + ((0) * 4));
      v31_6[1] = v3 - G32(D4A_TBL_43DB4C);
      v31_6[2] = v3 - G32(D4A_TBL_43DB50);
      v31_6[3] = v3 - G32(D4A_TBL_43DB54);
      G32(D4A_TBL_43D72C) = 0;
      v31_6[4] = v43 - G32(D4A_TBL_43DB58);
      v45 = 0;
      v42 = 0;
      v31_6[5] = v43 - G32(D4A_TBL_43DB5C);
      /* asm 0x413731 jumps straight into the body (0x41374A) with v45=0,
       * v42=0 on row R0; the increment/exit-check at 0x413738..0x413748
       * only runs between bodies. 6 bodies: v45=0..5, v42=0..20, rows
       * R0..R5. Row 0's accumulator doubles as threshold[0]. */
      v13 = (D4A_TBL_43DB60);
      do
      {
        /* asm 0x41374A..0x413797: the row holds dwords (M32 at v14*4 and
         * +1024 accumulator), not bytes; BYTE accesses corrupt the model. */
        v14 = (unsigned char)G32(D4A_OUT_COUNT);
        M32((v13 + (1024))) -= M32((v13 + ((v14) * 4)));
        /* asm 0x41375F..0x41376C reads the v31..v36 record off the STACK;
         * routing it through the arena accessor (M32((char*)v31_6+v42))
         * clamps the stack address into the arena tail and yields zeros
         * (v42/4 == v45, so element [v45]). */
        v15 = M32(D4A_SEL_HIST + ((((unsigned int)v31_6[v45]) & 0x1FF) * 4));
        M32((v13 + ((v14) * 4))) = v15;
        M32((v13 + (1024))) += v15;
        /* asm 0x41378C: jge (signed). The accumulator wraps negative on
         * deficit, which an unsigned compare can never see. */
        if ( (signed int)M32((v13 + (1024))) < (signed int)M32(D4A_TBL_43DF60 + ((257 * G32(D4A_TBL_43D72C)) * 4)) )
          G32(D4A_TBL_43D72C) = v45;
        v42 += 4;
        ++v45;
        v13 = v13 + 1028;
      }
      while ( v45 < 6 );
      G32(D4A_TBL_43D73C) = G32(D4A_TBL_43D738);
      G32(D4A_TBL_43D738) = v3;
      /* REVERTED current-row experiment 26-Oct: crashed inside Gmud.dll
       * (0 bytes flushed vs 572416 before). Row-0 absolute base restored
       * pending EDX-entry proof. */
      v16 = M32(D4A_ROW_REGION + ((29) * 4));
      M32(D4A_ROW_REGION + ((28) * 4)) = v47 + (M32(D4A_ROW_REGION + ((28) * 4)) << 8);
      v17 = v47;
      v18 = M32(D4A_ROW_REGION + ((30) * 4));
      M32(D4A_ROW_REGION + ((29) * 4)) = M32(D4A_BYTE_CURVE_0 + ((v47) * 4)) + (v16 << 8);
      v19 = v18 << 8;
      result = M32(D4A_BYTE_CURVE_2 + ((v17) * 4));
      /* asm 0x4137E1 jumps to 0x413A8B which stores to [edx+78h] with
       * edx = the region base (0x43F378 here). It must NOT share LABEL_80
       * below: there v2 is the >=7 region base, but here v2 holds [0]'s
       * VALUE, so the shared store corrupts low arena and [30] never
       * updates (stuck at its -1 init, breaking 412FFA's v2 select).
       * (Row-0 base restored - see note above.) */
      M32(D4A_ROW_REGION + ((30) * 4)) = result + v19;
      return result;
    }
    v20 = 31 * G32(D4A_MOD_CNT);
    M8(D4A_TBL_43F568 + (G32(D4A_MOD_CNT))) = G8(D4A_PRED_FLAG);
    v2 = D4A_ROW_REGION + ((v20) * 4);
    if ( (unsigned char)G8(D4A_MODEL_ID) < 8u )
      v21 = (unsigned char)result - 128;
    else
      v21 = (char)result;
    v22 = 16 * (char)(LOBYTE(M32(D4A_TBL_43DB48 + ((0) * 4))) - v21);
    M32(v2 + ((12) * 4)) += M32(D4A_PERM_TAB + ((16 * (char)(LOBYTE(M32(D4A_TBL_43DB48 + (0) * 4)) - v21) & 0xFFF) * 4));
    M32(v2 + ((13) * 4)) += M32(D4A_PERM_TAB + (((v22 - M32(v2)) & 0xFFF) * 4));
    M32(v2 + ((14) * 4)) += M32(D4A_PERM_TAB + ((((unsigned short)v22 + (unsigned short)M32(v2)) & 0xFFF) * 4));
    M32(v2 + ((15) * 4)) += M32(D4A_PERM_TAB + (((v22 - M32(v2 + ((1) * 4))) & 0xFFF) * 4));
    M32(v2 + ((16) * 4)) += M32(D4A_PERM_TAB + ((((unsigned short)v22 + (unsigned short)M32(v2 + ((1) * 4))) & 0xFFF) * 4));
    M32(v2 + ((17) * 4)) += M32(D4A_PERM_TAB + (((v22 - M32(v2 + ((2) * 4))) & 0xFFF) * 4));
    M32(v2 + ((18) * 4)) += M32(D4A_PERM_TAB + ((((unsigned short)v22 + (unsigned short)M32(v2 + ((2) * 4))) & 0xFFF) * 4));
    M32(v2 + ((19) * 4)) += M32(D4A_PERM_TAB + (((v22 - M32(v2 + ((3) * 4))) & 0xFFF) * 4));
    M32(v2 + ((20) * 4)) += M32(D4A_PERM_TAB + ((((unsigned short)v22 + (unsigned short)M32(v2 + ((3) * 4))) & 0xFFF) * 4));
    M32(v2 + ((21) * 4)) += M32(D4A_PERM_TAB + ((((unsigned short)v22 - (unsigned short)G32(D4A_TBL_43D740)) & 0xFFF) * 4));
    M32(v2 + ((22) * 4)) += M32(D4A_PERM_TAB + ((((unsigned short)G32(D4A_TBL_43D740) + (unsigned short)v22) & 0xFFF) * 4));
    M32(v2 + ((23) * 4)) += M32(D4A_BYTE_CURVE_4 + (((unsigned char)(LOBYTE(M32(D4A_TBL_43DB48 + (0) * 4)) - v21)) * 4));
    M32(v2 + ((24) * 4)) += M32(D4A_BYTE_CURVE_4 + (((unsigned char)(M8(v2 + 0x28) - v21)) * 4));
    v23 = (char)(v21 - M8(v2 + 0x28));   /* asm 0x413922/0x413934: byte
                                          * [edx+28h] (dword 10), not dword [40] */
    M32(v2 + ((11) * 4)) = v23;
    G32(D4A_TBL_43D740) = v23;
    M32(v2 + ((10) * 4)) = v21;
    ++M32(v2 + ((27) * 4));
    /* asm 0x41394A: test BYTE [edx+6Ch],0Fh - low-nibble of the low byte,
     * not a dword test. EDX here is the current row (path B only; path A
     * jumps 0x4137E1->0x413A8B with EDX=region base). */
    if ( !(M8(v2 + 108) & 0xF) )
    {
      v24 = 1;
      /* asm 0x413959: lea eax,[edx+4] - address steps, same as <7 v9. */
      v25 = (int)(v2 + 4);
      v26 = 0;
      v27 = M32(v2 + ((12) * 4));
      M32(v2 + ((12) * 4)) = 0;
      do
      {
        if ( v27 > M32(v25 + 48) )
        {
          v26 = v24;
          v27 = M32(v25 + 48);
        }
        v25 += 4;
        ++v24;
        M32(v25 + 44) = 0;
      }
      while ( v24 < 11 );
      switch ( v26 )
      {
        /* asm 0x413995+: jle/jge (signed), same as the <7 switch. */
        case 1:
          if ( (signed int)M32(v2 + ((5) * 4)) > -32 )
            --M32(v2 + ((5) * 4));
          break;
        case 2:
          if ( (signed int)M32(v2 + ((5) * 4)) < 32 )
            ++M32(v2 + ((5) * 4));
          break;
        case 3:
          if ( (signed int)M32(v2 + ((6) * 4)) > -32 )
            --M32(v2 + ((6) * 4));
          break;
        case 4:
          if ( (signed int)M32(v2 + ((6) * 4)) < 32 )
            ++M32(v2 + ((6) * 4));
          break;
        case 5:
          if ( (signed int)M32(v2 + ((7) * 4)) > -32 )
            --M32(v2 + ((7) * 4));
          break;
        case 6:
          if ( (signed int)M32(v2 + ((7) * 4)) < 32 )
            ++M32(v2 + ((7) * 4));
          break;
        case 7:
          if ( (signed int)M32(v2 + ((8) * 4)) > -32 )
            --M32(v2 + ((8) * 4));
          break;
        case 8:
          if ( (signed int)M32(v2 + ((8) * 4)) < 32 )
            ++M32(v2 + ((8) * 4));
          break;
        case 9:
          if ( (signed int)M32(v2 + ((8) * 4)) > -32 )
            --M32(v2 + ((9) * 4));
          break;
        case 10:
          if ( (signed int)M32(v2 + ((8) * 4)) < 32 )
            ++M32(v2 + ((9) * 4));
          break;
        default:
          break;
      }
      /* asm 0x413A13: test BYTE [edx+6Ch],0FFh - fires when the low
       * BYTE wraps to 0 (every 256 path-B calls), not when the whole
       * dword is 0 (every 2^32). Same EDX=current-row as above. */
      if ( !(M8(v2 + 108) & 0xFF) )
      {
        M32(v2 + ((23) * 4)) -= M32(v2 + ((25) * 4));
        M32(v2 + ((25) * 4)) = M32(v2 + ((23) * 4));
        M32(v2 + ((24) * 4)) -= M32(v2 + ((26) * 4));
        M32(v2 + ((26) * 4)) = M32(v2 + ((24) * 4));
      }
    }
    if ( G8(D4A_MODEL_ID) == 9 )
    {
      v28 = G32(D4A_TBL_43D740);
      G32(D4A_TBL_43D740) = G32(D4A_TBL_43D744);
      G32(D4A_TBL_43D744) = v28;
    }
    result = v47;
    M32(v2 + ((28) * 4)) = v47 + (M32(v2 + ((28) * 4)) << 8);
    if ( G8(D4A_MODEL_ID) == 7 )
    {
      v29 = v47;
      v30 = M32(v2 + ((30) * 4));
      M32(v2 + ((29) * 4)) = M32(D4A_BYTE_CURVE_1 + ((v47) * 4)) + (M32(v2 + ((29) * 4)) << 8);
      v19 = v30 << 8;
      result = M32(D4A_BYTE_CURVE_3 + ((v29) * 4));
LABEL_80:
      M32(v2 + ((30) * 4)) = result + v19;
      return result;
    }
  }
  return result;
}

static int d4_read_header(dec4_ctx *c)
{
  unsigned int v0;
  unsigned int v1;
  int v2;
  int v3;
  int result;
  unsigned int v5;
  unsigned int i;
  int v7;
  int v8;
  int v9;
  int v10;
  unsigned int v11;
  unsigned int j;
  unsigned char v13;
  int v14;
  int v15;
  int v16;
  int v17;
  unsigned int v18;
  unsigned int k;
  unsigned char v20;
  int v21;
  int v22;
  int v23;

  G32(D4A_TRI_TOTAL) = 2;
  v0 = G32(D4A_RC_RANGE) / 2u;
  G32(D4A_RC_RANGE) = v0;
  v1 = G32(D4A_RC_CODE) / v0;
  G8(D4A_PHASE) = v1;
  G32(D4A_TRI_LOW) = (unsigned char)v1;
  G32(D4A_TRI_HIGH) = (unsigned char)v1 + 1;
  v2 = M32(D4A_TRI_TOTAL + (1) * 4);
  v3 = M32(D4A_TRI_TOTAL + (2) * 4) - v2;
  result = G32(D4A_RC_RANGE) * v2;
  v5 = G32(D4A_RC_RANGE) * v3;
  i = (unsigned long)(G32(D4A_RC_CODE) - result);
  d4_rcrenorm(c, &i, &v5);
  G32(D4A_RC_CODE) = (int)i;
  G32(D4A_RC_RANGE) = v5;
  if ( !G8(D4A_PHASE) )
  {
    v23 = 0;
    do
    {
      G32(D4A_TRI_TOTAL) = 2048;
      G32(D4A_RC_RANGE) = (unsigned)d4_div(G32(D4A_RC_RANGE), 0x800u);
      v9 = (unsigned short)((unsigned)d4_div(G32(D4A_RC_CODE), G32(D4A_RC_RANGE)));
      G32(D4A_TRI_LOW) = v9;
      G32(D4A_TRI_HIGH) = v9 + 1;
      v10 = M32(D4A_TRI_TOTAL + (1) * 4);
      v11 = G32(D4A_RC_RANGE) * (M32(D4A_TRI_TOTAL + (2) * 4) - v10);
      j = G32(D4A_RC_CODE) - G32(D4A_RC_RANGE) * v10;
  d4_rcrenorm(c, &j, &v11);
      G32(D4A_RC_CODE) = j;
      G32(D4A_TRI_TOTAL) = 2;
      G32(D4A_HDR_SIZE) += v9 << v23;
      G32(D4A_RC_RANGE) = v11 / 2;
      v16 = (unsigned short)(j / (v11 / 2));
      G32(D4A_TRI_LOW) = v16;
      G32(D4A_TRI_HIGH) = v16 + 1;
      v23 += 11;
      v17 = M32(D4A_TRI_TOTAL + (1) * 4);
      v18 = v11 / 2 * (M32(D4A_TRI_TOTAL + (2) * 4) - v17);
      k = (unsigned long)(j - G32(D4A_RC_RANGE) * v17);
  d4_rcrenorm(c, &k, &v18);
      G32(D4A_RC_CODE) = (int)k;
      G32(D4A_RC_RANGE) = v18;
    }
    while ( v16 );
    result = G32(D4A_HDR_SIZE);
    G32(D4A_NEXT_BLK) = G32(D4A_HDR_SIZE);
  }
  return result;
}


/* ---------------- public API ---------------- */

void UhaDec4_06Free(void *ctx);

int UhaDec4_06Init(void **ctx, int method)
{
    dec4_ctx *c;
    if (!ctx) return 1;
    *ctx = 0;
    if (method < 24 || method > 39) return 1;
    c = (dec4_ctx *)calloc(1, sizeof(*c));
    if (!c) return 2;
    c->mem = (unsigned char *)calloc(1, (size_t)D4_ARENA);
    if (!c->mem) { free(c); return 2; }
    d4_load_tables(c);
    if (d4_decoder_init(c, 0, method) != 0) { free(c->mem); free(c); return 2; }
    *ctx = c;
    return 0;
}

void UhaDec4_06Free(void *ctx)
{
    dec4_ctx *c = (dec4_ctx *)ctx;
    if (!c) return;
    if (c->mem) free(c->mem);
    free(c);
}

int UhaDec4_06(void *ctx, const unsigned long *cum, unsigned long ncum,
                        Uha4ReadCb rcb, void *rctx,
                        Uha4WriteCb wcb, void *wctx,
                        unsigned long out_len);


/* Container cumulative file-end table (dword_45D128).  The table lives in
 * the arena as 32-bit entries (the original was a malloc'ed int[]), with one
 * extra terminator entry = out_len so the walk in d4_block_model_init can never run
 * past the end. */
#define A_FILE_TABLE 0x00500000UL

int UhaDec4_06(void *ctx,
                        const unsigned long *cum, unsigned long ncum,
                        Uha4ReadCb rcb, void *rctx,
                        Uha4WriteCb wcb, void *wctx,
                        unsigned long out_len)
{
    dec4_ctx *c = (dec4_ctx *)ctx;
    unsigned long i;

    if (!c || !rcb || !wcb)
        return 1;
    if (out_len == 0)
        return 0;
    c->rcb = rcb; c->rctx = rctx;
    c->wcb = wcb; c->wctx = wctx;
    c->in_pos = 0; c->in_avail = 0; c->in_eof = 0;
    G32(D4A_OUT_TOTAL) = (unsigned)out_len;
    G32(D4A_OUT_POS) = 0;
    G32(D4A_OUTBUF_LEN) = 0;
    G32(D4A_IN_CHUNKS) = 0;
    G32(D4A_IO_ERR) = 0;
    c->steps = 0;
    G32(D4A_BLK_END) = 0;
    G32(D4A_TBL_45D144) = 0;
    G32(D4A_TBL_45D124) = 0xFFFFFFFFU;
    G32(D4A_NEXT_BLK) = 0xFFFFFFFFU;
    if (cum && ncum) {
        for (i = 0; i < ncum; i++)
            M32(A_FILE_TABLE + 4 * i) = (unsigned)cum[i];
        M32(A_FILE_TABLE + 4 * ncum) = (unsigned)out_len;
        G32(D4A_FILE_TAB_PTR) = A_FILE_TABLE;
        c->file_table = A_FILE_TABLE;
        /* asm 0x40D410..0x40D451 (sub_40CDD1 tail): when flags2
         * bit6 (byte_4228BD) is set, scan the file-end table for a
         * 0xFB670 (1029744) file; the predecessor end becomes
         * dword_45D124 and byte_45E395 = (prev == 0) ? 10 : 0. */
        if (g_dec4_flags2 & 0x40) {
            unsigned prev = 0;
            for (i = 0; i < ncum; i++) {
                unsigned e = (unsigned)cum[i];
                if (e - prev == 0xFB670U) break;
                prev = e;
            }
            G32(D4A_TBL_45D124) = prev;
            G8(D4A_FLAG_45E395) = (prev == 0) ? 10 : 0;
        }
    } else {
        G32(D4A_FILE_TAB_PTR) = 0;
        c->file_table = 0;
    }
    d4_decode_main(c, 0, 0);
    if (G32(D4A_OUTBUF_LEN)) {
        unsigned n = G32(D4A_OUTBUF_LEN);
        if (c->wcb(c->wctx, c->outbuf, n) != 0) G32(D4A_IO_ERR) = 4;
        G32(D4A_OUTBUF_LEN) = 0;
    }
#ifdef SPLITDBG
    fprintf(stderr, "SPLITDBG end 422B90=%u out=%u total=%u blk=%u next=%u idx=%u steps=%lu\n",
        G32(D4A_IO_ERR), G32(D4A_OUT_POS), G32(D4A_OUT_TOTAL), G32(D4A_BLK_END), G32(D4A_NEXT_BLK),
        G32(D4A_TBL_45D144), c->steps);
#endif
    if (G32(D4A_IO_ERR) == 4) return 4;
    if (G32(D4A_IO_ERR)) return 3;
    if (G32(D4A_OUT_POS) != G32(D4A_OUT_TOTAL)) return 3;
    return 0;
}


/* ---- 0.4 core 4 (methods 24..39) ---- */

static unsigned char g4_04_flags2;

void UhaDec4_04SetFlags2(unsigned char flags2)
{
	g4_04_flags2 = flags2;
}

int UhaDec4_04Init(void** pctx, int method)
{
	(void)pctx; (void)method;
	return 1;
}

void UhaDec4_04Free(void* ctx)
{
	(void)ctx;
}

int UhaDec4_04(void* ctx, const unsigned long* cum, unsigned long ncum,
	Uha4ReadCb rcb, void* rctx, Uha4WriteCb wcb, void* wctx,
	unsigned long out_len)
{
	(void)ctx; (void)cum; (void)ncum; (void)rcb; (void)rctx;
	(void)wcb; (void)wctx; (void)out_len;
	return 7;
}

/* ---- 0.3 core 4 (methods 22..35) ---- */

static unsigned char g4_03_flags2;

void UhaDec4_03SetFlags2(unsigned char flags2)
{
	g4_03_flags2 = flags2;
}

int UhaDec4_03Init(void** pctx, int method)
{
	(void)pctx; (void)method;
	return 1;
}

void UhaDec4_03Free(void* ctx)
{
	(void)ctx;
}

int UhaDec4_03(void* ctx, const unsigned long* cum, unsigned long ncum,
	Uha4ReadCb rcb, void* rctx, Uha4WriteCb wcb, void* wctx,
	unsigned long out_len)
{
	(void)ctx; (void)cum; (void)ncum; (void)rcb; (void)rctx;
	(void)wcb; (void)wctx; (void)out_len;
	return 7;
}
