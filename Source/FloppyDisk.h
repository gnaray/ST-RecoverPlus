//---------------------------------------------------------------------------
// Copyright Christophe Bertrand.
// This file is licensed under the Microsoft Reciprocal License (Ms-RL). You can find a copy of this license in the file "Ms-RL License.htm", in the root directory of the distribution archive.
//---------------------------------------------------------------------------



#ifndef FloppyDiskH
#define FloppyDiskH


//---------------------------------------------------------------------------

#include <Classes.hpp>
#include <windows.h>
#include "fdrawcmd.h" // for the "fdrawcmd.sys" driver.
#include <vector>
#include "Track.h"





#define ColorSectorNotRead (0xffffff)
#define ColorSectorReadOK (0xff00)
#define ColorSectorError (0xff)
#define ColorSectorDifficult (0xff0000)








struct SSectors
{
	bool	is_read[85][12];
	bool	difficult_to_read[85][12];
	bool	error[85][12];
};


#pragma pack(push,1)
struct FD_TIMED_SCAN_RESULT_32 // based on FD_TIMED_SCAN_RESULT
{
	BYTE count;                         // count of returned headers
	BYTE firstseen;                     // offset of first sector detected
	DWORD tracktime;                    // total time for track (in microseconds)
	FD_TIMED_ID_HEADER Headers[32];
};
#pragma pack(pop)
#if ((sizeof(FD_TIMED_SCAN_RESULT)+sizeof(FD_TIMED_ID_HEADER)*32) != (sizeof(FD_TIMED_SCAN_RESULT_32)))
#error CHRIS : FD_TIMED_SCAN_RESULT must have been modified in fdrawcmd.h.
#endif



class TFloppyDisk // ------------------------------------------
{
private:	// User declarations

	class TTrack*	current_Track;


	bool	init_current_track(unsigned track, unsigned side);

public:		// User declarations

	SSectors SectorsSideA;
	SSectors SectorsSideB;

	struct _direct_infos {
		unsigned	Selected_Track; // The head is currently placed here.
		unsigned	Selected_Side; // This side is currently selected.
		unsigned	Sector_under_treatment_0based; // 0-based.
	} direct_infos;



	bool	KnownDiskArchitecture;
	int	NbSectorsPerTrack;
	int NbTracks;
	int NbSides;
	unsigned NbBytesPerSector;

	unsigned 	SelectedFloppyDrive;  // floppy_drive: 0=A , 1=B , etc..





	HANDLE hDevice;
	// =====

	bool	Win9X;
	bool	fdrawcmd_sys_installed;

#pragma pack(push)
#pragma pack (1) // Byte-accurate alignment: essential here.
	struct t_Atari_ST_boot_sector
	{
		unsigned __int16	branching; // 0
		unsigned char	Text_Loader[6]; // 2
		unsigned __int8	serial_number_0; // 8
		unsigned __int8	serial_number_1; // 9
		unsigned __int8	serial_number_2; // 10
		unsigned __int16	Bytes_per_sector; // 11
		unsigned __int8	Sectors_per_cluster; // 13
		unsigned __int16	Nb_boot_reserved_sectors; // 14
		unsigned __int8	Nb_of_FATs; // 16
		unsigned __int16	Nb_maxi_entries_in_root; // 17
		unsigned __int16	Nb_sectors_on_floppy_disk; // 19
		unsigned __int8		Media_Descriptor_Code; // 21 (see "Bible ST" page 212).
		unsigned __int16	Nb_sectors_per_FAT; // 22
		unsigned __int16	Nb_sectors_per_track; // 24
		unsigned __int16	Nb_sides; // 26 ( = nb of heads).
		unsigned __int16	Nb_hidden_sectors; // 28
		unsigned __int8	content[510 - 30]; // 30
		unsigned __int16	Checksum; // 510 (ST bootable: $1234, i.e. 0x3412 ?).
	} Atari_ST_Boot_Sector;   // Must be 512 bytes, due to "sizeof".
	BYTE   _possible_continuation_of_the_boot_sector[4096 - 512]; // Stick after "Atari_ST_Boot_Sector".
#pragma pack(pop)



	__fastcall TFloppyDisk();
	__fastcall ~TFloppyDisk();


	bool		OpenFloppyDisk(unsigned floppy_drive, // floppy_drive: 0=A , 1=B , etc..
		DWORD allowed_time_ms, TStrings* LOG_strings,
		volatile bool*	p_canceller, // If this variable becomes true, the current operation is cancelled.
		bool save_raw_track_infos);


	bool	CD_ReadSector(
		unsigned track, // All arguments are 0-based.
		unsigned side,
		unsigned sector_0based,
		DWORD allowed_time_ms,
		TStrings* LOG_strings,
		struct SSector** pp_sector, // To receive an "SSector*".
		volatile bool*	p_canceller, // If this variable becomes true, the current operation is cancelled.
		bool save_raw_track_infos);


	bool		CloseFloppyDisk(void);

	struct STrackInfo*	CD_AnalyseSectorsTime(
		unsigned track,
		unsigned side);//,    // All arguments are 0-based.

}; // ==============================================


 // ==============================================


#endif
