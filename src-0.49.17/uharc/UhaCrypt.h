#ifndef AFX_UHA_CRYPT_H__UHA_CRYPT_INCLUDED__
#define AFX_UHA_CRYPT_H__UHA_CRYPT_INCLUDED__

/* UHARC header byte cipher and table S-box ciphers.
 * Written from FORMAT.md section 6 (6.1 header cipher, 6.2/6.3 table
 * ciphers, 6.6 password form). All arithmetic is byte-wide.
 */

#ifdef __cplusplus
extern "C" {
#endif

/* Header cipher: raw[4+k] ^= (key + k*k) & 0xFF.
 * key is the low byte of the stored header CRC (outside the range).
 * len is the encrypted range length: 40 (0.2), 36 (0.3/0.4), 31 (0.6).
 * The buffer must be at least 4+len bytes; only [4, 4+len) is touched.
 */
void UhaHeaderDecrypt(unsigned char* raw, unsigned len, unsigned char key);

/* Table cipher state (256-byte S-box + pos/mix). Self-inverse stream:
 * out = in ^ (S[pos] + S[pos+1]) for the 0.3/0.4/0.6 cipher,
 * out = in ^ S[pos+1] after S[pos+1] += S[pos] for the 0.2 cipher.
 */
typedef struct UhaSBox
{
	unsigned char s[256];
	unsigned pos;
	unsigned mix;
	int is02;		/* 1 = 0.2 stream, 0 = 0.3/0.4/0.6 stream */
} UhaSBox;

/* Default-password S-box init (fresh per table).
 * pwd == NULL selects the default "UHARC Default Password".
 * is02 selects the 0.2 fill (no length prefix).
 * For the password form, sb[0] is the password length (6.2) or the
 * 0.2 fill cycles the password (6.3).
 */
void UhaSBoxInit(UhaSBox* b, const char* pwd, int is02);

/* Transform buf[0..len) in place (same routine encrypts and decrypts).
 * Follows FORMAT.md 6.2 / 6.3 byte-for-byte.
 */
void UhaSBoxUpdate(UhaSBox* b, unsigned char* buf, unsigned long len);

#ifdef __cplusplus
}
#endif

#endif
