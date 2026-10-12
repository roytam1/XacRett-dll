#ifndef AFX_ACEBLOCK_H__D4E5F6A7_B8C9_4D0E_1F2A_3B4C50617283__INCLUDED_
#define AFX_ACEBLOCK_H__D4E5F6A7_B8C9_4D0E_1F2A_3B4C50617283__INCLUDED_

// ACE 2.0 blocked-mode decoder object (clean-room port of the decode path
// documented in seb3773/ace-c, used as reference only, never vendored).
// Covers plain LZ77 runs plus the EXE/DELTA/SOUND/PIC preprocessing
// filters, with solid-dictionary support across members.
struct AceBlock;

// Pull callback: fill buf with up to len stream bytes, return bytes read
// (0 = end of member stream).
typedef int (*AcePullCb)(void* ctx, BYTE* buf, int len);

AceBlock* AceBlkCreate(void);
void AceBlkFree(AceBlock* b);
// Begin one member: dicsize is the LZ77 dictionary bytes (clamped
// internally to 1KB..4MB, never shrunk); filesize is the uncompressed
// member size; continue_dict keeps history (solid archives).
void AceBlkBegin(AceBlock* b, unsigned long dicsize, unsigned long filesize,
	int continue_dict);
// Pull up to len decoded bytes into out. Returns bytes produced
// (0 = member complete or decode failure; failures also fail the member
// CRC check downstream, so no separate error channel is needed).
int AceBlkRead(AceBlock* b, AcePullCb pull, void* ctx, BYTE* out, int len);
// Register stored (verbatim) bytes into the dictionary (solid archives).
void AceBlkRegister(AceBlock* b, const BYTE* buf, int len);

#endif
