//*****************************************************************
//
//	This code is strictly reserved by SYBERA GmbH
//  Copyright (c) 2019 SYBERA
//
//*****************************************************************

#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include "c:\sha\sha64exp.h"
#include "c:\sha\globdef64.h"
#include "c:\sha\sha64debug.h"
#include "c:\eth\Eth64CoreDef.h"
#include "c:\eth\Eth64Macros.h"
#include "c:\ect\Ecat64CoreDef.h"
#include "c:\ect\Ecat64Macros.h"
#include "c:\ect\Ecat64SilentDef.h"
#include "c:\ect\Sha64EcatCore.h"

#pragma comment( lib, "c:\\sha\\sha64dll.lib" )
#pragma comment( lib, "c:\\eth\\sha64ethcore.lib" )
#pragma comment( lib, "c:\\ect\\sha64ecatcore.lib" )

#pragma warning(disable:4996)
#pragma warning(disable:4748)

#define REALTIME_PERIOD	  2000			//Realtime Master Sampling Period [usec]
#define SYNC_CYCLES			 1			//Realtime Master Sampling Cycles
#define MAX_SYSTIME_DIFF	 500			//[nsec]

//************************************************
//!!! Structs need to have 1 byte alignment !!!
#pragma pack(push, old_alignment)
#pragma pack(1)

typedef struct _RX_PDO_MAP
{
	//Group 1600
	USHORT	ControlWord;	//6040,0 : ControlWord
	LONG	TargetPos;		//607A,0 : TargetPos

} RX_PDO_MAP, *PRX_PDO_MAP;

typedef struct _TX_PDO_MAP
{
	//Group 1A00
	USHORT	StatusWord;		//6041,0 : StatusWord
	LONG    ActPos;			//6064,0 : ActualPos
	SHORT	ActTorque;		//6077,0 : ActualTorque

	//Group 1A01
	LONG	FollowErr;		//60F4,0 : FollowingError
	LONG	DigIn;			//60FD,0 : DigitalInputs

} TX_PDO_MAP, *PTX_PDO_MAP;

//Set back old alignment
#pragma pack(pop, old_alignment)
//************************************************

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
ULONG			__DeviceIndex = 0;
RX_PDO_MAP		__RxPdoMap = { 0 };
TX_PDO_MAP		__TxPdoMap = { 0 };

//***********************
SILENT_INIT_ELEMENTS();
//***********************

void static DoLogic(PSTATION_INFO pDrive)
{
	//Increase target position
	__RxPdoMap.TargetPos += 200;
}


void static AppTask(PVOID)
{
	//Call enter wrapper function
	__EcatState = __fpEcatEnter(
							__pSystemStack,
							__pSystemList,
							(USHORT)__StationNum,
							&__StateObject);

	//Control state silent
	__SilentStateTransition(
					__pSystemList,
					__StationNum,
					&__StateObject);

	//Check operation state and decrease ready count
	if (__EcatState == ECAT_STATE_READY) { __ReadyCnt--; }
	else                                 { __ReadyCnt = SYNC_CYCLES; }

	//Check ready count
	if (__ReadyCnt == 1)
	{
		//Get station pointer by index
		PSTATION_INFO pDrive  = &__pSystemList[__DeviceIndex];
		
		//Get RX telegram data (receiving from drive)
		memcpy(&__TxPdoMap, pDrive->RxTel.s.data, sizeof(TX_PDO_MAP));

		//******************************************************************
		if (__bDriveReady) { DoLogic(pDrive); }
		//******************************************************************

		//Set TX telegram data (sending to drive)
		memcpy(pDrive->TxTel.s.data, &__RxPdoMap, sizeof(RX_PDO_MAP));

		//Update counter
		__UpdateCnt++;
	}

	//Call exit function
	__fpEcatExit();
	
	//Increase loop count
	__LoopCnt++;
}


__inline int __GetStationIndexByName(
							PSTATION_INFO pStationList,
							USHORT StationNum,
							char* pszStationName)
{
	int cnt = 0;

	//Loop through station list
	for (USHORT i=0; i<StationNum;i++)
		if (strstr(pStationList[i].szName, pszStationName))
				return i;

	//Drive not found
	return -1;
}


void main(void)
{
	printf("\n*** EtherCAT Drive Test ***\n\n");

	//Required ECAT parameters
	ECAT_PARAMS EcatParams;
	memset(&EcatParams, 0, sizeof(ECAT_PARAMS));
	EcatParams.PhysAddr = DEFAULT_PHYSICAL_ADDRESS;
	EcatParams.LogicalAddr = DEFAULT_LOGICAL_ADDRESS;
	EcatParams.SyncCycles = SYNC_CYCLES;			//Set cycles for synchronisation interval
	EcatParams.EthParams.dev_num = 0;				//Set NIC index [0..7]
	EcatParams.EthParams.period = REALTIME_PERIOD;	//Set realtime period [µsec]
	EcatParams.EthParams.fpAppTask = AppTask;

	//Create ECAT realtime core
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

			//Get station index
			__DeviceIndex = __GetStationIndexByName(__pUserList, __StationNum, "iPOS");

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
						//Init process telegrams (required for AL_STATE_SAFE_OP)
						Ecat64InitProcessTelegrams();
						
						//***************************************************
						//Change state to OPERATIONAL silent
						//***************************************************
						__Ecat64SilentChangeState(__pUserStack, REALTIME_PERIOD, AL_STATE_OP);

						//Init target pos
						__RxPdoMap.TargetPos = __TxPdoMap.ActPos;

						//Startup drive
						__RxPdoMap.ControlWord  = 0x80; Sleep(100);
						__RxPdoMap.ControlWord |= 0x06; Sleep(100);
						__RxPdoMap.ControlWord |= 0x07; Sleep(100);
						__RxPdoMap.ControlWord |= 0x0F; Sleep(100);

						//Start drive operation
						Sleep(100);
						__bDriveReady = TRUE;
								
						//Do a check loop
						printf("\nPress any key ...\n"); 
						while (!kbhit())
						{
							//Display TX and RX information
							printf("Update Count: %i, Status: 0x%04x Pos: 0x%08x\r",
								__UpdateCnt,
								__TxPdoMap.StatusWord,
								__TxPdoMap.ActPos);
		
							//Do some delay
							Sleep(100);
						}

						//Stop the drive (Power Off, Disable)
						__RxPdoMap.ControlWord = 0x80;
						Sleep(100);

						//Disable logic
						__bDriveReady = FALSE;
						Sleep(100);

						//***************************************************
						//Change state to INIT silent
						//***************************************************
						__Ecat64SilentChangeState(__pUserStack, REALTIME_PERIOD, AL_STATE_INIT);
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
