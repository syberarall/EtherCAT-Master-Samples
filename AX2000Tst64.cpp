//*****************************************************************
//
//	This code is strictly reserved by SYBERA GmbH
//  Copyright (c) 2012 SYBERA
//
//*****************************************************************
//
// This sample demonstrates how to use ETHERCAT Realtime Core
// in Realtime Level2 with AX2000 drive
//
//*****************************************************************
#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include "c:\eth\Eth64CoreDef.h"
#include "c:\eth\Eth64Macros.h"
#include "c:\ect\Ecat64CoreDef.h"
#include "c:\ect\Ecat64Macros.h"
#include "c:\ect\Sha64EcatCore.h"

#pragma comment( lib, "c:\\sha\\sha64dll.lib" )
#pragma comment( lib, "c:\\eth\\sha64ethcore.lib" )
#pragma comment( lib, "c:\\ect\\sha64ecatcore.lib" )

#pragma warning(disable:4996)
#pragma warning(disable:4748)

#define REALTIME_PERIOD	  1000		//Realtime sampling period [usec]
#define SYNC_CYCLES			 2
#define MAX_SYSTIME_DIFF	 500			//[nsec]


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
STATE_OBJECT    __StateObject = { 0 };	//Cyclic state object
ULONG			__ReadyCnt = 0;			//Ready state counter
BOOLEAN			__bInitDone = FALSE;	//Initialization ready flag

ULONG			__DrivePos = 0;
USHORT			__DriveStat = 0;
ULONG			__DriveAct = 0;
USHORT			__DriveCtrl = 0;
USHORT			__DriveIndex = 0;


__inline int __GetStationIndexByName(
							PSTATION_INFO pStationList,
							USHORT StationNum,
							char* pszStationName)
{
	//Loop through station list
	for (USHORT i=0; i<StationNum;i++)
		if (strstr(pStationList[i].szName, pszStationName))
			return i;

	//Drive not found
	return 0;
}


void static AppTask(PVOID)
{
	//Check if initialization has been done
	if (__bInitDone == FALSE)
		return;

	//Call enter wrapper function
	__EcatState = __fpEcatEnter(
							__pSystemStack,
							__pSystemList,
							(USHORT)__StationNum,
							&__StateObject);

	//Check operation state and decrease ready count
	if (__EcatState == ECAT_STATE_READY) { __ReadyCnt--; }
	else								 { __ReadyCnt = SYNC_CYCLES; }

	//Check ready count
	if (__ReadyCnt == 1)
	{
		//Get drive station by index
		PSTATION_INFO pDrive = &__pSystemList[__DriveIndex];

		//DBG_INITIAL_BREAK();

		//*********************************
		// Note: This sample requires to be configured as:
		// Output:
		// 4 Byte Velocity/Position Control Value
		// 2 Byte Drive Control Value
		// Input:
		// 4 Byte Speed/Position Status Value
		// 2 Byte Drive Status Value
		//*********************************

		//Get RX telegram data
		__DrivePos =(ULONG)(pDrive->RxTel.s.data[0] << 0) +
					(ULONG)(pDrive->RxTel.s.data[1] << 8) +
					(ULONG)(pDrive->RxTel.s.data[2] << 16) +
					(ULONG)(pDrive->RxTel.s.data[3] << 24);

		__DriveStat =	(USHORT)(pDrive->RxTel.s.data[6] << 0) +
						(USHORT)(pDrive->RxTel.s.data[7] << 8);

		//Set TX telegram data
		pDrive->TxTel.s.data[0] = (UCHAR)(__DriveAct >> 0);
		pDrive->TxTel.s.data[1] = (UCHAR)(__DriveAct >> 8);
		pDrive->TxTel.s.data[2] = (UCHAR)(__DriveAct >> 16);
		pDrive->TxTel.s.data[3] = (UCHAR)(__DriveAct >> 24);

		pDrive->TxTel.s.data[4] = (UCHAR)(__DriveCtrl >> 0);
		pDrive->TxTel.s.data[5] = (UCHAR)(__DriveCtrl >> 8);

		__UpdateCnt++;
	}

	//Call exit function
	__fpEcatExit();
	
	//Increase loop count
	__LoopCnt++;
}


__inline void __InitProcessTelegram(PSTATION_INFO pStation)
{
	TYPE32	Addr = { 0 };
	USHORT	DataSize[2] = { 0, 0 };
	USHORT	Len = 0;
	UCHAR	Cmd = 0;
	ULONG	i;

	//Get logical address of first descriptor, if descriptors are present
	if (pStation->OutDescNum) { Addr.bit32 = pStation->FmmuList[pStation->OutDescList[0].Fmmu].s.LogicalAddr; }
	if (pStation->InDescNum)  { Addr.bit32 = pStation->FmmuList[pStation->InDescList[0].Fmmu].s.LogicalAddr; }

	//Get data length, if descriptors are present
	for (i=0; i<pStation->OutDescNum; i++)	{ DataSize[0] += pStation->OutDescList[i].Len; }
	for (i=0; i<pStation->InDescNum;  i++)	{ DataSize[1] += pStation->InDescList[i].Len; }

	//Set max. data length
	if ((DataSize[1]) && (DataSize[1] >= DataSize[0]))	{ Len = DataSize[1]; }
	if ((DataSize[0]) && (DataSize[0] >= DataSize[1]))	{ Len = DataSize[0]; }

	//Set command
	if (( pStation->OutDescNum) && ( pStation->InDescNum))	{ Cmd = LRW_CMD; }
	if (( pStation->OutDescNum) && (!pStation->InDescNum))	{ Cmd = LWR_CMD; }
	if ((!pStation->OutDescNum) && ( pStation->InDescNum))	{ Cmd = LRD_CMD; }
	if ((!pStation->OutDescNum) && (!pStation->InDescNum))	{ Cmd = NOP_CMD; }

	//Set cyclic telegram
	__EcatSetCyclicTelegram(&pStation->TxTel, (UCHAR)pStation->Index, Cmd, Addr.bit16[0], Addr.bit16[1], Len, NULL, 0x0000);

	//Set station update
	pStation->bUpdate = TRUE;
}


void InitProcessTelegrams(void)
{
	//Loop through all enabled stations
	for (short i=0; i<__StationNum; i++)
	if (!__pUserList[i].bDisable)
		__InitProcessTelegram(&__pUserList[i]);
}


__inline void __SyncControl(
						ULONG Period,
						ULONG SyncCycles)
{
	//Loop through all stations
	for (int i=0; i<__StationNum; i++)
	{
		//Activate sync unit
		Ecat64SyncControl(
					&__pUserList[i],
					Period * SyncCycles * 1000,	//Sync0 cycle time [nsec]
					0,			//Sync1 cycle time [nsec]
					20*1000,	//Sync0 cycle shift [nsec]
					0,			//Sync1 cycle shift [nsec]
					TRUE,		//Sync0 pulse flag
					FALSE,		//Sync1 pulse flag
					FALSE);		//Sync PDI control
	}
}


void main(void)
{
	printf("\n*** EtherCAT Drive AX2000 Test ***\n\n");

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

		//Set initialization done flag
		__bInitDone = TRUE;

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
			//Get LXM32 station index
			__DriveIndex = __GetStationIndexByName(__pUserList, __StationNum, "AX2000");

			//Set fixed station addresses and
			//Init FMMUs and SYNCMANs
			if (ERROR_SUCCESS == Ecat64InitStationAddresses(EcatParams.PhysAddr))
			if (ERROR_SUCCESS == Ecat64InitFmmus(EcatParams.LogicalAddr))
			if (ERROR_SUCCESS == Ecat64InitSyncManagers())
			{
				//Init PDO assignment
				Ecat64PdoAssignment();
				
				//Change state to PRE OPERATIONAL
				if (ERROR_SUCCESS == Ecat64ChangeAllStates(AL_STATE_PRE_OP))
				{
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
					{
						//Check DC quality
//						ULONG MaxSysTimeDiff = 0;
//						Ecat64CheckDcQuality(&MaxSysTimeDiff);

						//Activate DC sync unit
						__SyncControl(REALTIME_PERIOD, SYNC_CYCLES);

						//Init process telegrams (required for AL_STATE_SAFE_OP)
						InitProcessTelegrams();
						
						//********************************************************
						//Change state to SAFE OPERATIONAL cyclic
						//********************************************************
						if (ERROR_SUCCESS == Ecat64CyclicStateControl(&__StateObject, AL_STATE_SAFE_OP))
						{
							//Do some delay
							Sleep(500);
	
							//********************************************************
							//Change state to SAFE OPERATIONAL cyclic
							//********************************************************
							if (ERROR_SUCCESS == Ecat64CyclicStateControl(&__StateObject, AL_STATE_OP))
							{
								//Start drive
								__DriveAct  = 0;
								__DriveCtrl = 0x0006; Sleep(100);
								__DriveCtrl = 0x0007; Sleep(100);
								__DriveCtrl = 0x000f; Sleep(100);
								__DriveCtrl = 0x001f; Sleep(100);
	
								//Do a check loop
								printf("\nPress any key ...\n"); 
								while (!kbhit())
								{
									//Increase speed
									__DriveAct += 1000;
									if (__DriveAct > 0x00100000)
										__DriveAct = 0x00100000;
	
									//Display TX and RX information
									printf("Update Count: %i, DrivePos: 0x%08x, DriveStat: 0x%04x\r", __UpdateCnt, __DrivePos, __DriveStat);
	
									//Do some delay
									Sleep(100);
								}
	
								//Stop the drive
								__DriveCtrl = 0;
								__DriveAct = 0;
								Sleep(100);
	
								//Change state to SAFE OPERATIONAL then to INIT
								Ecat64ChangeAllStates(AL_STATE_SAFE_OP);
								Ecat64ChangeAllStates(AL_STATE_INIT);
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
}
