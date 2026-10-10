
#include "stdafx.h"
#include "lzxTool.h"
#include <stdlib.h>
#include <stdio.h>

bool CLzxTool::Check( const char* fname, unsigned long fsize )
{
	bool ret=false;
	FILE *in_file= fopen(fname,"rb");
	int actual;

	if(!in_file)
		return false;

	actual = fread(info_header, 1, 10, in_file);
	if(ferror(in_file) || actual != 10)
	{
		fclose(in_file);
		return false;
	}
	if((info_header[0] == 76) && (info_header[1] == 90) && (info_header[2] == 88)) /* LZX */
	{
		ret=true;
	}
	else
		ret=false;

	fclose(in_file);

	return ret;
}

bool CLzxTool::Extract( const char* fname, const char* ddir )
{
	int result = 1; /* assume an error */
	FILE *in_file= fopen(fname,"rb");
	int actual;

	if(!in_file)
		return false;

	actual = fread(info_header, 1, 10, in_file);
	if(ferror(in_file) || actual != 10)
	{
		fclose(in_file);
		return false;
	}
	if((info_header[0] == 76) && (info_header[1] == 90) && (info_header[2] == 88)) /* LZX */
	{
		result = extract_archive(in_file);
	}
	else
		result=1;

	fclose(in_file);

	return(result==0);
}

const unsigned int CLzxTool::crc_table[256]=
{
 0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,
 0xE963A535,0x9E6495A3,0x0EDB8832,0x79DCB8A4,0xE0D5E91E,0x97D2D988,
 0x09B64C2B,0x7EB17CBD,0xE7B82D07,0x90BF1D91,0x1DB71064,0x6AB020F2,
 0xF3B97148,0x84BE41DE,0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,
 0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,0x14015C4F,0x63066CD9,
 0xFA0F3D63,0x8D080DF5,0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,
 0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,0x35B5A8FA,0x42B2986C,
 0xDBBBC9D6,0xACBCF940,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,
 0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F4B5,0x56B3C423,
 0xCFBA9599,0xB8BDA50F,0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,
 0x2F6F7C87,0x58684C11,0xC1611DAB,0xB6662D3D,0x76DC4190,0x01DB7106,
 0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,0x9FBFE4A5,0xE8B8D433,
 0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6A0DBB,0x086D3D2D,
 0x91646C97,0xE6635C01,0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,
 0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,0x65B0D9C6,0x12B7E950,
 0x8BBEB8EA,0xFCB9887C,0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,
 0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,
 0xA4D1C46D,0xD3D6F4FB,0x4369E96A,0x346ED9FC,0xAD678846,0xDA60B8D0,
 0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,0x5005713C,0x270241AA,
 0xBE0B1010,0xC90C2086,0x5768B525,0x206F85B3,0xB966D409,0xCE61E49F,
 0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,
 0xB7BD5C3B,0xC0BA6CAD,0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,
 0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,0xE3630B12,0x94643B84,
 0x0D6D6A3E,0x7A6A5AA8,0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,
 0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,0xF762575D,0x806567CB,
 0x196C3671,0x6E6B06E7,0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,
 0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,0xD6D6A3E8,0xA1D1937E,
 0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,
 0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA867DF55,
 0x316E8EEF,0x4669BE79,0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,
 0xCC0C7795,0xBB0B4703,0x220216B9,0x5505262F,0xC5BA3BBE,0xB2BD0B28,
 0x2BB45A92,0x5CB36A04,0xC2D7FFA7,0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,
 0x9B64C2B0,0xEC63F226,0x756AA39C,0x026D930A,0x9C0906A9,0xEB0E363F,
 0x72076785,0x05005713,0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,
 0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,0x86D3D2D4,0xF1D4E242,
 0x68DDB3F8,0x1FDA836E,0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,
 0x88085AE6,0xFF0F6A70,0x66063BCA,0x11010B5C,0x8F659EFF,0xF862AE69,
 0x616BFFD3,0x166CCF45,0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,
 0xA7672661,0xD06016F7,0x4969474D,0x3E6E77DB,0xAED16A4A,0xD9D65ADC,
 0x40DF0B66,0x37D83BF0,0xA9BCAE53,0xDEBB9EC5,0x47B2CF7F,0x30B5FFE9,
 0xBDBDF21C,0xCABAC28A,0x53B39330,0x24B4A3A6,0xBAD03605,0xCDD70693,
 0x54DE5729,0x23D967BF,0xB3667A2E,0xC4614AB8,0x5D681B02,0x2A6F2B94,
 0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D
};
const unsigned char CLzxTool::table_one[32]=
{
 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13,14,14
};
const unsigned int CLzxTool::table_two[32]=
{
 0,1,2,3,4,6,8,12,16,24,32,48,64,96,128,192,256,384,512,768,1024,
 1536,2048,3072,4096,6144,8192,12288,16384,24576,32768,49152
};
const unsigned int CLzxTool::table_three[16]=
{
 0,1,3,7,15,31,63,127,255,511,1023,2047,4095,8191,16383,32767
};
const unsigned char CLzxTool::table_four[34]=
{
 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16
};
void CLzxTool::crc_calc(unsigned char *memory, unsigned int length)
{
	register unsigned int temp;

	if(length)
	{
		temp = ~sum; /* was (sum ^ 4294967295) */
		do
		{
			temp = crc_table[(*memory++ ^ temp) & 255] ^ (temp >> 8);
		} while(--length);
		sum = ~temp; /* was (temp ^ 4294967295) */
	}
}
int CLzxTool::make_decode_table(int number_symbols, int table_size,
                      unsigned char *length, unsigned short *table)
{
	register unsigned char bit_num = 0;
	register int symbol;
	unsigned int leaf; /* could be a register */
	unsigned int table_mask, bit_mask, pos, fill, next_symbol, reverse;
	int abort = 0;

	pos = 0; /* consistantly used as the current position in the decode table */

	bit_mask = table_mask = 1 << table_size;

	bit_mask >>= 1; /* don't do the first number */
	bit_num++;

	while((!abort) && (bit_num <= table_size))
	{
		for(symbol = 0; symbol < number_symbols; symbol++)
		{
			if(length[symbol] == bit_num)
			{
				reverse = pos; /* reverse the order of the position's bits */
				leaf = 0;
				fill = table_size;
				do /* reverse the position */
				{
					leaf = (leaf << 1) + (reverse & 1);
					reverse >>= 1;
				} while(--fill);
				if((pos += bit_mask) > table_mask)
				{
					abort = 1;
					break; /* we will overrun the table! abort! */
				}
				fill = bit_mask;
				next_symbol = 1 << bit_num;
				do
				{
					table[leaf] = symbol;
					leaf += next_symbol;
				} while(--fill);
			}
		}
		bit_mask >>= 1;
		bit_num++;
	}

	if((!abort) && (pos != table_mask))
	{
		for(symbol = pos; symbol < table_mask; symbol++) /* clear the rest of the table */
		{
			reverse = symbol; /* reverse the order of the position's bits */
			leaf = 0;
			fill = table_size;
			do /* reverse the position */
			{
				leaf = (leaf << 1) + (reverse & 1);
				reverse >>= 1;
			} while(--fill);
			table[leaf] = 0;
		}
		next_symbol = table_mask >> 1;
		pos <<= 16;
		table_mask <<= 16;
		bit_mask = 32768;

		while((!abort) && (bit_num <= 16))
		{
			for(symbol = 0; symbol < number_symbols; symbol++)
			{
				if(length[symbol] == bit_num)
				{
					reverse = pos >> 16; /* reverse the order of the position's bits */
					leaf = 0;
					fill = table_size;
					do /* reverse the position */
					{
						leaf = (leaf << 1) + (reverse & 1);
						reverse >>= 1;
					} while(--fill);
					for(fill = 0; fill < bit_num - table_size; fill++)
					{
						if(table[leaf] == 0)
						{
							table[(next_symbol << 1)] = 0;
							table[(next_symbol << 1) + 1] = 0;
							table[leaf] = next_symbol++;
						}
						leaf = table[leaf] << 1;
						leaf += (pos >> (15 - fill)) & 1;
					}
					table[leaf] = symbol;
					if((pos += bit_mask) > table_mask)
					{
						abort = 1;
						break; /* we will overrun the table! abort! */
					}
				}
			}
			bit_mask >>= 1;
			bit_num++;
		}
	}
	if(pos != table_mask) abort = 1; /* the table is incomplete! */

	return(abort);
}
int CLzxTool::read_literal_table()
{
	register unsigned int control;
	register int shift;
	unsigned int temp; /* could be a register */
	unsigned int symbol, pos, count, fix, max_symbol;
	int abort = 0;

	control = global_control;
	shift = global_shift;

	if(shift < 0) /* fix the control word if necessary */
	{
		shift += 16;
		control += *source++ << (8 + shift);
		control += *source++ << shift;
	}

	/* read the decrunch method */
	decrunch_method = control & 7;
	control >>= 3;
	if((shift -= 3) < 0)
	{
		shift += 16;
		control += *source++ << (8 + shift);
		control += *source++ << shift;
	}

	/* Read and build the offset huffman table */
	if((!abort) && (decrunch_method == 3))
	{
		for(temp = 0; temp < 8; temp++)
		{
			offset_len[temp] = control & 7;
			control >>= 3;
			if((shift -= 3) < 0)
			{
				shift += 16;
				control += *source++ << (8 + shift);
				control += *source++ << shift;
			}
		}
		abort = make_decode_table(8, 7, offset_len, offset_table);
	}

	/* read decrunch length */

	if(!abort)
	{
		decrunch_length = (control & 255) << 16;
		control >>= 8;
		if((shift -= 8) < 0)
		{
			shift += 16;
			control += *source++ << (8 + shift);
			control += *source++ << shift;
		}
		decrunch_length += (control & 255) << 8;
		control >>= 8;
		if((shift -= 8) < 0)
		{
			shift += 16;
			control += *source++ << (8 + shift);
			control += *source++ << shift;
		}
		decrunch_length += (control & 255);
		control >>= 8;
		if((shift -= 8) < 0)
		{
			shift += 16;
			control += *source++ << (8 + shift);
			control += *source++ << shift;
		}
	}

	/* read and build the huffman literal table */

	if((!abort) && (decrunch_method != 1))
	{
		pos = 0;
		fix = 1;
		max_symbol = 256;

		do
		{
			for(temp = 0; temp < 20; temp++)
			{
				huffman20_len[temp] = control & 15;
				control >>= 4;
				if((shift -= 4) < 0)
				{
					shift += 16;
					control += *source++ << (8 + shift);
					control += *source++ << shift;
				}
			}
			abort = make_decode_table(20, 6, huffman20_len, huffman20_table);

			if(abort) break; /* argh! table is corrupt! */

			do
			{
				if((symbol = huffman20_table[control & 63]) >= 20)
				{
					do /* symbol is longer than 6 bits */
					{
						symbol = huffman20_table[((control >> 6) & 1) + (symbol << 1)];
						if(!shift--)
						{
							shift += 16;
							control += *source++ << 24;
							control += *source++ << 16;
						}
						control >>= 1;
					} while(symbol >= 20);
					temp = 6;
				}
				else
				{
					temp = huffman20_len[symbol];
				}
				control >>= temp;
				if((shift -= temp) < 0)
				{
					shift += 16;
					control += *source++ << (8 + shift);
					control += *source++ << shift;
				}
				switch(symbol)
				{
				case 17:
				case 18:
					{
						if(symbol == 17)
						{
							temp = 4;
							count = 3;
						}
						else /* symbol == 18 */
						{
							temp = 6 - fix;
							count = 19;
						}
						count += (control & table_three[temp]) + fix;
						control >>= temp;
						if((shift -= temp) < 0)
						{
							shift += 16;
							control += *source++ << (8 + shift);
							control += *source++ << shift;
						}
						while((pos < max_symbol) && (count--))
							literal_len[pos++] = 0;
						break;
					}
				case 19:
					{
						count = (control & 1) + 3 + fix;
						if(!shift--)
						{
							shift += 16;
							control += *source++ << 24;
							control += *source++ << 16;
						}
						control >>= 1;
						if((symbol = huffman20_table[control & 63]) >= 20)
						{
							do /* symbol is longer than 6 bits */
							{
								symbol = huffman20_table[((control >> 6) & 1) + (symbol << 1)];
								if(!shift--)
								{
									shift += 16;
									control += *source++ << 24;
									control += *source++ << 16;
								}
								control >>= 1;
							} while(symbol >= 20);
							temp = 6;
						}
						else
						{
							temp = huffman20_len[symbol];
						}
						control >>= temp;
						if((shift -= temp) < 0)
						{
							shift += 16;
							control += *source++ << (8 + shift);
							control += *source++ << shift;
						}
						symbol = table_four[literal_len[pos] + 17 - symbol];
						while((pos < max_symbol) && (count--))
							literal_len[pos++] = symbol;
						break;
					}
				default:
					{
						symbol = table_four[literal_len[pos] + 17 - symbol];
						literal_len[pos++] = symbol;
						break;
					}
				}
			} while(pos < max_symbol);
			fix--;
			max_symbol += 512;
		} while(max_symbol == 768);

		if(!abort)
			abort = make_decode_table(768, 12, literal_len, literal_table);
	}

	global_control = control;
	global_shift = shift;

	return(abort);
}
void CLzxTool::decrunch()
{
	register unsigned int control;
	register int shift;
	unsigned int temp; /* could be a register */
	unsigned int symbol, count;
	unsigned char *string;

	control = global_control;
	shift = global_shift;

	do
	{
		if((symbol = literal_table[control & 4095]) >= 768)
		{
			control >>= 12;
			if((shift -= 12) < 0)
			{
				shift += 16;
				control += *source++ << (8 + shift);
				control += *source++ << shift;
			}
			do /* literal is longer than 12 bits */
			{
				symbol = literal_table[(control & 1) + (symbol << 1)];
				if(!shift--)
				{
					shift += 16;
					control += *source++ << 24;
					control += *source++ << 16;
				}
				control >>= 1;
			} while(symbol >= 768);
		}
		else
		{
			temp = literal_len[symbol];
			control >>= temp;
			if((shift -= temp) < 0)
			{
				shift += 16;
				control += *source++ << (8 + shift);
				control += *source++ << shift;
			}
		}
		if(symbol < 256)
		{
			*destination++ = symbol;
		}
		else
		{
			symbol -= 256;
			count = table_two[temp = symbol & 31];
			temp = table_one[temp];
			if((temp >= 3) && (decrunch_method == 3))
			{
				temp -= 3;
				count += ((control & table_three[temp]) << 3);
				control >>= temp;
				if((shift -= temp) < 0)
				{
					shift += 16;
					control += *source++ << (8 + shift);
					control += *source++ << shift;
				}
				count += (temp = offset_table[control & 127]);
				temp = offset_len[temp];
			}
			else
			{
				count += control & table_three[temp];
				if(!count) count = last_offset;
			}
			control >>= temp;
			if((shift -= temp) < 0)
			{
				shift += 16;
				control += *source++ << (8 + shift);
				control += *source++ << shift;
			}
			last_offset = count;

			count = table_two[temp = (symbol >> 5) & 15] + 3;
			temp = table_one[temp];
			count += (control & table_three[temp]);
			control >>= temp;
			if((shift -= temp) < 0)
			{
				shift += 16;
				control += *source++ << (8 + shift);
				control += *source++ << shift;
			}
			string = (decrunch_buffer + last_offset < destination) ?
						destination - last_offset : destination + 65536 - last_offset;
			do
			{
				*destination++ = *string++;
			} while(--count);
		}
	} while((destination < destination_end) && (source < source_end));

	global_control = control;
	global_shift = shift;
}
FILE *CLzxTool::open_output(char *filename)
{
	FILE *file;
	if (m_Endflg) return NULL;//追加 By Bonty
	//ワイルドカード処理
	if (!CheckStr(filename))//追加 By Bonty
	{
		return NULL;
	}

	kiutil::pathMake(filename);
	if(!(file = fopen(filename, "wb")))
	{
		//perror("FOpen");
	}
	return(file);
}
int CLzxTool::extract_normal(FILE *in_file)
{
	struct filename_node *node;
	FILE *out_file = 0;
	unsigned char *pos;
	unsigned char *temp;
	unsigned int count;
	int abort = 0;

	global_control = 0; /* initial control word */
	global_shift = -16;
	last_offset = 1;
	unpack_size = 0;
	decrunch_length = 0;

	for(count = 0; count < 8; count++)
		offset_len[count] = 0;
	for(count = 0; count < 768; count ++)
		literal_len[count] = 0;

	source_end = (source = read_buffer + 16384) - 1024;
	pos = destination_end = destination = decrunch_buffer + 258 + 65536;

	for(node = filename_list; (!abort) && node; node = node->next)
	{
		//printf("Extracting \"%s\"...", node->filename);
		fflush(stdout);

		out_file = open_output(node->filename);

		sum = 0; /* reset CRC */

		unpack_size = node->length;

		while(unpack_size > 0)
		{
			if(pos == destination) /* time to fill the buffer? */
			{
				/* check if we have enough data and read some if not */
				if(source >= source_end) /* have we exhausted the current read buffer? */
				{
					temp = read_buffer;
					if(count = temp - source + 16384)
					{
						do /* copy the remaining overrun to the start of the buffer */
						{
							*temp++ = *source++;
						} while(--count);
					}
					source = read_buffer;
					count = source - temp + 16384;

					if(pack_size < count) count = pack_size; /* make sure we don't read too much */

					if(fread(temp, 1, count, in_file) != count)
					{
						//printf("\n");
						/*if(ferror(in_file))
							//perror("FRead(Data)");
						else
							//fprintf(stderr, "EOF: Data\n");*/
						abort = 1;
						break; /* fatal error */
					}
					pack_size -= count;

					temp += count;
					if(source >= temp) break; /* argh! no more data! */
				} /* if(source >= source_end) */

				/* check if we need to read the tables */
				if(decrunch_length <= 0)
				{
					if(read_literal_table()) break; /* argh! can't make huffman tables! */
				}

				/* unpack some data */
				if(destination >= decrunch_buffer + 258 + 65536)
				{
					if(count = destination - decrunch_buffer - 65536)
					{
						temp = (destination = decrunch_buffer) + 65536;
						do /* copy the overrun to the start of the buffer */
						{
							*destination++ = *temp++;
						} while(--count);
					}
					pos = destination;
				}
				destination_end = destination + decrunch_length;
				if(destination_end > decrunch_buffer + 258 + 65536)
					destination_end = decrunch_buffer + 258 + 65536;
				temp = destination;

				decrunch();

				decrunch_length -= (destination - temp);
			}

			/* calculate amount of data we can use before we need to fill the buffer again */
			count = destination - pos;
			if(count > unpack_size) count = unpack_size; /* take only what we need */

			crc_calc(pos, count);

			if(out_file) /* Write the data to the file */
			{
				if(fwrite(pos, 1, count, out_file) != count)
				{
					//perror("FWrite"); /* argh! write error */
					fclose(out_file);
					out_file = 0;
				}
			}
			unpack_size -= count;
			pos += count;
		}

		if(out_file)
		{
			fclose(out_file);
			//if(!abort) //printf(" crc %s\n", (node->crc == sum) ? "good" : "bad");
		}
	} /* for */

	return(abort);
}
int CLzxTool::extract_store(FILE *in_file)
{
	struct filename_node *node;
	FILE *out_file;
	unsigned int count;
	int abort = 0;

	for(node = filename_list; (!abort) && node; node = node->next)
	{
		//printf("Storing \"%s\"...", node->filename);
		fflush(stdout);

		out_file = open_output(node->filename);

		sum = 0; /* reset CRC */

		unpack_size = node->length;
		if(unpack_size > pack_size) unpack_size = pack_size;

		while(unpack_size > 0)
		{
			count = (unpack_size > 16384) ? 16384 : unpack_size;

			if(fread(read_buffer, 1, count, in_file) != count)
			{
				//printf("\n");
				/*if(ferror(in_file))
					//perror("FRead(Data)");
				else
					//fprintf(stderr, "EOF: Data\n");*/
				abort = 1;
				break; /* fatal error */
			}
			pack_size -= count;

			crc_calc(read_buffer, count);

			if(out_file) /* Write the data to the file */
			{
				if(fwrite(read_buffer, 1, count, out_file) != count)
				{
					//perror("FWrite"); /* argh! write error */
					fclose(out_file);
					out_file = 0;
				}
			}
			unpack_size -= count;
		}

		if(out_file)
		{
			fclose(out_file);
			//if(!abort) //printf(" crc %s\n", (node->crc == sum) ? "good" : "bad");
		}
	} /* for */

	return(abort);
}
int CLzxTool::extract_unknown(FILE *in_file)
{
	struct filename_node *node;
	int abort = 0;

	for(node = filename_list; node; node = node->next)
	{
		//printf("Unknown \"%s\"\n", node->filename);
	}

	return(abort);
}
int CLzxTool::extract_archive(FILE *in_file)
{
 unsigned int temp;
 struct filename_node **filename_next;
 struct filename_node *node;
 struct filename_node *temp_node;
 int actual;
 int abort;
 int result = 1; /* assume an error */
 SYSTEMTIME stime;
 FILETIME ftime;
 filename_list = 0; /* clear the list */
 filename_next = &filename_list;

 do
 {
  if (m_Endflg) return 0;//追加 By Bonty
  abort = 1; /* assume an error */
  actual = fread(archive_header, 1, 31, in_file);
  if(!ferror(in_file))
  {
   if(actual) /* 0 is normal and means EOF */
   {
    if(actual == 31)
    {
     sum = 0; /* reset CRC */
     crc = (archive_header[29] << 24) + (archive_header[28] << 16) + (archive_header[27] << 8) + archive_header[26]; /* header crc */
     archive_header[29] = 0; /* Must set the field to 0 before calculating the crc */
     archive_header[28] = 0;
     archive_header[27] = 0;
     archive_header[26] = 0;
     crc_calc(archive_header, 31);
     temp = archive_header[30]; /* filename length */
     actual = fread(header_filename, 1, temp, in_file);
     if(!ferror(in_file))
     {
      if(actual == temp)
      {
       header_filename[temp] = 0;
       crc_calc(header_filename, temp);
       temp = archive_header[14]; /* comment length */
       actual = fread(header_comment, 1, temp, in_file);
       if(!ferror(in_file))
       {
        if(actual == temp)
        {
         header_comment[temp] = 0;
         crc_calc(header_comment, temp);
         if(sum == crc)
         {
          unpack_size = (archive_header[5] << 24) + (archive_header[4] << 16) + (archive_header[3] << 8) + archive_header[2]; /* unpack size */
          pack_size = (archive_header[9] << 24) + (archive_header[8] << 16) + (archive_header[7] << 8) + archive_header[6]; /* packed size */
          pack_mode = archive_header[11]; /* pack mode */
          crc = (archive_header[25] << 24) + (archive_header[24] << 16) + (archive_header[23] << 8) + archive_header[22]; /* data crc */
          
		  temp = (archive_header[18] << 24) + (archive_header[19] << 16) + (archive_header[20] << 8) + archive_header[21]; /* date */
          stime.wYear=1970+((temp >> 17) & 63);
          stime.wMonth=(temp >> 23) & 15;
          stime.wDayOfWeek=1;//適当でいいか^^;
          stime.wDay=(temp >> 27) & 31;
          stime.wHour=(temp >> 12) & 31;
          stime.wMinute=(temp >> 6) & 63;
          stime.wSecond=temp & 63;
          stime.wMilliseconds=0;
          SystemTimeToFileTime(&stime,&ftime);
          if(node = (struct filename_node *)malloc(sizeof(struct filename_node))) /* allocate a filename node */
          {
           *filename_next = node; /* add this node to the list */
           filename_next = &(node->next);
           node->next = 0;
           node->length = unpack_size;
           node->crc = crc;
		   node->ftime=ftime;
           for(temp = 0; node->filename[temp] = header_filename[temp]; temp++);

           if(pack_size)
           {
            switch(pack_mode)
            {
             case 0: /* store */
             {
              abort = extract_store(in_file);
              break;
             }
             case 2: /* normal */
             {
              abort = extract_normal(in_file);
              break;
             }
             default: /* unknown */
             {
              abort = extract_unknown(in_file);
              break;
             }
            }
            if(abort) break; /* a read error occured */

            temp_node = filename_list; /* free the list now */
            while(node = temp_node)
            {
			 kiutil::timeSet(node->filename,&node->ftime);
             temp_node = node->next;
             free(node);
            }
            filename_list = 0; /* clear the list */
            filename_next = &filename_list;

            if(fseek(in_file, pack_size, SEEK_CUR))
            {
             //perror("FSeek(Data)");
             break;
            }
           }
           else
            abort = 0; /* continue */
          }
          //else
           //fprintf(stderr, "MAlloc(Filename_node)\n");
         }
         //else
          //fprintf(stderr, "CRC: Archive_Header\n");
        }
        //else
         //fprintf(stderr, "EOF: Header_Comment\n");
       }
       //else
        //perror("FRead(Header_Comment)");
      }
      //else
       //fprintf(stderr, "EOF: Header_Filename\n");
     }
     //else
      //perror("FRead(Header_Filename)");
    }
    //else
     //fprintf(stderr, "EOF: Archive_Header\n");
   }
   else
   {
    result = 0; /* normal termination */
   }
  }
  //else
   //perror("FRead(Archive_Header)");
 } while(!abort);

/* free the filename list in case an error occured */
 temp_node = filename_list;
 while(node = temp_node)
 {
  temp_node = node->next;
  free(node);
 }

 return(result);
}