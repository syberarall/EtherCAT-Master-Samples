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
		__UpdateCnt++;
	}

	//Call exit function
	__fpEcatExit();

	//Increase loop count
	__LoopCnt++;
}


__inline BOOLEAN __SdoDownloadParallel(
								PSTATION_INFO pStationList,
								ULONG SdoIndex)
{
	ULONG Result = -1;
	int InfoNum = 0;

	//Allocate memory for mailbox information list
	PMAILBOX_INFO pMailboxInfo = (PMAILBOX_INFO) malloc(__StationNum * sizeof(MAILBOX_INFO));
	if(pMailboxInfo)
	{
		//Reset memory
		memset(pMailboxInfo, 0, __StationNum*sizeof(MAILBOX_INFO));

		//Loop throug all stations
		for (int i=0; i<__StationNum; i++)
		{
			//Check station for SDO list
			PSTATION_INFO pStation = (PSTATION_INFO)&pStationList[i];
			pStation->bUpdate = FALSE;

			if (pStation->SdoNum)
			if (pStation->SdoNum > SdoIndex)
			{
				//Init mailbox information
				pMailboxInfo[InfoNum].StationIndex = pStation->Index;
				pMailboxInfo[InfoNum].Len = sizeof(SDO_LEGACY);
				memcpy((PUCHAR)&pMailboxInfo[InfoNum].Buffer, (PUCHAR)&pStation->SdoList[SdoIndex].bytes, sizeof(SDO_LEGACY));

				//Increase mail
				InfoNum++;
			}
		}

		//Check mailbox information list
		if (InfoNum)
		{
			//Check mailbox for pending response
			//Write COE command from mailbox
			//Read COE command from mailbox
			if (ERROR_SUCCESS == (Result = Ecat64MailboxCheckAll(pMailboxInfo, InfoNum)))
			if (ERROR_SUCCESS == (Result = Ecat64MailboxWriteAll(pMailboxInfo, InfoNum, MBX_TYPE_COE)))
			if (ERROR_SUCCESS == (Result = Ecat64MailboxReadAll( pMailboxInfo, InfoNum)))
			{
				//Check SDO response
				for(int i=0;i<InfoNum;i++)
				{
					//Check SDO command
					PSDO_INIT_HDR pSdoInitHdr = (PSDO_INIT_HDR)&pMailboxInfo[i].Buffer[sizeof(MBX_HDR) + sizeof(COE_HDR)];
					if (pSdoInitHdr->s.bits.Command != SDO_INIT_DOWNLOAD_RESP)
					{
						Result = -1;
						break;
					}
				}
			}
		}

		//Release TX data
		free(pMailboxInfo);
   }

   //Something failed
   return (ERROR_SUCCESS == Result) ? TRUE : FALSE;
}


void SdoDownload(void)
{
	int i=0;
	while (__SdoDownloadParallel(__pUserList, i++));
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

			//Change state to PRE OPERATIONAL
			if (ERROR_SUCCESS == Ecat64ChangeAllStates(AL_STATE_PRE_OP))
			{
				//Download station SDOs parallel
				SdoDownload();

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
