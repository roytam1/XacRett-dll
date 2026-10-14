//////////////////////////////////////////////////////////////////////
// jcalg1.cpp
//
// JCALG1 decompressor entry points required by XacRett's Jc1Tool.cpp.
//
// The original project linked the vendor's "jcalg1lib/Jcalg1_static.lib"
// (JCALG1 r5, (c)1999 Jeremy Collake -- http://www.bitsum.com/jcalg1.htm).
// That binary is not part of the 0.49.11 source drop, and the upstream
// release is distributed as x86 assembly.
//
// This is a from-scratch C implementation of the JCALG1 decompression
// algorithm (an LZ77/LZSS derivative), providing the two symbols
// Jc1Tool.cpp actually references. The 10-byte block header is:
//
//   +0  WORD  wSig  ('J','C')
//   +2  DWORD dwUncompressedSize
//   +6  DWORD dwChecksum   (unused here; the host keeps no CRC)
//
// Signature at +0 and size at +2 are what the reference itself does:
// JCALG1_gusocb.asm is "cmp word ptr [eax],'CJ' / mov eax,[eax+2]",
// and CompressedData.cpp starts its bit reader at pSource+10. Reading
// the signature at +8 or the size at +4 is wrong: an r5.32 file then
// looks invalid, CJc1Tool::Check() fails, and the file gets misrouted
// to the much laxer standalone-LZMA provider.//
// The bitstream follows the header. Bits are consumed most-significant
// first. Format summary:
//
//   1                        -> literal: LiteralBits bits + LiteralOffset
//   0 1                      -> phrase (index+length from a variable code)
//   0 0 1  <4 bits>          -> one-byte phrase / literal-size change
//   0 0 0                    -> short match, or stream terminator
//////////////////////////////////////////////////////////////////////

#include <windows.h>
#include <string.h>

//--------------------------------------------------------------------
namespace {

// MSB-first bit reader over a little-endian DWORD stream.
class CBitReader
{
public:
	CBitReader(const unsigned char* p) : m_p(p), m_buf(0), m_left(0) {}

	int GetBit()
	{
		if (!m_left) {
			m_left  = 32;
			m_buf   = (unsigned)*(const unsigned*)m_p;
			m_p    += 4;
		}
		int bit = (int)(m_buf >> 31);
		m_buf <<= 1;
		--m_left;
		return bit;
	}

	int GetBits(int count)
	{
		if (!count) return 0;
		if (m_left >= count) {
			int v = (int)(m_buf >> (32 - count));
			m_buf <<= count;
			m_left -= count;
			return v;
		}
		int need = count - m_left;
		int v    = (int)((m_buf >> (32 - m_left)) << need);
		m_buf    = (unsigned)*(const unsigned*)m_p;
		m_p     += 4;
		v       |= (int)(m_buf >> (32 - need));
		m_buf   <<= need;
		m_left   = 32 - need;
		return v;
	}

	// Elias-gamma style variable length integer.
	int GetInteger()
	{
		int v = 1;
		do {
			v = (v << 1) + GetBit();
		} while (GetBit());
		return v;
	}

private:
	const unsigned char* m_p;
	unsigned             m_buf;
	int                  m_left;
};

// The reference decoder uses the DESTINATION buffer as its history
// (CompressedData.cpp TransferMatch: "s = p - MatchOffset"), which is
// what lets JCALG1 support 4K..1M windows. An earlier version of this
// port kept a private 64K ring instead and only pushed bytes into it,
// never into the output - so every phrase match silently produced zero
// bytes and 53248 came out as 12168. The output pointer is the history.
struct CState
{
	int  lastIndex;
	int  indexBase;
	int  literalBits;
	int  literalOffset;

	CState() : lastIndex(1), indexBase(8), literalBits(8),
	           literalOffset(0)
	{}

	// distance is clamped so a corrupt stream can never read before the
	// start of the destination; legal streams never trip this.
	static int SafeDist(const unsigned char* out, const unsigned char* base,
	                     int distance)
	{
		if (distance < 1) return 1;
		if (distance > (int)(out - base)) return 1;
		return distance;
	}

	static void CopyMatch(unsigned char*& out, const unsigned char* base,
	                      int distance, int length)
	{
		const unsigned char* s = out - SafeDist(out, base, distance);
		for (int i = 0; i < length; ++i) {
			*out++ = *s++;
		}
	}
};

bool ValidHeader(const void* pBlock)
{
	if (!pBlock) return false;
	const unsigned char* p = (const unsigned char*)pBlock;
	return (p[0] == 'J' && p[1] == 'C');
}

} // namespace

//--------------------------------------------------------------------
extern "C" DWORD _stdcall JCALG1_GetUncompressedSizeOfCompressedBlock(
	const void *pBlock)
{
	if (!ValidHeader(pBlock)) return 0;
	return *(const DWORD*)((const unsigned char*)pBlock + 2);
}

//--------------------------------------------------------------------
extern "C" DWORD _stdcall JCALG1_Decompress_Fast(
	const void *Source,
	void *Destination)
{
	if (!ValidHeader(Source) || !Destination) return 0;

	CBitReader bits((const unsigned char*)Source + 10);
	CState     st;
	unsigned char* out = (unsigned char*)Destination;
	// the header states the exact output size; never write past it, so a
	// corrupt stream fails by stopping short instead of trashing the heap
	unsigned char* const limit = out +
		*(const DWORD*)((const unsigned char*)Source + 2);

	for (;;) {
		if (bits.GetBit()) {
			// literal run
			*out++ = (unsigned char)(bits.GetBits(st.literalBits)
			                       + st.literalOffset);
			if (out >= limit) break;
			continue;
		}
		if (bits.GetBit()) {
			// phrase match
			int high = bits.GetInteger();
			int len;
			if (high == 2) {
				len = bits.GetInteger();
				st.CopyMatch(out, (const unsigned char*)Destination,
				              st.lastIndex, len);
			} else {
				st.lastIndex = ((high - 3) << st.indexBase)
				             + bits.GetBits(st.indexBase);
				len = bits.GetInteger();
				if      (st.lastIndex >= 0x10000) len += 3;
				else if (st.lastIndex >= 0x37FF)  len += 2;
				else if (st.lastIndex >= 0x27F)   len += 1;
				else if (st.lastIndex <= 127)     len += 4;
				st.CopyMatch(out, (const unsigned char*)Destination,
				              st.lastIndex, len);
			}
			continue;
		}
		if (bits.GetBit()) {
			// one byte phrase / literal size change
			int v = bits.GetBits(4) - 1;
			if (v == 0) {
				*out++ = 0;
			} else if (v > 0) {
				int od = CState::SafeDist(out, (const unsigned char*)Destination, v);
				*out++ = *(out - od);
			} else if (bits.GetBit()) {
				// next block: copy 256 literal bytes
				do {
					for (int i = 0; i < 256 && out < limit; ++i)
						*out++ = (unsigned char)bits.GetBits(8);
				} while (bits.GetBit());
			} else {
				// new literal size
				st.literalBits = 7 + bits.GetBit();
				st.literalOffset = 0;
				if (st.literalBits != 8)
					st.literalOffset = bits.GetBits(8);
			}
			continue;
		}
		// short match, or end of stream
		int newIndex = bits.GetBits(7);
		int len      = 2 + bits.GetBits(2);
		if (newIndex == 0) {
			if (len == 2) break;           // end of decompression
			st.indexBase = bits.GetBits(len + 1);
		} else {
			st.lastIndex = newIndex;
			st.CopyMatch(out, (const unsigned char*)Destination,
			              newIndex, len);
		}
	}
	return (DWORD)(out - (unsigned char*)Destination);
}