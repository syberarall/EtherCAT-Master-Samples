//*****************************************************************
//
//	This code is strictly reserved by SYBERA GmbH
//  Copyright (c) 2017 SYBERA
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
ULONG			__LoopCnt = 0;			//Realtime Cycle Counter
ULONG			__ReadyCnt = 0;			//Ready state counter
ULONG			__ErrCnt = 0;			//Error counter
ULONG			__ErrStationIndex = -1;	//Error station index
STATE_OBJECT    __StateObject = { 0 };	//Cyclic state object
BOOLEAN			__bErrFlag = FALSE;		//Ethernet error flag


__inline void __CheckError(void)
{
	//Check station error
	for (int i=0; i<__StationNum; i++)
		if (__pSystemList[i].RxTel.s.hdr.irq & (1<<15))
		{
			//Reset error
			__pSystemList[i].RxTel.s.hdr.irq &= ~(1<<15);

			//Set error count and station index
			__ErrStationIndex = i;
			__ErrCnt++;
		}

	//Check general PHY error
	__bErrFlag = __pSystemStack->hdr.err_flag;
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
		//DBG_INITIAL_BREAK();

		//*********************************
		//Do the logical station operation
		//e.g. set input to output value (station index == 2)
		//__pSystemList[2].TxTel.s.data[0] = __pSystemList[1].RxTel.s.data[0];
		//*********************************

		__UpdateCnt++;
	}

	//Call exit function
	__fpEcatExit();

	//Increase loop count
	__LoopCnt++;
}


__inline void __CheckStationError(PSTATION_INFO pStation)
{
	RX_ERR_CNT		RxErrCnts = { 0 }; 
	ADD_ERR_CNT		AddErrCnts = { 0 };
	LOST_LINK_CNT	LostLinkCnts = { 0 };

	//First try to reset ethernet core error flag
	if (__pUserStack->hdr.err_flag)
		__pUserStack->hdr.err_flag = FALSE;

	//Do some delay
	Sleep(100);

	//Check flag again
	if (__pUserStack->hdr.err_flag == FALSE)
	{
		//Send ethercat command
		if (ERROR_SUCCESS == Ecat64SendCommand(FPRD_CMD, pStation->PhysAddr, 0x300, sizeof(RX_ERR_CNT), (PUCHAR)&RxErrCnts))
		if (ERROR_SUCCESS == Ecat64SendCommand(FPRD_CMD, pStation->PhysAddr, 0x308, sizeof(ADD_ERR_CNT), (PUCHAR)&AddErrCnts))
		if (ERROR_SUCCESS == Ecat64SendCommand(FPRD_CMD, pStation->PhysAddr, 0x310, sizeof(LOST_LINK_CNT), (PUCHAR)&LostLinkCnts))
		{
			//Print station error counters
			printf("FrameErrCntPort0 : %i\n", RxErrCnts.s.FrameErrCntPort0);
			printf("PhysErrCntPort0  : %i\n", RxErrCnts.s.PhysErrCntPort0);
			printf("FrameErrCntPort1 : %i\n", RxErrCnts.s.FrameErrCntPort1);
			printf("PhysErrCntPort1  : %i\n", RxErrCnts.s.PhysErrCntPort1);
			printf("FrameErrCntPort2 : %i\n", RxErrCnts.s.FrameErrCntPort2);
			printf("PhysErrCntPort2  : %i\n", RxErrCnts.s.PhysErrCntPort2);
			printf("FrameErrCntPort3 : %i\n", RxErrCnts.s.FrameErrCntPort3);
			printf("PhysErrCntPort3  : %i\n", RxErrCnts.s.PhysErrCntPort3);

			printf("PrevErrCntPort0  : %i\n", AddErrCnts.s.PrevErrCntPort0);
			printf("PrevErrCntPort1  : %i\n", AddErrCnts.s.PrevErrCntPort1);
			printf("PrevErrCntPort2  : %i\n", AddErrCnts.s.PrevErrCntPort2);
			printf("PrevErrCntPort3  : %i\n", AddErrCnts.s.PrevErrCntPort3);
			printf("MalFormErrCnt    : %i\n", AddErrCnts.s.MalFormErrCnt);
			printf("LocProblemErrCnt : %i\n", AddErrCnts.s.LocProblemErrCnt);

			printf("LostLinkCntPort0 : %i\n", LostLinkCnts.s.LostLinkCntPort0);
			printf("LostLinkCntPort1 : %i\n", LostLinkCnts.s.LostLinkCntPort1);
			printf("LostLinkCntPort2 : %i\n", LostLinkCnts.s.LostLinkCntPort2);
			printf("LostLinkCntPort3 : %i\n", LostLinkCnts.s.LostLinkCntPort3);

			//Wait for key pressed
			printf("Press any key ...\n");
			while (!kbhit()) { Sleep(100); }
		}
	}
}


void main(void)
{
	printf("\n*** EtherCAT Core Realtime Level2 Test ***\n\n");

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
								//Do some delay
								Sleep(100);

								//Do a check loop
								printf("\nPress any key ...\n"); 
								while (!kbhit())
								{
									//Display TX and RX information
									printf("Loop Count: %i, Update Count: %i, Error Count: %i, Error Flag: %i\r", __LoopCnt, __UpdateCnt, __ErrCnt, __bErrFlag);

									//***************************************************************
									//Check station error
									if (__ErrStationIndex != -1)
									{
										PSTATION_INFO pStation = &__pUserList[__ErrStationIndex];
										printf("\n\nError on Station %s, Index: %i\n", pStation->szName, __ErrStationIndex);
										__CheckStationError(pStation);
									}
									//***************************************************************

									//Do some delay
									Sleep(100);
								}

								//********************************************************
								//Change state to INIT cyclic
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
