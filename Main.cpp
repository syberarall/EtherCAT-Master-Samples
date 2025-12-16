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
#include "c:\ect\Ecat64Macros.h"
#include "c:\ect\Ecat64Control.h"
#include "c:\ect\Ecat64SilentDef.h"

#pragma comment( lib, "c:\\sha\\sha64dll.lib" )
#pragma comment( lib, "c:\\eth\\sha64ethcore.lib" )
#pragma comment( lib, "c:\\ect\\sha64ecatcore.lib" )

#pragma warning(disable:4996)
#pragma warning(disable:4748)

#define MAX_SYSTIME_DIFF	 500			//[nsec]
#define REALTIME_PERIOD	 1000		//Realtime sampling period [usec]
#define SYNC_CYCLES		    2

//************************************
#define SEQ_DEBUG			1
//#define SILENT_MODE		1
//#define DC_CONTROL		1
//************************************

//Declare global elements
PETH_STACK		__pUserStack = NULL;	//Ethernet Core Stack (used outside Realtime Task)
PETH_STACK		__pSystemStack = NULL;	//Ethernet Core Stack (used inside Realtime Task)
PSTATION_INFO	__pUserList = NULL;		//Station List (used outside Realtime Task)
PSTATION_INFO	__pSystemList = NULL;	//Station List (used inside Realtime Task)
USHORT			__StationNum = 0;		//Number of Stations
FP_ECAT_ENTER	__fpEcatEnter = NULL;	//Function pointer to Wrapper EcatEnter
FP_ECAT_EXIT	__fpEcatExit = NULL;	//Function pointer to Wrapper EcatExit
ULONG			__EcatState = 0;		//CyclicSdoControl Wrapper State
ULONG			__UpdateCnt = 0;		//Station Update Counter
ULONG			__LoopCnt = 0;			//Realtime Cycle Counter
ULONG			__ReadyCnt = 0;			//Ready state counter
STATE_OBJECT    __StateObject = { 0 };	//Cyclic state object

ULONG           __DebugCnt = 0;


//Get 


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


void static DoLogic()
{
	//Do the logical station operation
	//e.g. set input to output value (station index == 2)
	//__pSystemList[2].TxTel.s.data[0] = __pSystemList[1].RxTel.s.data[0];
}


enum _SDO_CONTROL_STATES
{
	SDO_CONTROL_IDLE = 0,
	SDO_UPLOAD_REQ_WRITE,
	SDO_UPLOAD_REQ_READ,
	SDO_UPLOAD_RESP_WRITE,
	SDO_UPLOAD_RESP_READ
};


USHORT __WorkCount = 0;
UCHAR __Mailbox[MAX_ECAT_DATA] = { 0 };
ULONG __DataOffs = 0;
ULONG __DataSize = 0;
ULONG __ControlState = SDO_CONTROL_IDLE;
USHORT __SdoIndex = 0;
USHORT __SdoSubIndex = 0;


__inline SHORT __UpdateMailboxCount(PSHORT pMailboxCnt)
{
	//Return updated mailbox count (range 1 - 7)
	return (*pMailboxCnt = (*pMailboxCnt % 7) + 1);
}


__inline BOOLEAN __SdoInitUploadReqWrite(
								PSTATION_INFO pStation,
								USHORT SdoIndex,
								UCHAR SdoSubIndex)
{
	PUCHAR pMailbox = (PUCHAR)__Mailbox;
	USHORT MailboxOffs = pStation->SyncManList[0].s.PhysicalAddr;
	USHORT MailboxSize = pStation->SyncManList[0].s.Length;
	USHORT CoeDataSize = sizeof(COE_HDR) + sizeof(SDO_INIT_HDR) + sizeof(ULONG);

	//Set mailbox header
	PMBX_HDR pMbxHdr = (PMBX_HDR)&__Mailbox[0];
	pMbxHdr->s.Length = CoeDataSize;
	pMbxHdr->s.Address = 0x0000;
	pMbxHdr->s.bits.Channel = 0;
	pMbxHdr->s.bits.Priority = 0;
	pMbxHdr->s.bits.Type = MBX_TYPE_COE;
	pMbxHdr->s.bits.Cnt = __UpdateMailboxCount(&pStation->ExtInfo.MailboxCnt);

	//Set CoE header
	PCOE_HDR pCoeHdr = (PCOE_HDR)&__Mailbox[sizeof(MBX_HDR)];
	pCoeHdr->bits.Num = 0;
	pCoeHdr->bits.Service = COE_SERVICE_SDOREQ;

	//Set SDO Init header (SDO Init Upload Expedited Request)
	PSDO_INIT_HDR pSdoInitHdr = (PSDO_INIT_HDR)&__Mailbox[sizeof(MBX_HDR) + sizeof(COE_HDR)];
	pSdoInitHdr->s.bits.SizeIndicator = FALSE;
	pSdoInitHdr->s.bits.TransferType = FALSE;
	pSdoInitHdr->s.bits.DataSetSize = 0;
	pSdoInitHdr->s.bits.CompleteAccess = FALSE;
	pSdoInitHdr->s.bits.Command = SDO_INIT_UPLOAD_REQ;
	pSdoInitHdr->s.Index = SdoIndex;
	pSdoInitHdr->s.SubIndex = SdoSubIndex;

	//Set working count
	__WorkCount = 0x0000;

	//Set mailbox write telegram
	__EcatSetCyclicTelegram(
						&pStation->TxTel,
						(UCHAR)pStation->Index,
						FPWR_CMD,
						pStation->PhysAddr,
						MailboxOffs,
						MailboxSize,
						(PUCHAR)pMailbox,
						__WorkCount);

	//Set station update
	pStation->bUpdate = TRUE;
	return TRUE;
}


__inline BOOLEAN __SdoInitUploadReqRead(PSTATION_INFO pStation)
{
	//Check working count
	PUSHORT pRxWorkCnt = (PUSHORT)&pStation->RxTel.bytes[sizeof(ECAT_TELEGRAM_HDR) + pStation->RxTel.s.hdr.bits.length];
	return ((*pRxWorkCnt) == (__WorkCount + 1)) ? FALSE : TRUE;
}


__inline ULONG __SdoInitUploadRespWrite(PSTATION_INFO pStation)
{
	PUCHAR pMailbox = (PUCHAR)&__Mailbox[0];
	USHORT MailboxOffs = pStation->SyncManList[1].s.PhysicalAddr;
	USHORT MailboxSize = pStation->SyncManList[1].s.Length;

	//Set working count
	__WorkCount = 0x0000;

	//Set mailbox write telegram
	__EcatSetCyclicTelegram(
						&pStation->TxTel,
						(UCHAR)pStation->Index,
						FPRD_CMD,
						pStation->PhysAddr,
						MailboxOffs,
						MailboxSize,
						(PUCHAR)pMailbox,
						0x0000);
	//Set station update
	pStation->bUpdate = TRUE;
	return TRUE;
}


__inline BOOLEAN __SdoInitUploadRespRead(
									PSTATION_INFO pStation,
									PULONG pSdoSize,
									PULONG pMailboxOffs)
{
	//Reset data offset and size
	__DataOffs = sizeof(MBX_HDR) + sizeof(COE_HDR);
	__DataSize = 0;

	//Check working count
	PUSHORT pRxWorkCnt = (PUSHORT)&pStation->RxTel.bytes[sizeof(ECAT_TELEGRAM_HDR) + pStation->RxTel.s.hdr.bits.length];
	if ((*pRxWorkCnt) == (__WorkCount + 1))
	{
		PECAT_TELEGRAM_HDR pTelHdr = (PECAT_TELEGRAM_HDR)&pStation->RxTel.bytes[0];
		PUCHAR pMailbox = &pStation->RxTel.bytes[sizeof(ECAT_TELEGRAM_HDR)];
		PMBX_HDR pMbxHdr = (PMBX_HDR)&__Mailbox[0];
		PSDO_INIT_HDR pSdoInitHdr = (PSDO_INIT_HDR)&pMailbox[__DataOffs];

		//Check mailbox command
		if (pSdoInitHdr->s.bits.Command == SDO_INIT_UPLOAD_RESP)
		{
			//Increase mailbox offset
			__DataOffs += sizeof(SDO_INIT_HDR);

			//Check for expedited transfer
			if (pSdoInitHdr->s.bits.TransferType)
			{
				//Get SDO data size (max. 4 Byte)
				switch (pSdoInitHdr->s.bits.DataSetSize)
				{
				case 3 : __DataSize = 1; break;
				case 2 : __DataSize = 2; break;
				case 1 : __DataSize = 3; break;
				case 0 : __DataSize = 4; break;
				}
			}
			else
			{
				//Get data size and increase mailbox offset
				__DataSize = *(PULONG)&pMailbox[__DataOffs];
				__DataOffs += sizeof(ULONG);
			}
	
			//Copy mailbox data
			memcpy(__Mailbox, pMailbox, pTelHdr->bits.length);
			return TRUE;
		}
	}

	//Mailbox not ready yet
	return FALSE;
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
	if (( pStation->OutDescNum) && ( pStation->InDescNum))	{ Cmd = (pStation->DlInfo.s.byte9.LRWnotSup) ? LRD_CMD : LRW_CMD; }
	if (( pStation->OutDescNum) && (!pStation->InDescNum))	{ Cmd = LWR_CMD; }
	if ((!pStation->OutDescNum) && ( pStation->InDescNum))	{ Cmd = LRD_CMD; }
	if ((!pStation->OutDescNum) && (!pStation->InDescNum))	{ Cmd = NOP_CMD; }

	//Set cyclic telegram
	__EcatSetCyclicTelegram(&pStation->TxTel, (UCHAR)pStation->Index, Cmd, Addr.bit16[0], Addr.bit16[1], Len, NULL, 0x0000);

	//Set station update
	pStation->bUpdate = TRUE;
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
		//****************************************
		//Do the cyclic SDO control state machine
		//****************************************

		PSTATION_INFO pStation = &__pSystemList[3];
		__InitProcessTelegram(pStation);

		switch (__ControlState)
		{
		case SDO_UPLOAD_REQ_WRITE  : __ControlState += __SdoInitUploadReqWrite (pStation, __SdoIndex, __SdoSubIndex); break;
		case SDO_UPLOAD_REQ_READ   : __ControlState += __SdoInitUploadReqRead(  pStation); break;
		case SDO_UPLOAD_RESP_WRITE : __ControlState += __SdoInitUploadRespWrite(pStation); break;
		case SDO_UPLOAD_RESP_READ  : __ControlState += __SdoInitUploadRespRead( pStation, &__DataSize, &__DataOffs);break;
		default                    : DoLogic();
		}

		/***/ SYSTEM_SEQ_CAPTURE("APP", __LINE__, __ControlState, FALSE); /***/

		__UpdateCnt++;
	}

	//Fix RX race condition
	//__AdjustRxTable(&__pSystemStack->rx_table);

	//Call exit function
	__fpEcatExit();

	//Increase loop count
	__LoopCnt++;
}


void main(void)
{
//******************
#ifdef SEQ_DEBUG
	SEQ_ATTACH();						//Attach to sequence memory
	SEQ_RESET(TRUE, TRUE, NULL, 0);	//Reset/Init sequence memory
#endif
//******************

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
						if (ERROR_SUCCESS == __Ecat64SilentChangeState(__pUserStack, REALTIME_PERIOD, AL_STATE_OP))	{ {
#else
						if (ERROR_SUCCESS == Ecat64CyclicStateControl(&__StateObject, AL_STATE_SAFE_OP)) { Sleep(500);
						if (ERROR_SUCCESS == Ecat64CyclicStateControl(&__StateObject, AL_STATE_OP))		 { Sleep(100);
#endif
								//Do a check loop
								printf("\nPress any key ...\n"); 
								while (!kbhit())
								{
									//Trigger SDO upload request
									__SdoIndex     = 0x8010;
									__SdoSubIndex  = 0x01;
									__ControlState =   SDO_UPLOAD_REQ_WRITE;
									while (__ControlState <= SDO_UPLOAD_RESP_READ) { Sleep(10); }

									//Display TX and RX information
									printf("Loop Count: %i, Update Count: %i\r", __LoopCnt, __UpdateCnt);

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
