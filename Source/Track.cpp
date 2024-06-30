//---------------------------------------------------------------------------
// Copyright Christophe Bertrand.
// This file is licensed under the Microsoft Reciprocal License (Ms-RL). You can find a copy of this license in the file "Ms-RL License.htm", in the root directory of the distribution archive.
//---------------------------------------------------------------------------


#pragma hdrstop


#include "Track.h"
#include "FdrawcmdSys.h"


//---------------------------------------------------------------------------

#pragma package(smart_init)

//---------------------------------------------------------------------------

#include <time.h>
#include <math.h>


#define CP_NUMBER_OF_SECTOR_READ_ATTEMPTS (60)

#define CLDIS_Raw_byte_duration_in_microseconds (32) // 200000 microsec./6250 bytes per track.
#define CLDIS_Minimum_bytes_number_between_sectors (54) // normal: 102. 54, this is for formatting in 11 sectors/track, to be read in 2 rounds !






double log2(double x)
{
	return log(x) / log(2);
}

#pragma pack(push) // All msdos data structures must be packed on a 1 byte boundary
#pragma pack (1)
struct
{
	DWORD StartingSector;
	WORD NumberOfSectors;
	DWORD pBuffer;
}ControlBlock;
typedef struct _DIOC_REGISTERS
{
	DWORD reg_EBX;
	DWORD reg_EDX;
	DWORD reg_ECX;
	DWORD reg_EAX;
	DWORD reg_EDI;
	DWORD reg_ESI;
	DWORD reg_Flags;
} DIOC_REGISTERS;
#pragma pack(pop)

//---------------------------------------------------------------------------

TTrack::TTrack(int Track_number_0based, int Side_number_0based)
{
	Track_0based = Track_number_0based;
	Side_0based = Side_number_0based;

	memset(&Sectors_content, 0, sizeof(Sectors_content));

	memset(&Sectors_array_0based, 0, sizeof(Sectors_array_0based));


	raw_track_info_saving_already_done = false;
}

//---------------------------------------------------------------------------
TTrack::~TTrack()
{
}
//---------------------------------------------------------------------------
int	TTrack::get_track_number_0based(void)
{
	return Track_0based;
}
//---------------------------------------------------------------------------
int	TTrack::get_side_number_0based(void)
{
	return Side_0based;
}
//---------------------------------------------------------------------------
bool	TTrack::CP_identify_raw_sectors( // Returns if "sector_0based" was found.
	class TFloppyDisk* floppy_disk, // Calling class.
	unsigned track, // All arguments are 0-based.
	unsigned side,
	unsigned sector_0based,
	TStrings* LOG_strings,
	SSectorInfoInRawTrack16kb &Raw_Track_Result,
	BYTE* pSectorMemory) // Where we'll ALSO copy the data from the searched sector.

{
	bool return_value = false;

	// Here, I call "raw track" the bytes read in RAW,
	// and "Track Timer" the time data on the track.

	// 3) Find out where the sectors are on the timer_track.
	STrackInfo* timer_track = CP_Analyse_Sectors_Time(floppy_disk, track, side);
	if (timer_track->OperationSuccess)
	{

		// a) We determine the 1st sector common to the 2 tracks (raw and timer).
		bool common_sector_found = false;
		unsigned i_raw_sector, i_timer_sector;
		{
			for (i_raw_sector = 0; i_raw_sector < Raw_Track_Result.Nb_Sectors_found; i_raw_sector++)
			{
				if (Raw_Track_Result.Sector_Infos[i_raw_sector].Sector_Identified)
				{
					for (i_timer_sector = 0; i_timer_sector < (unsigned)timer_track->fdrawcmd_Timed_Scan_Result->count; i_timer_sector++)
					{
						if (Raw_Track_Result.Sector_Infos[i_raw_sector].Sector_ID_1based
							== (unsigned)timer_track->fdrawcmd_Timed_Scan_Result->Headers[i_timer_sector].sector)
						{
							common_sector_found = true;
							break;
						}
					}
					if (common_sector_found)
						break;
				}
			}
		}
		if (common_sector_found)
		{

			// b)	Calculate the actual length of the raw track, in bytes.
			//    Because the ideal length is 6250, but on my floppy drive it is 6285.
			unsigned	raw_track_size = 6250; // default value.
			unsigned	raw_track_duration = 6250 * CLDIS_Raw_byte_duration_in_microseconds; // default value.
			{
				// For that, you have to find the same sector twice in the raw track.
				bool couple_found = false;
				for (unsigned i1 = 0; i1 < Raw_Track_Result.Nb_Sectors_found; i1++)
				{
					if (Raw_Track_Result.Sector_Infos[i1].Sector_Identified)
					{
						for (unsigned i2 = i1 + 1; i2 < Raw_Track_Result.Nb_Sectors_found; i2++)
						{
							if (Raw_Track_Result.Sector_Infos[i2].Sector_Identified)
							{
								if (Raw_Track_Result.Sector_Infos[i1].Sector_ID_1based
									== Raw_Track_Result.Sector_Infos[i2].Sector_ID_1based)
								{
									couple_found = true;
									int diff =
										Raw_Track_Result.Sector_Infos[i2].Index_In_Encoded_Track_Content
										- Raw_Track_Result.Sector_Infos[i1].Index_In_Encoded_Track_Content;
									const int tmaximaxi = (6250 * 6) / 5;// tolerance: 20%.
									if (diff > tmaximaxi) // 6250+20%
										if (diff > (tmaximaxi * 2)) // 6250*2+20%
											diff /= 3; // we did three turns.
										else
											diff /= 2; // we did two turns.
									if ((diff < 6125) || (diff > 6375)) // guardrail (motor may not be running at the correct speed).
									{
#ifdef _DEBUG
										DebugBreak();
#endif
										diff = 6250;
									}
									raw_track_size = diff;
									raw_track_duration = diff*CLDIS_Raw_byte_duration_in_microseconds;
									break;
								}
							}
						} // end for i2
						if (couple_found)
							break;
					}
				} // end for i1
			}  // end block

			// - - - - - -

			// c) We calculate when reading started in the raw track.
			//		Because it is read from the first syncro data found,
			//		and not at the physical start of the track (unlike the timer).				<<==  VERY IMPORTANT TO NOTE !!!
			int raw_track_start_time;
			{
				raw_track_start_time =
					timer_track->fdrawcmd_Timed_Scan_Result->Headers[i_timer_sector].reltime
					- Raw_Track_Result.Sector_Infos[i_raw_sector].Index_In_Encoded_Track_Content
					* CLDIS_Raw_byte_duration_in_microseconds;
			}
			if (raw_track_start_time < 0)
				raw_track_start_time =
				(raw_track_start_time + 3 * raw_track_duration) % raw_track_duration;



			// -----------------------------------

			std::vector<struct SBlock>	blocks_array;
			blocks_array.reserve(CP_NB_MAX_SECTORS_PER_RAW_TRACK);

			Init_and_complete_area_blocks_array(floppy_disk, &blocks_array, timer_track, LOG_strings, raw_track_duration);

			// -----------------------------------



			// From here, we should use "blocks_array", instead of
			// "timer_track" (basically contains the same information, but with the discovered sectors added).


			// d) We build a table of the sectors found, by gathering
			//      all the information available in the 2 tracks.
			// For that, the time is divided into zones of 4096 micro-seconds (i.e. 128 bytes).
			unsigned sector_areas_1based[8192 / 128];
			memset(sector_areas_1based, 0, sizeof(sector_areas_1based));

			if (blocks_array.size() > 0)
			{
				const unsigned one_area_duration_in_microseconds =
					128 * CLDIS_Raw_byte_duration_in_microseconds;
				const int sector_duration =
					(128 << blocks_array[0].sector_size_in_bytes_bit_code)
					* CLDIS_Raw_byte_duration_in_microseconds;

				// d1) The time track (priority) is used, with the sectors completed by calculation.
				for (i_timer_sector = 0; i_timer_sector <
					(unsigned)blocks_array.size(); //timer_track_TO_BE_REPLACED->fdrawcmd_Timed_Scan_Result->count ;
					i_timer_sector++)
				{
					const unsigned nsect_1based =
						blocks_array[i_timer_sector].sector_number_1based_in_2nd;
					if (nsect_1based != 0)
					{
						const unsigned start_time =
							blocks_array[i_timer_sector].start_time + blocks_array[i_timer_sector].free_space_duration_in_1st;
						const unsigned end_time = start_time
							+ sector_duration;

						for (
							unsigned area_i = start_time / one_area_duration_in_microseconds;
							area_i < end_time / one_area_duration_in_microseconds;
							area_i++)
						{
							sector_areas_1based[area_i] = nsect_1based;
						}
					}
				} // next i_timer_sector

				// d2) We give the sector number to those who lack an ID in the raw track.
				{
					for (unsigned i3 = 0; i3 < Raw_Track_Result.Nb_Sectors_found; i3++)
					{
						if (!Raw_Track_Result.Sector_Infos[i3].Sector_Identified)
						{
							const unsigned start_byte =
								Raw_Track_Result.Sector_Infos[i3].Index_In_Encoded_Track_Content
								% raw_track_size;
							// I count for a sector of 128 bytes because we do not know its size.
							const unsigned mid_sector_time =
								((start_byte +/*128*/floppy_disk->NbBytesPerSector / 2) * CLDIS_Raw_byte_duration_in_microseconds
									+ raw_track_start_time)
								% raw_track_duration;
							unsigned num1based =
								sector_areas_1based[mid_sector_time / one_area_duration_in_microseconds];
							if (num1based != 0) // If we found the sector number (1-based).
							{
								Raw_Track_Result.Sector_Infos[i3].Sector_ID_1based = num1based;
								Raw_Track_Result.Sector_Infos[i3].Sector_Identified = true;
								Raw_Track_Result.Sector_Infos[i3].Track_ID = this->Track_0based;
								Raw_Track_Result.Sector_Infos[i3].Side_ID = this->Side_0based;
								Raw_Track_Result.Sector_Infos[i3].Size_ID =
									log2(floppy_disk->NbBytesPerSector / 128.0);
							}
							else
							{
#ifdef _DEBUG
								DebugBreak();
#endif
								LOG_strings->Add("Warning: sector not identified. There is a default in the sector scheme analysis. Please contact the author.");
							}

							// FINALLY we're at the point where we can use anonymous sectors.
							// ----------------------------------------------------------------

							if (Raw_Track_Result.Sector_Infos[i3].Sector_Identified)
							{
								bool is_this_the_searched_sector =
									Raw_Track_Result.Sector_Infos[i3].Sector_ID_1based == (sector_0based + 1);
								// Here I should also test the track ID, side and size.
								// But it may interfere with the reading of special diskettes.

								SSector* sector_from_raw =
									&Sectors_array_0based[
										Raw_Track_Result.Sector_Infos[i3].Sector_ID_1based - 1];

								if (!sector_from_raw->Read_correctly)
								{
									BYTE* mem =
										is_this_the_searched_sector ?
										pSectorMemory
										: reserve_sector_memory(
											Raw_Track_Result.Sector_Infos[i3].Sector_ID_1based - 1,
											floppy_disk->NbBytesPerSector);

									if (mem != NULL)
									{
										memcpy(mem, &Raw_Track_Result.Decoded_Track_Content[
											Raw_Track_Result.Sector_Infos[i3].Index_In_Decoded_Track_Content],
											floppy_disk->NbBytesPerSector);
										sector_from_raw->Read_correctly = true;
										sector_from_raw->Byte_size = floppy_disk->NbBytesPerSector;
										sector_from_raw->Reading_of_Raw_Track_success = true;
										sector_from_raw->Reading_tried_number++;
										if (is_this_the_searched_sector)
										{
											return_value = true;
											//break; // "break" if we limit ourselves to our currently searched area (debug).
										}
									}
								}
							}

						}
					}
				}
			}
			blocks_array.clear();
		}
	}  // endif timer_track->OperationSuccess
	return return_value;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
bool	TTrack::CP_ReadSector(
	TFloppyDisk* floppy_disk, // Calling class.
	unsigned track, // All arguments are 0-based.
	unsigned side,
	unsigned sector_0based,
	DWORD allowed_time_ms,
	SSector** p_p_s_sector, // Writes a pointer to a class in the provided memory.
	TStrings* LOG_strings,
	volatile bool*	p_canceller, // If this variable becomes true, the current operation is cancelled.
	bool save_raw_track_infos)
{
	const DWORD time_limit = GetTickCount() + allowed_time_ms;

	SSector* sector = &Sectors_array_0based[sector_0based];

	*p_p_s_sector = sector;

	if (sector->Read_correctly) // Case where this sector has already been read.
		return true;

	if (*p_canceller)
		return false;



	const bool determine_architecture = floppy_disk->KnownDiskArchitecture;
	BYTE*	pSectorMemory =
		determine_architecture ?
		reserve_sector_memory(sector_0based, floppy_disk->NbBytesPerSector)
		: (BYTE*)&floppy_disk->Atari_ST_Boot_Sector;//_sector_memory : // if we don't know the archi, we write in this memory.

	sector->pContent = pSectorMemory; // Even for the boot-sector, which is quite special and limited to 512 bytes.

	bool OK = false;

	const unsigned startinglogicalsector =
		((!floppy_disk->KnownDiskArchitecture) || ((track == 0) && (side == 0) && (sector_0based == 0))) ?
		0 :
		(track*floppy_disk->NbSectorsPerTrack*floppy_disk->NbSides + side*floppy_disk->NbSectorsPerTrack)
		+ sector_0based;

	const numberofsectors = 1;

	if (floppy_disk->Win9X)
	{
		// code for win 95/98
		ControlBlock.StartingSector = (DWORD)startinglogicalsector;
		ControlBlock.NumberOfSectors = (DWORD)numberofsectors;
		ControlBlock.pBuffer = (DWORD)pSectorMemory;

		//-----------------------------------------------------------
		// SI contains read/write mode flags
		// SI=0h for read and SI=1h for write
		// CX must be equal to ffffh for
		// int 21h's 7305h extention
		// DS:BX -> base addr of the
		// control block structure
		// DL must contain the drive number
		// (01h=A:, 02h=B: etc)
		//-----------------------------------------------------------

		DIOC_REGISTERS reg;

		reg.reg_ESI = 0x00;
		reg.reg_ECX = -1;
		reg.reg_EBX = (DWORD)(&ControlBlock);
		reg.reg_EDX = floppy_disk->SelectedFloppyDrive + 1;
		reg.reg_EAX = 0x7305;

		//  6 == VWIN32_DIOC_DOS_DRIVEINFO // This is Interrupt 21h Function 730X commands.
		BOOL  fResult;
		DWORD cb;

		fResult = DeviceIoControl(floppy_disk->hDevice,
			6,
			&(reg),
			sizeof(reg),
			&(reg),
			sizeof(reg),
			&cb,
			0);

		OK = !(!fResult || (reg.reg_Flags & 0x0001));
	}
	else
	{       // Code Windows NT

		if (!floppy_disk->fdrawcmd_sys_installed)
		{
			// Setting the pointer to point to the start of the sector we want to read ..
			SetFilePointer(floppy_disk->hDevice, (startinglogicalsector*floppy_disk->NbBytesPerSector), NULL, FILE_BEGIN);

			DWORD bytesread = 0;
			OK = ReadFile(floppy_disk->hDevice, pSectorMemory, floppy_disk->NbBytesPerSector*numberofsectors, &bytesread, NULL);
			sector->Normal_reading_by_controller_tried = true;
			if (OK)
			{
				sector->Read_correctly = true;
				sector->Reading_tried_number++;
				sector->Byte_size = bytesread;
				sector->Normal_reading_by_controller_tried = true;
				sector->Normal_reading_by_controller_success = true;
			}
		}
		else
		{
			// With this driver (fdrawcmd.sys), we are forced to use another method.
			DWORD dwRet;
			FD_READ_WRITE_PARAMS rwp;

			// details of sector to read
			rwp.flags = FD_OPTION_MFM;
			rwp.phead = side;
			rwp.cyl = track;
			rwp.head = side;
			rwp.sector = sector_0based + 1; // 1-based.
			rwp.size = 2;
			rwp.eot = sector_0based + 1 + 1; // Sector 'next' last to read, 1-based.
			rwp.gap = 0x0a;
			rwp.datalen = 0xff;

			OK = CP_select_track_and_side(floppy_disk, track, side);
			if (OK)
			{
				floppy_disk->direct_infos.Selected_Track = track; // The head is currently placed here.
				floppy_disk->direct_infos.Selected_Side = side; // This side is currently selected.

				bool is_sector_read = false;
				int nb_tries_here = 0;


				// -------------------------------------------------------------
				while ((!is_sector_read)
					&& (((int)time_limit - GetTickCount()) > 0)
					&& (!*p_canceller)
					&& (nb_tries_here < CP_NUMBER_OF_SECTOR_READ_ATTEMPTS))
				{
					if ((nb_tries_here % 16) == 15) // Every 16 loops, we recalibrate the head... just in case.
					{
//						DeviceIoControl(floppy_disk->hDevice, IOCTL_FDCMD_RECALIBRATE, NULL, 0, NULL, 0, &dwRet, NULL);
						FdCmdRecalibrate(floppy_disk->hDevice);
						// seek to cyl "track"
						CP_select_track_and_side(floppy_disk, track, side);
					}


					// read sector
//					is_sector_read = DeviceIoControl(floppy_disk->hDevice, IOCTL_FDCMD_READ_DATA, &rwp,
//						sizeof(rwp), pSectorMemory, floppy_disk->NbBytesPerSector, &dwRet, NULL);
					is_sector_read = FdCmdRead(floppy_disk->hDevice, FD_OPTION_MFM, side, track, side,
						sector_0based + 1, 2, 1, // 1-based.
						pSectorMemory, LengthToSizeCode(floppy_disk->NbBytesPerSector));

					sector->Reading_tried_number++;
					sector->Normal_reading_by_controller_tried = true; // VERY IMPORTANT: we set 'true', whether the reading was successful or not.
					sector->Normal_reading_by_controller_success = is_sector_read;
					sector->Read_correctly |= is_sector_read;

					if (!is_sector_read)
					{
						// Here we ask for a more in-depth technique to read this sector.

						struct SSectorInfoInRawTrack16kb Raw_Track_Result;

						bool recover_ok = CP_analyse_raw_track( // Only with "fdrawcmd.sys".
							false,//bool Purely_informative,// Does not modify sector data.
							//out_Detailed_infos,//TStringList* out_Detailed_infos,
							floppy_disk,//TFloppyDisk* floppy_disk, // Calling class.
							time_limit - GetTickCount(),//DWORD allowed_time_ms,
							p_p_s_sector,//SSector** p_p_s_sector, // Writes a pointer to a class in the provided memory.
							LOG_strings,//TStrings* LOG_strings,
							p_canceller,//volatile bool*	p_canceller, // If this variable becomes true, the current operation is cancelled.
							save_raw_track_infos,//bool save_raw_track_infos);
							&Raw_Track_Result);



						// See if we can find our sector in the raw track.
						if (recover_ok)
						{
							// Find out if our sector has been well read.
							for (unsigned i = 0; i < Raw_Track_Result.Nb_Sectors_found; i++)
							{
								if (Raw_Track_Result.Sector_Infos[i].Sector_Identified)
								{
									const bool is_this_the_searched_sector =
										Raw_Track_Result.Sector_Infos[i].Sector_ID_1based == (sector_0based + 1);
									// Here I should also test the track ID, side and size.
									// But it may interfere with the reading of special diskettes.

									SSector* sector_from_raw =
										&Sectors_array_0based[
											Raw_Track_Result.Sector_Infos[i].Sector_ID_1based - 1];


									if (sector_from_raw->Read_correctly)
									{
										if (is_this_the_searched_sector)
											is_sector_read = true;
									}
									else
									{
										BYTE* mem =
											is_this_the_searched_sector ?
											pSectorMemory
											: reserve_sector_memory(
												Raw_Track_Result.Sector_Infos[i].Sector_ID_1based - 1,//unsigned sector_0based,
												floppy_disk->NbBytesPerSector);//unsigned bytes_number);

										if (mem != NULL)
										{
											memcpy(mem, &Raw_Track_Result.Decoded_Track_Content[
												Raw_Track_Result.Sector_Infos[i].Index_In_Decoded_Track_Content],
												floppy_disk->NbBytesPerSector);
											sector_from_raw->Read_correctly = true;
											sector_from_raw->Byte_size = floppy_disk->NbBytesPerSector;
											sector_from_raw->Reading_of_Raw_Track_success = true;
											sector_from_raw->Reading_tried_number++;
											if (is_this_the_searched_sector)
											{
												is_sector_read = true;
												//break; // "break" if we limit ourselves to our currently searched area (debug).
											}
										}
									}
								}
							} // next i
						} // endif (recover_ok)

					} // endif ( ! is_sector_read || (always_save_raw_track && (nb_tries_here==0)))
					nb_tries_here++;
				} // end while // --------------------------------------------------

				OK &= is_sector_read;

				if (is_sector_read && !sector->Read_correctly)
					asm nop // ERROR

					if (is_sector_read && (!determine_architecture))
					{		// We had stored the content in a tempo memory, we have to copy it.
						BYTE* p = reserve_sector_memory(sector_0based, floppy_disk->NbBytesPerSector);
						OK &= (p != NULL);
						if (p != NULL)
						{
							memcpy(p, pSectorMemory, floppy_disk->NbBytesPerSector);
							sector->pContent = p;
						}
					} // endif (is_sector_read && (!determine_architecture))
			} // endif CP_select_track_and_side(floppy_disk, track, side);
		} // endif ( ! floppy_disk->fdrawcmd_sys_installed)
	} // endif (floppy_disk->Win9X)

	return OK;
}
//---------------------------------------------------------------------------
bool	TTrack::CP_select_track_and_side(
	TFloppyDisk* floppy_disk, // Calling class.
	unsigned track, // 0-based.
	unsigned side) // 0-based.
{
	if (floppy_disk == NULL)
		return false;
	if (!floppy_disk->fdrawcmd_sys_installed)
		return false;

	// With this driver (fdrawcmd.sys), we are forced to use another method.
	DWORD dwRet;
	FD_SEEK_PARAMS sp;

	// details of seek location
	sp.cyl = track;
	sp.head = side;

	// seek to cyl "track"
//	return DeviceIoControl(floppy_disk->hDevice, IOCTL_FDCMD_SEEK, &sp, sizeof(sp), NULL, 0, &dwRet, NULL);
	return FdCmdSeek(floppy_disk->hDevice, track, side);
}
//---------------------------------------------------------------------------
bool	TTrack::CP_analyse_raw_track(// The raw track is read.
	bool Purely_informative,// Does not modify sector data. Otherwise, we extract the information to the sectors.
	//TStringList* out_Detailed_infos,
	TFloppyDisk* floppy_disk, // Calling class.
	DWORD allowed_time_ms,
	SSector** p_p_s_sector, // Writes a pointer to a class in the provided memory.
	TStrings* LOG_strings,
	volatile bool*	p_canceller, // If this variable becomes true, the current operation is cancelled.
	bool save_raw_track_infos, // In a text file and a binary file.
	struct SSectorInfoInRawTrack16kb* Raw_Track_Result)
{
	/*	Short explanation:

	There are two reasons to call this function.

	- Either we can't read one of the sectors, and we need a better
	analysis. In this case, "Purely_informative"="false", and we will investigate
	missing sector information.

	- Either we want to launch an in-depth analysis of the track AFTER normal reading
	sectors, to provide debugging information.
	In this case, "Purely_informative"="true", and we won't keep the information
	on the sectors. On the other hand, we will write an information file of the track.
	Note, VERY IMPORTANT, that even in this last case, it is possible that we
	has already called this function before (for the same track), because of a difficulty
	to read a sector.
	*/


	if (*p_canceller)
		return false;
	if (!floppy_disk->fdrawcmd_sys_installed)
		return false;

	if (Raw_Track_Result == NULL)
		return false;
	memset(Raw_Track_Result, 0, sizeof(SSectorInfoInRawTrack16kb));

	bool OK = false;

	OK = CP_select_track_and_side(floppy_disk, Track_0based, Side_0based);

	if (!OK)
		return false;

	floppy_disk->direct_infos.Selected_Track = Track_0based; // The head is currently placed here.
	floppy_disk->direct_infos.Selected_Side = Side_0based; // This side is currently selected.

	int nb_tries_here = 0;


	// -------------------------------------------------------------

	bool recover_ok;

	// 1) Reads the raw track to find sectors on it.
	recover_ok = ReadRawTrackSectors(floppy_disk, Raw_Track_Result);
	if (!recover_ok)
		return false;

	{
		// We will also try to locate the sectors not yet found.

		// We see if we need to go further.
		int nb_raw_sectors_without_ID = 0;
		for (unsigned i = 0; i < Raw_Track_Result->Nb_Sectors_found; i++)
			if (!Raw_Track_Result->Sector_Infos[i].Sector_Identified)
				nb_raw_sectors_without_ID++;

		if (nb_raw_sectors_without_ID > 0)
			CP_identify_raw_sectors( // **************************
				floppy_disk,//class TFloppyDisk* floppy_disk, // Calling class.
				Track_0based,//unsigned track, // All arguments are 0-based.
				Side_0based,//unsigned side,
				1000,// sector_0based NO IMPORTANCE HERE.
				LOG_strings,//TStrings* LOG_strings,
				*Raw_Track_Result,//SSectorInfoInRawTrack16kb &Raw_Track_Result,
				NULL);//pSectorMemory);//BYTE* pSectorMemory); // Where we'll ALSO copy the data from the searched sector.


		// for more extensive testing: saving the raw track:
		// (Can also be used by users who encounter a problem).

		if (save_raw_track_infos)
		{
			//if ( ! raw_track_info_saving_already_done) // With this check, we only keep a trace of analysis, which seems insufficient to me.
			{
				raw_track_info_saving_already_done = true;

				AnsiString name, s;
				{
					time_t tt;
					time(&tt);
					struct tm* t = localtime(&tt);

					if (t != NULL)
					{
						name.printf("%04d-%02d-%02d %02d.%02d.%02d ",
							t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
							t->tm_hour, t->tm_min, t->tm_sec);
					}
				}
				s.printf("Raw track (track %02d, head %d)", Track_0based, Side_0based);
				name = name + s;

				s = name + ".floppy_raw_track";
				HANDLE hf = CreateFile(
					s.c_str(),//LPCTSTR lpFileName,
					GENERIC_WRITE,//DWORD dwDesiredAccess,
					FILE_SHARE_READ,//DWORD dwShareMode,
					NULL,//LPSECURITY_ATTRIBUTES lpSecurityAttributes,
					CREATE_ALWAYS,//DWORD dwCreationDisposition,
					FILE_ATTRIBUTE_NORMAL,//DWORD dwFlagsAndAttributes,
					NULL);//HANDLE hTemplateFile
				if (hf == INVALID_HANDLE_VALUE)
				{
					//DWORD dwerr=GetLastError();
					//asm nop
				}
				else
				{
					DWORD e;
					const BOOL okf = WriteFile(
						hf,//HANDLE hFile,
						Raw_Track_Result->Encoded_Track_Content,//LPCVOID lpBuffer,
						sizeof(Raw_Track_Result->Encoded_Track_Content),//DWORD nNumberOfBytesToWrite,
						&e,//LPDWORD lpNumberOfBytesWritten,
						NULL);//LPOVERLAPPED lpOverlapped
					if (!okf)
					{
						//DWORD dwerr=GetLastError();
						asm nop
					}
					else
					{ // we also save useful information:
						TStringList* l = new TStringList();
						l->Add("Raw track information, created by ST RecoverPlus.");
						s.printf("Track %d - Head %d", Track_0based, Side_0based);
						l->Add(s);
						//s.printf("Looking for sector (base 1): %d .. and cannot find it.",sector_0based+1);
						//l->Add(s);
						//s.printf("Relative beginning of track in octets: %d",-);
						l->Add("List of found sectors:");
						const char tt[2][4] = { "No","Yes" };
						for (unsigned is = 0; is < Raw_Track_Result->Nb_Sectors_found; is++)
						{
							s.printf("Index in raw octets:%d (0x%X) - Identified sector ? %s - ID found ? %s - ID track:%d ID head:%d ID sector_1based:%d ID size bits:%d",
								Raw_Track_Result->Sector_Infos[is].Index_In_Encoded_Track_Content,
								Raw_Track_Result->Sector_Infos[is].Index_In_Encoded_Track_Content,
								tt[Raw_Track_Result->Sector_Infos[is].Sector_Identified & 1],
								tt[Raw_Track_Result->Sector_Infos[is].ID_found_directly & 1],
								Raw_Track_Result->Sector_Infos[is].Track_ID,
								Raw_Track_Result->Sector_Infos[is].Side_ID,
								Raw_Track_Result->Sector_Infos[is].Sector_ID_1based,
								Raw_Track_Result->Sector_Infos[is].Size_ID);
							l->Add(s);
						}

						s = name + " - Informations.txt";
						l->SaveToFile(s);
						delete l;
					}

					CloseHandle(hf);
				} // endif (hf == INVALID_HANDLE_VALUE)
			} // endif ( ! already saved)
		}  // endif save_raw_track_infos
	} // endif if ((! is_sector_read) || (always_save_raw_track && (nb_tries_here==0)))

	nb_tries_here++;

	return OK;
}
//---------------------------------------------------------------------------

BYTE* TTrack::reserve_sector_memory(unsigned sector_0based, unsigned bytes_number)
{
	// We check if this sector already has an assigned memory.
	if (Sectors_content.sectors_memory_index_1based[sector_0based + 1] != 0)
		return &Sectors_content.memory[Sectors_content.sectors_memory_index_1based[sector_0based + 1]];

	// Now we reserve a memory.

	if ((Sectors_content.free_memory_index + bytes_number)
						> sizeof(Sectors_content.memory))
		return NULL; // More enough memory available. Which would be very strange!

	// We avoid starting at the start, otherwise the 1st index will be zero, which would pose a higher problem.
	if (Sectors_content.free_memory_index < 16)
		Sectors_content.free_memory_index = 16;

	if ((sector_0based + 1 + 1)
	> (sizeof(Sectors_content.sectors_memory_index_1based)
		/ sizeof(Sectors_content.sectors_memory_index_1based[0])))
		return NULL; // The more enough indexes available.

	Sectors_content.sectors_memory_index_1based[sector_0based + 1] = Sectors_content.free_memory_index;
	Sectors_content.free_memory_index += bytes_number;

	// Also updates the table of the sectors sectors array.
	SSector* s =
		&Sectors_array_0based[sector_0based];
	s->pContent = &Sectors_content.memory[Sectors_content.sectors_memory_index_1based[sector_0based + 1]];
	s->Byte_size = bytes_number;

	return s->pContent;
}
// ====================================
// ---------------------------------------------------------------------------------

bool	TTrack::Init_and_complete_area_blocks_array(
	TFloppyDisk* floppy_disk, // Calling class.
	std::vector<struct SBlock>*	blocks_array,
	STrackInfo* timer_track,
	TStrings* LOG_strings,
	int track_1_turn_duration) // in microseconds.
{
	// We are looking for empty spaces that can host a sector.

	if (blocks_array == NULL)
		return false;

	bool return_value = false;

	{
		// First we determine the size of the sectors, and their separation.
		int	sectors_size = 512;
		if (timer_track->fdrawcmd_Timed_Scan_Result->count != 0)
			sectors_size = 128 << timer_track->fdrawcmd_Timed_Scan_Result->Headers[0].size;// By default.
		int duration_between_sectors =
			(CLDIS_Minimum_bytes_number_between_sectors + sectors_size)
			* CLDIS_Raw_byte_duration_in_microseconds;// Minimum by default.
		int nb_intersectors = 0;
		int total_duration_between_sectors = 0;
		bool	sectors_of_uniform_size;
		{
			int tmini = 1000000;
			int tmaxi = -1;
			int previous_sector_time = 0;
			for (int i_timer_sector = 0; i_timer_sector < timer_track->fdrawcmd_Timed_Scan_Result->count; i_timer_sector++)
			{
				const int t =
					128 << timer_track->fdrawcmd_Timed_Scan_Result->Headers[i_timer_sector].size;
				if (t > tmaxi)
					tmaxi = t;
				if (t < tmini)
					tmini = t;
				int i = timer_track->fdrawcmd_Timed_Scan_Result->Headers[i_timer_sector].reltime;
				if (((i - previous_sector_time) > (CLDIS_Minimum_bytes_number_between_sectors*CLDIS_Raw_byte_duration_in_microseconds))
					&& ((i - previous_sector_time) <= (((t * 3) / 2 + CLDIS_Minimum_bytes_number_between_sectors)*CLDIS_Raw_byte_duration_in_microseconds))
					&& (i_timer_sector >= 2))
				{
					total_duration_between_sectors +=
						i - previous_sector_time - (t*CLDIS_Raw_byte_duration_in_microseconds);
					nb_intersectors++;
				}

				previous_sector_time = i;
			} // next i_timer_sector
			sectors_of_uniform_size = (tmini == tmaxi);
			sectors_size = tmini;
			if (nb_intersectors != 0)
				duration_between_sectors = total_duration_between_sectors / nb_intersectors;
		}
		//if (sectors_of_uniform_size)
		{			// This algo can only be applied if the sectors are all of the same size.
					// And also that we found at least 1 sector.
			int previous_sector_end_time = 0;

			for (int i_timer_sector = 0; i_timer_sector < timer_track->fdrawcmd_Timed_Scan_Result->count; i_timer_sector++)
			{
				SBlock bl;
				bl.start_time = previous_sector_end_time;
				bl.free_space_duration_in_1st =
					timer_track->fdrawcmd_Timed_Scan_Result->Headers[i_timer_sector].reltime
					- bl.start_time;
				bl.sector_number_1based_in_2nd =
					timer_track->fdrawcmd_Timed_Scan_Result->Headers[i_timer_sector].sector;
				bl.sector_size_in_bytes_bit_code =
					timer_track->fdrawcmd_Timed_Scan_Result->Headers[i_timer_sector].size;
				bl.found_by_controller = true;

				blocks_array->push_back(bl);
				previous_sector_end_time =
					timer_track->fdrawcmd_Timed_Scan_Result->Headers[i_timer_sector].reltime
					+ (128 << timer_track->fdrawcmd_Timed_Scan_Result->Headers[i_timer_sector].size)
					* CLDIS_Raw_byte_duration_in_microseconds; //sector_duration;
			// We take the opportunity to calculate the average of normal spaces between sectors.
			// We obviously do not count the dismissed spaces by the not detected sectors.
			/*int intersector=bl.free_space_duration_in_1st;
			if ((intersector>=minimum_duration_betweeb_sectors)
				&& (intersector<=maximum_duration_betweeb_sectors))
			{
				//duration_between_sectors += intersector;
				nb_normal_intersectors++;
			} */
			}

			const int sector_duration = sectors_size * CLDIS_Raw_byte_duration_in_microseconds;
			const int block_duration = sector_duration + duration_between_sectors;
			const int mini_block_duration = block_duration - 12/*NB_MAX_SECTORS_PER_TRACK*/ * CLDIS_Raw_byte_duration_in_microseconds; // normalement, l'espacement est très régulier.

			{    // The final space can be well contained well of the sectors not yet identified.
				const int time_remaining_after_last_identified_sector =
					track_1_turn_duration - previous_sector_end_time;
				if (time_remaining_after_last_identified_sector >= mini_block_duration)
				{
					// A block without a sector is created.
					SBlock bl;
					bl.start_time = previous_sector_end_time;
					bl.free_space_duration_in_1st =
						track_1_turn_duration - bl.start_time;
					bl.sector_number_1based_in_2nd = 0;
					bl.sector_size_in_bytes_bit_code = 0;
					bl.found_by_controller = false;

					blocks_array->push_back(bl);
				}
			}

			if (sectors_of_uniform_size)// && (nb_normal_intersectors != 0))
			{			// The final space can well contain the sectors not yet identified.
						// And also that we found at least 1 sector (by controller).
				// Next step: we fill the gaps with sectors.
				bool sector_found_by_controller[CP_NB_MAX_SECTORS_PER_RAW_TRACK + 2];
				memset(sector_found_by_controller, false, sizeof(sector_found_by_controller));
				int nb_new_sectors = 0;
				// Here, we add sectors where there is space, creating new "blocks".
				for (int izone = 0; izone < (int)(blocks_array->size()); izone++)
				{
					const int espace = (*blocks_array)[izone].free_space_duration_in_1st;
					// The 1st sector of the track has a short space before it, this is normal.
					const int track_start_addition = (izone == 0)
						? (44 - 16)*CLDIS_Raw_byte_duration_in_microseconds
						: 0;
					const int nb_sector_to_insert =
						(espace/*-duration_between_sectors*/ + track_start_addition)
						/ mini_block_duration;

					// We take this opportunity to list the sectors found by the controller.
					{
						const unsigned is = (*blocks_array)[izone].sector_number_1based_in_2nd;
						if ((is != 0) && (is <= CP_NB_MAX_SECTORS_PER_RAW_TRACK))
							sector_found_by_controller[(*blocks_array)[izone].sector_number_1based_in_2nd] = true;
					}

					if (nb_sector_to_insert > 0)
					{ // TODO: Here, it would be necessary to take into account "track_start_addition"
						int instant = (*blocks_array)[izone].start_time;

						// First update the data of the discovered sector.
						{
							const int space_decrease =
								(duration_between_sectors/*interespaces*/ + sector_duration)*nb_sector_to_insert;
							(*blocks_array)[izone].start_time
								+= space_decrease;
							(*blocks_array)[izone].free_space_duration_in_1st
								-= space_decrease;
						}
						// Then fill in the free space.
						for (int iblockinsert = 0; iblockinsert < nb_sector_to_insert; iblockinsert++)
						{
							// A sector is created at the beginning of the free space.
							SBlock bl;
							bl.start_time = instant;
							bl.free_space_duration_in_1st = duration_between_sectors;//interespaces;
							bl.sector_number_1based_in_2nd = 0; // Its number does not exist yet.
							bl.sector_size_in_bytes_bit_code =
								(*blocks_array)[izone].sector_size_in_bytes_bit_code; // Identical since all sectors have the same size.
							bl.found_by_controller = false;
							instant += duration_between_sectors/*interespaces*/ + sector_duration;
							blocks_array->insert(blocks_array->begin() + izone + iblockinsert, bl);
							nb_new_sectors++;
						}
						izone += nb_sector_to_insert; // No need to test the new blocks.
					}
				}  // next izone
				/* Deletes the last zone, if it does not have room to contain
						at least one sector. */
				if (blocks_array->size() != 0)
				{
					int ilast = blocks_array->size() - 1;
					if ((*blocks_array)[ilast].sector_size_in_bytes_bit_code == 0)
						blocks_array->pop_back(); // Eliminates the last.
				}

				// Final step: we number the added sectors.
				// For that, we determine the general order.
				if (nb_new_sectors == 1)
				{
					// Here, I apply the simplest method:
					//  Find the only missing sector.
					int missing_sector_number_1based = 0;
					for (int is = 1; is < (int)(sizeof(sector_found_by_controller) / sizeof(sector_found_by_controller[0])); is++)
					{
						if (!sector_found_by_controller[is])
						{
							missing_sector_number_1based = is;
							for (int izone = 0; (unsigned)izone < blocks_array->size(); izone++)
							{
								if (!(*blocks_array)[izone].found_by_controller)
								{
									(*blocks_array)[izone].sector_number_1based_in_2nd
										= missing_sector_number_1based;
									break;
								}
							}
							break;
						}
					}
				}
				else
				{
					if (nb_new_sectors != 0)  // Here we analyze the order of the sectors.
					{
						//LOG_strings->Add("Warning: more than one (1) sector discovered by raw track read.\nWe need more analysis (not developed now). Please ask to the author ;) Check 'Save track information (debug)', and send the track information files to me.");

						/* 1) Determines the increment of sector numbers
									(not necessarily "1", because of the interleaving of sectors,
									for example for the tracks of 11 sectors, to speed up reading.*/
						bool found_increment = false;
						int increment = 1;
						const int _NbSectorsPerTrack = floppy_disk->NbSectorsPerTrack;
						for (int izone = 1; izone < (int)blocks_array->size(); izone++)
						{
							if ((*blocks_array)[izone].sector_number_1based_in_2nd != 0)
								if ((*blocks_array)[izone - 1].sector_number_1based_in_2nd != 0)
								{
									increment =
										(((*blocks_array)[izone].sector_number_1based_in_2nd
											- (*blocks_array)[izone - 1].sector_number_1based_in_2nd)
											+ _NbSectorsPerTrack)
										% _NbSectorsPerTrack;
									found_increment = true;
									break;
								}
						}

						// 2) Prepare a list of sectors, numbering those that are missing.
						if (found_increment)
						{
							int sectors_numbers_1based[CP_NB_MAX_SECTORS_PER_RAW_TRACK + 2];
							memset(sectors_numbers_1based, 0, sizeof(sectors_numbers_1based));
							// First from the end towards the start, to ensure the number of the 1st sector of the track.
							int n_previous = 0;
							for (int izone = blocks_array->size() - 1; izone >= 0; izone--)
							{
								int n = (*blocks_array)[izone].sector_number_1based_in_2nd;
								if ((n == 0) && (n_previous != 0))
								{
									n =
										(((n_previous - 1 - increment) + _NbSectorsPerTrack)
											% _NbSectorsPerTrack)
										+ 1;// "+1" since 1-based.
								}
								sectors_numbers_1based[izone] = n;
								n_previous = n;
							}
							// Then from start to finish, to number all sectors until the end.
							n_previous = 0;
							for (int izone = 0; izone < (int)blocks_array->size(); izone++)
							{
								int n = sectors_numbers_1based[izone];
								if ((n == 0) && (n_previous != 0))
								{
									n =
										(((n_previous - 1 + increment) + _NbSectorsPerTrack)
											% _NbSectorsPerTrack)
										+ 1;// "+1" since 1-based.
									sectors_numbers_1based[izone] = n;
								}
								n_previous = n;
							}

							/* 3) We build a table of sectors in number order,
										to see if all are only present once,
										to see if my increasing is valid.*/
							int number_of_appearing_sectors_1based[CP_NB_MAX_SECTORS_PER_RAW_TRACK + 2];
							memset(number_of_appearing_sectors_1based, 0, sizeof(number_of_appearing_sectors_1based));
							for (int izone = 0; izone < (int)blocks_array->size(); izone++)
							{
								int n = sectors_numbers_1based[izone];
								if (n < CP_NB_MAX_SECTORS_PER_RAW_TRACK)
								{
									number_of_appearing_sectors_1based[n]++;
								}
							}
							// Each sector number must be present once and only one.
							bool increment_valid = true;
							if (number_of_appearing_sectors_1based[0] != 0) // Special, because normally no sector with number 0 (since 1-based).
								increment_valid = false;
							else
								for (int izone = 1; izone < (int)blocks_array->size(); izone++)
								{
									if (number_of_appearing_sectors_1based[izone] != 1)
									{
										increment_valid = false;
										LOG_strings->Add("Warning: more than one (1) sector discovered by raw track read, AND the increment is not regular (not even regular interlaced sectors).\n We need more analysis (not developed now). Please ask to the author ;) Check 'Save track information (debug)', and send the track information files to me.");
										break;
									}
								}

							// 4) If the increment is good, it is applied.
							if (increment_valid)
							{
								for (int izone = 0; izone < (int)blocks_array->size(); izone++)
								{
									if ((*blocks_array)[izone].sector_number_1based_in_2nd == 0)
										(*blocks_array)[izone].sector_number_1based_in_2nd =
										sectors_numbers_1based[izone];
								}
							}
						}
					}
				}
				return_value = true;
			}
		}

	}
	return return_value;
}
// ====================================
bool TTrack::ReadRawTrackSectors( // Reads the currently selected track.
	class TFloppyDisk* floppy_disk, // Calling class.
	SSectorInfoInRawTrack16kb* Result)
{
	if (Result == NULL)
	{
		return false;
	}

/*
	FD_READ_WRITE_PARAMS rwp =
	{ FD_OPTION_MFM,
	floppy_disk->direct_infos.Selected_Side,
	floppy_disk->direct_infos.Selected_Track,
	floppy_disk->direct_infos.Selected_Side,
	1,//start_
	7,//size_ :  127<<7 = 16384 bytes.
	255,// eot_, // or 1 ?
	1,  // gap
	0xff };  // datalen
	DWORD dwRet;
	bool OK =
		DeviceIoControl(
			floppy_disk->hDevice, IOCTL_FDCMD_READ_TRACK, &rwp, sizeof(rwp),
			&Result->Encoded_Track_Content,//		 pv_,
			16384,//uLength_,
			&dwRet, NULL);
*/
	bool OK = FdCmdReadTrack(floppy_disk->hDevice, FD_OPTION_MFM,
		floppy_disk->direct_infos.Selected_Side,
		floppy_disk->direct_infos.Selected_Track,
		floppy_disk->direct_infos.Selected_Side,
		1,//start_
		7,//size_ :  127<<7 = 16384 bytes.
		255,// eot_, // or 1 ?
		Result->Encoded_Track_Content, 16384);

	if (OK)
	{
		OK &= DecodeTrack(Result);
	}


	return OK;
}
// ====================================
// Checksum a data block
WORD CrcBlock(const void* pcv_, size_t uLen_, WORD wCRC_ = 0xffff)
{
	static WORD awCRC[256];

	// Build the block if not already built
	if (!awCRC[1])
	{
		for (int i = 0; i < 256; i++)
		{
			WORD w = i << 8;

			// 8 shifts, for each bit in the update byte
			for (int j = 0; j < 8; j++)
				w = (w << 1) ^ ((w & 0x8000) ? 0x1021 : 0);

			awCRC[i] = w;
		}
	}

	// Update the CRC with each byte in the block
	const BYTE* pb = reinterpret_cast<const BYTE*>(pcv_);
	while (uLen_--)
		wCRC_ = (wCRC_ << 8) ^ awCRC[((wCRC_ >> 8) ^ *pb++) & 0xff];

	return wCRC_;
}
// ====================================
// Decode a bit-shifted run of bytes
void DecodeRun(BYTE *pb_, int nShift_, BYTE* pbOut_, int nLen_)
{
	for (; nLen_--; pb_++)
		*pbOut_++ = (pb_[0] << nShift_) | (pb_[1] >> (8 - nShift_));
}
// ====================================
bool TTrack::DecodeTrack(SSectorInfoInRawTrack16kb* Result)
{
	// Here, we have the 16 KB track loaded, and we must fill in the structure.
	const int nLen_ = 16384;
	BYTE* pb_ = &Result->Encoded_Track_Content[0];
	bool OK = false;

	int nTrack = 0, nSide = 0, nBits_size = 2;
	unsigned	free_mem_index = 0;

	int nShift, nHeader = 0;
	int nSector = 0, nSize = 512; // Default sector size, in case no sector ID is found.

	for (int nPos = 0; nPos < nLen_ - 10; nPos++)
	{
		// Check all 8 shift positions for the start of an address mark
		for (nShift = 0; nShift < 8; nShift++)
		{
			BYTE b = (pb_[nPos] << nShift) | (pb_[nPos + 1] >> (8 - nShift));
			if (b == 0xa1)
				break;
		}

		// Move to next byte?
		if (nShift >= 8)
			continue;

		// Decode enough for an ID header (A1 A1 A1 idam cyl head sector size crchigh crclow)
		BYTE ab[10];
		DecodeRun(pb_ + nPos, nShift, ab, sizeof(ab));

		// Check for start of address mark
		if (ab[0] == 0xa1 && ab[1] == 0xa1 && ab[2] == 0xa1)
		{
			if (ab[3] == 0xfe)	// id address mark
			{
				WORD wCrc = CrcBlock(ab, sizeof(ab) - 2), wDiskCrc = (ab[8] << 8) | ab[9];

				if (wDiskCrc == wCrc)
				{
					nTrack = ab[4];
					nSide = ab[5];
					nSector = ab[6];
					nBits_size = ab[7]; // 0=128; 1=256; 2=512; etc..
					nSize = 128 << (ab[7] & 3);
					nHeader = nPos;
				}
			}
			else if (ab[3] == 0xfb || ab[3] == 0xf8)	// normal/deleted data address marks
			{
				if (nPos + 4 + nSize + 2 < nLen_)	// enough data available?
				{
					BYTE ab[4 + 4096 + 2];
					DecodeRun(pb_ + nPos, nShift, ab, 4 + nSize + 2);
					WORD wCrc = CrcBlock(ab, 4 + nSize), wDiskCrc = (ab[4 + nSize] << 8) | ab[4 + nSize + 1];

					// We found a valid and verified sector.
					if (wDiskCrc == wCrc)
					{
						// If the data is too far from the last header, it lacks a header
						const bool id_found = (nPos - nHeader <= 50);

						{
							const unsigned isect = Result->Nb_Sectors_found;

							//Result->Sector_Infos[isect].Well_read_data=true;
							Result->Sector_Infos[isect].Index_In_Encoded_Track_Content
								= nPos;
							Result->Sector_Infos[isect].Index_In_Decoded_Track_Content
								= free_mem_index;
							Result->Sector_Infos[isect].ID_found_directly = id_found;
							Result->Sector_Infos[isect].Sector_Identified = id_found;
							if (id_found)
							{
								Result->Sector_Infos[isect].Track_ID = nTrack;
								Result->Sector_Infos[isect].Side_ID = nSide;
								Result->Sector_Infos[isect].Sector_ID_1based = nSector;
								Result->Sector_Infos[isect].Size_ID = nBits_size;
							}
							else
								asm nop
								memcpy(
									&Result->Decoded_Track_Content[free_mem_index],
									&ab[4],
									nSize);
							free_mem_index += nSize;
							Result->Nb_Sectors_found++;
							OK = true;
						}
					}

				}
			}
			else
				asm nop
		}
	}

	return OK;
}
// ====================================
STrackInfo*	TTrack::CP_Analyse_Sectors_Time( // Analyzes the track, and provides a time map of the sectors.
	class TFloppyDisk* floppy_disk, // Calling class.
	unsigned track,
	unsigned side)
{
	static STrackInfo returned_infos;
	//
	memset(&returned_infos, 0, sizeof(returned_infos));
	returned_infos.OperationSuccess = false; // By default.
	if (!floppy_disk->fdrawcmd_sys_installed)
		return &returned_infos;


	bool OK = false;

	DWORD dwRet;
	{
		// With this driver (fdrawcmd.sys), we are forced to use another method.
		FD_SEEK_PARAMS seekp;

		// details of seek location
		seekp.cyl = track;
		seekp.head = side;

		OK = CP_select_track_and_side(floppy_disk, Track_0based, Side_0based);
		if (OK)
		{
			floppy_disk->direct_infos.Selected_Track = track; // The head is currently placed here.
			floppy_disk->direct_infos.Selected_Side = side; // This side is currently selected.
		}

	}

	// set up scan parameters
	FD_SCAN_PARAMS sp;
	sp.flags = FD_OPTION_MFM;
	sp.head = side;


	if (OK)
	{				// Reads the description of the sectors, in order, with their time.
		static struct FD_TIMED_SCAN_RESULT_32 tsr;
		returned_infos.fdrawcmd_Timed_Scan_Result = &tsr;

		// seek and scan track
//		OK &= DeviceIoControl(floppy_disk->hDevice, IOCTL_FD_TIMED_SCAN_TRACK, &sp, sizeof(sp), &tsr, sizeof(tsr), &dwRet, NULL);
		OK &= FdCmdTimedScan(floppy_disk->hDevice, FD_OPTION_MFM, side,
			reinterpret_cast<FD_TIMED_SCAN_RESULT*>(&tsr), sizeof(tsr));
	}

	returned_infos.OperationSuccess = OK;
	return &returned_infos;
}
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

