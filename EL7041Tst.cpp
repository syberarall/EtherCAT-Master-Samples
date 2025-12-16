//*****************************************************************
//
//  Automated generated codefor EL7041-1000 rev:0x001303e8 ***//
//
//*****************************************************************
//
// EL7041 Test
//

#include <windows.h>
#include <stdio.h>
#include <conio.h>

#include "DevConfig_EL1008_rev00100000.h"
#include "DevConfig_EL2004_rev00000000.h"
#include "DevConfig_EL7041-1000_rev001303e8.h"

#pragma comment( lib, "c:\\sha\\sha64dll.lib" )
#pragma comment( lib, "c:\\eth\\sha64ethcore.lib" )
#pragma comment( lib, "c:\\ect\\sha64ecatcore.lib" )

#pragma warning(disable:4996)
#pragma warning(disable:4748)

#define MAX_SYSTIME_DIFF	 500			//[nsec]
#define REALTIME_PERIOD	   1000		//Realtime sampling period [usec]
#define SYNC_CYCLES           2

//************************************
//#define SEQ_DEBUG			1
#define DC_CONTROL		1
//************************************

typedef struct _DEVICE_INFO
{
	union
	{
		struct
		{
			TX_MAP_EL1008_2001021749	TxMap;

		} Type1;

		struct
		{
			RX_MAP_EL2004_2001021801	RxMap;
			int							Index;	

		} Type2;

		struct
		{
			TX_MAP_EL7041_2001021802	TxMap;
			RX_MAP_EL7041_2001021802	RxMap;
			int							Index;	

		} Type3;

	} u;

	PSTATION_INFO	pDeviceUser;
	PSTATION_INFO	pDeviceSystem;
	BOOLEAN			bDeviceReady;

} DEVICE_INFO, *PDEVICE_INFO;


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
PDEVICE_INFO	__pDeviceList = NULL;	//Device information List


//***************************
#ifdef SEQ_DEBUG
	SEQ_INIT_ELEMENTS(); //Init sequencing elements
#endif
//***************************


void static DoLogic(PDEVICE_INFO pDeviceInfo)
{
	//ToDo: Payload logic

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

	//Check operation state and increase ready count
	if (__EcatState == ECAT_STATE_READY) { __ReadyCnt--; }
	else                                 { __ReadyCnt = SYNC_CYCLES; }

	//Check ready count
	if (__ReadyCnt == 1)
	{
		//Loop through all drives
		for (int i=0; i<__DeviceNum; i++)
		{
			//Get device information pointer
			PDEVICE_INFO pDeviceInfo = &__DeviceList[i];
			if (pDeviceInfo->pSystem)
			{
				//Get RX telegram data (receiving from drive)
				memcpy(&pDeviceInfo->TxMap, pDeviceInfo->pSystem->RxTel.s.data, sizeof(pDeviceInfo->TxMap));

				//******************************************************************
				if (__bDeviceReady) { DoLogic(pDeviceInfo); }
				//******************************************************************

				//Set TX telegram data (sending to drive)
				memcpy(pDeviceInfo->pSystem->TxTel.s.data, &pDeviceInfo->RxMap, sizeof(pDeviceInfo->RxMap));
			}
		}

		//Update counter
		__UpdateCnt++;
	}

	//Call exit function
	__fpEcatExit();
	
	//Increase loop count
	__LoopCnt++;
}


__inline BOOLEAN __FindStationByName(
							char* pszStationName,
							int Index,
							PSTATION_INFO* ppUserStation,
							PSTATION_INFO* ppSystemStation)
{
	int cnt = 0;

	//Loop through station list
	for (USHORT i=0; i<__StationNum;i++)
		if (strstr(__pUserList[i].szName, pszStationName))
			if (cnt++ == Index)
			{
				//Get station pointer and return index
				if (ppUserStation)   { *ppUserStation   = &__pUserList[i]; }
				if (ppSystemStation) { *ppSystemStation = &__pSystemList[i]; }
				return TRUE;
			}

	//Station not found
	if (ppUserStation)   { *ppUserStation   = NULL; }
	if (ppSystemStation) { *ppSystemStation = NULL; }
	return FALSE;
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

			//Enumerate devices
			__DeviceNum = 0;
			for (int i=0; i<MAX_DEVICES; i++)
				if (__FindStationByName(
							DEVICE_NAME, i,
							&__DeviceList[i].pUser,
							&__DeviceList[i].pSystem)) { __DeviceNum++;	}

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

				//Download SDO requests
				for (int i=0; i<__DeviceNum; i++)
					if (__SdoControl_EL7041_1912301700(__DeviceList[i].pUser) == FALSE)
						goto EXIT;
				
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
EXIT:
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
