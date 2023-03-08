//---------------------------------------------------------------------------
// Copyright Christophe Bertrand.
// This file is licensed under the Microsoft Reciprocal License (Ms-RL). You can find a copy of this license in the file "Ms-RL License.htm", in the root directory of the distribution archive.
//---------------------------------------------------------------------------



#ifndef AnalyseDiskThreadH
#define AnalyseDiskThreadH
//---------------------------------------------------------------------------
#include <Classes.hpp>


#include "FloppyDisk.h"
#include "Constants.h"

//---------------------------------------------------------------------------
class TAnalyseDiskThread : public TThread
{
	typedef struct tagTHREADNAME_INFO
	{
		DWORD dwType;     // must be 0x1000
		LPCSTR szName;    // pointer to name (in user's address space)
		DWORD dwThreadID; // Thread ID (-1 = caller thread)
		DWORD dwFlags;    // reserved for future use, must be zero
	} THREADNAME_INFO;
private:
	void SetName();
	void TAnalyseDiskThread::update();
protected:
	void __fastcall Execute();
public:

	FD_TIMED_SCAN_RESULT_32	Tracks_analysis_array[NB_MAX_TRACKS][2];// Maxi 85 tracks and 2 sides.
	TFloppyDisk* floppy_disk;
	bool		ThreadRunning;

	__fastcall TAnalyseDiskThread(bool CreateSuspended);
};
//---------------------------------------------------------------------------
#endif
