/*

Copyright (c) 2026, Roy Tam

Permission to use, copy, modify, and/or distribute this software
for any purpose with or without fee is hereby granted, provided
that the above copyright notice and this permission notice appear
in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

 */

//
// Decoder for the -lx1- algorithm.
//
// -lx1- is UNLHA32.DLL's experimental format. The literal/length
// coding is identical to -lh6- (510 codes), with a 32 KiB sliding
// window, but the offset coding is completely different: each
// block starts with a 2-bit mode selecting one of four offset
// coders:
//
//   mode 0: no offset table; offsets are raw 15-bit values.
//   mode 1: fixed 128-symbol Huffman table (no table stored).
//   mode 2: 128 offset lengths stored as raw 4-bit values.
//   mode 3: small 16-symbol Huffman table, decoded like -lh6-.
//
// Reverse engineered from UNLHA32.DLL v2.50 (decode triple
// sub_10013960/sub_10013A70/sub_10013B40, block header sub_100138EA).
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "lha_decoder.h"

#include "bit_stream_reader.c"

// Include tree decoder.

typedef uint16_t TreeElement;
#include "tree_decode.c"

// Threshold for copying. The first copy code starts from here.

#define COPY_THRESHOLD       3 /* bytes */

// 32 KiB history ring buffer (15-bit offsets):

#define HISTORY_BITS         15   /* 2^15 = 32768 */
#define RING_BUFFER_SIZE     (1 << HISTORY_BITS)

// Required size of the output buffer.

#define OUTPUT_BUFFER_SIZE   RING_BUFFER_SIZE

// Number of different command codes. 0-255 range are literal byte
// values, while higher values indicate copy from history.

#define NUM_CODES            510

// Temp table parameters (same as -lh*-):

#define TEMP_CODE_BITS       5
#define MAX_TEMP_CODES       ((1 << TEMP_CODE_BITS) - 1)

// Offset alphabet sizes:

#define LX1_NUM_OFFSET_CODES 128
#define LX1_MODE3_NUM_CODES  16

// Name of the decoder:

#define DECODER_NAME lha_lx1_decoder

typedef struct {
	// Input bit stream.

	BitStreamReader bit_stream_reader;

	// Ring buffer of past data.

	uint8_t ringbuf[RING_BUFFER_SIZE];
	unsigned int ringbuf_pos;

	// Number of commands remaining before we start a new block.

	unsigned int block_remaining;

	// Offset coding mode for the current block (0-3).

	int offset_mode;

	// Tables.

	TreeElement temp_tree[MAX_TEMP_CODES * 2];
	TreeElement code_tree[NUM_CODES * 2];
	TreeElement offset_tree[LX1_NUM_OFFSET_CODES * 2];
} LX1Decoder;

// Initialize the history ring buffer.

static void init_ring_buffer(LX1Decoder *decoder)
{
	memset(decoder->ringbuf, ' ', RING_BUFFER_SIZE);
	decoder->ringbuf_pos = 0;
}

static int lha_lx1_init(void *data, LHADecoderCallback callback,
                        void *callback_data)
{
	LX1Decoder *decoder = data;

	bit_stream_reader_init(&decoder->bit_stream_reader,
	                       callback, callback_data);

	init_ring_buffer(decoder);

	decoder->block_remaining = 0;
	decoder->offset_mode = 0;

	init_tree(decoder->code_tree, NUM_CODES * 2);
	init_tree(decoder->offset_tree, LX1_NUM_OFFSET_CODES * 2);
	init_tree(decoder->temp_tree, MAX_TEMP_CODES * 2);

	return 1;
}

// Read a length value - this is normally a value in the 0-7 range, but
// sometimes can be longer.

static int read_length_value(LX1Decoder *decoder)
{
	int i, len;

	len = read_bits(&decoder->bit_stream_reader, 3);

	if (len < 0) {
		return -1;
	}

	if (len == 7) {
		for (;;) {
			i = read_bit(&decoder->bit_stream_reader);

			if (i < 0) {
				return -1;
			} else if (i == 0) {
				break;
			}

			++len;
		}
	}

	return len;
}

// Read the temporary table used for encoding the code table.
// Identical to the -lh6- temp table (19 codes max, 2-bit skip
// after the third length).

static int read_temp_table(LX1Decoder *decoder)
{
	int i, j, n, len, code;
	uint8_t code_lengths[MAX_TEMP_CODES];

	n = read_bits(&decoder->bit_stream_reader, TEMP_CODE_BITS);

	if (n < 0) {
		return 0;
	}

	if (n == 0) {
		code = read_bits(&decoder->bit_stream_reader, 5);

		if (code < 0) {
			return 0;
		}

		set_tree_single(decoder->temp_tree, code);
		return 1;
	}

	if (n > MAX_TEMP_CODES) {
		n = MAX_TEMP_CODES;
	}

	for (i = 0; i < n; ++i) {
		len = read_length_value(decoder);

		if (len < 0) {
			return 0;
		}

		code_lengths[i] = len;

		if (i == 2) {
			len = read_bits(&decoder->bit_stream_reader, 2);

			if (len < 0) {
				return 0;
			}

			for (j = 0; j < len; ++j) {
				++i;
				code_lengths[i] = 0;
			}
		}
	}

	build_tree(decoder->temp_tree, MAX_TEMP_CODES * 2, code_lengths, n);

	return 1;
}

static int read_skip_count(LX1Decoder *decoder, int skiprange)
{
	int result;

	if (skiprange == 0) {
		result = 1;
	} else if (skiprange == 1) {
		result = read_bits(&decoder->bit_stream_reader, 4);

		if (result < 0) {
			return -1;
		}

		result += 3;
	} else {
		result = read_bits(&decoder->bit_stream_reader, 9);

		if (result < 0) {
			return -1;
		}

		result += 20;
	}

	return result;
}

static int read_code_table(LX1Decoder *decoder)
{
	int i, j, n, skip_count, code;
	uint8_t code_lengths[NUM_CODES];

	n = read_bits(&decoder->bit_stream_reader, 9);

	if (n < 0) {
		return 0;
	}

	if (n == 0) {
		code = read_bits(&decoder->bit_stream_reader, 9);

		if (code < 0) {
			return 0;
		}

		set_tree_single(decoder->code_tree, code);

		return 1;
	}

	if (n > NUM_CODES) {
		n = NUM_CODES;
	}

	i = 0;

	while (i < n) {
		code = read_from_tree(&decoder->bit_stream_reader,
		                      decoder->temp_tree);

		if (code < 0) {
			return 0;
		}

		if (code <= 2) {
			skip_count = read_skip_count(decoder, code);

			if (skip_count < 0) {
				return 0;
			}

			for (j = 0; j < skip_count && i < n; ++j) {
				code_lengths[i] = 0;
				++i;
			}
		} else {
			code_lengths[i] = code - 2;
			++i;
		}
	}

	build_tree(decoder->code_tree, NUM_CODES * 2, code_lengths, n);

	return 1;
}

// Mode 3: small 16-symbol offset table. Lengths are raw-coded
// (3 bits + extension), no skip field.

static int read_mode3_offset_table(LX1Decoder *decoder)
{
	int i, n, len;
	uint8_t code_lengths[LX1_MODE3_NUM_CODES];

	n = read_bits(&decoder->bit_stream_reader, 5);

	if (n < 0) {
		return 0;
	}

	if (n == 0) {
		len = read_bits(&decoder->bit_stream_reader, 5);

		if (len < 0) {
			return 0;
		}

		set_tree_single(decoder->offset_tree, len);
		return 1;
	}

	if (n > LX1_MODE3_NUM_CODES) {
		n = LX1_MODE3_NUM_CODES;
	}

	for (i = 0; i < n; ++i) {
		len = read_length_value(decoder);

		if (len < 0) {
			return 0;
		}

		code_lengths[i] = len;
	}

	for (; i < LX1_MODE3_NUM_CODES; ++i) {
		code_lengths[i] = 0;
	}

	build_tree(decoder->offset_tree, LX1_NUM_OFFSET_CODES * 2,
	           code_lengths, LX1_MODE3_NUM_CODES);

	return 1;
}

// Mode 2: 128 offset lengths stored as raw 4-bit values.
// If the first three lengths are all 1, the table is a single
// code whose value follows in 7 bits.

static int read_mode2_offset_table(LX1Decoder *decoder)
{
	int i, len;
	uint8_t code_lengths[LX1_NUM_OFFSET_CODES];

	for (i = 0; i < LX1_NUM_OFFSET_CODES; ++i) {
		len = read_bits(&decoder->bit_stream_reader, 4);

		if (len < 0) {
			return 0;
		}

		code_lengths[i] = len;

		if (i == 2
		 && code_lengths[0] == 1
		 && code_lengths[1] == 1
		 && code_lengths[2] == 1) {
			len = read_bits(&decoder->bit_stream_reader, 7);

			if (len < 0) {
				return 0;
			}

			set_tree_single(decoder->offset_tree, len);
			return 1;
		}
	}

	build_tree(decoder->offset_tree, LX1_NUM_OFFSET_CODES * 2,
	           code_lengths, LX1_NUM_OFFSET_CODES);

	return 1;
}

// Mode 1: fixed 128-symbol offset table. The code lengths are
// derived from a static breakpoint list embedded in UNLHA32.DLL
// (unk_10026358 + 32): first length 2, length increments at
// symbols 1, 1, 3, 6, 13, 31 and 78.

static int build_mode1_offset_table(LX1Decoder *decoder)
{
	static const int breakpoints[] = { 1, 1, 3, 6, 13, 31, 78 };
	uint8_t code_lengths[LX1_NUM_OFFSET_CODES];
	unsigned int sym, bi;
	int len;

	len = 2;
	bi = 0;

	for (sym = 0; sym < LX1_NUM_OFFSET_CODES; ++sym) {
		while (bi < sizeof(breakpoints) / sizeof(*breakpoints)
		    && (unsigned int) breakpoints[bi] == sym) {
			++len;
			++bi;
		}

		code_lengths[sym] = len;
	}

	build_tree(decoder->offset_tree, LX1_NUM_OFFSET_CODES * 2,
	           code_lengths, LX1_NUM_OFFSET_CODES);

	return 1;
}

// Start reading a new block from the input stream.

static int start_new_block(LX1Decoder *decoder)
{
	int len, mode;

	len = read_bits(&decoder->bit_stream_reader, 16);

	if (len < 0) {
		return 0;
	}

	decoder->block_remaining = (size_t) len;

	if (!read_temp_table(decoder)) {
		return 0;
	}

	if (!read_code_table(decoder)) {
		return 0;
	}

	// -lx1--specific 2-bit offset coding mode.

	mode = read_bits(&decoder->bit_stream_reader, 2);

	if (mode < 0) {
		return 0;
	}

	decoder->offset_mode = mode;

	switch (mode) {
		case 0:
			// No offset table; raw 15-bit offsets.
			break;

		case 1:
			if (!build_mode1_offset_table(decoder)) {
				return 0;
			}
			break;

		case 2:
			if (!read_mode2_offset_table(decoder)) {
				return 0;
			}
			break;

		case 3:
			if (!read_mode3_offset_table(decoder)) {
				return 0;
			}
			break;

		default:
			return 0;
	}

	return 1;
}

static int read_code(LX1Decoder *decoder)
{
	return read_from_tree(&decoder->bit_stream_reader, decoder->code_tree);
}

// Standard -lh*-style offset: the symbol gives the bit length,
// followed by the low bits.

static int read_mode3_offset(LX1Decoder *decoder)
{
	int bits, result, k, hi;

	bits = read_from_tree(&decoder->bit_stream_reader,
	                      decoder->offset_tree);

	if (bits < 0) {
		return -1;
	}

	if (bits == 0) {
		return 0;
	} else if (bits == 1) {
		return 1;
	} else if (bits > 0x11) {
		// Not expected for the 16-symbol table, but match
		// UNLHA32's sub_100139D0 for completeness.
		k = bits - 0x11;
		hi = read_bits(&decoder->bit_stream_reader, k);

		if (hi < 0) {
			return -1;
		}

		hi |= 1 << k;

		result = read_bits(&decoder->bit_stream_reader, 16);

		if (result < 0) {
			return -1;
		}

		return (hi << 16) | result;
	} else {
		result = read_bits(&decoder->bit_stream_reader, bits - 1);

		if (result < 0) {
			return -1;
		}

		return result + (1 << (bits - 1));
	}
}

// Modes 1/2: 7-bit Huffman symbol plus 8 raw bits (15-bit offsets).

static int read_stored_offset(LX1Decoder *decoder)
{
	int sym, low;

	sym = read_from_tree(&decoder->bit_stream_reader,
	                     decoder->offset_tree);

	if (sym < 0) {
		return -1;
	}

	low = read_bits(&decoder->bit_stream_reader, 8);

	if (low < 0) {
		return -1;
	}

	return (sym << 8) | low;
}

static int read_offset(LX1Decoder *decoder)
{
	switch (decoder->offset_mode) {
		case 0:
			return read_bits(&decoder->bit_stream_reader, 15);

		case 1:
		case 2:
			return read_stored_offset(decoder);

		case 3:
			return read_mode3_offset(decoder);

		default:
			return -1;
	}
}

static void output_byte(LX1Decoder *decoder, uint8_t *buf,
                        size_t *buf_len, uint8_t b)
{
	buf[*buf_len] = b;
	++*buf_len;

	decoder->ringbuf[decoder->ringbuf_pos] = b;
	decoder->ringbuf_pos = (decoder->ringbuf_pos + 1) % RING_BUFFER_SIZE;
}

static void copy_from_history(LX1Decoder *decoder, uint8_t *buf,
                              size_t *buf_len, size_t count)
{
	int offset;
	unsigned int i, start;

	offset = read_offset(decoder);

	if (offset < 0) {
		return;
	}

	start = decoder->ringbuf_pos + RING_BUFFER_SIZE
	      - (unsigned int) offset - 1;

	for (i = 0; i < count; ++i) {
		output_byte(decoder, buf, buf_len,
		            decoder->ringbuf[(start + i) % RING_BUFFER_SIZE]);
	}
}

static size_t lha_lx1_read(void *data, uint8_t *buf)
{
	LX1Decoder *decoder = data;
	size_t result;
	int code, copy_count;

	while (decoder->block_remaining == 0) {
		if (!start_new_block(decoder)) {
			return 0;
		}
	}

	--decoder->block_remaining;

	result = 0;

	code = read_code(decoder);

	if (code < 0) {
		return 0;
	}

	if (code < 256) {
		output_byte(decoder, buf, &result, (uint8_t) code);
	} else {
		copy_count = code - 256 + COPY_THRESHOLD;

		copy_from_history(decoder, buf, &result, copy_count);
	}

	return result;
}

const LHADecoderType DECODER_NAME = {
	lha_lx1_init,
	NULL,
	lha_lx1_read,
	sizeof(LX1Decoder),
	OUTPUT_BUFFER_SIZE,
	RING_BUFFER_SIZE / 2
};
