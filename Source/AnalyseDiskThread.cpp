//---------------------------------------------------------------------------
// Copyright Christophe Bertrand.
// This file is licensed under the Microsoft Reciprocal License (Ms-RL). You can find a copy of this license in the file "Ms-RL License.htm", in the root directory of the distribution archive.
//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "AnalyseDiskThread.h"
#include "GUIForm1.h"

#pragma package(smart_init)
//---------------------------------------------------------------------------

//   Important: the methods and properties of VCL objects can only
//   be used in a method called using Synchronize, as follows:
//
//      Synchronize(&UpdateCaption);
//
//   where UpdateCaption would be in the form:
//
//      void __fastcall TAccessDiskThread::UpdateCaption()
//      {
//        GUIForm1->Caption = "Update in a thread";
//      }
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
__fastcall TAnalyseDiskThread::TAnalyseDiskThread(bool CreateSuspended)
	: TThread(CreateSuspended)
{
}
//---------------------------------------------------------------------------
void TAnalyseDiskThread::SetName()
{
	THREADNAME_INFO info;
	info.dwType = 0x1000;
	info.szName = "Disk analysis by thread";
	info.dwThreadID = -1;
	info.dwFlags = 0;

	__try
	{
		RaiseException(0x406D1388, 0, sizeof(info) / sizeof(DWORD), (DWORD*)&info);
	}
	__except (EXCEPTION_CONTINUE_EXECUTION)
	{
	}
}
//---------------------------------------------------------------------------
void TAnalyseDiskThread::update()
{
	Application->ProcessMessages();  // Let the display update.
}
//---------------------------------------------------------------------------
void __fastcall TAnalyseDiskThread::Execute()
{
	SetName();
	//---- Place the thread code here ----
	if (floppy_disk == NULL) {
		this->ReturnValue = false;
		return;
	}
	ThreadRunning = true;
	// --------------

	// Track analysis
	for (int p = 0; p < floppy_disk->NbTracks; p++) {
		if (Terminated) {
			break;
		}
		for (int f = 0; f < floppy_disk->NbSides; f++) {
			if (Terminated) {
				break;
			}
			STrackInfo* ip = floppy_disk->CD_AnalyseSectorsTime(p, f);//,false,false);
			if (ip->OperationSuccess) {
				/* We copy the track data in the table of the whole floppy disk.
				 Thus, the other thread can access it in parallel, even having
				 several delay messages (you never know). */
				Tracks_analysis_array[p][f] = *ip->fdrawcmd_Timed_Scan_Result;
				// We send a personalized message: WM_recover_maj_FormAnalyse_track.
				//const Data_recover_maj_FormAnalyse_track infos={p,f};
				PostMessage(
					GUIForm1->Handle, WM_recover_maj_FormAnalyse_track, p, f);
			}
		}
	}

	Application->ProcessMessages();  // Let the display update.

	// end ----------
	this->ReturnValue = true;
	ThreadRunning = false;
}
//---------------------------------------------------------------------------
