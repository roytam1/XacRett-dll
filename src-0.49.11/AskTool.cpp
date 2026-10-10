
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
		unsigned long int len;
		unsigned char *buffer= UnPack_ask(fname,len);
		if(buffer == NULL)
			return false;
		char outname[MAX_PATH];
		kiutil::getOriginalName( outname,fname,"org");
		if (m_Endflg) return true;//追加 By Bonty
		if (!CheckStr(outname)){free(buffer); return true;}//追加 By Bonty
		
		if(buffer[8]=='W'&&buffer[9]=='A'&&buffer[10]=='V'&&buffer[11]=='E'){
			kiutil::getOriginalName( outname,fname,"wav" );
		}else if(buffer[0]=='B'&&buffer[1]=='M'){
			kiutil::getOriginalName( outname,fname,"bmp");
		}else if(buffer[0]=='G'&&buffer[1]=='I'&&buffer[2]=='F'){
			kiutil::getOriginalName( outname,fname,"gif");
		}else if(buffer[0]=='M'&&buffer[1]=='T'){
			kiutil::getOriginalName( outname,fname,"mid");
		}else if(buffer[0]=='M'&&buffer[1]=='Z'){
			kiutil::getOriginalName( outname,fname,"exe");
		}else if(buffer[3]=='l'&&buffer[4]=='h'){
			kiutil::getOriginalName( outname,fname,"lzh");
		}else if(buffer[8]=='A'&&buffer[9]=='V'&&buffer[10]=='I'){
			kiutil::getOriginalName( outname,fname,"avi");
		}else if(buffer[6]=='J'&&buffer[7]=='F'&&buffer[8]=='I'&&buffer[9]=='F'){
			kiutil::getOriginalName( outname,fname,"jpg");
		}else{
			kiutil::getOriginalName( outname,fname,"org");
		}
		
		FILE*fp = fopen(outname,"wb");
		if(fp == NULL){
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
	fp = fopen(file,"rb");

	if(fp == NULL){
		return NULL;
	}
	/*ファイルのサイズを得る*/
	handle = open(file, O_RDONLY | O_BINARY );

	if(handle != -1 ) {
		file_length = filelength(handle );
		close(handle );
	}else{
		return NULL;
	}
   
	if((root = (unsigned char*)malloc(file_length))==NULL){
		return NULL;
	}
	fread(root,1,file_length,fp);
	fclose(fp);
	buffer = UnPack(root,file_length,length);
	free(root);
	return buffer;
}
unsigned char* CAskTool::UnPack(unsigned char *root,unsigned long int file_length,unsigned long int &length)
{
    unsigned long int pr;
    unsigned long int pb;
    short d;
    unsigned char l;
    unsigned char flag;
    unsigned char i,j;
    unsigned char *buffer;
    
    length = root[0]+(root[1]<<8)+(root[2]<<16)+(root[3]<<24);
    
    if((buffer = (unsigned char*)malloc(length))==NULL){
        free(root);
        return NULL;
    }
    
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
                buffer[pb] = root[pr];
                pb++;
                pr++;
            }else{
                d = root[pr];
                pr++;
                d += (((root[pr])&0x0f)<<8);
                d++;
                l = ((root[pr])>>4);
                l += 3;
                pr++;
                for(j=0;j<l;j++){
                    buffer[pb+j]=buffer[pb-d+j];
                }
                pb += l;
            }
            if(pr>=file_length||pb>=length)break;
        }
    }
    return buffer;
}
bool CAskTool::Cut(const char *file)
{
	int h;
	unsigned long int file_length,p=0,len;
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
    
    if((root = (unsigned char*)malloc(file_length))==NULL){
        return false;
    }
    FILE *fp;
    fp = fopen(file,"rb");
    
    if(fp == NULL){
        free(root);
		//MessageBox(NULL,"おおっと！ファイル開けず！！","ERROR",MB_OK);
        return false;
    }
    
    fread(root,1,file_length,fp);
    fclose(fp);

	num = root[p];
	p++;

	bool ret=true;

	if((fd_ = (_fd_*)malloc(sizeof(_fd_)*num))==NULL){
		ret=false;
		//MessageBox(NULL,"分割失敗！！","ERROR",MB_OK);
	}else{
		for(unsigned char i=0;i<num;i++){
			memset(fd_[i].name,'\0',sizeof(fd_[i].name));
			sscanf((char*)(root+p),"%[^\n]",fd_[i].name);
			p+=strlen(fd_[i].name)+1;

			fd_[i].size = (*(root+p)<<24)+(*(root+p+1)<<16)+(*(root+p+2)<<8)+(*(root+p+3));
			p+=4;

		}
		for(i=0;i<num;i++){
			/*sprintf(name,"%s\\",_dir);
			strcat(name,fd_[i].name);*/
			lstrcpy(name,fd_[i].name);
			kiutil::pathMake(name);
			if (m_Endflg) return true;//追加 By Bonty
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
			p+=fd_[i].size;
		}
	}
	free(root);
	return ret;
}