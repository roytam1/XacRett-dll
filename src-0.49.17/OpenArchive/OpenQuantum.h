#ifndef AFX_OPENQUANTUM_H__QUANTUM_OPEN_INCLUDED__
#define AFX_OPENQUANTUM_H__QUANTUM_OPEN_INCLUDED__


#include "OpenArchive.h"
#include "../QuantumDecode.h"

class COpenQuantum : public COpenArchive
{
public: //-- COpenArchive

	COpenQuantum() : COpenArchive( )
		{COpenQuantum::fp=NULL;}
	bool FindFirst(INDIVIDUALINFO *lpSubInfo);
	bool FindNext(INDIVIDUALINFO *lpSubInfo);
private:
	QuantumArchive	arc;
	int				FileNum;
	int				Index;
	bool			Opened;
	bool			set_Find(INDIVIDUALINFO *lpSubInfo);
};

#endif
