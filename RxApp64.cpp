//*****************************************************************
//
//	This code is strictly reserved by SYBERA GmbH
//  Copyright (c) 2012 SYBERA
//
//*****************************************************************
//
// This sample demonstrates the RTX process of a 2-process solution
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
#include "c:\sha\sha64exp.h"
#include "c:\sha\sha64debug.h"
#include "..\inc\common.h"

#pragma comment( lib, "c:\\sha\\sha64dll.lib" )
#pragma comment( lib, "c:\\eth\\sha64ethcore.lib" )
#pragma comment( lib, "c:\\ect\\sha64ecatcore.lib" )

#pragma warning(disable:4996)
#pragma warning(disable:4748)

//************************************
//#define SEQ_DEBUG			1
//************************************

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
ULONG			__LoopCnt = 0;			//Realtime Cycle Counter
ULONG			__ReadyCnt = 0;			//Ready state counter
STATE_OBJECT    __StateObject = { 0 };	//Cyclic state object
PMEM_INFO		__pUserMem = NULL;
PMEM_INFO		__pSystemMem = NULL;
ULONG			__LogicState = 0;


//***************************
#ifdef SEQ_DEBUG
	SEQ_INIT_ELEMENTS(); //Init sequencing elements
#endif
//***************************


void static ReadInput(void)
{
	//Check, if DI data has been processed
	if (__pSystemMem->bDiValid == TRUE)
		return;
	
	PSTATION_INFO pStation = __EcatGetStation(
										__pSystemList,
										__StationNum, 0x3ea);

	if (pStation)	{ __pSystemMem->DiData = pStation->RxTel.s.data[0]; }
	
	//Set DI valid flag
	__pSystemMem->bDiValid = TRUE;
}


void static DoLogic(void)
{
//****************************
#ifdef SEQ_DEBUG
	SYSTEM_SEQ_CAPTURE("SEQ", __LINE__, 0, TRUE);
#endif
//****************************
}


void static WriteOutput(void)
{
	//Check, if DO data is valid
	if (__pSystemMem->bDoValid == FALSE)
		return;
	
	PSTATION_INFO pStation = __EcatGetStation(
										__pSystemList,
										__StationNum, 0x3eb);


	if (pStation)	{ pStation->TxTel.s.data[0] = __pSystemMem->DoData; }
	
	//DO data has been sent
	__pSystemMem->bDoValid = FALSE;
}


void static AppTask(PVOID)
{
	//Call enter wrapper function
	__EcatState = __fpEcatEnter(
							__pSystemStack,
							__pSystemList,
							(USHORT)__StationNum,
							&__StateObject);

	//Check operation state and decrease ready count
	if (__EcatState == ECAT_STATE_READY) { __ReadyCnt--; }
	else                                 { __ReadyCnt = SYNC_CYCLES; }

	//Check ready count
	if (__ReadyCnt == 1)
	{
		//*********************************
		//Read input data
		ReadInput();

		//*********************************
		//Do logic
		DoLogic();

		//*********************************
		//Write output data
		WriteOutput();

//****************************
#ifdef SEQ_DEBUG
	SYSTEM_SEQ_CAPTURE("SEQ", __LINE__, __pSystemMem->DiData, TRUE);
	SYSTEM_SEQ_CAPTURE("SEQ", __LINE__, __pSystemMem->DoData, FALSE);
#endif
//****************************

		//Increase update count
		__UpdateCnt++;
	}

	//Call exit wrapper function
	__fpEcatExit();
	
	//Increase loop count
	__LoopCnt++;
}


void main(void)
{
//******************
#ifdef SEQ_DEBUG
	SEQ_ATTACH();					//Attach to sequence memory
	SEQ_RESET(TRUE, TRUE, NULL, 0);	//Reset/Init sequence memory
#endif
//******************

	HANDLE	hMem;
	ULONG	MemSize;
	ULONG	Error = 0;

	printf("\n*** EtherCAT SPS Application ***\n\n");

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
	//Attach to the BOOT memory
	//******************************
	if (ERROR_SUCCESS == Sha64AttachMemWithTag(
										BOOT_MEM_TAG,			//Memory tag
										&MemSize,				//Memory size
										(void**)&__pUserMem,	//Pointer to memory for Windows access
										(PVOID*)&__pSystemMem,	//Pointer to memory for Realtime access
										NULL,					//Physical memory address
										&hMem))					//Handle to memory device
	{
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

			//******************************
			//Enable Stations
			//******************************
			if (ERROR_SUCCESS == Sha64EcatEnableDC(&EcatParams, &__StateObject))
			{
				//Display version information
				Sha64EcatGetVersion(&EcatParams);
				printf("ECTCORE-DLL : %.2f\nECTCORE-DRV : %.2f\n", EcatParams.core_dll_ver / (double)100, EcatParams.core_drv_ver / (double)100);
				printf("ETHCORE-DLL : %.2f\nETHCORE-DRV : %.2f\n", EcatParams.EthParams.core_dll_ver / (double)100, EcatParams.EthParams.core_drv_ver / (double)100);
				printf("SHA-LIB     : %.2f\nSHA-DRV     : %.2f\n", EcatParams.EthParams.sha_lib_ver / (double)100, EcatParams.EthParams.sha_drv_ver / (double)100);
				printf("\n");

				//Display station information
				for (int i=0; i<__StationNum; i++)
					printf("Station: %i, Name: %6s\n", i, __pUserList[i].szName);

				__pUserMem->DoData = 0x0f;

				//Do a check loop
				printf("\nPress any key ...\n"); 
				while (!kbhit())
				{
					//*************************************
					//Display TX and RX information
					printf("DiData:0x%02x, Loop Count:%i, Update Count:%i\r", __pUserMem->DiData, __LoopCnt, __UpdateCnt);

					//Do some delay
					Sleep(100);
					//*************************************
				}

				//Disable Stations
				Sha64EcatDisableDC(&EcatParams, &__StateObject);
			}
			//Destroy ECAT core
			Sha64EcatDestroy(&EcatParams);
		}
		//Release global tagged memory
		Sha64DetachMem(hMem);
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
