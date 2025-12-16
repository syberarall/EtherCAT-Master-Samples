//*****************************************************************
//
//	This code is strictly reserved by SYBERA GmbH
//  Copyright (c) 2019 SYBERA
//
//*****************************************************************

#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include "c:\sha\globdef64.h"
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

#define REALTIME_PERIOD	  2000			//Realtime Master Sampling Period [usec]
#define SYNC_CYCLES			 1			//Realtime Master Sampling Cycles
#define MAX_SYSTIME_DIFF	 500			//[nsec]
#define MAX_DRIVES			10
#define DRIVE_NAME		 "AKD"

//************************************************
//!!! Structs need to have 1 byte alignment !!!
#pragma pack(push, old_alignment)
#pragma pack(1)

typedef union _DIGITAL_OUT
{
	UCHAR Bytes[4];

	struct
	{
		ULONG Reserved1 : 16;
		ULONG DOut1     : 1;
		ULONG DOut2     : 1;
		ULONG Reserved2 : 14;

	} s;

} DIGITAL_OUT, *PDIGITAL_OUT;

typedef struct _RX_PDO_MAP
{
	//Group 1600
	UCHAR	ModeOP;				//6060,0 : Mode of Operation
	UCHAR	Reserved;			//1026,1 : StdIn
	USHORT	ControlWord;		//6040,0 : ControlWord

	//Group 1601
	LONG	TargetPos;			//60C1,1 : TargetPos

	//Group 1602
	DIGITAL_OUT	DigitalOut;		//60FE,0 : Digital Outputs

} RX_PDO_MAP, *PRX_PDO_MAP;

typedef struct _TX_PDO_MAP
{
	//Group 1A00
	UCHAR	ModeOP;			//6060,0 : Mode of Operation
	UCHAR	Reserved;		//1026,2 : StdOut
	USHORT	StatusWord;		//6041,0 : StatusWord

	//Group 1A01
	LONG    ActPos;			//6063,0 : ActualPos

	//Group 1A02
	ULONG	DigitalIn;		//60FD,0 : Digital Inputs

	//Group 1A03
	USHORT	AnalogIn;		//3470,4 : Analog Input

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
DRIVE_INFO		__DriveInfo = { 0 };
USHORT			__Threshold = 0xd000;
ULONG			__Increments = 0;

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


LONG __TimerCnt = 0;
__inline LONG __TimerFunc(
						ULONG DueTime,
						ULONG Period)
{
	ULONG TimerNum = DueTime * 1000 / Period;
	__TimerCnt = (++__TimerCnt) % TimerNum;
	return (__TimerCnt == 1) ? 1 : 0;
}

void static DoLogic(PDRIVE_INFO pDriveInfo)
{
	//Increase target position
	pDriveInfo->RxPdoMap.TargetPos += __Increments;

	ULONG Result = __TimerFunc(1000, REALTIME_PERIOD);
	pDriveInfo->RxPdoMap.DigitalOut.s.DOut1 = Result ? 1 : 0;
	pDriveInfo->RxPdoMap.DigitalOut.s.DOut2 = (pDriveInfo->TxPdoMap.AnalogIn < __Threshold) ? 1 : 0;

//****************************
#ifdef SEQ_DEBUG
	SYSTEM_SEQ_CAPTURE("SEQ", __LINE__, Result, TRUE);
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
		//Get drive information pointer
		PDRIVE_INFO pDriveInfo = &__DriveInfo;
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
	//SDO Upload Request/Response
	ULONG SdoSize = 0;
	UCHAR SdoBuffer[MAX_ECAT_DATA] = { 0 };
	if (ERROR_SUCCESS == Ecat64SdoInitUploadReq( pStation, 0x2081, 0x00))
	if (ERROR_SUCCESS == Ecat64SdoInitUploadResp(pStation, &SdoSize, SdoBuffer))
	{
		//Homing Method 35 : Position setting
		ULONG Data = 35;
		if (ERROR_SUCCESS == Ecat64SdoInitDownloadReq (pStation, 0x6098, 0x00, -4, (PUCHAR)&Data))
		if (ERROR_SUCCESS == Ecat64SdoInitDownloadResp(pStation))
		{
			//Set Homing Position
			Data = 0;
			if (ERROR_SUCCESS == Ecat64SdoInitDownloadReq (pStation, 0x2081, 0x00, 4, (PUCHAR)&Data))
			if (ERROR_SUCCESS == Ecat64SdoInitDownloadResp (pStation))
				return TRUE;
		}
	}

   //Something failed
   return FALSE;
}

#define HOMING_DONE		0x1000

__inline void __EnableDrives(void)
{
	//Enable Drive
	__DriveInfo.RxPdoMap.ControlWord = 0x80; Sleep(100);
	__DriveInfo.RxPdoMap.ControlWord = 0x06; Sleep(100);
	__DriveInfo.RxPdoMap.ControlWord = 0x07; Sleep(100);

	//Enable Homing
	__DriveInfo.RxPdoMap.ModeOP = 6;
	__DriveInfo.RxPdoMap.ControlWord = 0x07; Sleep(100);
	__DriveInfo.RxPdoMap.ControlWord = 0x0F; Sleep(100);
	__DriveInfo.RxPdoMap.ControlWord = 0x1F; Sleep(100);
	while ((__DriveInfo.TxPdoMap.StatusWord & HOMING_DONE) == 0) { Sleep(100); }
	Sleep(1000);

	//Set synchronous mode
	__DriveInfo.RxPdoMap.ModeOP = 8;
	__DriveInfo.RxPdoMap.ControlWord = 0x07; Sleep(100);
	__DriveInfo.RxPdoMap.ControlWord = 0x0F; Sleep(100);
	__DriveInfo.RxPdoMap.ControlWord = 0x1F; Sleep(100);

	//Start logic drive operation
	__bDriveReady = TRUE;
}

__inline void __DisableDrives(void)
{
	//Stop the drive (Power Off, Disable)
	__DriveInfo.RxPdoMap.ControlWord = 0x80;
	
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
	SEQ_ATTACH();					//Attach to sequence memory
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
			UCHAR Data[0x100] = { 0 };

			//Search for drive
			if (__FindStationByName(
							DRIVE_NAME, 0,
							&__DriveInfo.pUser,
							&__DriveInfo.pSystem))
			{
				//Reset devices
				Ecat64ResetDevices();
				Ecat64SendCommand(BWR_CMD, 0x0000,  0x300, 8, Data);

				//Set fixed station addresses and
				//Init FMMUs and SYNCMANs
				if (ERROR_SUCCESS == Ecat64InitStationAddresses(EcatParams.PhysAddr))
				if (ERROR_SUCCESS == Ecat64InitFmmus(EcatParams.LogicalAddr))
				if (ERROR_SUCCESS == Ecat64InitSyncManagers())
				{
					//Change state to PRE OPERATIONAL
					//Init PDO assignment
					if (ERROR_SUCCESS == Ecat64ChangeAllStates(AL_STATE_PRE_OP))
					if (ERROR_SUCCESS == Ecat64PdoAssignment())
					{
						//******************************************
						//Do SDO Download/Upload
						//for (int i=0; i<MAX_DRIVES; i++)
						//	if (__DriveInfo[i].pUser)
						//		__SdoControl(__DriveInfo[i].pUser);
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
							Sleep(100);
						
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
										printf("Update:%i, Status:0x%04x ActPos:0x%08x DigitalIn:0x%08x AnalogIn:0x%04x\r",
											__UpdateCnt,
											__DriveInfo.TxPdoMap.StatusWord,
											__DriveInfo.TxPdoMap.ActPos,
											__DriveInfo.TxPdoMap.DigitalIn,
											__DriveInfo.TxPdoMap.AnalogIn);
		
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
