#ifndef AFX_MemmapFile_H__B79B751D_C16F_4C40_A5E7_03CCCE4F19FC__INCLUDED_
#define AFX_MemmapFile_H__B79B751D_C16F_4C40_A5E7_03CCCE4F19FC__INCLUDED_


class MemmapFile
{
public:
	MemmapFile()
		: handle_ ( INVALID_HANDLE_VALUE )
		, fmo_    ( NULL )
		, basePtr_( NULL ) {}
	~MemmapFile()
		{ Close(); }

	bool Open( const char* fname )
	{
		Close();

		// ファイルを読みとり専用で開く
		handle_ = ::CreateFile(
			fname, GENERIC_READ,
			FILE_SHARE_READ,
			NULL, OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL|FILE_FLAG_SEQUENTIAL_SCAN, NULL
		);
		if( handle_ == INVALID_HANDLE_VALUE )
			return false;

		// サイズを取得
		size_ = ::GetFileSize( handle_, NULL );

		if( size_==0 )
		{
			// 0バイトのファイルはマッピング出来ないので適当に回避
			basePtr_ = &size_;
		}
		else
		{
			// マッピングオブジェクトを作る
			fmo_ = ::CreateFileMapping(
				handle_, NULL, PAGE_READONLY, 0, 0, NULL );
			if( fmo_ == NULL )
			{
				::CloseHandle( handle_ );
				handle_ = INVALID_HANDLE_VALUE;
				return false;
			}

			// ビュー
			basePtr_ = ::MapViewOfFileEx( fmo_, FILE_MAP_READ, 0, 0, 0 ,(VOID*)0x00000000);
			if( basePtr_ == NULL )
			{
				::CloseHandle( fmo_ );
				::CloseHandle( handle_ );
				handle_ = INVALID_HANDLE_VALUE;
				return false;
			}
		}
		return true;
	}

	void Close()
	{
		if( handle_ != INVALID_HANDLE_VALUE )
		{
			// ヘンテコマッピングをしてなければここで解放
			if( basePtr_ != &size_ )
				::UnmapViewOfFile( basePtr_ );
			basePtr_ = NULL;

			if( fmo_ != NULL )
				::CloseHandle( fmo_ );
			fmo_ = NULL;

			::CloseHandle( handle_ );
			handle_ = INVALID_HANDLE_VALUE;
		}
	}

public:
	unsigned long size() const        { return size_; }
	const unsigned char* base() const { return static_cast<const unsigned char*>(basePtr_); }

private:
	HANDLE        handle_;
	HANDLE        fmo_;
	unsigned long size_;
	const void*   basePtr_;

private:
	MemmapFile(const MemmapFile&);
	void operator=(const MemmapFile&);
};

#endif