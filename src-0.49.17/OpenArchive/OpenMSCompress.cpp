#include "stdafx.h"
#include "OpenMSCompress.h"
#include <lzexpand.h>

bool COpenMSCompress::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	int original_size = 0;
	fseek(COpenMSCompress::fp,10,SEEK_SET );
	fread(&original_size,4,1,COpenMSCompress::fp );
	fseek(COpenMSCompress::fp,0,SEEK_SET );

	lpSubInfo->dwCompressedSize = (DWORD)filelength(fileno(COpenMSCompress::fp));
	lpSubInfo->dwOriginalSize = (DWORD)original_size;
	char	buff[FNAME_MAX32 + 1];
	if (COpenMSCompress::GetLzFileName(buff))
		lstrcpy(lpSubInfo->szFileName,buff);

	double tmp1 = (double)lpSubInfo->dwOriginalSize;
	double tmp2 = (double)lpSubInfo->dwCompressedSize;
	if (lpSubInfo->dwOriginalSize > 0 && lpSubInfo->dwCompressedSize > 0){
		lpSubInfo->wRatio = (WORD)((tmp2 *1000 / tmp1));
	}
	//“ú•t‚ð‘ŒÉ‚É‡‚í‚¹‚é
	WIN32_FIND_DATA find_f;
	HANDLE h = FindFirstFile( COpenMSCompress::szArcFileName,&find_f );
	if( h==INVALID_HANDLE_VALUE )
		return false;
	FindClose( h );
	::FileTimeToDosDateTime(&find_f.ftLastWriteTime,&lpSubInfo->wDate,&lpSubInfo->wTime);

	lstrcpy(lpSubInfo->szAttribute ,"A---W");
	lstrcpy(lpSubInfo->szMode,"SZDD");
	//lstrcpy(lpSubInfo->szMode,"");

	return true;
}

bool COpenMSCompress::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return false;
}

bool COpenMSCompress::GetLzFileName(LPSTR buf)
{
	char str[FNAME_MAX32 + 1];
	char buf2[FNAME_MAX32 + 1];
	lstrcpy(str,COpenMSCompress::szArcFileName);

	if (GetExpandedName(str,buf2))//¬Œ÷‚µ‚½‚ÆŒ¾‚¤‚±‚Æ
	{
		for(int i = FNAME_MAX32 + 1;buf2[i]!=0x5C;--i);
		lstrcpy(buf,&buf2[i+1]);
		return true;
	}
	//Ž¸”s‚µ‚½‚È‚çŽ©•ª‚Å‚â‚ç‚È‚«‚á¥¥¥0x5C='\'
	for(int i = FNAME_MAX32 + 1;str[i]!=0x5C;--i);
	lstrcpy(buf2,&str[i+1]);
	for(i = 0;buf2[i]!=0x00;i++);
	if (buf2[i-1] = '_')
		buf2[i-1] = 0x00;

	lower(&buf2[i-3]);
	if( 0==strcmp(&buf2[i-3],"ex") )buf2[i-1] = 'e';
	else if( 0==strcmp(&buf2[i-3],"co") )buf2[i-1] = 'm';
	else if( 0==strcmp(&buf2[i-3],"sc") )buf2[i-1] = 'r';

	else if( 0==strcmp(&buf2[i-3],"dl") )buf2[i-1] = 'l';
	else if( 0==strcmp(&buf2[i-3],"oc") )buf2[i-1] = 'x';
	else if( 0==strcmp(&buf2[i-3],"dr") )buf2[i-1] = 'v';
	else if( 0==strcmp(&buf2[i-3],"vx") )buf2[i-1] = 'd';
	else if( 0==strcmp(&buf2[i-3],"38") )buf2[i-1] = '6';
	else if( 0==strcmp(&buf2[i-3],"sy") )buf2[i-1] = 's';
	else if( 0==strcmp(&buf2[i-3],"cp") )buf2[i-1] = 'l';
	else if( 0==strcmp(&buf2[i-3],"li") )buf2[i-1] = 'b';
	else if( 0==strcmp(&buf2[i-3],"tt") )buf2[i-1] = 'f';

	else if( 0==strcmp(&buf2[i-3],"ch") )buf2[i-1] = 'm';
	else if( 0==strcmp(&buf2[i-3],"hl") )buf2[i-1] = 'p';
	else if( 0==strcmp(&buf2[i-3],"cn") )buf2[i-1] = 't';
	else if( 0==strcmp(&buf2[i-3],"da") )buf2[i-1] = 't';

	else if( 0==strcmp(&buf2[i-3],"tx") )buf2[i-1] = 't';
	else if( 0==strcmp(&buf2[i-3],"wr") )buf2[i-1] = 'i';
	else if( 0==strcmp(&buf2[i-3],"wa") )buf2[i-1] = 'v';
	else if( 0==strcmp(&buf2[i-3],"mi") )buf2[i-1] = 'd';
	else if( 0==strcmp(&buf2[i-3],"rm") )buf2[i-1] = 'i';
	else if( 0==strcmp(&buf2[i-3],"bm") )buf2[i-1] = 'p';
	else if( 0==strcmp(&buf2[i-3],"rl") )buf2[i-1] = 'e';
	else if( 0==strcmp(&buf2[i-3],"cu") )buf2[i-1] = 'r';
	else if( 0==strcmp(&buf2[i-3],"do") )buf2[i-1] = 'c';
	else if( 0==strcmp(&buf2[i-3],"ic") )buf2[i-1] = 'o';
	else if( 0==strcmp(&buf2[i-3],"re") )buf2[i-1] = 'g';
	else if( 0==strcmp(&buf2[i-3],"rt") )buf2[i-1] = 'f';
	else if( 0==strcmp(&buf2[i-3],"ht") )buf2[i-1] = 'm';
	else						  buf2[i-1] = '#';

	lstrcpy(buf,buf2);
	return true;
}

//lower: ‘å•¶Žš‚ð‘S‚Ä¬•¶Žš‚É•ÏŠ·‚·‚é
void COpenMSCompress::lower(char *p1)
{
    int i=0;
    while(*(p1+i)!=NULL)
	{ 
        if (*(p1+i) >= 'A' && *(p1+i) <= 'Z')
		{
            *(p1+i)=*(p1+i)-'A';
            *(p1+i)=*(p1+i)+'a';
        }
        i++;
    }
}
