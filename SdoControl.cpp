//*****************************************************************
//
//	This code is strictly reserved by SYBERA GmbH
//  Copyright (c) 2012 SYBERA
//
//*****************************************************************
//
// This sample demonstrates how to use ETHERCAT Realtime Core
// in Realtime Level1 with Beckhoff modules EK1100, EL1014 and EL2032
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
//#define DC_CONTROL		1
//************************************

#define MAX_SYSTIME_DIFF	 500			//[nsec]
#define REALTIME_PERIOD	  1000		//Realtime sampling period [usec]
#define SYNC_CYCLES			 2


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
ULONG			__ReadyCnt = 0;			//Ready state counter
STATE_OBJECT    __StateObject = { 0 };	//Cyclic state object


void static AppTask(PVOID)
{
	//Call enter wrapper function
	__EcatState = __fpEcatEnter(
							__pSystemStack,
							__pSystemList,
							(USHORT)__StationNum,
							NULL);

	//Check operation state and increase ready count
	if (__EcatState == ECAT_STATE_READY) { __ReadyCnt--; }
	else                                 { __ReadyCnt = SYNC_CYCLES; }

	//Check ready count
	if (__ReadyCnt == 1) { __UpdateCnt++; }

	//Call exit function
	__fpEcatExit();
}


void main(void)
{
	printf("\n*** EtherCAT Core Realtime Level2 Test ***\n\n");

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

		//Change state to INIT
		if (ERROR_SUCCESS == Ecat64ChangeAllStates(AL_STATE_INIT))
		{
			//Set fixed station addresses and
			//Init FMMUs and SYNCMANs
			if (ERROR_SUCCESS == Ecat64InitStationAddresses(EcatParams.PhysAddr))
			if (ERROR_SUCCESS == Ecat64InitFmmus(EcatParams.LogicalAddr))
			if (ERROR_SUCCESS == Ecat64InitSyncManagers())
			{
				//Change state to PRE OPERATIONAL
				if (ERROR_SUCCESS == Ecat64ChangeAllStates(AL_STATE_PRE_OP))
				{
//***************************************************************************

					//Get station pointer
					PSTATION_INFO pStation = (PSTATION_INFO)&__pUserList[3];

					//SDO Download Request/Response
					ULONG Error = 0;
					ULONG SdoData = 0;
					Error = Ecat64SdoInitDownloadReq( pStation, 0x1c13, 0x00, 1, (PUCHAR)&SdoData);
					Error = Ecat64SdoInitDownloadResp(pStation);

					//SDO Upload Request/Response
					ULONG SdoSize = 0;
					UCHAR SdoBuffer[MAX_ECAT_DATA] = { 0 };
					Error = Ecat64SdoInitUploadReq( pStation, 0x1a00, 0x01);
					Error = Ecat64SdoInitUploadResp(pStation, &SdoSize, SdoBuffer);

//*****************************************************************************************

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
						if (ERROR_SUCCESS == Ecat64CyclicStateControl(&__StateObject, AL_STATE_SAFE_OP)) { Sleep(500);
						if (ERROR_SUCCESS == Ecat64CyclicStateControl(&__StateObject, AL_STATE_OP))      { Sleep(100);

								//Do a check loop
								printf("\nPress any key ...\n"); 
								while (!kbhit())
								{
									//Display TX and RX information
									printf("Update Count: %i\r", __UpdateCnt);

									//Do some delay
									Sleep(100);
								}

								//********************************************************
								//Stop cyclic operation
								//********************************************************
								Ecat64CyclicStateControl(&__StateObject, AL_STATE_INIT);
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
