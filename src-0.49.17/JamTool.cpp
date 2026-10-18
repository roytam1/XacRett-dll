
#include "stdafx.h"
#include "JamTool.h"

bool CJamTool::Check( const char* fname, unsigned long fsize )
{
	// 拡張子チェック
	return ( 0==stricmp( kiutil::pathExt(fname),"jam" ) );
}

bool CJamTool::Extract( const char* fname, const char* ddir )
{
	bool ans = false;

	// jamファイルを開く
	if( !(jam=fopen( fname,"rb" )) )
		return false;

	// 3-byte header: flag, then the 16-bit password CRC
	jam_stored = jam_enc = false;
	jam_key = NULL;
	int h0 = fgetc(jam);
	int h1 = fgetc(jam);
	int h2 = fgetc(jam);
	if( h0==EOF ){ fclose(jam); return false; }
	jam_stored = (h0 & 1) ? true : false;
	jam_enc    = (h0 & 2) ? true : false;
	unsigned short want_crc = (unsigned short)( h1 | (h2 << 8) );

	// 出力先
	char outname[MAX_PATH];
	kiutil::getOriginalName( outname, fname,
		0==stricmp(kiutil::pathExt(fname),"jam") ? NULL : "unjam" );

	if (m_Endflg) return true;//追加 By Bonty
	//ワイルドカード処理
	if (!CheckStr(outname)){fclose(jam);return true;}//追加 By Bonty

	// 展開
	kiutil::pathMake83(outname);
	if( out=fopen( outname,"wb" ) )
	{
		if( jam_stored )
		{
			int c;
			while( (c=fgetc(jam))!=EOF ) fputc(c,out);
			ans = true;
		}
		else if( jam_enc )
		{
			char* pw = GetPassword();
			if( !pw || !jam_decrypt( pw, want_crc ) )
			{
				fclose(out); out=NULL;
				jam_enc_free();
				fclose( jam );
				DeleteFile(outname);
				return false;
			}
			ans = unjam();
			jam_enc_free();
		}
		else if( unjam() ) // Jam32.dll 互換モード
			ans = true;
		else
		{
			fseek( jam, 3, SEEK_SET ); // unjam.com 互換。
									   // ヘッダ3byteすっ飛ばす。
			fclose( out );
			if( out=fopen( outname, "wb" ) )
			{
				fseek( jam, 0, SEEK_SET );
				ans = unjam();
			}
		}

		if( out )
			fclose( out );
	}

	fclose( jam );
	return ans;
}

//--------------------------------------//

#define jam_dic_size (8192)
#define jam_nil      (256)
#define jam_fstcode  (257)
#define jam_base     (256)
#define jam_unused   (0)

static unsigned short jam_keycrc( const unsigned char* k, int n )
{ // CRYPTION.DOC: 16-bit CRC of the key string
	DWORD acc=0;
	for( int i=0; i<n; i++ )
	{
		acc |= k[i];
		for( int j=0; j<8; j++ )
		{
			acc <<= 1;
			if( acc & 0x01000000 )
				acc ^= 0x01102100;
		}
		acc &= 0x00ffff00;
	}
	return (unsigned short)( acc >> 8 );
}

int CJamTool::jam_getc()
{ // reads one byte, decrypting it when the stream is encrypted
	int c = fgetc(jam);
	if( c==EOF || !jam_enc || !jam_key )
		return c;
	unsigned char k = jam_key[jam_enc_i];
	unsigned char p = (unsigned char)( c ^ jam_prev ^ jam_veil ^ k );
	jam_key[jam_enc_i] = (unsigned char)( k + (jam_enc_i+1) );
	jam_enc_i++;
	if( jam_enc_i >= jam_enc_n ) jam_enc_i = 0;
	jam_prev = p;
	return p;
}

void CJamTool::jam_enc_free()
{
	if( jam_key ){ delete [] jam_key; jam_key = NULL; }
	jam_enc = false;
}

bool CJamTool::jam_decrypt( const char* pw, unsigned short stored_crc )
{ // sets up the cipher; refuses a wrong password
	int n = (int)strlen(pw);
	if( n<=0 )
		return false;
	jam_enc_n = n;
	jam_enc_i = 0;
	jam_veil = 0;
	jam_prev = 0;
	jam_key = new unsigned char[n];
	for( int i=0; i<n; i++ )
	{
		jam_key[i] = (unsigned char)pw[i];
		jam_veil ^= jam_key[i];
	}
	if( jam_keycrc( jam_key, n ) != stored_crc )
	{
		jam_enc_free();
		return false;
	}
	jam_enc = true;
	return true;
}

bool CJamTool::unjam()
{
	// 変数初期化
	bitbuf = bits = 0;
	dic = new jam_dic_item[ jam_dic_size ];
	code_len = 9, available = jam_fstcode-1;
	mapped = tagged = pure_char = false;
	int c, old_code = jam_nil;
 	
	// 辞書初期化
	for( int i=0; i!=jam_fstcode; i++ )
	{
		dic[i].name= i;
		dic[i].son = jam_nil;
		dic[i].dad = jam_nil;
	}

	while( true )
	{
		code = read_code();
		if( code==EOF || code==jam_nil || code>available )
			break;
		if( code<available || old_code==jam_nil )
			c = write_string( code );
		else
		{
			c = write_string( old_code );
			fputc( c,out );
		}

		if( old_code != jam_nil )
		{
			dic[available].name = c;
			dic[available].son  = jam_nil;
			dic[available].dad  = old_code;
		}
		available++;
		if( available==(1<<code_len) )
			code_len++;
		old_code = code;
		if( available==jam_dic_size )
		{
			shrink();
			while( available<=(1<<(code_len-1)) )
				code_len--;
			available--;
			old_code = jam_nil;
		}
	}

	delete [] dic;
	return (code==EOF);
}

int CJamTool::read_code()
{
	int val;
	if( !tagged )
	{
		val = read_bits(code_len);
		if( val==EOF )
			return EOF;
		if( !mapped )
		{
			if( val<=available ) // unshifted
			{
				if( val+available+1 < 1<<code_len ) // but shiftable
					mapped = true;
				return val;
			}
			val-=available+1; tagged=1; pure_char=1;
			return val;
		}
		else // mapped
		{
			mapped = false;
			if( val <= available-jam_base ) // unshifted
			{
				if( val+available+1-jam_base < 1<<code_len)
					mapped = true;
				return val+jam_base;
			}
			val-=available+1-jam_base; tagged=true; pure_char=true;
			return val+jam_base;
		}
	}
	else // tagged
	{
		if( pure_char )
		{
			val = read_bits(8);
			pure_char = false;
			return val;
		}
		else
		{
			int tag = read_bits(1);
			if( tag==EOF )
				return EOF;
			if( tag==0 ) // char
			{
				val = read_bits(8);
				return val;
			}
			else // tag==1
			{
				tagged = false;
				val = read_bits(code_len);
				if( val==EOF )
					return EOF;
				if( val > available-jam_base )
					val-=available-jam_base+1, tagged=true, pure_char=true;
				else if (val+available+1-jam_base<1<<code_len)
					mapped = true;
				return val+jam_base;
			}
		}
	}
}

int CJamTool::read_bits(int len)
{
	int c;
	while( bits<len )
	{
		c = jam_getc();
		if( c==EOF )
			return EOF;
		bitbuf = (bitbuf<<8)|c;
		bits += 8;
	}
	bits -= len;
	return (bitbuf>>bits)&((1<<len)-1);
}

unsigned char CJamTool::write_string( int code )
{
	unsigned char c;
	int i = code;
	while( dic[i].dad != jam_nil )
	{
		int j = dic[i].dad;
		dic[j].son = i;
		i = j;
	}
	c = dic[i].name;

	// write out the string
	do
	{
		int j = dic[i].son;
		dic[i].son = jam_nil;
		fputc( dic[i].name, out );
		i = j;
	} while( i!=jam_nil );

	return c;
}

void CJamTool::shrink()
{
	int i; // 'dad' will be used as 'next'

	// step one: mark leaves, reconstruct the tree structure, and
	//  transform the tree to well-formed
	for( i=available-1; i>=jam_fstcode; i-- )
	{
		int j = dic[i].dad;
		if( dic[j].son == jam_nil )
			dic[j].son=jam_unused;
		if (dic[i].son==jam_nil)
		{
			dic[i].son=jam_unused;
			if( i<available )
				available=i;
			continue;
		}
		if (dic[i].son==jam_unused) dic[i].son=jam_nil;
		if (dic[j].son==jam_unused) dic[j].son=jam_nil;
		dic[i].dad/*next*/=dic[j].son; dic[j].son=i;
	}
	for (i=0; i<jam_fstcode; i++)
		if (dic[i].son==jam_unused)
			dic[i].son=jam_nil;

	// step two: move the unused nodes to the end of the array.
	//  notice that the tree remains well-formed after each loop!
	i=0;
	while (i<available)
	{
		int j;
		// if i has a son behind available, move it to available
		j=dic[i].son;
		if (j!=jam_nil && j>available)
		{
			dic[available]=dic[j];
			dic[i].son=available;
			dic[j].son=jam_unused;
			do
				available++;
			while (dic[available].son!=jam_unused);
		}

		// do the same to the brother of i
		j=dic[i].dad/*next*/;
		if (j!=jam_nil && j>available)
		{
			dic[available]=dic[j];
			dic[i].dad/*next*/=available;
			dic[j].son=jam_unused;
			do
				available++;
			while (dic[available].son!=jam_unused);
		}
		i++;
	}

	// step three: transform the tree back to its original form
	for (i=0; i<available; i++)
	{
		int j=dic[i].son;
		dic[i].son=jam_nil;
		while (j!=jam_nil)
		{
			int k=dic[j].dad;/*next*/
			dic[j].dad=i;
			j=k;
		}
	}
}
