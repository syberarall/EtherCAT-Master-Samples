//*****************************************************************
//
//	This code is strictly reserved by SYBERA GmbH
//  Copyright (c) 2018 SYBERA
//
//*****************************************************************

#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include "SpsCommon.h"
#include "c:\sha\Sha64Exp.h"
#include "c:\sha\Sha64Debug.h"
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

//************************************
//#define SEQ_DEBUG			1
//************************************

#define REALTIME_PERIOD		1000		//Realtime sampling period [usec]
#define SYNC_CYCLES			   2

//Declare global elements
PETH_STACK		__pUserStack = NULL;	//Ethernet Core Stack (used outside Realtime Task)
PETH_STACK		__pSystemStack = NULL;	//Ethernet Core Stack (used inside Realtime Task)
PSTATION_INFO	__pUserList = NULL;		//Station List (used outside Realtime Task)
PSTATION_INFO	__pSystemList = NULL;	//Station List (used inside Realtime Task)
PSPS_POOL		__pUserPool = NULL;		//SPS Pool (used outside Realtime Task)
PSPS_POOL		__pSystemPool = NULL;	//SPSe Pool (used inside Realtime Task)
USHORT			__StationNum = 0;		//Number of Stations
FP_ECAT_ENTER	__fpEcatEnter = NULL;	//Function pointer to Wrapper EcatEnter
FP_ECAT_EXIT	__fpEcatExit = NULL;	//Function pointer to Wrapper EcatExit
ULONG			__EcatState = 0;		//Initial Wrapper State
ULONG			__UpdateCnt = 0;		//Station Update Counter
ULONG			__LoopCnt = 0;			//Realtime Cycle Counter
ULONG			__ReadyCnt = 0;			//Ready state counter
STATE_OBJECT    __StateObject = { 0 };	//Cyclic state object
BOOLEAN         __bSpsReady = FALSE;	//SPS ready flag


//***************************
#ifdef SEQ_DEBUG
	SEQ_INIT_ELEMENTS(); //Init sequencing elements
#endif
//***************************


__inline ULONG __CopyTxData(
						PSTATION_INFO pStation,
						PUCHAR pData,
						ULONG MaxSize)
{
	ULONG i, Size = 0;

	//Get station data length
	for (i=0; i<pStation->OutDescNum; i++)
		Size += pStation->OutDescList[i].Len;

//****************************
#ifdef SEQ_DEBUG
	SYSTEM_SEQ_CAPTURE("TX", __LINE__, Size, FALSE); //Set sequence capture
#endif
//****************************

	//Check data len
	if (Size >= MaxSize)
		return 0;
	
	//Copy SPS pool data to TX telegram
	memcpy(pStation->TxTel.s.data, pData, Size);

	//Return data size
	return Size;
}


__inline ULONG __CopyRxData(
						PSTATION_INFO pStation,
						PUCHAR pData,
						ULONG MaxSize)
{
	USHORT i, Size = 0;

	//Get station data length
	for (i=0; i<pStation->InDescNum; i++)
		Size += pStation->InDescList[i].Len;

//****************************
#ifdef SEQ_DEBUG
	SYSTEM_SEQ_CAPTURE("RX", __LINE__, Size, FALSE); //Set sequence capture
#endif
//****************************

	//Check data len
	if (Size >= MaxSize)
		return 0;

	//Copy SPS pool data to TX telegram
	memcpy(pData, pStation->RxTel.s.data, Size);

	//Return data size
	return Size;
}


void static UpdateSpsPool(void)
{
	ULONG i, Offs;

	//Set busy flag
	__pSystemPool->bBusy = TRUE;

	//Set pool busy flag
	if (__pSystemPool->bLock == FALSE)
	{
		//Loop through all stations
		Offs = 0;
		for (i=0; i<__StationNum; i++)
		if ((i < MAX_STATION_NUM) && (Offs < MAX_IMAGE_SIZE))
		{
//****************************
#ifdef SEQ_DEBUG
	SYSTEM_SEQ_CAPTURE("TX", __LINE__, Offs, FALSE); //Set sequence capture
#endif
//****************************
			//Copy data and increase pool offset
			Offs += __CopyTxData(
							&__pSystemList[i],
							&__pSystemPool->ImageData[TX_IMAGE][Offs],
							MAX_IMAGE_SIZE - Offs);
		}
		//Save Image size
		__pSystemPool->ImageSize[TX_IMAGE] = (USHORT)Offs;


		//Loop through all stations
		Offs = 0;
		for (i=0; i<__StationNum; i++)
		if ((i < MAX_STATION_NUM) && (Offs < MAX_IMAGE_SIZE))
		{
//****************************
#ifdef SEQ_DEBUG
	SYSTEM_SEQ_CAPTURE("RX", __LINE__, Offs, FALSE); //Set sequence capture
#endif
//****************************

			//Copy data and increase pool offset
			Offs += __CopyRxData(
							&__pSystemList[i],
							&__pSystemPool->ImageData[RX_IMAGE][Offs],
							MAX_IMAGE_SIZE - Offs);
		}
		//Save Image size
		__pSystemPool->ImageSize[RX_IMAGE] = (USHORT)Offs;
	}

	//Reset pool busy flag
	__pSystemPool->bBusy = FALSE;
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
		//******************************************************************
		if (__bSpsReady) { UpdateSpsPool(); }
		//******************************************************************

		__UpdateCnt++;
	}

	//Call exit function
	__fpEcatExit();
	
	//Increase loop count
	__LoopCnt++;

	//Update SPS pool flags
	__pSystemPool->bError = __pSystemStack->hdr.err_flag;
	__pSystemPool->bRun   = __pSystemStack->hdr.run_flag;
}


void main(void)
{
//******************
#ifdef SEQ_DEBUG
	SEQ_ATTACH();					//Attach to sequence memory
	SEQ_RESET(TRUE, TRUE, NULL, 0);	//Reset/Init sequence memory
#endif
//******************

	ULONG PoolSize = 0;
	HANDLE hPool = NULL;

	printf("\n*** EtherCAT Drive Test ***\n\n");

	//******************************
	//Attach to the BOOT memory
	//******************************
	if (ERROR_SUCCESS == Sha64AttachMemWithTag(
										BOOT_MEM_TAG,			//Memory tag
										&PoolSize,				//Memory size
										(void**)&__pUserPool,	//Pointer to memory for Windows access
										(PVOID*)&__pSystemPool,	//Pointer to memory for Realtime access
										NULL,					//Physical memory address
										&hPool))					//Handle to memory device
	{
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

				//Start SPS operation
				Sleep(100);
				__bSpsReady = TRUE;

				//Init pool elements
				__pUserPool->pStationList = __pUserList;
				__pUserPool->StationNum   = __StationNum;

//*****************************************************************************
UCHAR InData[100] = { 0 };
UCHAR OutData[100] = { 0 };
WRITE_POOL_DATA(__pUserPool, "\x5", 0, 1);
WRITE_POOL_DATA(__pUserPool, "\x05\x00", (1 + 4), 2);
WRITE_POOL_DATA(__pUserPool, "\xff\x0f", (1 + 6), 2);
READ_POOL_DATA(__pUserPool, InData, 0, __pUserPool->ImageSize[RX_IMAGE]);
//*****************************************************************************

				//Do a check loop
				printf("\nPress any key ...\n"); 
			  	while (!kbhit())
				{
					//Display TX and RX information
					printf("Update Count: %i\r", __UpdateCnt);

					//Do some delay
					Sleep(100);
				}
	
				//Stop the SPS operation
				__bSpsReady = FALSE;
				Sleep(100);
	
				//Disable Stations
				Sha64EcatDisableDC(&EcatParams, &__StateObject);
			}
			//Destroy ECAT core
 			Sha64EcatDestroy(&EcatParams);
		}
		//Release BOOT memory
		Sha64DetachMem(hPool);

		//Check Status
		if		(EcatParams.EthParams.pUserStack == NULL)	{ printf("\n*** Ethernet Stack Failed ***\n"); }
		else if (EcatParams.EthParams.err_cnts.Phy != 0)	{ printf("\n*** No Link ***\n"); }
		else if (EcatParams.pUserList == NULL)				{ printf("\n*** No Station List ***\n"); }
		else if (EcatParams.StationNum == 0)				{ printf("\n*** No Station Found ***\n"); }
		else												{ printf("\n*** OK ***\n"); }
	}

	//Wait for key press
	printf("\nPress any key ...\n"); 
	while (!kbhit()) { Sleep(100); }

//******************
#ifdef SEQ_DEBUG
	SEQ_DETACH(); //Detach from sequence memory
#endif
//******************
}
