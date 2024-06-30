//---------------------------------------------------------------------------
// Copyright Christophe Bertrand.
// This file is licensed under the Microsoft Reciprocal License (Ms-RL). You can find a copy of this license in the file "Ms-RL License.htm", in the root directory of the distribution archive.
//---------------------------------------------------------------------------



#pragma hdrstop

#include "FloppyDisk.h"
#include "FdrawcmdSys.h"
#include "Constants.h"


//---------------------------------------------------------------------------

#pragma package(smart_init)

#define min(a,b) ((a)<=(b)) ? a : b




__fastcall TFloppyDisk::TFloppyDisk(void)
{
	KnownDiskArchitecture = false;
	hDevice = INVALID_HANDLE_VALUE;
	Win9X = false;
	fdrawcmd_sys_installed = false;
	current_Track = NULL;
}
//---------------------------------------------------------------------------
bool		TFloppyDisk::OpenFloppyDisk(unsigned floppy_drive, // floppy_drive: 0=A , 1=B , etc..
	DWORD allowed_time_ms, TStrings* LOG_strings,
	volatile bool*	p_canceller, // If this variable becomes true, the current operation is cancelled.
	bool save_raw_track_infos)
{
	SetFdCmdLogger(LOG_strings);

	KnownDiskArchitecture = false;
	const DWORD time_limit = GetTickCount() + allowed_time_ms;

	SelectedFloppyDrive = floppy_drive;

	// Default values:
	NbSectorsPerTrack = NB_MAX_SECTORS_PER_TRACK;
	NbTracks = NB_MAX_TRACKS;
	NbSides = 2;
	NbBytesPerSector = 512;

	memset(&SectorsSideA, 0, sizeof(SectorsSideA));
	memset(&SectorsSideB, 0, sizeof(SectorsSideB));

	// Creating handle to vwin32.vxd (win 9x)
	hDevice = CreateFile("\\\\.\\vwin32",
		0,
		0,
		NULL,
		0,
		FILE_FLAG_DELETE_ON_CLOSE,
		NULL);

	Win9X = hDevice != INVALID_HANDLE_VALUE;

	if (!Win9X)
	{
		// win NT/2K/XP code
		{
			// Try to access the extended driver "fdrawcmd.sys".
			// See http://simonowen.com/fdrawcmd/
			char szDev[] = "\\\\.\\fdraw0"; // 0=A:  1=B:
			szDev[9] += floppy_drive;
			hDevice = CreateFile(szDev, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
			fdrawcmd_sys_installed = ((hDevice != NULL) && (hDevice != INVALID_HANDLE_VALUE));
		}

		if (!fdrawcmd_sys_installed)
		{
			// We are satisfied with the normal Windows driver, very limited.
			char _devicename[] = "\\\\.\\A:";
			_devicename[4] += floppy_drive;

			// Creating a handle to disk drive using CreateFile () function ..
			hDevice = CreateFile(_devicename,
				GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
				NULL, OPEN_EXISTING, 0, NULL);
		}
	}

	bool	OK = ((hDevice != NULL) && (hDevice != INVALID_HANDLE_VALUE));

	if (OK)
	{
		// We will test if there is really a disk in the drive.
		// Otherwise, we are entitled to rapid, empty head movements.
		if (fdrawcmd_sys_installed)
		{
			// With this driver (fdrawcmd.sys), we are forced to use another method.
			DWORD dwRet;
			// We better reset the floppy disk controller.
//			OK = DeviceIoControl(hDevice, IOCTL_FD_RESET, NULL, NULL, NULL, 0, &dwRet, NULL);
			OK = FdCmdReset(hDevice);
			direct_infos.Selected_Track = 0; // The head is currently placed here.
			direct_infos.Selected_Side = 0; // This side is currently selected.
			direct_infos.Sector_under_treatment_0based = 0; // à priori.

				  // Checks whether a disk is present in the drive.
//			OK &= DeviceIoControl(hDevice, IOCTL_FD_CHECK_DISK, NULL, NULL, NULL, 0, &dwRet, NULL);
			OK &= FdCmdCheckDisk(hDevice);

			// Recalibrate the track.
//			OK &= DeviceIoControl(hDevice, IOCTL_FDCMD_RECALIBRATE, NULL, 0, NULL, 0, &dwRet, NULL);
			OK &= FdCmdRecalibrate(hDevice);

			// Set data rate to double-density.
			BYTE datarate;
			datarate = FD_RATE_250K;
//			OK &= DeviceIoControl(hDevice, IOCTL_FD_SET_DATA_RATE, &datarate, sizeof(datarate), NULL, 0, &dwRet, NULL);
			OK &= FdCmdSetEncRate(hDevice, true, datarate);
		}
	}

	if (OK)
	{
		SSector* p_sector_infos = NULL;
		if (
			CD_ReadSector(0, 0, 0,	 // All arguments are 0-based.
				time_limit - GetTickCount(), LOG_strings, &p_sector_infos, p_canceller,
				save_raw_track_infos))
		{
			Atari_ST_Boot_Sector.Bytes_per_sector = // I already had a value of "2" here.
				Atari_ST_Boot_Sector.Bytes_per_sector >= 128
				? Atari_ST_Boot_Sector.Bytes_per_sector
				: 512;

			if (Atari_ST_Boot_Sector.Nb_sectors_per_track == 0) {
				OK = false; // Problem reading the start-up sector (it sometimes happens).
			}

			if (OK)
			{
				NbSectorsPerTrack = Atari_ST_Boot_Sector.Nb_sectors_per_track;
				NbTracks =
					Atari_ST_Boot_Sector.Nb_sectors_on_floppy_disk
					/ Atari_ST_Boot_Sector.Nb_sectors_per_track
					/ Atari_ST_Boot_Sector.Nb_sides;
				NbSides = Atari_ST_Boot_Sector.Nb_sides;
				NbBytesPerSector = Atari_ST_Boot_Sector.Bytes_per_sector;
				KnownDiskArchitecture = true;

				if (NbTracks > NB_MAX_TRACKS)
					NbTracks = NB_MAX_TRACKS; // sécurité
				if (NbSectorsPerTrack > NB_MAX_SECTORS_PER_TRACK)
					NbSectorsPerTrack = NB_MAX_SECTORS_PER_TRACK; // sécurité
			}
		}
	}

	if (OK)
		LOG_strings->Add("Disk open correctly");
	else
		LOG_strings->Add("Warning: Disk could NOT be open, or maybe the information in the boot sector are not adequate");

	return OK;
}
// ====================================
bool	TFloppyDisk::CD_ReadSector(
	unsigned track, // All arguments are 0-based.
	unsigned side,
	unsigned sector_0based,
	DWORD allowed_time_ms,
	TStrings* LOG_strings,
	SSector** pp_sector, //To receive an "SSector*".
	volatile bool*	p_canceller, // If this variable becomes true, the current operation is cancelled.
	bool save_raw_track_infos)
{
	if (*p_canceller)
		return false;

	init_current_track(track, side);

	if (current_Track == NULL)
		return false; // error.
	// -------------------------------

	direct_infos.Sector_under_treatment_0based = sector_0based; // True whether we manage to read it or not.

	SSector* p_s_sector = NULL;
	const bool	OK = current_Track->CP_ReadSector(
		this,//TFloppyDisk* floppy_disk, // Calling class.		track,//unsigned track, // All arguments are 0-based.
		track,
		side,//unsigned side,
		sector_0based,//unsigned sector_0based,
		allowed_time_ms,//DWORD allowed_time_ms,
		&p_s_sector,//SSector** p_p_s_sector) // Writes a pointer to a class in the provided memory.
		LOG_strings,//TStrings* LOG_strings);
		p_canceller, // If this variable becomes true, the current operation is cancelled.
		save_raw_track_infos);
	*pp_sector = p_s_sector;
	return OK;
}
// ====================================
bool		TFloppyDisk::CloseFloppyDisk(void)
{
	if (current_Track != NULL)
	{
		delete current_Track;
		current_Track = NULL;
	}

	bool OK = false;
	if ((hDevice != NULL) && (hDevice != INVALID_HANDLE_VALUE))
	{
		OK = CloseHandle(hDevice);
		if (OK)
		{
			hDevice = NULL;
		}
	}

	return OK;
}
// ====================================
__fastcall TFloppyDisk::~TFloppyDisk()
{
	TFloppyDisk::CloseFloppyDisk();
}
// ====================================
STrackInfo*	TFloppyDisk::CD_AnalyseSectorsTime(
	unsigned track,
	unsigned side)//,    // All arguments are 0-based.
{
	init_current_track(track, side);

	if (current_Track == NULL)
		return NULL; // error.
	// -------------------------------

	return	current_Track->CP_Analyse_Sectors_Time(
		this,//class TFloppyDisk* floppy_disk, // Calling class.
		track,//unsigned track,
		side);//unsigned side);//,    // All arguments are 0-based.
}
// ====================================

	// -------------------------------
	// On utilise la TTrack.
bool	TFloppyDisk::init_current_track(unsigned track, unsigned side)
{
	{
		bool must_create_class = false;
		if (current_Track == NULL)
			must_create_class = true;
		else
			if (
				(current_Track->get_track_number_0based() != (int)track)
				|| (current_Track->get_side_number_0based() != (int)side))
			{
				delete current_Track; // Only place where we destroy this class.
				current_Track = NULL;
				must_create_class = true;
			}
		if (must_create_class)
			current_Track = new TTrack(track, side);
	}
	return current_Track != NULL;
}
// ====================================
