//*****************************************************************
//
//	This code is strictly reserved by SYBERA GmbH
//  Copyright (c) 2015 SYBERA
//
//*****************************************************************
//
// Schneider-Electric LXM32MU90M2
//
// RX-PDOs
// 6040 : ControlWord (16 bit)
// 6060 : Mode of Operation (8 bit) [6 - Profile Position]
// 607A : Target Position (32 bit)
// 6081 : Profile Velocity (32 bit)
//
// TX-PDOs
// 6041 : StatusWord (16 bit)
// 6061 : Mode of Operation (8 bit)
// 6064 : Target Position (32 bit)
// 603F : Drive Error Code (16 bit)
//
//*****************************************************************

#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include "c:\sha\Sha64Debug.h"
#include "c:\eth\Sha64EthCore.h"
#include "c:\eth\Eth64CoreDef.h"
#include "c:\eth\Eth64Macros.h"
#include "c:\ect\Sha64EcatCore.h"
#include "c:\ect\Ecat64CoreDef.h"
#include "c:\ect\Ecat64Macros.h"
#include "c:\ect\Ecat64Control.h"
#include "c:\ect\Ecat64SilentDef.h"

#pragma comment( lib, "c:\\sha\\sha64dll.lib" )
#pragma comment( lib, "c:\\eth\\sha64ethcore.lib" )
#pragma comment( lib, "c:\\ect\\sha64ecatcore.lib" )

#pragma warning(disable:4996)
#pragma warning(disable:4748)

//************************************
#define SEQ_DEBUG			1
//#define SILENT_MODE		1
#define DC_CONTROL		1
//************************************

#define REALTIME_PERIOD	  2000		//Realtime sampling period [usec]
#define SYNC_CYCLES			 1
#define MAX_SYSTIME_DIFF	 500			//[nsec]
#define MAX_DRIVES			10
#define DRIVE_NAME		 "LXM"

//************************************************
//!!! Structs need to have 1 byte alignment !!!
#pragma pack(push, old_alignment)
#pragma pack(1)

typedef struct _RX_PDO_MAP
{
	USHORT	ControlWord;
	UCHAR	ModeOP;
	LONG	TargetPos;
	ULONG	ProfileVel;
	ULONG	ProfileAcc;
	ULONG	ProfileDec;

} RX_PDO_MAP, *PRX_PDO_MAP;

typedef struct _TX_PDO_MAP
{
	USHORT	StatusWord;
	UCHAR	ModeOfOp;
	LONG	TargetPos;
	LONG	InternalPos;
	USHORT	ErrCode;

} TX_PDO_MAP, *PTX_PDO_MAP;

//Set back old alignment
#pragma pack(pop, old_alignment)
//************************************************


typedef struct _DRIVE_INFO
{
	RX_PDO_MAP	RxPdoMap;
	TX_PDO_MAP	TxPdoMap;
	PSTATION_INFO pUser;
	PSTATION_INFO pSystem;

	ULONG  InternalPos;
	ULONG  ErrCount;
	USHORT WC;

} DRIVE_INFO, *PDRIVE_INFO;


//Declare global elements
PETH_STACK		__pUserStack = NULL;	//Ethernet Core Stack (used outside Realtime Task)
PETH_STACK		__pSystemStack = NULL;	//Ethernet Core Stack (used inside Realtime Task)
PSTATION_INFO	__pUserList = NULL;		//Station List (used outside Realtime Task)
PSTATION_INFO	__pSystemList = NULL;	//Station List (used inside Realtime Task)
USHORT			__StationNum = 0;		//Number of Stations
FP_ECAT_ENTER	__fpEcatEnter = NULL;	//Function pointer to Wrapper EcatEnter
FP_ECAT_EXIT	__fpEcatExit = NULL;	//Function pointer to Wrapper EcatExit
ULONG			__EcatState = 0;		//Initial Wrapper State
ULONG			__UpdateCnt = 0;		//Station Update Counter
ULONG			__LoopCnt = 0;			//Realtime Cycle Counter
ULONG			__ReadyCnt = 0;			//Ready state counter
STATE_OBJECT    __StateObject = { 0 };	//Cyclic state object
BOOLEAN         __bDriveReady = FALSE;  //Drive ready flag
DRIVE_INFO		__DriveList[MAX_DRIVES] = { 0 };
int				__DriveNum = 0;


//***************************
#ifdef SEQ_DEBUG
	SEQ_INIT_ELEMENTS(); //Init sequencing elements
#endif
//***************************

//**************************
#ifdef SILENT_MODE
	SILENT_INIT_ELEMENTS();
#endif
//**************************


void static DoLogic(PDRIVE_INFO pDriveInfo)
{
	//Check for internal position not changed
	if ( pDriveInfo->InternalPos)
	if ((pDriveInfo->InternalPos & (~0xF)) == (pDriveInfo->TxPdoMap.InternalPos & (~0xF)))
	{
		//Increase error count and check break threshold
		pDriveInfo->ErrCount++;
//		if (pDriveInfo->ErrCount > 10) { __pSystemStack->hdr.run_flag = FALSE; }
	}

	//Save internal position
	pDriveInfo->InternalPos = pDriveInfo->TxPdoMap.InternalPos;

	//Check drive status
//	if (pDriveInfo->TxPdoMap.StatusWord != 0x9237)
//		__pSystemStack->hdr.run_flag = FALSE;

	//Increase target position
	pDriveInfo->RxPdoMap.TargetPos += 1000;

//****************************
#ifdef SEQ_DEBUG
	SYSTEM_SEQ_CAPTURE("SEQ", __LINE__, pDriveInfo->TxPdoMap.TargetPos, TRUE);
	SYSTEM_SEQ_CAPTURE("SEQ", __LINE__, pDriveInfo->TxPdoMap.InternalPos, FALSE);
	SYSTEM_SEQ_CAPTURE("SEQ", __LINE__, pDriveInfo->TxPdoMap.StatusWord, FALSE);
#endif
//****************************
}


void static AppTask(PVOID)
{
	//Call enter wrapper function
	__EcatState = __fpEcatEnter(
							__pSystemStack,
							__pSystemList,
							(USHORT)__StationNum,
							&__StateObject);

#ifdef SILENT_MODE
	//Control state silent
	__SilentStateTransition(
					__pSystemList,
					__StationNum,
					&__StateObject);
#endif

	//Check operation state and decrease ready count
	if (__EcatState == ECAT_STATE_READY) { __ReadyCnt--; }
	else                                 { __ReadyCnt = SYNC_CYCLES; }

	//Check ready count
	if (__ReadyCnt == 1)
	{
		//Loop through all drives
		for (int i=0; i<__DriveNum; i++)
		{
			//Get drive information pointer
			PDRIVE_INFO pDriveInfo = &__DriveList[i];
			if (pDriveInfo->pSystem)
			{
				//Get RX telegram data (receiving from drive)
				memcpy(&pDriveInfo->TxPdoMap, pDriveInfo->pSystem->RxTel.s.data, sizeof(TX_PDO_MAP));

				//******************************************************************
				if (__bDriveReady) { DoLogic(pDriveInfo); }
				//******************************************************************

				//Set TX telegram data (sending to drive)
				memcpy(pDriveInfo->pSystem->TxTel.s.data, &pDriveInfo->RxPdoMap, sizeof(RX_PDO_MAP));
			}
		}

		//Update counter
		__UpdateCnt++;
	}

	//Call exit function
	__fpEcatExit();
	
	//Increase loop count
	__LoopCnt++;
}


__inline BOOLEAN __SdoControl(PSTATION_INFO pStation)
{
	//Fault Reset
	ULONG Data  = 0x0080;
	if (ERROR_SUCCESS == Ecat64SdoInitDownloadReq( pStation, 0x6040, 0x00, 2, (PUCHAR)&Data))
	if (ERROR_SUCCESS == Ecat64SdoInitDownloadResp(pStation))
	{
	//Homing Method 35 : Position setting
	ULONG Data  = 35;
	if (ERROR_SUCCESS == Ecat64SdoInitDownloadReq( pStation, 0x6098, 0x00, 1, (PUCHAR)&Data))
	if (ERROR_SUCCESS == Ecat64SdoInitDownloadResp(pStation))
	{
		//Set Homing Position
		Data  = 0;
		if (ERROR_SUCCESS == Ecat64SdoInitDownloadReq( pStation, 0x301B, 0x16, 4, (PUCHAR)&Data))
		if (ERROR_SUCCESS == Ecat64SdoInitDownloadResp(pStation))
			return TRUE;
	}
	}

	//Something failed
	return FALSE;
}


__inline void __EnableDrives(void)
{
	//Loop through all drives
	for (int i=0; i<__DriveNum; i++)
	{
		//Enable homing
		__DriveList[i].RxPdoMap.ModeOP = 6;
		__DriveList[i].RxPdoMap.ControlWord  = 0x06; Sleep(100);
		__DriveList[i].RxPdoMap.ControlWord |= 0x07; Sleep(100);
		__DriveList[i].RxPdoMap.ControlWord |= 0x0F; Sleep(100);
		__DriveList[i].RxPdoMap.ControlWord |= 0x1F; Sleep(100);

		//Startup drive
		__DriveList[i].RxPdoMap.ModeOP = 8;
		__DriveList[i].RxPdoMap.TargetPos  = 0;
		__DriveList[i].RxPdoMap.ProfileVel = 2000;
		__DriveList[i].RxPdoMap.ProfileAcc = 300;
		__DriveList[i].RxPdoMap.ProfileDec = 300;
		__DriveList[i].RxPdoMap.ControlWord  = 0x06; Sleep(100);
		__DriveList[i].RxPdoMap.ControlWord |= 0x07; Sleep(100);
		__DriveList[i].RxPdoMap.ControlWord |= 0x0F; Sleep(100);
		__DriveList[i].RxPdoMap.ControlWord |= 0x1F; Sleep(100);
	}

	//Start logic drive operation
	__bDriveReady = TRUE;
}


__inline void __DisableDrives(void)
{
	//Loop through all drives
	for (int i=0; i<__DriveNum; i++)
	{
		//Stop the drive (Power Off, Disable)
		__DriveList[i].RxPdoMap.ModeOP = 0;
		__DriveList[i].RxPdoMap.ControlWord = 0x0006;
		Sleep(100);
	}
	
	//Stop logic drive operation
	Sleep(100);
	__bDriveReady = FALSE;
	Sleep(100);
}


__inline BOOLEAN __FindStationByName(
							char* pszStationName,
							int Index,
							PSTATION_INFO* ppUserStation,
							PSTATION_INFO* ppSystemStation)
{
	int cnt = 0;

	//Loop through station list
	for (USHORT i=0; i<__StationNum;i++)
		if (strstr(__pUserList[i].szName, pszStationName))
			if (cnt++ == Index)
			{
				//Get station pointer and return index
				if (ppUserStation)   { *ppUserStation   = &__pUserList[i]; }
				if (ppSystemStation) { *ppSystemStation = &__pSystemList[i]; }
				return TRUE;
			}

	//Station not found
	if (ppUserStation)   { *ppUserStation   = NULL; }
	if (ppSystemStation) { *ppSystemStation = NULL; }
	return FALSE;
}


void main(void)
{
//******************
#ifdef SEQ_DEBUG
	SEQ_ATTACH();						//Attach to sequence memory
	SEQ_RESET(TRUE, TRUE, NULL, 0);	//Reset/Init sequence memory
#endif
//******************

	printf("\n*** EtherCAT Drive Test ***\n\n");

#ifdef DC_CONTROL
	//Set ethernet mode for DC
	__EcatSetEthernetMode(0, REALTIME_PERIOD, SYNC_CYCLES, TRUE);
#else
	//Set ethernet mode for DC
	__EcatSetEthernetMode(0, REALTIME_PERIOD, SYNC_CYCLES, FALSE);
#endif

	//Required ECAT parameters
	ECAT_PARAMS EcatParams;
	memset(&EcatParams, 0, sizeof(ECAT_PARAMS));
	EcatParams.PhysAddr = DEFAULT_PHYSICAL_ADDRESS;
	EcatParams.LogicalAddr = DEFAULT_LOGICAL_ADDRESS;
	EcatParams.SyncCycles = SYNC_CYCLES;			//Set cycles for synchronisation interval
	EcatParams.EthParams.dev_num = 0;				//Set NIC index [0..7]
	EcatParams.EthParams.period = REALTIME_PERIOD;	//Set realtime period [µsec]
	EcatParams.EthParams.fpAppTask = AppTask;

	//******************************
	//Create ECAT realtime core
	//******************************
	if (ERROR_SUCCESS == Sha64EcatCreate(&EcatParams))
	{
		//Init global elements
		__pUserStack	= EcatParams.EthParams.pUserStack;
		__pSystemStack	= EcatParams.EthParams.pSystemStack;
		__pUserList		= EcatParams.pUserList;
		__pSystemList	= EcatParams.pSystemList;
		__StationNum	= EcatParams.StationNum;
		__fpEcatEnter	= EcatParams.fpEcatEnter;
		__fpEcatExit	= EcatParams.fpEcatExit;

		//Display version information
		Sha64EcatGetVersion(&EcatParams);
		printf("ECTCORE-DLL : %.2f\nECTCORE-DRV : %.2f\n", EcatParams.core_dll_ver / (double)100, EcatParams.core_drv_ver / (double)100);
		printf("ETHCORE-DLL : %.2f\nETHCORE-DRV : %.2f\n", EcatParams.EthParams.core_dll_ver / (double)100, EcatParams.EthParams.core_drv_ver / (double)100);
		printf("SHA-LIB     : %.2f\nSHA-DRV     : %.2f\n", EcatParams.EthParams.sha_lib_ver / (double)100, EcatParams.EthParams.sha_drv_ver / (double)100);
		printf("\n");

		//Display station information
		for (int i=0; i<__StationNum; i++)
			printf("Station: %i, Name: %6s\n", i, __pUserList[i].szName);

		//******************************
		//Enable Stations
		//******************************

		//Change state to INIT
		if (ERROR_SUCCESS == Ecat64ChangeAllStates(AL_STATE_INIT))
		{
			//Enumerate drives
			__DriveNum = 0;
			for (int i=0; i<MAX_DRIVES; i++)
				if (__FindStationByName(
							DRIVE_NAME, i,
							&__DriveList[i].pUser,
							&__DriveList[i].pSystem)) { __DriveNum++; }

			//Set fixed station addresses and
			//Init FMMUs and SYNCMANs
			if (ERROR_SUCCESS == Ecat64InitStationAddresses(EcatParams.PhysAddr))
			if (ERROR_SUCCESS == Ecat64InitFmmus(EcatParams.LogicalAddr))
			if (ERROR_SUCCESS == Ecat64InitSyncManagers())
			{
				//Change state to PRE OPERATIONAL
				//Init PDO assignment (need to do it twice)
				if (ERROR_SUCCESS == Ecat64ChangeAllStates(AL_STATE_PRE_OP))
				if (ERROR_SUCCESS == Ecat64PdoAssignment())
				if (ERROR_SUCCESS == Ecat64PdoAssignment())
				{
					//******************************************
					//Do SDO Download/Upload
					for (int i=0; i<MAX_DRIVES; i++)
						if (__DriveList[i].pUser)
							__SdoControl(__DriveList[i].pUser);
					//******************************************

#ifdef DC_CONTROL
					ULONG CompLoops = 1000;
					ULONG SysTimeDiff = 0;

					//Init DC immediately after cyclic operation has started
					//and get static master drift per msec (nsec unit)
					//Check for acceptable DC quality and enable DC unit
					if (ERROR_SUCCESS == Ecat64ReadDcLocalTime())
					if (ERROR_SUCCESS == Ecat64CompDcOffset())
					if (ERROR_SUCCESS == Ecat64CompDcPropDelay())
					if (ERROR_SUCCESS == Ecat64DcControl())
					if (ERROR_SUCCESS == Ecat64CompDcDrift(&CompLoops))
					if (ERROR_SUCCESS == Ecat64CheckDcQuality(&SysTimeDiff))
					if (SysTimeDiff < MAX_SYSTIME_DIFF)
#endif
					{
						//Init process telegrams (required for AL_STATE_SAFE_OP)
						Ecat64InitProcessTelegrams();

						//********************************************************
						//Start cyclic operation
						//********************************************************
#ifdef SILENT_MODE
						if (ERROR_SUCCESS == __Ecat64SilentChangeState(__pUserStack, REALTIME_PERIOD, AL_STATE_OP)) { {
#else
						if (ERROR_SUCCESS == Ecat64CyclicStateControl(&__StateObject, AL_STATE_SAFE_OP)) { Sleep(500);
						if (ERROR_SUCCESS == Ecat64CyclicStateControl(&__StateObject, AL_STATE_OP))      { Sleep(100);
#endif
								//Startup drives
								__EnableDrives();
	
								//Do a check loop
								printf("\nPress any key ...\n"); 
								while (!kbhit())
								{
									//Display TX and RX information
									printf("Update:%i, Stat:0x%04x Mode:0x%02x, TargetPos:0x%08x IntPos:0x%08x, ErrCode:0x%04x, ErrCount:%i\r",
										__UpdateCnt,
										__DriveList[0].TxPdoMap.StatusWord,
										__DriveList[0].TxPdoMap.ModeOfOp,
										__DriveList[0].TxPdoMap.TargetPos,
										__DriveList[0].TxPdoMap.InternalPos,
										__DriveList[0].TxPdoMap.ErrCode,
										__DriveList[0].ErrCount);

									//Do some delay
									Sleep(100);
								}
	
								//Stop drives
								__DisableDrives();
	
								//********************************************************
								//Stop cyclic operation
								//********************************************************
#ifdef SILENT_MODE
								__Ecat64SilentChangeState(__pUserStack, REALTIME_PERIOD, AL_STATE_INIT);
#else
								Ecat64CyclicStateControl(&__StateObject, AL_STATE_INIT);
#endif
							}
						}
					}
				}
			}
		}
		//Destroy ECAT core
 		Sha64EcatDestroy(&EcatParams);
	}

	//Check Status
	if		(EcatParams.EthParams.pUserStack == NULL)	{ printf("\n*** Ethernet Stack Failed ***\n"); }
	else if (EcatParams.EthParams.err_cnts.Phy != 0)	{ printf("\n*** No Link ***\n"); }
	else if (EcatParams.pUserList == NULL)				{ printf("\n*** No Station List ***\n"); }
	else if (EcatParams.StationNum == 0)				{ printf("\n*** No Station Found ***\n"); }
	else												{ printf("\n*** OK ***\n"); }

	//Wait for key press
	printf("\nPress any key ...\n"); 
	while (!kbhit()) { Sleep(100); }

//******************
#ifdef SEQ_DEBUG
	SEQ_DETACH(); //Detach from sequence memory
#endif
//******************
}
