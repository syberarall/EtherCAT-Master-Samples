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


void static AppTask(PVOID)
{
	//Call enter wrapper function
	__EcatState = __fpEcatEnter(
							__pSystemStack,
							__pSystemList,
							(USHORT)__StationNum,
							NULL);

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

//************************************************************************************
//e.g. mcDSA-xx-EtherCAT
UCHAR	__FmmuList[2][16] = {
                              { 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x07, 0x00, 0x10, 0x00, 0x02, 0x01, 0x00, 0x00, 0x00 },
                              { 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x07, 0x00, 0x11, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00 }
                            };


__inline int __InitFmmus(
						PSTATION_INFO pStation,
						PUCHAR* ppFmmuList,
						USHORT FmmuNum)
{
	USHORT i, ado = 0x600;

	//Send all FMMUs of list
	pStation->FmmuNum = 0;
	for (i=0; i<FmmuNum; i++)
	{
		//Save number of FMMUs
		if (ERROR_SUCCESS == Ecat64SendCommand(FPWR_CMD, pStation->PhysAddr, ado, sizeof(FMMU), (PUCHAR)ppFmmuList[i]))
		{
			//Save FMMU to station
			memcpy(&pStation->FmmuList[i], ppFmmuList[i], sizeof(FMMU));
			pStation->FmmuNum++;
		}

		//Increase ADO
		ado += sizeof(FMMU);
	}

	//Return number of FMMUs sent
	return i;
}

//************************************************************************************
//e.g. mcDSA-xx-EtherCAT
UCHAR	__SyncmanList[4][8] = {
								{ 0x00, 0x12, 0x20, 0x00, 0x26, 0x00, 0x01, 0x00 },
								{ 0x20, 0x12, 0x20, 0x00, 0x22, 0x00, 0x01, 0x00 },
								{ 0x00, 0x10, 0x10, 0x00, 0x64, 0x00, 0x01, 0x00 },
								{ 0x00, 0x11, 0x10, 0x00, 0x20, 0x00, 0x01, 0x00 }
                              };

__inline int __InitSyncmans(
						PSTATION_INFO pStation,
						PUCHAR* ppSyncmanList,
						USHORT SyncmanNum)
{
	USHORT i, ado = 0x800;

	//Send all sync managers of list
	pStation->SyncManNum = 0;
	for (i=0; i<SyncmanNum; i++)
	{
		//Save number of sync managers
		if (ERROR_SUCCESS == Ecat64SendCommand(FPWR_CMD, pStation->PhysAddr, ado, sizeof(SYNCMAN), (PUCHAR)ppSyncmanList[i]))
		{
			//Save sync manager to station
			memcpy(&pStation->SyncManList[i], ppSyncmanList[i], sizeof(SYNCMAN));
			pStation->SyncManNum++;
		}

		//Increase ADO
		ado += sizeof(SYNCMAN);
	}

	//Return number of sync managers sent
	return i;
}
//************************************************************************************

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
			{
				int Index = __GetStationIndexByName(__pUserList, __StationNum, "mcDCA");
				PSTATION_INFO pStation = &__pUserList[Index];

				//Set FMMUs and SYNCMANs manually (instead out of ECATDEVICE.PAR)
				if (__InitFmmus(   pStation, (PUCHAR*)__FmmuList,    sizeof(__FmmuList)/sizeof(FMMU)))
				if (__InitSyncmans(pStation, (PUCHAR*)__SyncmanList, sizeof(__SyncmanList)/sizeof(SYNCMAN)))
				{
					//e.g. Set 6 bytes input and output
					pStation->InDescList[0].Len  = 6;
					pStation->OutDescList[0].Len = 6;

					//Change state to PRE OPERATIONAL
					//Init PDO assignment
					if (ERROR_SUCCESS == Ecat64ChangeAllStates(AL_STATE_PRE_OP))
					if (ERROR_SUCCESS == Ecat64PdoAssignment())
					{
						//Init process telegrams (required for AL_STATE_SAFE_OP)
						InitProcessTelegrams();
					
						//Change state to SAFE OPERATIONAL
						if (ERROR_SUCCESS == Ecat64ChangeAllStates(AL_STATE_SAFE_OP))
						{
							//Do some delay
							Sleep(500);

							//Change state to OPERATIONAL
							if (ERROR_SUCCESS == Ecat64ChangeAllStates(AL_STATE_OP))
							{

								//Do a check loop
								printf("\nPress any key ...\n"); 
								while (!kbhit())
								{
									//Display TX and RX information
									printf("Loop Count: %i, Update Count: %i\r", __LoopCnt, __UpdateCnt);

									//Do some delay
									Sleep(100);
								}

								//Change state to INIT
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
