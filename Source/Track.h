//---------------------------------------------------------------------------
// Copyright Christophe Bertrand.
// This file is licensed under the Microsoft Reciprocal License (Ms-RL). You can find a copy of this license in the file "Ms-RL License.htm", in the root directory of the distribution archive.
//---------------------------------------------------------------------------



#ifndef TrackH
#define TrackH
//---------------------------------------------------------------------------


#include <vector>
#include <windows.h>

#include "FloppyDisk.h"

// =================== General definitions ===========================

#define CP_NB_MAX_SECTORS_PER_RAW_TRACK (6400/128)


struct SSector
{
	//int		Number_1based;
	bool	Read_correctly;
	bool	Normal_reading_by_controller_tried; // Normal reading by controller.
	bool	Normal_reading_by_controller_success;
	//bool	Reading_of_Raw_Track_tried; // Reading from the raw track.
	bool	Reading_of_Raw_Track_success; // Reading from the raw track.
	int		Reading_tried_number; // 1=Read at the 1st. Otherwise, it was more difficult.
	int		Byte_size;
	//std::vector<BYTE>*	pContent; // Memory.
	BYTE*	pContent; // Memory.
};

// TODO: "STrackInfo" should be deleted, in favor of the simple TTrack.
struct STrackInfo
{
	bool							OperationSuccess;
	//unsigned __int8*				TrackContent;
	//unsigned						TrackSize;
	//FD_SCAN_RESULT*				fdrawcmd_Scan_Result; // KEEP for later.
	struct FD_TIMED_SCAN_RESULT_32*	fdrawcmd_Timed_Scan_Result; // N° sectors 1-based.
};



// ===================== The class in itself =========================

class TTrack // ------------------------------------------
{
public:

	struct _SSectorInfoInRawTrack
	{
		//bool		Well_read_data;
		unsigned	Index_In_Encoded_Track_Content; // Sector data in the source track.
		unsigned	Index_In_Decoded_Track_Content; // Sector data in the decoded track.
		bool		Sector_Identified;
		bool		ID_found_directly;

		// The following ID values are only valid if an ID has been found for this sector.
		unsigned	Track_ID;
		unsigned	Side_ID;
		unsigned	Sector_ID_1based; // 1-based.
		unsigned	Size_ID; // 0=128, 1=256, 2=512 3=1024, etc., i.e. 2^size_ID*128.
	};


private:	// User declarations.

	int			Track_0based; //	Number of Track 0-based.
	int			Side_0based; //	Number of Side  0-based.

	struct SSectorInfoInRawTrack16kb
	{
		unsigned	Nb_Sectors_found; // Including repetitives.
		unsigned __int8		Encoded_Track_Content[16 * 1024];
		unsigned __int8		Decoded_Track_Content[16 * 1024];
		_SSectorInfoInRawTrack	Sector_Infos[128]; // About 128 sectors of 128 bytes is the maximum in a raw track of 6,250 bytes read more times but on 16 KB ..
	};
	//struct SSectorInfoInRawTrack16kb CP_Raw_Track_Result;


	// Memory to store the content of all sectors of this track:
	struct
	{
		BYTE		memory[6400];
		//std::vector<int>	sectors_memory_index_1based; // If index = -1, then not yet created.
		int	sectors_memory_index_1based[CP_NB_MAX_SECTORS_PER_RAW_TRACK]; // If index = -1, then not yet created.
		unsigned	free_memory_index; // 0 at the start.
	} Sectors_content;

	//SSector* reserve_sector_structure(unsigned sector_0based);
	BYTE* reserve_sector_memory(unsigned sector_0based, unsigned bytes_number);

	struct SBlock
	{
		int	start_time; // index in microseconds (from start of track).
		int free_space_duration_in_1st;		// there is always an area.
		int	sector_number_1based_in_2nd; // necessarily occupied.
		int	sector_size_in_bytes_bit_code; // 0=128 bytes, 1=256, 2=512, .. , 7=16384.
		bool	found_by_controller; // otherwise, it is found by raw reading of the track.
	};



	// ------------------ Private functions ---------------


	bool	CP_identify_raw_sectors( // Returns if "sector_0based" was found.
		class TFloppyDisk* floppy_disk, // Calling class.
		unsigned track, // All arguments are 0-based.
		unsigned side,
		unsigned sector_0based,
		TStrings* LOG_strings,
		SSectorInfoInRawTrack16kb &Raw_Track_Result,
		BYTE* pSectorMemory); // Where we'll ALSO copy the data from the searched sector.

	bool	CP_select_track_and_side( // Only with "fdrawcmd.sys".
		TFloppyDisk* floppy_disk, // Calling class.
		unsigned track, // 0-based.
		unsigned side); // 0-based.

	bool	Init_and_complete_area_blocks_array(
		TFloppyDisk* floppy_disk, // Calling class.
		std::vector<struct SBlock>*	blocks_array,
		STrackInfo* timer_track,
		TStrings* LOG_strings,
		int track_1_turn_duration); // In microseconds.

	bool DecodeTrack(SSectorInfoInRawTrack16kb* Result);


public:		// User declarations.

	// Sector information is stored in a fixed-size array.
	struct SSector	Sectors_array_0based[CP_NB_MAX_SECTORS_PER_RAW_TRACK]; // The index of this array is 0-based.

	bool	raw_track_info_saving_already_done;

	// ------------------ Public functions ---------------

	TTrack(int Track_number_0based, int Side_number_0based);

	~TTrack();

	int	get_track_number_0based(void);
	int	get_side_number_0based(void);

	bool	TTrack::CP_ReadSector(
		class TFloppyDisk* floppy_disk, // Calling class.
		unsigned track, // All arguments are 0-based.
		unsigned side,
		unsigned sector_0based,
		DWORD allowed_time_ms,
		SSector** p_p_s_sector, // Writes a pointer to a class in the provided memory.
		TStrings* LOG_strings,
		volatile bool*	p_canceller, // If this variable becomes true, the current operation is cancelled.
		bool save_raw_track_infos);

	bool	TTrack::CP_ReadSector_OLD( // FOR ARCHIVES.
		TFloppyDisk* floppy_disk, // Calling class.
		unsigned track, // All arguments are 0-based.
		unsigned side,
		unsigned sector_0based,
		DWORD allowed_time_ms,
		SSector** p_p_s_sector, // Writes a pointer to a class in the provided memory.
		TStrings* LOG_strings,
		volatile bool*	p_canceller, // If this variable becomes true, the current operation is cancelled.
		bool save_raw_track_infos);

	STrackInfo*	CP_Analyse_Sectors_Time( // Analyzes the track, and provides a time map of the sectors.
		class TFloppyDisk* floppy_disk, // Calling class.
		unsigned track,
		unsigned side);//,    // All arguments are 0-based.

	bool	TTrack::CP_analyse_raw_track(// The raw track is read.
		bool Purely_informative,// Does not modify sector data. Otherwise, we extract the information to the sectors.
		TFloppyDisk* floppy_disk, // Calling class.
		DWORD allowed_time_ms,
		SSector** p_p_s_sector, // Writes a pointer to a class in the provided memory.
		TStrings* LOG_strings,
		volatile bool*	p_canceller, // If this variable becomes true, the current operation is cancelled.
		bool save_raw_track_infos, // In a text file and a binary file.
		struct SSectorInfoInRawTrack16kb* Raw_Track_Result);


private:   // -----------------------------------------

	bool ReadRawTrackSectors( // Reads the currently selected track.
		class TFloppyDisk* floppy_disk, // Calling class.
		SSectorInfoInRawTrack16kb* Result);


};
// ==============================================


#endif
