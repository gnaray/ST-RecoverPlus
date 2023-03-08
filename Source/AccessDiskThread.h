//---------------------------------------------------------------------------
// Copyright Christophe Bertrand.
// This file is licensed under the Microsoft Reciprocal License (Ms-RL). You can find a copy of this license in the file "Ms-RL License.htm", in the root directory of the distribution archive.
//---------------------------------------------------------------------------


#ifndef AccessDiskThreadH
#define AccessDiskThreadH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Grids.hpp>

#include "FloppyDisk.h"



//---------------------------------------------------------------------------
class TAccessDiskThread : public TThread
{
	typedef struct tagTHREADNAME_INFO
	{
		DWORD dwType;     // must be 0x1000
		LPCSTR szName;    // pointer to name (in user's address space)
		DWORD dwThreadID; // Thread ID (-1 = caller thread)
		DWORD dwFlags;    // reserved for future use, must be zero
	} THREADNAME_INFO;
private:

	RECT invalid_rect;
	TDrawGrid* grid;

	void SetName();
	void __fastcall UpdateDisplay();
protected:
	void __fastcall Execute();
public:

	TFloppyDisk* floppy_disk;
	bool		ThreadRunning;


	__fastcall TAccessDiskThread(bool CreateSuspended);
};
//---------------------------------------------------------------------------
#endif
