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
//   +0  DWORD dwChecksum
//   +4  DWORD dwUncompressedSize
//   +8  WORD  wSig  ('C','J')
//
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

struct CState
{
	int  lastIndex;
	int  indexBase;
	int  literalBits;
	int  literalOffset;
	unsigned char history[0x10000];
	int  historyPos;

	CState() : lastIndex(1), indexBase(8), literalBits(8),
	           literalOffset(0), historyPos(0)
	{
		memset(history, 0, sizeof(history));
	}

	unsigned char Emit(unsigned char c)
	{
		history[historyPos & 0xFFFF] = c;
		++historyPos;
		return c;
	}

	void CopyMatch(int distance, int length)
	{
		for (int i = 0; i < length; ++i) {
			unsigned char c = history[(historyPos - distance) & 0xFFFF];
			Emit(c);
		}
	}
};

bool ValidHeader(const void* pBlock)
{
	if (!pBlock) return false;
	const unsigned char* p = (const unsigned char*)pBlock;
	return (p[8] == 'C' && p[9] == 'J');
}

} // namespace

//--------------------------------------------------------------------
extern "C" DWORD _stdcall JCALG1_GetUncompressedSizeOfCompressedBlock(
	const void *pBlock)
{
	if (!ValidHeader(pBlock)) return 0;
	return *(const DWORD*)((const unsigned char*)pBlock + 4);
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

	for (;;) {
		if (bits.GetBit()) {
			// literal run
			*out++ = st.Emit((unsigned char)(bits.GetBits(st.literalBits)
			                                + st.literalOffset));
			continue;
		}
		if (bits.GetBit()) {
			// phrase match
			int high = bits.GetInteger();
			int len;
			if (high == 2) {
				len = bits.GetInteger();
				st.CopyMatch(st.lastIndex, len);
			} else {
				st.lastIndex = ((high - 3) << st.indexBase)
				             + bits.GetBits(st.indexBase);
				len = bits.GetInteger();
				if      (st.lastIndex >= 0x10000) len += 3;
				else if (st.lastIndex >= 0x37FF)  len += 2;
				else if (st.lastIndex >= 0x27F)   len += 1;
				else if (st.lastIndex <= 127)     len += 4;
				st.CopyMatch(st.lastIndex, len);
			}
			continue;
		}
		if (bits.GetBit()) {
			// one byte phrase / literal size change
			int v = bits.GetBits(4) - 1;
			if (v == 0) {
				*out++ = st.Emit(0);
			} else if (v > 0) {
				unsigned char c = st.history[(st.historyPos - v) & 0xFFFF];
				*out++ = st.Emit(c);
			} else if (bits.GetBit()) {
				// next block: copy 256 literal bytes
				do {
					for (int i = 0; i < 256; ++i)
						*out++ = st.Emit((unsigned char)bits.GetBits(8));
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
			st.CopyMatch(st.lastIndex, len);
		}
	}
	return (DWORD)(out - (unsigned char*)Destination);
}