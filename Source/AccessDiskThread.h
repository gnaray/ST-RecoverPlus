//---------------------------------------------------------------------------
// Copyright Christophe Bertrand.
// This file is licensed under the Microsoft Reciprocal License (Ms-RL). You can find a copy of this license in the file "Ms-RL License.htm", in the root directory of the distribution archive.
//---------------------------------------------------------------------------


#ifndef AccessDiskThreadH
#define AccessDiskThreadH
//---------------------------------------------------------------------------
#include <Classes.hpp>
#include <Grids.hpp>

#include "FloppyDisk.h"



//---------------------------------------------------------------------------
class TAccessDiskThread : public TThread
{
  typedef struct tagTHREADNAME_INFO
  {
    DWORD dwType;     // doit être 0x1000
    LPCSTR szName;    // pointeur sur le nom (dans l'espace d'adresse de l'utilisateur)
    DWORD dwThreadID; // ID de thread (-1=thread de l'appelant)
    DWORD dwFlags;    // réservé pour une future utilisation, doit être zéro
  } THREADNAME_INFO;
private:

	RECT rect_invalide;
	TDrawGrid* grid;

	void SetName();
	void __fastcall MetAJourLAffichage();
protected:
	void __fastcall Execute();
public:

	TFloppyDisk* classe_disque;
	bool		Thread_en_route;


	__fastcall TAccessDiskThread(bool CreateSuspended);
};
//---------------------------------------------------------------------------
#endif
