
#include "stdafx.h"
#include "askTool.h"

bool CAskTool::Check( const char* fname, unsigned long fsize )
{
	// 拡張子チェック
	LPCSTR ext=kiutil::pathExt(fname);
	if( 0==stricmp( ext,"ask" ) )
		return true;
	if( 0==stricmp( ext,"apk" ) )
		return true;

	return false;
}

bool CAskTool::Extract( const char* fname, const char* ddir )
{
	LPCSTR ext=kiutil::pathExt(fname);
	if( 0==stricmp( ext,"ask" ) )
	{
		unsigned long int len=0;
		unsigned char *buffer= UnPack_ask(fname,len);
		if(buffer == NULL)
			return false;
		char outname[MAX_PATH];
		kiutil::getOriginalName( outname,fname,"org");
		if (m_Endflg) return true;//追加 By Bonty
		if (!CheckStr(outname)){free(buffer); return true;}//追加 By Bonty
		
		// Every byte index below is inside the unpacked buffer, so the
		// extension sniffs have to respect `len`.  A short buffer used
		// to be read 12 bytes deep regardless of its real size.
		char* ext2 = "org";
		if( len>=12 )
		{
			if(buffer[8]=='W'&&buffer[9]=='A'&&buffer[10]=='V'&&buffer[11]=='E')
				ext2 = "wav";
			else if(buffer[0]=='B'&&buffer[1]=='M')  ext2 = "bmp";
			else if(buffer[0]=='G'&&buffer[1]=='I'&&buffer[2]=='F') ext2 = "gif";
			else if(buffer[0]=='M'&&buffer[1]=='T')  ext2 = "mid";
			else if(buffer[0]=='M'&&buffer[1]=='Z')  ext2 = "exe";
			else if(buffer[3]=='l'&&buffer[4]=='h')  ext2 = "lzh";
			else if(buffer[8]=='A'&&buffer[9]=='V'&&buffer[10]=='I') ext2 = "avi";
			else if(buffer[6]=='J'&&buffer[7]=='F'&&buffer[8]=='I'&&buffer[9]=='F') ext2 = "jpg";
		}
		if( 0!=stricmp(ext2,"org") )
			kiutil::getOriginalName( outname,fname,ext2 );
		
		FILE*fp = fopen(outname,"wb");
		if(fp == NULL){
			free(buffer);
			return false;
		}else{
			fwrite(buffer,1,len,fp);
			fclose(fp);
		}
		free(buffer);
	}else if( 0==stricmp( ext,"apk" ) )
	{
		return Cut(fname);
	}else
		return false;

	return true;
}

unsigned char* CAskTool::UnPack_ask(const char *file,unsigned long int &length)
{
	int handle;
	unsigned long int file_length;
	unsigned char *root;
	unsigned char *buffer;
	FILE *fp;

	length = 0;
	fp = fopen(file,"rb");
	if( fp==NULL )
		return NULL;
	/*ファイルのサイズを得る*/
	handle = open(file, O_RDONLY | O_BINARY );
	if( handle==-1 )
	{
		fclose(fp);
		return NULL;
	}
	file_length = filelength(handle );
	close(handle );
	
	// UnPack reads a 7-byte header, so a shorter file cannot be
	// decoded; the extra byte keeps every header scan in bounds.
	if( file_length<8 || file_length>0x7fffffffUL )
	{
		fclose(fp);
		return NULL;
	}
	if((root = (unsigned char*)malloc(file_length+1))==NULL){
		fclose(fp);
		return NULL;
	}
	if( (unsigned long int)fread(root,1,file_length,fp)!=file_length )
	{
		free(root);
		fclose(fp);
		return NULL;
	}
	root[file_length] = 0;
	fclose(fp);
	// UnPack no longer frees root: the caller owns it, and it used to
	// free it too -> double free (and in Cut(), an interior pointer).
	buffer = UnPack(root,file_length,length);
	free(root);
	return buffer;
}

unsigned char* CAskTool::UnPack(unsigned char *root,unsigned long int file_length,unsigned long int &length)
{
    unsigned long int pr;
    unsigned long int pb;
    unsigned long int i,j;
    unsigned long int want;
    short d;
    unsigned char l;
    unsigned char flag;
    unsigned char *buffer;

    length = 0;
    if( file_length<7 || root==NULL )
        return NULL;

    length = (unsigned long int)root[0]
           | ((unsigned long int)root[1]<<8)
           | ((unsigned long int)root[2]<<16)
           | ((unsigned long int)root[3]<<24);

    // length comes straight out of the file.  It used to be handed to
    // malloc unchecked, so a 1-byte .ask asked for 4 GB and then wrote
    // 39 MB of output.  Ceiling it also makes the three fixed header
    // bytes below safe.
    if( length<3 || length>0x10000000UL )
        return NULL;

    // An absolute ceiling is not enough: a 2 KB file was still
    // writing 249 MB.  This is a plain LZ77, so an expansion of
    // 4096:1 is far beyond anything real (bzip2 caps out near
    // 1000:1 per block), and the floor keeps small files usable.
    {
        unsigned long int cap = file_length*4096UL;
        if( cap<0x100000UL )
            cap = 0x100000UL;
        if( length>cap )
            return NULL;
    }

    if((buffer = (unsigned char*)malloc(length))==NULL)
        return NULL;

    buffer[0] = root[4];
    buffer[1] = root[5];
    buffer[2] = root[6];
    pr = 7;
    pb = 3;

    while(pr<file_length&&pb<length)
	{
        flag = root[pr];
        pr++;
        for(i=0;i<8;i++){
            if(((flag>>i)&1)==0){
                if( pr>=file_length || pb>=length )
                    goto done;
                buffer[pb] = root[pr];
                pb++;
                pr++;
            }else{
                // One 16-bit back-reference: low 12 bits are the
                // distance-1, top 4 bits the length-3.
                if( pr+2>file_length )
                    goto done;
                d = (short)( (unsigned short)root[pr]
                           | (unsigned short)((root[pr+1]&0x0f)<<8) );
                l = (unsigned char)((root[pr+1]>>4)+3);
                pr += 2;
                d++;
                // A distance larger than what has been decoded yet
                // read from before the start of buffer.
                if( d<1 || (unsigned long int)d>pb )
                    goto done;
                // The copy used to run past the end of buffer.
                want = length-pb;
                if( (unsigned long int)l>want )
                    l = (unsigned char)want;
                for(j=0;j<(unsigned long int)l;j++){
                    buffer[pb+j]=buffer[pb-d+j];
                }
                pb += l;
            }
            if(pr>=file_length||pb>=length)break;
        }
    }
done:
    return buffer;
}

bool CAskTool::Cut(const char *file)
{
	int h;
	unsigned long int file_length,p=0,len;
	unsigned long int i,k;
	unsigned char *root;
	unsigned char num;
	typedef struct{
		char name[30];
		unsigned long int size;
	}_fd_;
	char name[50];
	unsigned char *ans;
	_fd_ *fd_;

    /*ファイルのサイズを得る*/

    h = open(file, O_RDONLY | O_BINARY );
    
    if(h != -1 ) {
      file_length = filelength(h);
      close(h);
    }else{
        return false;
    }

    if( file_length<1 || file_length>0x7fffffffUL )
        return false;

    if((root = (unsigned char*)malloc(file_length+1))==NULL){
        return false;
    }
    FILE *fp;
    fp = fopen(file,"rb");
    
    if(fp == NULL){
        free(root);
		//MessageBox(NULL,"おおっと！ファイル開けず！！","ERROR",MB_OK);
        return false;
    }
    
    if( (unsigned long int)fread(root,1,file_length,fp)!=file_length )
    {
        free(root);
        fclose(fp);
        return false;
    }
    // No NUL terminator before: the name scan below walked off the heap.
    root[file_length] = 0;
    fclose(fp);

	num = root[p];
	p++;

	bool ret=true;
	bool stop=false;

	if( (fd_ = (_fd_*)malloc(sizeof(_fd_)*(num+1)))==NULL){
		ret=false;
		//MessageBox(NULL,"分割失敗！！","ERROR",MB_OK);
	}else{
		for(i=0;i<(unsigned long int)num;i++){
			memset(fd_[i].name,'\0',sizeof(fd_[i].name));
			// The entry name is bounded by both the 30-byte field and
			// the end of the file.  sscanf("%[^\n]") had neither, so a
			// long or unterminated entry smashed the stack (0xC00000FD).
			k=0;
			while( p+k<file_length && root[p+k]!='\n'
			    && k<sizeof(fd_[i].name)-1 )
			{
				fd_[i].name[k] = (char)root[p+k];
				k++;
			}
			fd_[i].name[k] = '\0';
			p += k;
			if( p<file_length )
				p++;              // step over the newline
			if( p+4>file_length )
			{
				ret=false;
				break;
			}
			fd_[i].size = ((unsigned long int)root[p]<<24)
			            | ((unsigned long int)root[p+1]<<16)
			            | ((unsigned long int)root[p+2]<<8)
			            | ((unsigned long int)root[p+3]);
			p += 4;
		}
		for(i=0; i<(unsigned long int)num && ret; i++){
			if( fd_[i].size>file_length || p+fd_[i].size<p )
				break;
			lstrcpy(name,fd_[i].name);
			kiutil::pathMake(name);
			if (m_Endflg){ stop=true; break; }//追加 By Bonty
			if (CheckStr(name))//追加 By Bonty
			{
				if((ans=UnPack(root+p,fd_[i].size,len))==NULL){
					ret=false;
					//MessageBox(NULL,"解凍失敗したかも","ERROR",MB_OK);
				}else{
					fp = fopen(name,"wb");
					if(fp == NULL){
						ret=false;
						//MessageBox(NULL,"解凍はできたけどファイルに保存できない","ERROR",MB_OK);
					}else{
						fwrite(ans,len,1,fp);
						fclose(fp);
					}
					free(ans);
				}
			}
			p += fd_[i].size;
		}
		free(fd_);       // was leaked
	}
	free(root);
	(void)stop;
	return ret;
}
