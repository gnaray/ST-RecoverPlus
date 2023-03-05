//---------------------------------------------------------------------------
// Copyright Christophe Bertrand.
// This file is licensed under the Microsoft Reciprocal License (Ms-RL). You can find a copy of this license in the file "Ms-RL License.htm", in the root directory of the distribution archive.
//---------------------------------------------------------------------------


#include <vcl.h>
#pragma hdrstop


#include "GUIForm1.h"
#include "AccessDiskThread.h"
#include "FloppyDisk.h"
#include "AnalyseDiskThread.h"
#include "Constants.h"

#include <math.h>

//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TGUIForm1 *GUIForm1;

// ======================



TFloppyDisk* floppy_disk=NULL;


TFloppyDisk* floppy_disk_analyse=NULL;
TAnalyseDiskThread* Analyse_Disk_thread=NULL;

const COLORREF even_track_color=0xff8000;
const COLORREF odd_track_color=0x00c040;


// ======================


//---------------------------------------------------------------------------
__fastcall TGUIForm1::TGUIForm1(TComponent* Owner)
	: TForm(Owner)
{
	PleaseCancelCurrentOperation=false;
	ImageSide0->Picture->Bitmap->Canvas->Brush->Color=GUIForm1->Color;
	ImageSide0->Picture->Bitmap->Width = ImageSide0->ClientWidth;
	ImageSide0->Picture->Bitmap->Height = ImageSide0->ClientHeight;

	ImageSide1->Picture->Bitmap->Canvas->Brush->Color=GUIForm1->Color;
	ImageSide1->Picture->Bitmap->Width = ImageSide1->ClientWidth;
	ImageSide1->Picture->Bitmap->Height = ImageSide1->ClientHeight;

	PageControlResults->ActivePage=TabSheetSidesGrids;

	Application->HintPause=0;
	Application->HintShortPause=0;
}
//---------------------------------------------------------------------------
void __fastcall TGUIForm1::ButtonReadDiskClick(TObject *Sender)
{
	// Procedure to read the sectors of the diskette.


	this->MemoLOG->Lines->Clear();

	// Now we read the disc.
	floppy_disk=new TFloppyDisk();
	if (floppy_disk==NULL)
		return;

	if ( floppy_disk->OpenFloppyDisk(ComboBoxDisk->ItemIndex,
		Time_ComboBoxMaxiTime_in_ms[ComboBoxMaxiTime->ItemIndex],
		this->MemoLOG->Lines, &PleaseCancelCurrentOperation,
		GUIForm1->CheckBoxSaveRawTrackInfos->Checked ))
	{
		// You are asked to choose the .ST file to save.
		if (SaveDialogDiskImage->Execute())
		{
			const DWORD start_time=GetTickCount();
			const DWORD authorized_duration=Time_ComboBoxMaxiTime_in_ms[ComboBoxMaxiTime->ItemIndex];

			BitBtnCancel->Enabled=true;
			TAccessDiskThread* th=new TAccessDiskThread(true);
			if (th != NULL) {
				th->floppy_disk=floppy_disk;

				// Erase the graphics (method to be reviewed later).
				this->DrawGridSideASectors->Invalidate();
				this->DrawGridSideBSectors->Invalidate();
				PageControlResults->ActivePage=TabSheetSidesGrids;

				unsigned track,side,sector_0based;
				track=floppy_disk->direct_infos.Selected_Track;
				side=floppy_disk->direct_infos.Selected_Side;
				sector_0based=floppy_disk->direct_infos.Sector_under_treatment_0based;

				// Starts the Thread which performs the reading and which orders the drawing.
				#ifndef _DEBUG
					th->Priority=tpHigher;
				#endif
				th->Resume();

				Sleep(200);
				while(th->ThreadRunning)
				{
					const DWORD elapsed_time= GetTickCount() - start_time;
					const bool is_time_over= elapsed_time > authorized_duration;
					if (PleaseCancelCurrentOperation || is_time_over) {
						th->Terminate();
						if (PleaseCancelCurrentOperation)
							MemoLOG->Lines->Add("Operation canceled by user (you !).");
						if (is_time_over)
							MemoLOG->Lines->Add("Time is up, process gaveup (see Giveup time option).");
						break;
					}
					Application->ProcessMessages();
					if (track!=floppy_disk->direct_infos.Selected_Track
					|| side!=floppy_disk->direct_infos.Selected_Side
					|| sector_0based!=floppy_disk->direct_infos.Sector_under_treatment_0based) {
						track=floppy_disk->direct_infos.Selected_Track;
						side=floppy_disk->direct_infos.Selected_Side;
						sector_0based=floppy_disk->direct_infos.Sector_under_treatment_0based;
//						static char texteinfos[256];
//						StringCbPrintf(texteinfos,sizeof(texteinfos)-1,
//							"Track:%d Side/Head:%d Sector:%d",
//							track,side,sector_0based+1);
////						LabelInformation->Caption=textetemps;
						LabelInformation->Caption=AnsiString().sprintf(
							"Track:%d Side/Head:%d Sector:%d",
							track,side,sector_0based+1
						);
					}
					static DWORD old_second=elapsed_time/1000;
					if ((elapsed_time/1000) != old_second)
					{
//						static char textetemps[256];
						DWORD d=elapsed_time/1000;
						const DWORD hour=d/3600;
						d %= 3600;
						const DWORD minute=d/60;
						d %= 60;
						const DWORD second=d;
//						StringCbPrintf(textetemps,sizeof(textetemps)-1,
//							"Time: %u:%02u:%02u",
//							hour,minute,second);
//						LabelElapsedTime->Caption=textetemps;
						LabelElapsedTime->Caption=AnsiString().sprintf(
							"Time: %u:%02u:%02u",
							hour,minute,second
						);
						old_second = elapsed_time/1000;
					}
					Sleep(200);
				}

				/* The following pass is necessary for the case where the window
				was hidden at the end of the reading. Otherwise, the graphs will appear incomplete
				when we enlarge the window. */
				for (int p=0;p<floppy_disk->NbTracks;p++)
					for (int s=0;s<floppy_disk->NbSectorsPerTrack;s++)
					{
						TGridDrawState ds;
						TRect invalid_rect = DrawGridSideASectors->CellRect(p, s);
						DrawGridSideASectorsDrawCell(DrawGridSideASectors,p,s, invalid_rect,ds);
                        invalid_rect = DrawGridSideBSectors->CellRect(p, s);
						DrawGridSideASectorsDrawCell(DrawGridSideBSectors,p,s, invalid_rect,ds);
					}

				Application->ProcessMessages();
				delete th;
				PleaseCancelCurrentOperation=false;
			}
			// end
			BitBtnCancel->Enabled=false;
			floppy_disk->CloseFloppyDisk();
		}
	}
	else
		Application->MessageBox("No disk in drive",Caption.c_str(),MB_OK | MB_ICONERROR);

	MemoLOG->Lines->Add("Operation terminated.");

	delete floppy_disk;
	floppy_disk=NULL;
}
//---------------------------------------------------------------------------
void __fastcall TGUIForm1::DrawGridSideASectorsDrawCell(TObject *Sender, int ACol,
			int ARow, TRect &Rect, TGridDrawState State)
{

	const TDrawGrid* grid=(TDrawGrid*) Sender;

	static int colors_array[2][NB_MAX_TRACKS][NB_MAX_SECTORS_PER_TRACK]; // To memorize colors.
	enum icolors {
		I_ColorSectorNotRead=0,
		I_ColorSectorReadOK,
		I_ColorSectorDifficult,
		I_ColorSectorError };
	const COLORREF colors[4]={
		ColorSectorNotRead,
		ColorSectorReadOK,
		ColorSectorDifficult,
		ColorSectorError };

	const unsigned igrid=
		Sender==DrawGridSideASectors ? 0 : 1;

	COLORREF color=0;

	if (floppy_disk != NULL)
	{    // We use the fresh info from the floppy disk.
		const SSectors* sects=
			Sender==DrawGridSideASectors ? &floppy_disk->SectorsSideA : &floppy_disk->SectorsSideB;
		unsigned icolor=I_ColorSectorNotRead;

		if (sects->is_read[ACol][ARow] )
		{
			if (sects->error[ACol][ARow] )
			{
				if (sects->difficult_to_read[ACol][ARow] )
				{
					icolor=I_ColorSectorDifficult;
				}
				else
					icolor=I_ColorSectorReadOK;
			}
			else
			{
				icolor=I_ColorSectorError;
			}
		}
		color=colors[icolor];
		colors_array[igrid][ACol][ARow]=icolor;
	}
	else  // Otherwise, we use the memory of the last analysis.
	{
		color=colors[colors_array[igrid][ACol][ARow]];
	}

	grid->Canvas->Brush->Color=(TColor) color;
	grid->Canvas->FillRect(Rect);
}
//---------------------------------------------------------------------------
void __fastcall TGUIForm1::ButtonAnalyseClick(TObject *Sender)
{
	this->MemoLOG->Lines->Clear();

	PageControlResults->ActivePage=TabSheetSides;
	AnalyseDisk();
}
//---------------------------------------------------------------------------
void __fastcall TGUIForm1::BitBtnCancelClick(TObject *Sender)
{
	const int r=Application->MessageBox(
		"Do you really want to cancel the current operation ?",
		Caption.c_str(),
		MB_YESNO | MB_ICONQUESTION);
	PleaseCancelCurrentOperation=(r == IDYES);
}
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
void __fastcall TGUIForm1::On_recover_maj_FormAnalyse_track(TMessage &Message)
{
	// To receive a personalized message: WM_recover_maj_FormAnalyse_track.
	const unsigned track=Message.WParam;
	const unsigned side=Message.LParam;

	// Circular arcs are calculated, using a time of 200 ms per track.

	const DWORD sector_duration_in_microseconds=(200000 * 512 / 6250);
	const double PI2 = 3.1415926535897932385 * 2;

	if (Analyse_Disk_thread!=NULL) {
		const FD_TIMED_SCAN_RESULT_32* ap=&Analyse_Disk_thread->Tracks_analysis_array[track][side];
		TImage* image= (side==0) ? GUIForm1->ImageSide0 : GUIForm1->ImageSide1;
		image->Picture->Bitmap->Canvas->Pen->Width=2;

		const int outer_track_radius = 400/2-track*2 ;
		//const int inner_track_radius = outer_track_radius-1 ;

		image->Picture->Bitmap->Canvas->Pen->Color= (TColor)  //random(0xffffff);
			(((track & 1)==0) ? even_track_color : odd_track_color);

		for (int s=0;s < ap->count ;s++ ) {
			const double start_sector_angle=
				(double)ap->Headers[s].reltime * PI2 / (double)ap->tracktime;
			const double end_sector_angle=
				(double)(ap->Headers[s].reltime+sector_duration_in_microseconds)
				* PI2 / (double)ap->tracktime;
			const double xstart=cos(start_sector_angle)*outer_track_radius;
			const double ystart=sin(start_sector_angle)*outer_track_radius;
			const double xend=cos(end_sector_angle)*outer_track_radius;
			const double yend=sin(end_sector_angle)*outer_track_radius;

			// Small macros to place yourself in the center of the image, and not upside down.
			#define x(posXrelative) (200+(posXrelative))
			#define y(posYrelative) (200-(posYrelative))

			image->Picture->Bitmap->Canvas->Arc(
				x(-outer_track_radius), // X1,
				y(-outer_track_radius),// int Y1,
				x(outer_track_radius),// int X2,
				y(outer_track_radius),// int Y2,
				x(xstart),// int X3,
				y(ystart),// int Y3,
				x(xend),//	int X4,
				y(yend));// int Y4);
			#undef x
			#undef y
		}
		image->Invalidate();
	}


}
//---------------------------------------------------------------------------
bool	__fastcall TGUIForm1::AnalyseDisk(void)
{
	// Called by the Analysis button of GUIForm1.

	const DWORD start_time=GetTickCount();
	const DWORD authorized_duration =Time_ComboBoxMaxiTime_in_ms[ComboBoxMaxiTime->ItemIndex];


	floppy_disk_analyse=new TFloppyDisk();
	if (floppy_disk_analyse==NULL)
	{
		return false;
	}

	bool OK=false;

	if (floppy_disk_analyse->OpenFloppyDisk(ComboBoxDisk->ItemIndex,
		Time_ComboBoxMaxiTime_in_ms[ComboBoxMaxiTime->ItemIndex],
		this->MemoLOG->Lines, &PleaseCancelCurrentOperation,
		GUIForm1->CheckBoxSaveRawTrackInfos->Checked) )
	{
		if ( ! floppy_disk_analyse->fdrawcmd_sys_installed) {
			Application->MessageBox("'fdrawcmd.sys' is needed, and not installed. See your manual for more information.",Application->Name.c_str(),MB_OK | MB_ICONERROR);
			OK=true;
		}
		else
		{
			BitBtnCancel->Enabled=true;
			{
				ImageSide0->Picture->Bitmap->Canvas->FillRect(
					ImageSide0->Picture->Bitmap->Canvas->ClipRect);
				ImageSide1->Picture->Bitmap->Canvas->FillRect(
					ImageSide1->Picture->Bitmap->Canvas->ClipRect);
				// Draws a line to indicate the start marker of the track.
				ImageSide0->Picture->Bitmap->Canvas->MoveTo(200,200);
				ImageSide0->Picture->Bitmap->Canvas->LineTo(400,200);
				ImageSide1->Picture->Bitmap->Canvas->MoveTo(200,200);
				ImageSide1->Picture->Bitmap->Canvas->LineTo(400,200);
			}

			Analyse_Disk_thread=new TAnalyseDiskThread(true);
			if (Analyse_Disk_thread != NULL) {
				Analyse_Disk_thread->floppy_disk=floppy_disk_analyse;

				unsigned track,side,sector;
				track=floppy_disk_analyse->direct_infos.Selected_Track;
				side=floppy_disk_analyse->direct_infos.Selected_Side;

				#ifndef _DEBUG
					Analyse_Disk_thread->Priority=tpHigher;
				#endif
				Analyse_Disk_thread->Resume();

				Sleep(200);
				while(Analyse_Disk_thread->ThreadRunning)
				{
					const DWORD elapsed_time= GetTickCount() - start_time;
					const bool is_time_over= elapsed_time > authorized_duration;
					if (PleaseCancelCurrentOperation || is_time_over) {
						Analyse_Disk_thread->Terminate();
						if (PleaseCancelCurrentOperation)
							MemoLOG->Lines->Add("Operation canceled by user (you !).");
						if (is_time_over)
							MemoLOG->Lines->Add("Time is up, process giveup (see Giveup time option).");
						break;
					}
					Application->ProcessMessages();
					if (track!=floppy_disk_analyse->direct_infos.Selected_Track
					|| side!=floppy_disk_analyse->direct_infos.Selected_Side
					) { 
						track=floppy_disk_analyse->direct_infos.Selected_Track;
						side=floppy_disk_analyse->direct_infos.Selected_Side;
//						static char texteinfos[256];
//						StringCbPrintf(texteinfos,sizeof(texteinfos)-1,
//							"Track:%d Side/Head:%d",
//							track,side);//,sector+1);
//						LabelInformation->Caption=texteinfos;
						LabelInformation->Caption=AnsiString().sprintf(
							"Track:%d Side/Head:%d",
							track,side
						);//,sector+1);
					}
					Sleep(200);
				}

				Application->ProcessMessages();
				delete Analyse_Disk_thread;
				Analyse_Disk_thread=NULL;
				PleaseCancelCurrentOperation=false;
			}
			BitBtnCancel->Enabled=false;
		}
		// end
		floppy_disk_analyse->CloseFloppyDisk();
	}
	else
	{
		Application->MessageBox("No disk in drive",Caption.c_str(),MB_OK | MB_ICONERROR);
		OK=true;
	}

	delete floppy_disk_analyse;
	floppy_disk_analyse=NULL;

	MemoLOG->Lines->Add("Operation terminated.");

	return OK;
}


void __fastcall TGUIForm1::DrawGridSideASectorsMouseMove(TObject *Sender,
      TShiftState Shift, int X, int Y)
{
	AnsiString s;
	TDrawGrid* dg=(TDrawGrid*) Sender;
	TGridCoord gc=dg->MouseCoord (X,Y);
	s.printf("Track %d - Sector %d",
		gc.X, gc.Y+1);
	dg->Hint=s;
}
//---------------------------------------------------------------------------

