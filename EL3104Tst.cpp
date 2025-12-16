//*****************************************************************
//
//	This code is strictly reserved by SYBERA GmbH
//  Copyright (c) 2017 SYBERA
//
//*****************************************************************
//
// EL3702 Test
//

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
//#define SEQ_DEBUG			1
//#define SILENT_MODE		1
//#define DC_CONTROL		1
//************************************

#define REALTIME_PERIOD	  500		//Realtime sampling period [usec]
#define SYNC_CYCLES			2
#define MAX_SYSTIME_DIFF  500		//[nsec]


//************************************************
//!!! Structs need to have 1 byte alignment !!!
#pragma pack(push, old_alignment)
#pragma pack(1)

typedef union _EL3104
{
	struct
	{
		USHORT	ch1_stat;		//EL3104 : 2 Byte - channel 1 analog status
		USHORT	ch1_data;		//EL3104 : 2 Byte - channel 1 analog input
		USHORT	ch2_stat;		//EL3104 : 2 Byte - channel 2 analog status
		USHORT	ch2_data;		//EL3104 : 2 Byte - channel 2 analog input
		USHORT	ch3_stat;		//EL3104 : 2 Byte - channel 3 analog status
		USHORT	ch3_data;		//EL3104 : 2 Byte - channel 3 analog input
		USHORT	ch4_stat;		//EL3104 : 2 Byte - channel 4 analog status
		USHORT	ch4_data;		//EL3104 : 2 Byte - channel 4 analog input
	} s;

	UCHAR bytes[16];
} EL3104, *PEL3104;

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
ULONG			__DeviceIndex = 0;
EL3104			__El3104Map = { 0 };
UCHAR			__EL3104SdoInitCh1[] = { 0x00, 0x20, 0x31, 0x00, 0x80, 0x01, 0x12, 0x00,   0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00,   0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x40 };
UCHAR			__EL3104SdoInitCh2[] = { 0x00, 0x20, 0x31, 0x10, 0x80, 0x01, 0x12, 0x00,   0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00,   0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x40 };
UCHAR			__EL3104SdoInitCh3[] = { 0x00, 0x20, 0x31, 0x20, 0x80, 0x01, 0x12, 0x00,   0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00,   0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x40 };
UCHAR			__EL3104SdoInitCh4[] = { 0x00, 0x20, 0x31, 0x30, 0x80, 0x01, 0x12, 0x00,   0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00,   0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,   0x00, 0x00, 0x00, 0x40 };


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
		//Get station pointer by index
		PSTATION_INFO pDevice = &__pSystemList[__DeviceIndex];

		//Get RX telegram data
		memcpy(&__El3104Map, pDevice->RxTel.s.data, sizeof(EL3104));

		__UpdateCnt++;

//****************************
#ifdef SEQ_DEBUG
	SYSTEM_SEQ_CAPTURE("SEQ", __LINE__, __UpdateCnt, TRUE);
#endif
//****************************
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
	//Loop through station list
	for (USHORT i=0; i<StationNum;i++)
		if (strstr(pStationList[i].szName, pszStationName))
			return i;

	//Drive not found
	return 0;
}


__inline BOOLEAN __SdoVariableDownload(
								PSTATION_INFO pStation,
								PUCHAR pSdo,
								USHORT SdoLen)
{
	ULONG Result = -1;

	//Allocate mailbox data
	USHORT MailboxSize = pStation->SyncManList[0].s.Length;
	PUCHAR pMailbox = (PUCHAR)malloc(MailboxSize);
	if (pMailbox)
	{
		//Reset mailbox data
		memset(pMailbox, 0, MailboxSize);
		memcpy(pMailbox, pSdo, SdoLen);

		//Check mailbox for pending response
		//Write COE command from mailbox
		//Read COE command from mailbox
//		if (ERROR_SUCCESS == (Result = Ecat64MailboxCheck(pStation)))
		if (ERROR_SUCCESS == (Result = Ecat64MailboxWrite(pStation, pMailbox, SdoLen, MBX_TYPE_COE)))
		if (ERROR_SUCCESS == (Result = Ecat64MailboxRead(pStation, pMailbox)))
		{
			//Check SDO command
			PSDO_INIT_HDR pSdoInitHdr = (PSDO_INIT_HDR)&pSdo[sizeof(MBX_HDR) + sizeof(COE_HDR)];
			if (pSdoInitHdr->s.bits.Command != SDO_INIT_DOWNLOAD_RESP)
				Result = -1;
		}
		//Release mailbox
		free(pMailbox);
	}

	//Something failed
	return (ERROR_SUCCESS == Result) ? TRUE : FALSE;
}


void main(void)
{
//******************
#ifdef SEQ_DEBUG
	SEQ_ATTACH();						//Attach to sequence memory
	SEQ_RESET(TRUE, FALSE, NULL, 0);	//Reset/Init sequence memory
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
		Sleep(1000);

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
			__DeviceIndex = __GetStationIndexByName(__pUserList, __StationNum, "EL3104");

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
				if (ERROR_SUCCESS == Ecat64ChangeAllStates(AL_STATE_PRE_OP))
				{
					//Init PDO assignment
					Ecat64PdoAssignment();
					__SdoVariableDownload(&__pUserList[__DeviceIndex], __EL3104SdoInitCh1, sizeof(__EL3104SdoInitCh1));
					__SdoVariableDownload(&__pUserList[__DeviceIndex], __EL3104SdoInitCh2, sizeof(__EL3104SdoInitCh2));
					__SdoVariableDownload(&__pUserList[__DeviceIndex], __EL3104SdoInitCh3, sizeof(__EL3104SdoInitCh3));
					__SdoVariableDownload(&__pUserList[__DeviceIndex], __EL3104SdoInitCh4, sizeof(__EL3104SdoInitCh4));

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
#ifdef SILENT_MODE
						if (ERROR_SUCCESS == __Ecat64SilentChangeState(__pUserStack, REALTIME_PERIOD, AL_STATE_OP)) { {
#else
						if (ERROR_SUCCESS == Ecat64CyclicStateControl(&__StateObject, AL_STATE_SAFE_OP)) { Sleep(500);
						if (ERROR_SUCCESS == Ecat64CyclicStateControl(&__StateObject, AL_STATE_OP))      { Sleep(100);
#endif
								//Do a check loop
								printf("\nPress any key ...\n"); 
								while (!kbhit())
								{
									//Display TX and RX information
									printf("CH1:%04x-%04x, CH2:%04x-%04x, CH3:%04x-%04x, CH4:%04x-%04x Update:%i\r",
										__El3104Map.s.ch1_data,
										__El3104Map.s.ch1_stat,
										__El3104Map.s.ch2_data,
										__El3104Map.s.ch2_stat,
										__El3104Map.s.ch3_data,
										__El3104Map.s.ch3_stat,
										__El3104Map.s.ch4_data,
										__El3104Map.s.ch4_stat,
										__UpdateCnt);

									//Do some delay
									Sleep(100);
								}
	
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
