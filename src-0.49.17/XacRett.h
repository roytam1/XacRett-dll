#ifndef AFX_XACRETT_H__03ED6241_3976_11D4_8D96_A6394BD8C334__INCLUDED_
#define AFX_XACRETT_H__03ED6241_3976_11D4_8D96_A6394BD8C334__INCLUDED_


//DLLのバージョン
#define DLLVERSION 49
#define SUBDLLVERSION 17

#ifndef FNAME_MAX32
#define FNAME_MAX32 512
#endif
typedef struct {
	DWORD 			dwFileSize;
	DWORD			dwWriteSize;
	char			szSourceFileName[FNAME_MAX32 + 1];
	char			dummy1[3];
	char			szDestFileName[FNAME_MAX32 + 1];
	char			dummy[3];
}	EXTRACTINGINFO, *LPEXTRACTINGINFO;

typedef struct {
	EXTRACTINGINFO exinfo;
	DWORD dwCompressedSize;
	DWORD dwCRC;
	UINT  uOSType;
	WORD  wRatio;
	WORD  wDate;
	WORD  wTime;
	char  szAttribute[8];
	char  szMode[8];
} EXTRACTINGINFOEX, *LPEXTRACTINGINFOEX;
/* ウィンドウメッセージ */
#ifndef WM_ARCEXTRACT
#define	WM_ARCEXTRACT			"wm_arcextract"
#define	ARCEXTRACT_BEGIN		0
#define	ARCEXTRACT_INPROCESS	1
#define	ARCEXTRACT_END			2
#define ARCEXTRACT_OPEN			3
#define ARCEXTRACT_COPY			4
#define ARCEXTRACT_WRITEFILE_OPEN			20
#endif

// APIを示す一様な数値
#if !defined(ISARC_FUNCTION_START)
#define ISARC_FUNCTION_START				0
#define ISARC								0	/* XacRett */
#define ISARC_GET_VERSION					1	/* XacRettGetVersion */
#define ISARC_GET_CURSOR_INTERVAL			2	/* XacRettGetCursorInterval */
#define ISARC_SET_CURSOR_INTERVAL			3	/* XacRettSetCursorInterval */
#define ISARC_GET_BACK_GROUND_MODE			4	/* XacRettGetBackGroundMode */
#define ISARC_SET_BACK_GROUND_MODE			5	/* XacRettSetBackGroundMode */
#define ISARC_GET_CURSOR_MODE				6	/* XacRettGetCursorMode */
#define ISARC_SET_CURSOR_MODE				7	/* XacRettSetCursorMode */
#define ISARC_GET_RUNNING					8	/* XacRettGetRunning */

#define ISARC_CHECK_ARCHIVE					16	/* XacRettCheckArchive */
#define ISARC_CONFIG_DIALOG					17	/* XacRettConfigDialog */
#define ISARC_GET_FILE_COUNT				18	/* XacRettGetFileCount */
#define ISARC_QUERY_FUNCTION_LIST			19	/* XacRettQueryFunctionList */
#define ISARC_HOUT							20	/* XacRettHOut */
#define ISARC_STRUCTOUT						21	/* XacRettStructOut */
#define ISARC_GET_ARC_FILE_INFO				22	/* XacRettGetArcFileInfo */

#define ISARC_OPEN_ARCHIVE					23	/* XacRettOpenArchive */
#define ISARC_CLOSE_ARCHIVE					24	/* XacRettCloseArchive */
#define ISARC_FIND_FIRST					25	/* XacRettFindFirst */
#define ISARC_FIND_NEXT						26	/* XacRettFindNext */
#define ISARC_EXTRACT						27	/* XacRettExtract */
#define ISARC_ADD							28	/* XacRettAdd */
#define ISARC_MOVE							29	/* XacRettMove */
#define ISARC_DELETE						30	/* XacRettDelete */
#define ISARC_SETOWNERWINDOW				31	/* XacRettSetOwnerWindow */
#define ISARC_CLEAROWNERWINDOW				32	/* XacRettClearOwnerWindow */
#define ISARC_SETOWNERWINDOWEX				33	/* XacRettSetOwnerWindowEx */
#define ISARC_KILLOWNERWINDOWEX				34	/* XacRettKillOwnerWindowEx */

#define ISARC_GET_ARC_FILE_NAME				40	/* XacRettGetArcFileName */
#define ISARC_GET_ARC_FILE_SIZE				41	/* XacRettGetArcFileSize */
#define ISARC_GET_ARC_ORIGINAL_SIZE			42	/* XacRettArcOriginalSize */
#define ISARC_GET_ARC_COMPRESSED_SIZE		43	/* XacRettGetArcCompressedSize */
#define ISARC_GET_ARC_RATIO					44	/* XacRettGetArcRatio */
#define ISARC_GET_ARC_DATE					45	/* XacRettGetArcDate */
#define ISARC_GET_ARC_TIME					46	/* XacRettGetArcTime */
#define ISARC_GET_ARC_OS_TYPE				47	/* XacRettGetArcOSType */
#define ISARC_GET_ARC_IS_SFX_FILE			48	/* XacRettGetArcIsSFXFile */
#define ISARC_GET_ARC_WRITE_TIME_EX			49	/* XacRettGetArcWriteTimeEx */
#define ISARC_GET_ARC_CREATE_TIME_EX		50	/* XacRettGetArcCreateTimeEx */
#define	ISARC_GET_ARC_ACCESS_TIME_EX		51	/* XacRettGetArcAccessTimeEx */
#define	ISARC_GET_ARC_CREATE_TIME_EX2		52	/* XacRettGetArcCreateTimeEx2 */
#define ISARC_GET_ARC_WRITE_TIME_EX2		53	/* XacRettGetArcWriteTimeEx2 */
#define ISARC_GET_FILE_NAME					57	/* XacRettGetFileName */
#define ISARC_GET_ORIGINAL_SIZE				58	/* XacRettGetOriginalSize */
#define ISARC_GET_COMPRESSED_SIZE			59	/* XacRettGetCompressedSize */
#define ISARC_GET_RATIO						60	/* XacRettGetRatio */
#define ISARC_GET_DATE						61	/* XacRettGetDate */
#define ISARC_GET_TIME						62	/* XacRettGetTime */
#define ISARC_GET_CRC						63	/* XacRettGetCRC */
#define ISARC_GET_ATTRIBUTE					64	/* XacRettGetAttribute */
#define ISARC_GET_OS_TYPE					65	/* XacRettGetOSType */
#define ISARC_GET_METHOD					66	/* XacRettGetMethod */
#define ISARC_GET_WRITE_TIME				67	/* XacRettGetWriteTime */
#define ISARC_GET_CREATE_TIME				68	/* XacRettGetCreateTime */
#define ISARC_GET_ACCESS_TIME				69	/* XacRettGetAccessTime */
#define ISARC_GET_WRITE_TIME_EX				70	/* XacRettGetWriteTimeEx */
#define ISARC_GET_CREATE_TIME_EX			71	/* XacRettGetCreateTimeEx */
#define ISARC_GET_ACCESS_TIME_EX			72	/* XacRettGetAccessTimeEx */
#define ISARC_SET_ENUM_MEMBERS_PROC			80  /* XacRettSetEnumMembersProc */
#define ISARC_CLEAR_ENUM_MEMBERS_PROC		81	/* XacRettClearEnumMembersProc */
#define ISARC_GET_ARC_FILE_SIZE_EX			82	/* XacRettGetArcFileSizeEx */
#define ISARC_GET_ARC_ORIGINAL_SIZE_EX		83	/* XacRettArcOriginalSizeEx */
#define ISARC_GET_ARC_COMPRESSED_SIZE_EX	84	/* XacRettGetArcCompressedSizeEx */
#define ISARC_GET_ORIGINAL_SIZE_EX			85	/* XacRettGetOriginalSizeEx */
#define ISARC_GET_COMPRESSED_SIZE_EX		86	/* XacRettGetCompressedSizeEx */
#define ISARC_SETOWNERWINDOWEX64			87	/* XacRettSetOwnerWindowEx64 */
#define ISARC_KILLOWNERWINDOWEX64			88	/* XacRettKillOwnerWindowEx64 */
#define ISARC_SET_ENUM_MEMBERS_PROC64		89  /* XacRettSetEnumMembersProc64 */
#define ISARC_CLEAR_ENUM_MEMBERS_PROC64		90	/* XacRettClearEnumMembersProc64 */
#define ISARC_OPEN_ARCHIVE2					91	/* XacRettOpenArchive2 */
#define ISARC_GET_ARC_READ_SIZE				92	/* XacRettGetArcReadSize */
#define ISARC_GET_ARC_READ_SIZE_EX			93	/* XacRettGetArcReadSizeEx*/
#define ISARC_FUNCTION_END					ISARC_GET_ARC_READ_SIZE_EX

#endif	/* ISARC_FUNCTION_START */

// エラーメッセージ
#if !defined(ERROR_START)
#define ERROR_START				0x8000
/* WARNING */
#define ERROR_DISK_SPACE		0x8005
#define ERROR_READ_ONLY			0x8006
#define ERROR_USER_SKIP			0x8007
#define ERROR_UNKNOWN_TYPE		0x8008
#define ERROR_METHOD			0x8009
#define ERROR_PASSWORD_FILE		0x800A
#define ERROR_VERSION			0x800B
#define ERROR_FILE_CRC			0x800C
#define ERROR_FILE_OPEN			0x800D
#define ERROR_MORE_FRESH		0x800E
#define ERROR_NOT_EXIST			0x800F
#define ERROR_ALREADY_EXIST		0x8010

#define ERROR_TOO_MANY_FILES	0x8011

/* ERROR */
#define ERROR_MAKEDIRECTORY		0x8012
#define ERROR_CANNOT_WRITE		0x8013
#define ERROR_HUFFMAN_CODE		0x8014
#define ERROR_COMMENT_HEADER	0x8015
#define ERROR_HEADER_CRC		0x8016
#define ERROR_HEADER_BROKEN		0x8017
#define ERROR_ARC_FILE_OPEN		0x8018
#define ERROR_NOT_ARC_FILE		0x8019
#define ERROR_CANNOT_READ		0x801A
#define ERROR_FILE_STYLE		0x801B
#define ERROR_COMMAND_NAME		0x801C
#define ERROR_MORE_HEAP_MEMORY	0x801D
#define ERROR_ENOUGH_MEMORY		0x801E
#if !defined(ERROR_ALREADY_RUNNING)
#define ERROR_ALREADY_RUNNING	0x801F
#endif
#define ERROR_USER_CANCEL		0x8020
#define ERROR_HARC_ISNOT_OPENED	0x8021
#define ERROR_NOT_SEARCH_MODE	0x8022
#define ERROR_NOT_SUPPORT		0x8023
#define ERROR_TIME_STAMP		0x8024
#define ERROR_TMP_OPEN			0x8025
#define ERROR_LONG_FILE_NAME	0x8026
#define ERROR_ARC_READ_ONLY		0x8027
#define ERROR_SAME_NAME_FILE	0x8028
#define ERROR_NOT_FIND_ARC_FILE 0x8029
#define ERROR_RESPONSE_READ		0x802A
#define ERROR_NOT_FILENAME		0x802B
#define ERROR_TMP_COPY			0x802C
#define ERROR_EOF				0x802D
#define ERROR_ADD_TO_LARC		0x802E
#define ERROR_TMP_BACK_SPACE	0x802F
#define ERROR_SHARING			0x8030
#define ERROR_NOT_FIND_FILE		0x8031
#define ERROR_LOG_FILE			0x8032
#define	ERROR_NO_DEVICE			0x8033
#define ERROR_GET_ATTRIBUTES	0x8034
#define ERROR_SET_ATTRIBUTES	0x8035
#define ERROR_GET_INFORMATION	0x8036
#define ERROR_GET_POINT			0x8037
#define ERROR_SET_POINT			0x8038
#define ERROR_CONVERT_TIME		0x8039
#define ERROR_GET_TIME			0x803a
#define ERROR_SET_TIME			0x803b
#define ERROR_CLOSE_FILE		0x803c
#define ERROR_HEAP_MEMORY		0x803d
#define ERROR_HANDLE			0x803e
#define ERROR_TIME_STAMP_RANGE	0x803f
#define ERROR_MAKE_ARCHIVE		0x8040
#define ERROR_NOT_CONFIRM_NAME	0x8041
#define ERROR_UNEXPECTED_EOF	0x8042
#define ERROR_INVALID_END_MARK	0x8043
#define ERROR_INVOLVED_LZH		0x8044
#define ERROR_NO_END_MARK		0x8045
#define ERROR_HDR_INVALID_SIZE	0x8046
#define ERROR_UNKNOWN_LEVEL		0x8047
#define ERROR_BROKEN_DATA		0x8048

#define ERROR_END	ERROR_BROKEN_DATA
#endif /* ERROR_START */

// 書庫形式
#define ARCHIVETYPE_UNKNOWN 0
#define ARCHIVETYPE_ACE		1
#define ARCHIVETYPE_ARC		2
#define ARCHIVETYPE_ARG		3
#define ARCHIVETYPE_ARJ		4
#define ARCHIVETYPE_AR		5
#define ARCHIVETYPE_ASD		6
#define ARCHIVETYPE_ATOB	7
#define ARCHIVETYPE_BEL		8
#define ARCHIVETYPE_BGA		9
#define ARCHIVETYPE_BOO		10
#define ARCHIVETYPE_BZ2		11
#define ARCHIVETYPE_CAB		12
#define ARCHIVETYPE_COMPACT 13
#define ARCHIVETYPE_COMPRESSZ 14
#define ARCHIVETYPE_CPIO	15
#define ARCHIVETYPE_CPT		16
#define ARCHIVETYPE_DZ		17
#define ARCHIVETYPE_FISH	18
#define ARCHIVETYPE_FRZ		19
#define ARCHIVETYPE_GCA		20
#define ARCHIVETYPE_GZ		21
#define ARCHIVETYPE_HQX		22
#define ARCHIVETYPE_IMP		23
#define ARCHIVETYPE_ISH		24
#define ARCHIVETYPE_IS		25
#define ARCHIVETYPE_JAK		26
#define ARCHIVETYPE_JAM		27
#define ARCHIVETYPE_LZH		28
#define ARCHIVETYPE_MACBIN	29
#define ARCHIVETYPE_MIME	30
#define ARCHIVETYPE_MSCOMPRESS 31
#define ARCHIVETYPE_PACKZ	32
#define ARCHIVETYPE_PIT		33
#define ARCHIVETYPE_QPAC	34
#define ARCHIVETYPE_RAR		35
#define ARCHIVETYPE_REZ		36
#define ARCHIVETYPE_SHAR	37
#define ARCHIVETYPE_SPL		38
#define ARCHIVETYPE_TAR		39
#define ARCHIVETYPE_UUDECODE 40//ver0.48.10から変わっています
#define ARCHIVETYPE_YNC		41
#define ARCHIVETYPE_ZAC		42
#define ARCHIVETYPE_ZIP		43
#define ARCHIVETYPE_ZOO		44
#define ARCHIVETYPE_YZ1		45
#define ARCHIVETYPE_TBZ		46
#define ARCHIVETYPE_TGZ		47
#define ARCHIVETYPE_TAZ		48
#define ARCHIVETYPE_TAz		49
#define ARCHIVETYPE_CPIOGZRPM 50
#define ARCHIVETYPE_CPIOZ	51
#define ARCHIVETYPE_CPIOBZ2 52
#define ARCHIVETYPE_XXDECODE 53
#define ARCHIVETYPE_BLZ		54
#define ARCHIVETYPE_ALZ		55
#define ARCHIVETYPE_DMG		56
#define ARCHIVETYPE_ISO		57
#define ARCHIVETYPE_LZX		58
#define ARCHIVETYPE_JC1		59
#define ARCHIVETYPE_APK		60
#define ARCHIVETYPE_SQX		61
#define ARCHIVETYPE_ASK		62
#define ARCHIVETYPE_LBR	63
#define ARCHIVETYPE_AIN	64
#define ARCHIVETYPE_LZMA	65
#define ARCHIVETYPE_XZ	66
#define ARCHIVETYPE_TLZMA	67
#define ARCHIVETYPE_TXZ	68
#define ARCHIVETYPE_ZSTD	69
#define ARCHIVETYPE_TZST	70
#define ARCHIVETYPE_LZIP	72
#define ARCHIVETYPE_TLZ	73
#define ARCHIVETYPE_DGC	74
#define ARCHIVETYPE_DGD	75
#define ARCHIVETYPE_EGG		76
#define ARCHIVETYPE_7Z	71
#define ARCHIVETYPE_CPIOLZMA	77
#define ARCHIVETYPE_CPIOXZ	78
#define ARCHIVETYPE_CPIOZST	79

#define NUM_ARCHIVETYPE		79

#endif
