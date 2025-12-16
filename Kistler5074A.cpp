//*****************************************************************
//
// Kistler 5074A Test
//
//*****************************************************************

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
#include "c:\ect\Ecat64SdoDef.h"
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

#define REALTIME_PERIOD	  1000		//Realtime sampling period [usec]
#define SYNC_CYCLES			 1
#define MAX_SYSTIME_DIFF	 500			//[nsec]

//************************************************
//!!! Structs need to have 1 byte alignment !!!
#pragma pack(push, old_alignment)
#pragma pack(1)

typedef union _CH_5074A_CONTROL
{
	UCHAR byte[1];

	struct
	{
		UCHAR Operate		: 1;
		UCHAR PeakCtrl		: 1;
		UCHAR IntegralCtrl	: 1;
		UCHAR Reserved1		: 1;
		UCHAR Reserved2		: 1;
		UCHAR Reserved3		: 1;
		UCHAR Reserved4		: 1;
		UCHAR Reserved5		: 1;
	} s;

} CH_5074A_CONTROL, *PCH_5074A_CONTROL;


typedef union _CH_5074A_STATUS
{
	UCHAR byte[1];

	struct
	{
		UCHAR Opstate		: 1;
		UCHAR PeakState		: 1;
		UCHAR IntegralState : 1;
		UCHAR ParamActive	: 1;
		UCHAR Reserved		: 1;
		UCHAR Overload		: 1;
		UCHAR Warning		: 1;
		UCHAR Error			: 1;
	} s;

} CH_5074A_STATUS, *PCH_5074A_STATUS;


typedef struct _RX_PDO_MAP
{
	CH_5074A_CONTROL	Ch1Control;
	CH_5074A_CONTROL	Ch2Control;
	CH_5074A_CONTROL	Ch3Control;
	CH_5074A_CONTROL	Ch4Control;

} RX_PDO_MAP, *PRX_PDO_MAP;


typedef struct _TX_PDO_MAP
{
	CH_5074A_STATUS		Ch1Status;
	CH_5074A_STATUS		Ch2Status;
	CH_5074A_STATUS		Ch3Status;
	CH_5074A_STATUS		Ch4Status;
	USHORT	Ch1Samples[10];
	USHORT	Ch2Samples[10];
	USHORT	Ch3Samples[10];
	USHORT	Ch4Samples[10];

} TX_PDO_MAP, *PTX_PDO_MAP;

//Set back old alignment
#pragma pack(pop, old_alignment)
//************************************************

//Declare global elements
PETH_STACK		__pUserStack = NULL;		//Ethernet Core Stack (used outside Realtime Task)
PETH_STACK		__pSystemStack = NULL;		//Ethernet Core Stack (used inside Realtime Task)
PSTATION_INFO	__pUserList = NULL;			//Station List (used outside Realtime Task)
PSTATION_INFO	__pSystemList = NULL;		//Station List (used inside Realtime Task)
USHORT			__StationNum = 0;			//Number of Stations
FP_ECAT_ENTER	__fpEcatEnter = NULL;		//Function pointer to Wrapper EcatEnter
FP_ECAT_EXIT	__fpEcatExit = NULL;		//Function pointer to Wrapper EcatExit
ULONG			__EcatState = 0;			//Initial Wrapper State
ULONG			__UpdateCnt = 0;			//Station Update Counter
ULONG			__LoopCnt = 0;				//Realtime Cycle Counter
ULONG			__ReadyCnt = 0;				//Ready state counter
STATE_OBJECT    __StateObject = { 0 };		//Cyclic state object
ULONG			__DeviceIndex = 0;
BOOLEAN			__bDeviceReady = FALSE;
TX_PDO_MAP		__TxPdoMap = { 0 };
RX_PDO_MAP		__RxPdoMap = { 0 };


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
		//Get station pointer by index
		PSTATION_INFO pStation = &__pSystemList[__DeviceIndex];

		 //Get RX telegram data
		memcpy (&__TxPdoMap, pStation->RxTel.s.data, sizeof (TX_PDO_MAP));

		//******************************************************************
		if (__bDeviceReady) { DoLogic();	}
		//******************************************************************

		 //Set TX telegram data
         memcpy (pStation->TxTel.s.data, &__RxPdoMap, sizeof (RX_PDO_MAP));

		__UpdateCnt++;
	}

	//Call exit function
	__fpEcatExit();
	
	//Increase loop count
	__LoopCnt++;

//****************************
#ifdef SEQ_DEBUG
	SYSTEM_SEQ_CAPTURE("SEQ", __LINE__, __EcatState, TRUE);
	SYSTEM_SEQ_CAPTURE("SEQ", __LINE__, __ReadyCnt,  FALSE);
#endif
//****************************
}


UCHAR __SdoList5074A[][10] =
{
	{ 0x00, 0x20, 0x2f, 0x13, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00 },

	//Status CH1-4 bytes
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x01, 0x00, 0x1a, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x02, 0x10, 0x1a, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x03, 0x20, 0x1a, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x04, 0x30, 0x1a, 0x00, 0x00 },

	//CH1 Oversample Words
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x05, 0x40, 0x1a, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x06, 0x41, 0x1a, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x07, 0x42, 0x1a, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x08, 0x43, 0x1a, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x09, 0x44, 0x1a, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x0a, 0x45, 0x1a, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x0b, 0x46, 0x1a, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x0c, 0x47, 0x1a, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x0d, 0x48, 0x1a, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x0e, 0x49, 0x1a, 0x00, 0x00 },

	//CH2 Oversample Words
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x0f, 0x80, 0x1a, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x10, 0x81, 0x1a, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x11, 0x82, 0x1a, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x12, 0x83, 0x1a, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x13, 0x84, 0x1a, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x14, 0x85, 0x1a, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x15, 0x86, 0x1a, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x16, 0x87, 0x1a, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x17, 0x88, 0x1a, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x18, 0x89, 0x1a, 0x00, 0x00 },

	//CH3 Oversample Words
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x19, 0xc0, 0x1a, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x1a, 0xc1, 0x1a, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x1b, 0xc2, 0x1a, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x1c, 0xc3, 0x1a, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x1d, 0xc4, 0x1a, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x1e, 0xc5, 0x1a, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x1f, 0xc6, 0x1a, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x20, 0xc7, 0x1a, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x21, 0xc8, 0x1a, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x22, 0xc9, 0x1a, 0x00, 0x00 },

	//CH4 Oversample Words
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x23, 0x00, 0x1b, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x24, 0x01, 0x1b, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x25, 0x02, 0x1b, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x26, 0x03, 0x1b, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x27, 0x04, 0x1b, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x28, 0x05, 0x1b, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x29, 0x06, 0x1b, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x2a, 0x07, 0x1b, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x2b, 0x08, 0x1b, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x2c, 0x09, 0x1b, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x13, 0x1c, 0x2c, 0x09, 0x1b, 0x00, 0x00 },
	
	{ 0x00, 0x20, 0x2f, 0x13, 0x1c, 0x00, 0x2c, 0x00, 0x00, 0x00 },

	//Control CH1-4 bytes
	{ 0x00, 0x20, 0x2f, 0x12, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x12, 0x1c, 0x01, 0x00, 0x16, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x12, 0x1c, 0x02, 0x10, 0x16, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x12, 0x1c, 0x03, 0x20, 0x16, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2b, 0x12, 0x1c, 0x04, 0x30, 0x16, 0x00, 0x00 },
	{ 0x00, 0x20, 0x2f, 0x12, 0x1c, 0x00, 0x04, 0x00, 0x00, 0x00 },
};


__inline ULONG __SdoDownload(
						PSTATION_INFO pStation,
						PSDO_LEGACY pSdoList,
						int SdoNum)
{
	UCHAR Mailbox[MAX_ECAT_DATA];
	int i = 0;

	//Loop through SDO list
	for (i=0; i<SdoNum; i++)
	{
		//Get pointer to COE command
		PUCHAR pCoeCmd = (PUCHAR)&pSdoList[i];

		//Check mailbox for pending response
		//Write COE command to mailbox
		//Read COE command from mailbox
		if (ERROR_SUCCESS == Ecat64MailboxCheck(pStation))
		if (ERROR_SUCCESS == Ecat64MailboxWrite(pStation, pCoeCmd, sizeof(SDO_LEGACY), MBX_TYPE_COE))
		if (ERROR_SUCCESS == Ecat64MailboxRead(pStation, Mailbox))
		{
			//Check SDO response
			PSDO_INIT_HDR pSdoInitHdr = (PSDO_INIT_HDR)&Mailbox[sizeof(MBX_HDR) + sizeof(COE_HDR)];
			if (pSdoInitHdr->s.bits.Command != SDO_INIT_DOWNLOAD_RESP)
				break;
		}

		//Do some delay
		Sleep(10);
   }

   //Something failed
   return (i == SdoNum) ? ERROR_SUCCESS : (-1);
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


void main(void)
{
//******************
#ifdef SEQ_DEBUG
	SEQ_ATTACH();						//Attach to sequence memory
	SEQ_RESET(TRUE, FALSE, NULL, 0);	//Reset/Init sequence memory
#endif
//******************

	printf("\n*** EtherCAT Kistler 5074A Test ***\n\n");

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
			__DeviceIndex = __GetStationIndexByName(__pUserList, __StationNum, "CA Type 5074");

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

				if (ERROR_SUCCESS == __SdoDownload(
											&__pUserList[__DeviceIndex],
											(PSDO_LEGACY)__SdoList5074A,
											sizeof(__SdoList5074A)/sizeof(SDO_LEGACY)))

				{
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
								//Set device ready flag
								__bDeviceReady = TRUE;
								__RxPdoMap.Ch1Control.s.Operate = 1;

								//Do a check loop
								printf("\nPress any key ...\n"); 
								while (!kbhit())
								{
									//Display TX and RX information
									printf("Update Count: %i, CH1: 0x%02x [%04x-%04x-%04x ...] CH2: 0x%02x [%04x-%04x-%04x ...]\r",
										__UpdateCnt,
										__TxPdoMap.Ch1Status.byte[0],
										__TxPdoMap.Ch1Samples[0],
										__TxPdoMap.Ch1Samples[1],
										__TxPdoMap.Ch1Samples[2],
										__TxPdoMap.Ch2Status.byte[0],
										__TxPdoMap.Ch2Samples[0],
										__TxPdoMap.Ch2Samples[1],
										__TxPdoMap.Ch2Samples[2]);
		
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
