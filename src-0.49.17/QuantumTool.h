#ifndef AFX_QUANTUMTOOL_H__QUANTUM_TOOL_INCLUDED__
#define AFX_QUANTUMTOOL_H__QUANTUM_TOOL_INCLUDED__

#include "Tool.h"

// The Quantum archives of Cinematronics / Dynamix (Quantum 0.90 and 0.96),
// which are produced by PAQ.EXE and read by UNPAQ.EXE.  See
// QuantumDecode.h for the format.

class CQuantumTool : public CTool
{
public:
	CQuantumTool() : CTool( "Quantum Extract" ){}

	bool IsType( const char* ext )
		{return 0==stricmp(ext,"q");}
	bool Check( const char* fname, unsigned long fsize );
	bool Extract( const char* fname, const char* ddir );
};

#endif
