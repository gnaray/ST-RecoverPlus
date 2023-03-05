//---------------------------------------------------------------------------
// Copyright Christophe Bertrand.
// This file is licensed under the Microsoft Reciprocal License (Ms-RL). You can find a copy of this license in the file "Ms-RL License.htm", in the root directory of the distribution archive.
//---------------------------------------------------------------------------


#include <vcl.h>
#pragma hdrstop

#include "AccessDiskThread.h"
#include "GUIForm1.h"
//#include "FloppyDisk.h"
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

__fastcall TAccessDiskThread::TAccessDiskThread(bool CreateSuspended)
	: TThread(CreateSuspended)
{
}
//---------------------------------------------------------------------------
void TAccessDiskThread::SetName()
{
	THREADNAME_INFO info;
	info.dwType = 0x1000;
	info.szName = "Access disk";
	info.dwThreadID = -1;
	info.dwFlags = 0;

	__try
	{
		RaiseException( 0x406D1388, 0, sizeof(info)/sizeof(DWORD),(DWORD*)&info );
	}
	__except (EXCEPTION_CONTINUE_EXECUTION)
	{
	}
}
//---------------------------------------------------------------------------
void __fastcall TAccessDiskThread::Execute()
{
	SetName();
	//---- Place the thread code here ----

	if (floppy_disk==NULL)
	{
		this->ReturnValue=false;
		return;
	}

	// Create the file to save disk image:

	const HANDLE himagefile=CreateFile(
		GUIForm1->SaveDialogDiskImage->FileName.c_str(),//LPCTSTR lpFileName,
		GENERIC_WRITE ,//DWORD dwDesiredAccess,
		FILE_SHARE_READ ,//DWORD dwShareMode,
		NULL, //LPSECURITY_ATTRIBUTES lpSecurityAttributes,
		CREATE_ALWAYS,//DWORD dwCreationDisposition,
		FILE_ATTRIBUTE_NORMAL,//DWORD dwFlagsAndAttributes,
		NULL);//HANDLE hTemplateFile	);
	if (himagefile == INVALID_HANDLE_VALUE)
	{
		return; // writing error.
	}


	ThreadRunning=true;

	const DWORD maxi_hour=
		Time_ComboBoxMaxiTime_in_ms[GUIForm1->ComboBoxMaxiTime->ItemIndex]
		+ GetTickCount();


	std::vector<BYTE> phrase;
	{
		phrase.reserve(floppy_disk->NbBytesPerSector);
		const BYTE repetition[]="======== SORRY, THIS SECTOR CANNOT BE READ FROM FLOPPY DISK BY ST RECOVER. ========\r\n";
		for (unsigned iph=0;iph<floppy_disk->NbBytesPerSector;iph+=sizeof(repetition)-1)
		{
			int tcop=sizeof(repetition)-1;
			if ((iph+tcop) > floppy_disk->NbBytesPerSector)
				tcop=floppy_disk->NbBytesPerSector-iph;
			memcpy(&phrase[iph],repetition,tcop);
		}
	}

	SSector*	p_sector_infos=NULL;




		// Reading sectors

	for (int p=0; p<floppy_disk->NbTracks; p++ )
	{
		if (Terminated)
		{
			break;
		}
		for (int f=0; f<floppy_disk->NbSides; f++ )
		{
			if (Terminated)
			{
				break;
			}
			static BYTE Track_sectors_content[6400];
			BYTE*		pContent=Track_sectors_content;
			grid=
				f==0 ? GUIForm1->DrawGridSideASectors : GUIForm1->DrawGridSideBSectors;
			SSectors* sects=
				f==0 ? &floppy_disk->SectorsSideA : &floppy_disk->SectorsSideB;
			for (int s=0; s<floppy_disk->NbSectorsPerTrack; s++)
			{
				if (Terminated)
				{
					break;
				}
				const bool OK=floppy_disk->CD_ReadSector(p,f,s,
					maxi_hour-GetTickCount(),GUIForm1->MemoLOG->Lines,
					&p_sector_infos, &GUIForm1->PleaseCancelCurrentOperation,
					GUIForm1->CheckBoxSaveRawTrackInfos->Checked );
				if ( ! GUIForm1->PleaseCancelCurrentOperation)
				{
					sects->is_read[p][s]=true;
					sects->difficult_to_read[p][s]= // Difficulty if we could not read in normal mode (with simple controller).
						p_sector_infos->Normal_reading_by_controller_tried
						&& ! p_sector_infos->Normal_reading_by_controller_success;
					sects->error[p][s] = OK;
					if (((pContent-Track_sectors_content)+p_sector_infos->Byte_size)<=(int)sizeof(Track_sectors_content))
					{
						if (OK) // We copy the data in the sector.
							memcpy(pContent,p_sector_infos->pContent,p_sector_infos->Byte_size);
						else // If the sector could not read, we put a sentence as content, otherwise we would have a discrepancy in the disk image.
						{
								memcpy(pContent,&phrase[0],p_sector_infos->Byte_size);
						}
						pContent += p_sector_infos->Byte_size;// InfosSect->Size;
					}

					invalid_rect = grid->CellRect(p, s);

					InvalidateRect(grid->Handle, &invalid_rect, TRUE);
				} // endif ( ! Terminated)
			}
			{
				DWORD NumberOfBytesWritten=0;
				/*BOOL writing_ok=*/ WriteFile(
					himagefile,//HANDLE hFile,
					Track_sectors_content,//LPCVOID lpBuffer,
					pContent-Track_sectors_content,//DWORD nNumberOfBytesToWrite,
					&NumberOfBytesWritten,//LPDWORD lpNumberOfBytesWritten,
					NULL);//LPOVERLAPPED lpOverlapped
			}
		}
	} // next p

	Application->ProcessMessages();  // Let the display update.

	phrase.clear();

	CloseHandle(himagefile);

	ThreadRunning=false;
	this->ReturnValue=true;
}
//---------------------------------------------------------------------------
void __fastcall TAccessDiskThread::UpdateDisplay()
{
	InvalidateRect(grid->Handle, &invalid_rect, TRUE);
}
