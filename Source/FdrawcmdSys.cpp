#include "FdrawcmdSys.h"

#include <math.h>
#include <SysUtils.hpp>

static const int RW_GAP = 0x0a;

double log2(double x)
{
	return log(x) / log(2);
}

//////////

int LengthToSizeCode(int sizeCode)
{
	return log2(sizeCode / 128);
}

// Map a size code to how it's treated by the uPD765 FDC on the PC
int SizeCodeToRealSizeCode(int size)
{
	// Sizes above 8 are treated as 8 (32K)
	return (size <= 7) ? size : 8;
}

// Return the sector length for a given sector size code
int SizeCodeToLength(int size)
{
	// 2 ^ (7 + size)
	return 128 << SizeCodeToRealSizeCode(size);
}

//////////

BYTE DtlFromSize(int size)
{
    // Data length used only for 128-byte sectors.
    return (size == 0) ? 0x80 : 0xff;
}

static TStrings* logStrings = NULL;

void SetFdCmdLogger(TStrings* LOG_strings)
{
	logStrings = LOG_strings;
}

//////////

bool FdCmdSetEncRate(HANDLE device, bool MFM, int datarate)
{
	// Set perpendicular mode and write-enable for 1M data rate
	FD_PERPENDICULAR_PARAMS pp;
	pp.ow_ds_gap_wgate = (datarate == FD_RATE_1M) ? 0xbc : 0x00;
	DWORD dwRet;
	DeviceIoControl( // Not in ST_RecoverPlus.
		device, IOCTL_FDCMD_PERPENDICULAR_MODE, &pp, sizeof(pp),
		NULL, 0, &dwRet, NULL);

	if (datarate < FD_RATE_500K || datarate > FD_RATE_1M)
		throw Exception(AnsiString("unsupported datarate (")
				+ AnsiString(datarate) + ")");

	BYTE rate = datarate;
	bool OK = DeviceIoControl(
		device, IOCTL_FD_SET_DATA_RATE, &rate, sizeof(rate),
		NULL, 0, &dwRet, NULL);
	logStrings->Add(AnsiString().sprintf(
		"FdCmdSetEncRate: success=%u, ret=%u, MFM=%u, datarate=%d",
		OK, dwRet, MFM, datarate));
	return OK;
}

bool FdCmdRecalibrate(HANDLE device)
{
	// ToDo: should we check TRACK0 and retry if not signalled?
	DWORD dwRet;
	bool OK = DeviceIoControl(
		device, IOCTL_FDCMD_RECALIBRATE, NULL, 0,
		NULL, 0, &dwRet, NULL);
	logStrings->Add(AnsiString().sprintf(
		"FdCmdRecalibrate: success=%u, ret=%u",
		OK, dwRet));
	return OK;
}

bool FdCmdSeek(HANDLE device, int cyl, int head /*= -1*/)
{
	if (cyl == 0) // Not in ST_RecoverPlus.
	{
		bool OK = FdCmdRecalibrate(device);
		logStrings->Add(AnsiString().sprintf(
			"FdCmdSeek(recalibrate): success=%u, cyl=%d, head=%d",
			OK, cyl, head));
		return OK;
	}

	FD_SEEK_PARAMS sp;
	sp.cyl = static_cast<BYTE>(cyl);
	int sp_size = sizeof(sp);
	if (head >= 0)
	{
		if (head < 0 || head > 1)
			throw Exception(AnsiString("unsupported head (")
				+ AnsiString(head) + ")");
		sp.head = static_cast<BYTE>(head);
	}
	else
		sp_size -= sizeof(sp.head);

	DWORD dwRet;
	bool OK = DeviceIoControl(
		device, IOCTL_FDCMD_SEEK, &sp, sp_size,
		NULL, 0, &dwRet, NULL);
	logStrings->Add(AnsiString().sprintf(
		"FdCmdSeek: success=%u, ret=%u, cyl=%d, head=%d",
		OK, dwRet, cyl, head));
	return OK;
}

bool FdCmdReadTrack(HANDLE device, int flags, int phead, int cyl, int head,
	int sector, int size, int eot, void* buffer, int bufferLen)
{
	FD_READ_WRITE_PARAMS rwp;
	rwp.flags = static_cast<BYTE>(flags);
	rwp.phead = static_cast<BYTE>(phead);
	rwp.cyl = static_cast<BYTE>(cyl);
	rwp.head = static_cast<BYTE>(head);
	rwp.sector = static_cast<BYTE>(sector);
	rwp.size = static_cast<BYTE>(size);
	rwp.eot = static_cast<BYTE>(eot);
	rwp.gap = RW_GAP;
	rwp.datalen = 0xff;

	DWORD dwRet;
	bool OK = DeviceIoControl(
		device, IOCTL_FDCMD_READ_TRACK, &rwp, sizeof(rwp),
		buffer, bufferLen, &dwRet, NULL);
	logStrings->Add(AnsiString().sprintf(
		"FdCmdReadTrack: success=%u, ret=%u, flags=%d, phead=%d, cyl=%d, head=%d, sector=%d, size=%d, eot=%d, (gap=%u), bufferlen=%d",
		OK, dwRet, flags, phead, cyl, head, sector, size, eot, rwp.gap, bufferLen));
	return OK;
}

bool FdCmdRead(HANDLE device, int flags, int phead, int cyl, int head,
	int sector, int size, int count,
	void* buffer, size_t data_offset /*= 0*/, bool deleted /*= false*/)
{
	FD_READ_WRITE_PARAMS rwp;
	rwp.flags = static_cast<BYTE>(flags);
	rwp.phead = static_cast<BYTE>(phead);
	rwp.cyl = static_cast<BYTE>(cyl);
	rwp.head = static_cast<BYTE>(head);
	rwp.sector = static_cast<BYTE>(sector);
	rwp.size = static_cast<BYTE>(size);
	rwp.eot = static_cast<BYTE>(sector + count);
	rwp.gap = RW_GAP;
	rwp.datalen = DtlFromSize(size); // 0xff in ST_RevocerPlus.

	DWORD dwRet;
	bool OK = DeviceIoControl(
		device, IOCTL_FDCMD_READ_DATA, &rwp, sizeof(rwp),
		static_cast<BYTE*>(buffer) + data_offset, count * SizeCodeToLength(size), &dwRet, NULL);
	logStrings->Add(AnsiString().sprintf(
		"FdCmdRead: success=%u, ret=%u, flags=%d, phead=%d, cyl=%d, head=%d, sector=%d, size=%d, count=%d, (gap=%u), deleted=%u",
		OK, dwRet, flags, phead, cyl, head, sector, size, count, rwp.gap, deleted));
	return OK;
}

bool FdCmdTimedScan(HANDLE device, int flags, int head, FD_TIMED_SCAN_RESULT* timed_scan, int size)
{
	FD_SCAN_PARAMS sp;
	sp.flags = static_cast<BYTE>(flags);
	sp.head = static_cast<BYTE>(head);

	DWORD dwRet;
	bool OK = DeviceIoControl(
		device, IOCTL_FD_TIMED_SCAN_TRACK, &sp, sizeof(sp),
		timed_scan, size, &dwRet, NULL);
	logStrings->Add(AnsiString().sprintf(
		"FdCmdTimedScan: success=%u, ret=%u, flags=%d, head=%d",
		OK, dwRet, flags, head));
	return OK;
}

bool FdCmdCheckDisk(HANDLE device)
{
	DWORD dwRet;
	bool OK = DeviceIoControl(
		device, IOCTL_FD_CHECK_DISK, NULL, 0,
		NULL, 0, &dwRet, NULL);
	logStrings->Add(AnsiString().sprintf("FdCmdCheckDisk: success=%u, ret=%u",
		OK, dwRet));
	return OK;
}

bool FdCmdReset(HANDLE device)
{
	DWORD dwRet;
	bool OK = DeviceIoControl(
		device, IOCTL_FD_RESET, NULL, 0,
		NULL, 0, &dwRet, NULL);
	logStrings->Add(AnsiString().sprintf("FdCmdReset: success=%u, ret=%u",
		OK, dwRet));
	return OK;
}

