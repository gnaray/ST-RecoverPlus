//---------------------------------------------------------------------------
// Copyright Christophe Bertrand.
// This file is licensed under the Microsoft Reciprocal License (Ms-RL). You can find a copy of this license in the file "Ms-RL License.htm", in the root directory of the distribution archive.
//---------------------------------------------------------------------------



#ifndef GUIForm1H
#define GUIForm1H
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Controls.hpp>
#include <StdCtrls.hpp>
#include <Forms.hpp>
#include <Grids.hpp>
#include <Buttons.hpp>
#include <ComCtrls.hpp>
#include <ExtCtrls.hpp>
#include <Dialogs.hpp>



//---------------------------------------------------------------------------

// Personalized message to order the update of the graph.
#define WM_recover_maj_FormAnalyse_track (WM_APP+1)

/*		ComboBoxMaxiTime   : Give maximum time to read the floppy disk.

0		Giveup after 5 mn
1		Giveup after 15 mn
2		Giveup after 30 mn (Recommended)
3		Giveup after 60 mn
4		Giveup after 2 h
5		Giveup after 5 h
6		Never giveup (well, after 20 days)
*/
const int Time_ComboBoxMaxiTime_in_ms[7]={ 300000,900000,1800000,3600000,7200000,18000000,1728000000 };


//---------------------------------------------------------------------------
class TGUIForm1 : public TForm
{
__published:	// Components managed by EDI
	TButton *ButtonReadDisk;
	TLabel *LabelInformation;
	TButton *ButtonAnalyse;
	TBitBtn *BitBtnCancel;
	TPageControl *PageControlResults;
	TTabSheet *TabSheetSidesGrids;
	TDrawGrid *DrawGridSideASectors;
	TDrawGrid *DrawGridSideBSectors;
	TTabSheet *TabSheetSides;
	TImage *ImageSide0;
	TImage *ImageSide1;
	TComboBox *ComboBoxMaxiTime;
	TSaveDialog *SaveDialogDiskImage;
	TComboBox *ComboBoxDisk;
	TLabel *Label1;
	TMemo *MemoLOG;
	TLabel *LabelElapsedTime;
	TCheckBox *CheckBoxSaveRawTrackInfos;
	void __fastcall ButtonReadDiskClick(TObject *Sender);
	void __fastcall DrawGridSideASectorsDrawCell(TObject *Sender, int ACol,
          int ARow, TRect &Rect, TGridDrawState State);
	void __fastcall ButtonAnalyseClick(TObject *Sender);
	void __fastcall BitBtnCancelClick(TObject *Sender);
	void __fastcall DrawGridSideASectorsMouseMove(TObject *Sender,
          TShiftState Shift, int X, int Y);
protected:
	void __fastcall On_recover_maj_FormAnalyse_track(TMessage &Message);
	BEGIN_MESSAGE_MAP  // See http://www.programmez.com/tutoriels.php?tutoriel=38&titre=Les-messages-Windows-personnalises-avec-C++-Builder
		VCL_MESSAGE_HANDLER(WM_recover_maj_FormAnalyse_track, TMessage, On_recover_maj_FormAnalyse_track)
	END_MESSAGE_MAP(TForm)
private:	// User declarations
public:		// User declarations

	bool PleaseCancelCurrentOperation;

	__fastcall TGUIForm1(TComponent* Owner);
	bool	__fastcall AnalyseDisk(void);
};
//---------------------------------------------------------------------------
extern PACKAGE TGUIForm1 *GUIForm1;
//---------------------------------------------------------------------------
#endif
