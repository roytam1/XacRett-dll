#include "stdafx.h"
#include "OpenMime.h"

bool COpenMime::FindFirst(INDIVIDUALINFO *lpSubInfo)
{
	return Find_sub(lpSubInfo);
}

bool COpenMime::FindNext(INDIVIDUALINFO *lpSubInfo)
{
	return Find_sub(lpSubInfo);
}

bool COpenMime::Find_sub(INDIVIDUALINFO *lpSubInfo)
{

	char outname[MAX_PATH]={0};
	static int nowpos=0;

	if(!DoHeader( outname))
		return false;

	if( *outname==0 )
		{
		kiutil::getOriginalName( outname,COpenMime::szArcFileName,
				0==stricmp(kiutil::pathExt(COpenMime::szArcFileName),"b64") ? NULL : "b64dec" );
		}
	lstrcpy(lpSubInfo->szFileName,outname);
	lstrcpy(lpSubInfo->szAttribute,"A---W");
	nowpos=ftell(fp);
	skip();
	lpSubInfo->dwCompressedSize=ftell(fp)-nowpos;
	lpSubInfo->dwOriginalSize =lpSubInfo->dwCompressedSize/1.3686;
	//lstrcpy(lpSubInfo->szMode,"");
	return true;
}

void COpenMime::skip()
{
char* str = (char*)COpenMime::common_buf;
//--------------------------------------------------------------------//


	while( kiutil::getline(str,XACR_BUFSIZE,COpenMime::fp ) )
	{
		if( str==strstr(str,"--") )
			break;
	}

}

bool COpenMime::DoHeader( char* name,int count )
{
	*name=0;

	char t,str[5000];
	int dmy,mode=0;//0=DEF,1=InCT,2=InCD,3=InTE
	bool bB64Found=false;

	while( kiutil::getline(str,sizeof(str),COpenMime::fp) && count!=0 )
	{
// sharutilså`éÆÅc
		if( str==strstr(str,"begin-base64") )
		{bB64Found=true;sscanf(str,"begin-base64 %d %s",&dmy,name);break;}
///////////////////
		count--;
		if( bB64Found && *str=='\0' )
			break;

		t=str[13];str[13]=0;
		if( 0==stricmp(str,"Content-Type:") )mode=1;
		str[13]=t;

		t=str[20];str[20]=0;
		if( 0==stricmp(str,"Content-Disposition:") )mode=2;
		str[20]=t;

		t=str[26];str[26]=0;
		if( 0==stricmp(str,"Content-Transfer-Encoding:") )mode=3;
		str[26]=t;

		switch( mode )
		{
		case 1:
		case 2:{
			kiutil::getline(str,sizeof(str),COpenMime::fp);
			char *p,*e;
			for( p=str; *p!=0; p++ )
			{
				if( *p!='n' && *p!='N' )continue;
				if( *(p+1)!='a' && *(p+1)!='A' )continue;
				t=p[5];p[5]=0;
				if( 0!=stricmp(p,"name=") ){p[5]=t;continue;}
				p[5]=t;
				break;
			}
			if( p!=NULL )
			{
				p+=5;
				while(*p==' ')p++;
				if( *(p++)!='\"' )
					break;
				for( e=p; *e!='\0' && *e!='\"'; e=CharNext(e) );
				*e=0;
				strncpy( name,p,MAX_PATH );
			}}break;
		case 3:{
			for( char* p=str; *p!=0; p++ )
			{
				if( *p!='b' && *p!='B' )continue;
				if( *(p+1)!='a' && *(p+1)!='A' )continue;
				t=p[6];p[6]=0;
				if( 0!=stricmp(p,"base64") ){p[6]=t;continue;}
				p[6]=t;bB64Found=true;
				break;
			}}break;
		}

		if( mode!=0 && str[strlen(str)-2]!=';' )
			mode=0;
	}

	return bB64Found;
}