//*****************************************************************
//
//	This code is strictly reserved by SYBERA GmbH
//  Copyright (c) 2012 SYBERA
//
//*****************************************************************

#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include "c:\sha\globdef64.h"
#include "c:\sha\sha64debug.h"
#include "c:\eth\Sha64EthCore.h"
#include "c:\eth\Eth64CoreDef.h"
#include "c:\eth\Eth64Macros.h"
#include "c:\ect\Sha64EcatCore.h"
#include "c:\ect\Ecat64CoreDef.h"
#include "c:\ect\Ecat64RegDef.h"
#include "c:\ect\Ecat64Macros.h"
#include "c:\ect\Ecat64Control.h"
#include "c:\ect\Ecat64SilentDef.h"

//Configuration Header Includes
#include "DevConfig_iPOS4808_2003051122.h"

//Add libraries to project
#pragma comment( lib, "c:\\sha\\sha64dll.lib" )
#pragma comment( lib, "c:\\eth\\sha64ethcore.lib" )
#pragma comment( lib, "c:\\ect\\sha64ecatcore.lib" )

#pragma warning(disable:4996)
#pragma warning(disable:4748)

#define REALTIME_PERIOD      1000		//Realtime sampling period [usec]
#define SYNC_CYCLES			    1
#define NUM_HOTPLUG_DEVICES		2
#define TIMEOUT_TIME		 3000

//************************************
//#define SEQ_DEBUG			1
//************************************

//Declare global elements
PETH_STACK		__pUserStack = NULL;	//Ethernet Core Stack (used outside Realtime Task)
PETH_STACK		__pSystemStack = NULL;	//Ethernet Core Stack (used inside Realtime Task)
PSTATION_INFO	__pUserList = NULL;		//Station List (used outside Realtime Task)
PSTATION_INFO	__pSystemList = NULL;	//Station List (used inside Realtime Task)
PSTATION_INFO	__pHotPlugUserList = NULL;		//Station List (used outside Realtime Task)
PSTATION_INFO	__pHotPlugSystemList = NULL;	//Station List (used inside Realtime Task)
HANDLE			__hHotPlugList = NULL;
USHORT			__StationNum = 0;		//Number of Stations
FP_ECAT_ENTER	__fpEcatEnter = NULL;	//Function pointer to Wrapper EcatEnter
FP_ECAT_EXIT	__fpEcatExit = NULL;	//Function pointer to Wrapper EcatExit
ULONG			__EcatState = 0;		//HotPlug Wrapper State
ULONG			__UpdateCnt = 0;		//Station Update Counter
ULONG			__LoopCnt = 0;			//Realtime Cycle Counter
ULONG			__ReadyCnt = 0;			//Ready state counter
STATE_OBJECT    __StateObject = { 0 };	//Cyclic state object
BOOLEAN			__bListChange = FALSE;
BOOLEAN			__bLogicReady = FALSE;

//***************************
#ifdef SEQ_DEBUG
	SEQ_INIT_ELEMENTS(); //Init sequencing elements
#endif
//***************************


void static DoLogic()
{
	//Do the logical station operation
	//e.g. set input to output value (station index == 2)
	//__pSystemList[2].TxTel.s.data[0] = __pSystemList[1].RxTel.s.data[0];

//****************************
#ifdef SEQ_DEBUG
	SYSTEM_SEQ_CAPTURE("SEQ", __LINE__, 0, TRUE);
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
		//*********************************
		//Do the logical station operation
		if (__bLogicReady) { DoLogic(); }
		//*********************************

		__UpdateCnt++;
	}

	//Call exit function
	__fpEcatExit();

	//*********************************
	// Change to HotPlug station list
	if (__bListChange == TRUE)
	{
		__pUserList   = __pHotPlugUserList;
		__pSystemList = __pHotPlugSystemList;
		__StationNum += NUM_HOTPLUG_DEVICES;
		__bListChange = FALSE;
	}
	//*********************************

	//Increase loop count
	__LoopCnt++;
}


__inline void __HotPlugSaveDriveImage(
								PSTATION_INFO pStation,
								char* pszFileName)
{
	FILE* pFile;
	pFile = fopen(pszFileName, "wb");
	fwrite(pStation, sizeof(STATION_INFO), 1, pFile);
	fclose(pFile);
}


__inline void __HotPlugLoadDriveImage(
								PSTATION_INFO pStation,
								char* pszFileName)
{
	FILE* pFile;
	pFile = fopen(pszFileName, "rb");
	fread(pStation, sizeof(STATION_INFO), 1, pFile);
	fclose(pFile);
}


__inline ULONG __HotPlugChangeState(
						PSTATION_INFO pStation,
						USHORT StateReq)
{
	AL_CONTROL AlControl = { 0 };
	AL_STATUS  AlStatus  = { 0 };
	ULONG Result;
	clock_t TimeoutTime = clock() + TIMEOUT_TIME;

	//Change state to INIT
	do
	{
		AlControl.s.State = StateReq;
		AlControl.s.Acknowledge = pStation->AlStatus.s.bits.ErrInd;
		//Result = Ecat64SendCommand(APWR_CMD, -pStation->Index, ECAT_REG_AL_CONTROL, sizeof(AL_CONTROL), (PUCHAR)&AlControl);
		//Result = Ecat64SendCommand(APRD_CMD, -pStation->Index, ECAT_REG_AL_STATUS,  sizeof(AL_STATUS),  (PUCHAR)&AlStatus);
		Result = Ecat64SendCommand(FPWR_CMD, pStation->PhysAddr, ECAT_REG_AL_CONTROL, sizeof(AL_CONTROL), (PUCHAR)&AlControl);
		Result = Ecat64SendCommand(FPRD_CMD, pStation->PhysAddr, ECAT_REG_AL_STATUS,  sizeof(AL_STATUS),  (PUCHAR)&AlStatus);

		//Check TimeOut condition
		if (clock() > TimeoutTime)
			return -1;

	} while (AlStatus.s.bits.State != StateReq);

	//Everything is OK
	return ERROR_SUCCESS;
}


ULONG HotPlugStartup(void)
{
	PSTATION_INFO pHpList[NUM_HOTPLUG_DEVICES] = { 0 };
	pHpList[0] = (PSTATION_INFO)&__pHotPlugUserList[__StationNum + 0];
	pHpList[1] = (PSTATION_INFO)&__pHotPlugUserList[__StationNum + 1];
	
	//Reset flag
	__bListChange = FALSE;

	//Create new station list
	memcpy(__pHotPlugUserList, __pUserList, __StationNum * sizeof(STATION_INFO));
	__HotPlugLoadDriveImage(pHpList[0], "StationInfoDriveA.bin");
	__HotPlugLoadDriveImage(pHpList[1], "StationInfoDriveB.bin");

	//Set station address
	if (ERROR_SUCCESS != Ecat64SendCommand(APWR_CMD, -pHpList[0]->Index, ECAT_REG_STATION_ADDR, 2, (PUCHAR)&pHpList[0]->PhysAddr)) { return -1; }
	if (ERROR_SUCCESS != Ecat64SendCommand(APWR_CMD, -pHpList[1]->Index, ECAT_REG_STATION_ADDR, 2, (PUCHAR)&pHpList[1]->PhysAddr)) { return -1; }

	//Change state to INIT
	if (ERROR_SUCCESS != __HotPlugChangeState(pHpList[0], AL_STATE_INIT)) { return -1; }
	if (ERROR_SUCCESS != __HotPlugChangeState(pHpList[1], AL_STATE_INIT)) { return -1; }

	//Set FMMUs (!!! both drives must be equal !!!)
	for (USHORT i = 0; i < pHpList[0]->FmmuNum; i++)
	{
		if (ERROR_SUCCESS != Ecat64SendCommand(FPWR_CMD, pHpList[0]->PhysAddr, ECAT_REG_FMMU0 + i * 16, 16, (PUCHAR)&pHpList[0]->FmmuList[i])) { return -1; }
		if (ERROR_SUCCESS != Ecat64SendCommand(FPWR_CMD, pHpList[1]->PhysAddr, ECAT_REG_FMMU0 + i * 16, 16, (PUCHAR)&pHpList[1]->FmmuList[i])) { return -1; }
	}

	//Set SYNCMANs (!!! both drives must be equal !!!)
	for (USHORT i = 0; i < pHpList[0]->SyncManNum; i++)
	{
		if (ERROR_SUCCESS != Ecat64SendCommand(FPWR_CMD, pHpList[0]->PhysAddr, ECAT_REG_SYNCMAN0 + i * 8, 8, (PUCHAR)&pHpList[0]->SyncManList[i])) { return -1; }
		if (ERROR_SUCCESS != Ecat64SendCommand(FPWR_CMD, pHpList[1]->PhysAddr, ECAT_REG_SYNCMAN0 + i * 8, 8, (PUCHAR)&pHpList[1]->SyncManList[i])) { return -1; }
	}

	//Change state to PREOP
	if (ERROR_SUCCESS != __HotPlugChangeState(pHpList[0], AL_STATE_PRE_OP)) { return -1; }
	if (ERROR_SUCCESS != __HotPlugChangeState(pHpList[1], AL_STATE_PRE_OP)) { return -1; }

	if (__SdoControl_iPOS4808_2003051122(pHpList[0]) == FALSE) { return -1; }
	if (__SdoControl_iPOS4808_2003051122(pHpList[1]) == FALSE) { return -1; }

	//Set state before change
	pHpList[0]->AlStatus.s.bits.State = AL_STATE_OP;
	pHpList[1]->AlStatus.s.bits.State = AL_STATE_OP;
	
	//Change station list
	__bListChange = TRUE;
	Sleep(100);

	//Change state to SAFEOP
	if (ERROR_SUCCESS != __HotPlugChangeState(pHpList[0], AL_STATE_SAFE_OP)) { return -1; }	Sleep(500);
	if (ERROR_SUCCESS != __HotPlugChangeState(pHpList[1], AL_STATE_SAFE_OP)) { return -1; }	Sleep(500);

	//Change state to OP
	if (ERROR_SUCCESS != __HotPlugChangeState(pHpList[0], AL_STATE_OP)) { return -1; }	Sleep(100);
	if (ERROR_SUCCESS != __HotPlugChangeState(pHpList[1], AL_STATE_OP)) { return -1; }	Sleep(100);

	//Everythig is OK
	return ERROR_SUCCESS;
}


void main(void)
{
//******************
#ifdef SEQ_DEBUG
	SEQ_ATTACH();						//Attach to sequence memory
	SEQ_RESET(TRUE, FALSE, NULL, 0);	//Reset/Init sequence memory
#endif
//******************

	printf("\n*** EtherCAT Core Realtime Level2 Test ***\n\n");

	//Set ethernet mode for Non-DC
	__EcatSetEthernetMode(0, REALTIME_PERIOD, SYNC_CYCLES, FALSE);
	
	//Reset global elements
	__bListChange = FALSE;
	__bLogicReady = FALSE;
	
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

		//***********************************
		//Allocate memory for HotPlug list
		//***********************************
		if (ERROR_SUCCESS == Sha64AllocMemWithTag(
											TRUE,															//Cached memory
											0,																//Memory tag
											(__StationNum + NUM_HOTPLUG_DEVICES) * sizeof(STATION_INFO),	//Memory size
											(void**)&__pHotPlugUserList,									//Pointer to memory for Windows access
											(void**)&__pHotPlugSystemList,									//Pointer to memory for Realtime access
											NULL,															//Physical memory address
											&__hHotPlugList))												//Handle to memory device
		{
			//******************************
			//Enable Stations
			//******************************

			//Change state to INIT
			if (ERROR_SUCCESS == Ecat64ChangeAllStates(AL_STATE_INIT))
			{
				//Reset devices
				Ecat64ResetDevices();

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
						//****************************
						// ToDo : SDO Download/Upload
						//****************************
						{
							//Init process telegrams (required for AL_STATE_SAFE_OP)
							Ecat64InitProcessTelegrams();

							//*************************
							//Start cyclic operation
							//*************************
							if (ERROR_SUCCESS == Ecat64CyclicStateControl(&__StateObject, AL_STATE_SAFE_OP)) { Sleep(500);
							if (ERROR_SUCCESS == Ecat64CyclicStateControl(&__StateObject, AL_STATE_OP))      { Sleep(100);

									//*********************************
									// Save HotPlug drive image once
									//*********************************
									/*{
										PSTATION_INFO pHpList[0] = (PSTATION_INFO)&__pUserList[__StationNum + 0];
										PSTATION_INFO pHpList[1] = (PSTATION_INFO)&__pUserList[__StationNum + 1];
										__HotPlugSaveDriveImage(pHpList[0], "StationInfoDriveA.bin");
										__HotPlugSaveDriveImage(pHpList[1], "StationInfoDriveB.bin");
										exit(0);
									}*/

									//******************************************************************
									//ToDo: Check and wait for HotPlug device power ON before startup
									//...
									Sleep(3000);
									//******************************************************************

									//*************************************************************
									// Hotplug Startup Drives
									// !!! Note: HotPlug devices must be end devices in line !!!
									//*************************************************************
									HotPlugStartup();
									__bLogicReady = TRUE;

									//Do a check loop
									printf("\nPress any key ...\n");
									while (!kbhit())
									{
										//Display TX and RX information
										printf("Loop Count: %i, Update Count: %i\r", __LoopCnt, __UpdateCnt);

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
			//Free Memory
			Sha64FreeMem(__hHotPlugList);
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
