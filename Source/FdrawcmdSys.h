#ifndef FdrawcmdSysH

#define FdrawcmdSysH 1
//---------------------------------------------------------------------------

#include <windows.h>
#include <Classes.hpp>

#include "fdrawcmd.h"

int LengthToSizeCode(int sizeCode);

void SetFdCmdLogger(TStrings* LOG_strings);

bool FdCmdSetEncRate(HANDLE device, bool MFM, int datarate);

bool FdCmdRecalibrate(HANDLE device);

bool FdCmdSeek(HANDLE device, int cyl, int head = -1);

bool FdCmdReadTrack(HANDLE device, int flags, int phead, int cyl, int head,
	int sector, int size, int eot, void* buffer, int bufferLen);

bool FdCmdRead(HANDLE device, int flags, int phead, int cyl, int head,
	int sector, int size, int count,
	void* buffer, size_t data_offset = 0, bool deleted = false);

bool FdCmdTimedScan(HANDLE device, int flags, int head, FD_TIMED_SCAN_RESULT* timed_scan, int size);

bool FdCmdCheckDisk(HANDLE device);

bool FdCmdReset(HANDLE device);

#endif // FdrawcmdSysH

