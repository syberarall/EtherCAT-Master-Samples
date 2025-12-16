//***************************************
//  Automated generated code template ***
//  EtherCAT Code Type : [Expert DC]  ***
//***************************************

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

//Configuration Header Includes
#include "DevConfig_PD4_2407151442.h"

#pragma comment( lib, "c:\\sha\\sha64dll.lib" )
#pragma comment( lib, "c:\\eth\\sha64ethcore.lib" )
#pragma comment( lib, "c:\\ect\\sha64ecatcore.lib" )

#pragma warning(disable:4996)
#pragma warning(disable:4748)

#define REALTIME_PERIOD	   2000		//Realtime sampling period [usec]
#define SYNC_CYCLES           1
#define MAX_SYSTIME_DIFF	 500			//[nsec]

//************************************
//#define SEQ_DEBUG			1
//************************************


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
BOOLEAN			__bLogicReady = FALSE;		//Logic Ready Flag
ULONG           __DigOut = 0;


//***************************
#ifdef SEQ_DEBUG
	SEQ_INIT_ELEMENTS(); //Init sequencing elements
#endif
//***************************


__inline PVOID __GetMapDataByName(PSTATION_INFO pList, char* szName, int Index,	BOOLEAN bInput)
{
	int cnt = 0;

	//Loop through station list
	for (USHORT i=0; i<__StationNum;i++)
		if (strstr(pList[i].szName, szName))
			if (cnt++ == Index)
			{
				//Get station pointer and return index
				if (bInput) { return pList[i].RxTel.s.data; }
				else        { return pList[i].TxTel.s.data; }
			}

	//Station not found
	return NULL;
}


void static DoLogic()
{
	//Get mapped data pointers
	PTX_MAP_PD4_2407151442 pTxMap_PD4_0 = (PTX_MAP_PD4_2407151442)__GetMapDataByName(__pSystemList, "PD4", 0, TRUE);    //1. Device
	PRX_MAP_PD4_2407151442 pRxMap_PD4_0 = (PRX_MAP_PD4_2407151442)__GetMapDataByName(__pSystemList, "PD4", 0, FALSE);   //1. Device
	//PTX_MAP_PD4_2407151442 pTxMap_PD4_1 = (PTX_MAP_PD4_2407151442)__GetMapDataByName(__pSystemList, "PD4", 1, TRUE);    //2. Device
	//PRX_MAP_PD4_2407151442 pRxMap_PD4_1 = (PRX_MAP_PD4_2407151442)__GetMapDataByName(__pSystemList, "PD4", 1, FALSE);   //2. Device
	//PTX_MAP_PD4_2407151442 pTxMap_PD4_2 = (PTX_MAP_PD4_2407151442)__GetMapDataByName(__pSystemList, "PD4", 2, TRUE);    //3. Device
	//PRX_MAP_PD4_2407151442 pRxMap_PD4_2 = (PRX_MAP_PD4_2407151442)__GetMapDataByName(__pSystemList, "PD4", 2, FALSE);   //3. Device
	//... more devices


	//ToDo: Payload logic
	//...
	pRxMap_PD4_0->PDO_607a += 100;
//	pRxMap_PD4_0->PDO_60fe  = __DigOut;

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


	//Check operation state and increase ready count
	if (__EcatState == ECAT_STATE_READY) { __ReadyCnt--; }
	else                                 { __ReadyCnt = SYNC_CYCLES; }

	//Check ready count
	if (__ReadyCnt == 1)
	{
		//**********************************
		if (__bLogicReady) { DoLogic(); }
		//**********************************

		//Update counter
		__UpdateCnt++;
	}

	//Call exit function
	__fpEcatExit();
	
	//Increase loop count
	__LoopCnt++;
}


__inline ULONG __SdoControl(void)
{
	//Loop through station list
	for (USHORT i=0; i<__StationNum;i++)
	{
		 PSTATION_INFO pStation = (PSTATION_INFO)&__pUserList[i];

		 //Do Device SDO control
		if (strstr(pStation->szName, "PD4")) { if (__SdoControl_PD4_2407151442(pStation) == FALSE) { return (-1); } }
	}

	//Do default PDO assignment
	return Ecat64PdoAssignment();
}


__inline BOOLEAN __HomingSdoDownload(PSTATION_INFO pStation)
{
	//SDO Upload Request/Response
	ULONG SdoSize = 0;
	UCHAR SdoBuffer[MAX_ECAT_DATA] = { 0 };
	Ecat64SdoInitUploadReq( pStation, 0x100A, 0x00);
	Ecat64SdoInitUploadResp(pStation, &SdoSize, SdoBuffer);

	//Homing Method 35 : Position setting
	ULONG Data  = 35;
	if (ERROR_SUCCESS == Ecat64SdoInitDownloadReq( pStation, 0x6098, 0x00, 1, (PUCHAR)&Data))
	if (ERROR_SUCCESS == Ecat64SdoInitDownloadResp(pStation))
	{
		//Set Homing Position
		Data  = 0;
		if (ERROR_SUCCESS == Ecat64SdoInitDownloadReq( pStation, 0x301B, 0x16, 4, (PUCHAR)&Data))
		if (ERROR_SUCCESS == Ecat64SdoInitDownloadResp(pStation))
			return TRUE;
	}

	//Something failed
	return FALSE;
}


__inline void __Homing(void)
{
	ULONG Data;
	ULONG Size;

	//Get mapped data pointers
	PTX_MAP_PD4_2407151442 pTxMap_PD4_0 = (PTX_MAP_PD4_2407151442)__GetMapDataByName(__pUserList, "PD4", 0, TRUE);		//1. Device
	PRX_MAP_PD4_2407151442 pRxMap_PD4_0 = (PRX_MAP_PD4_2407151442)__GetMapDataByName(__pUserList, "PD4", 0, FALSE);		//1. Device
/*
	//Set Homing Mode
 	Data = 17;
	if (ERROR_SUCCESS == Ecat64SdoInitDownloadReq( &__pUserList[6], 0x6098, 0x00, 1, (PUCHAR)&Data))
	if (ERROR_SUCCESS == Ecat64SdoInitDownloadResp(&__pUserList[6]))

	//Set Homing Speed
 	Data = 200;
	if (ERROR_SUCCESS == Ecat64SdoInitDownloadReq( &__pUserList[6], 0x6099, 0x01, 4, (PUCHAR)&Data))
	if (ERROR_SUCCESS == Ecat64SdoInitDownloadResp(&__pUserList[6]))

	//Set Homing Speed
 	Data = 200;
	if (ERROR_SUCCESS == Ecat64SdoInitDownloadReq( &__pUserList[6], 0x6099, 0x02, 4, (PUCHAR)&Data))
	if (ERROR_SUCCESS == Ecat64SdoInitDownloadResp(&__pUserList[6]))

	//Set Homing Acceleration
 	Data = 500;
	if (ERROR_SUCCESS == Ecat64SdoInitDownloadReq( &__pUserList[6], 0x609A, 0x00, 4, (PUCHAR)&Data))
	if (ERROR_SUCCESS == Ecat64SdoInitDownloadResp(&__pUserList[6]))

	//Set 24V Logic
 	Data = 0xFFFFFFFF;
	if (ERROR_SUCCESS == Ecat64SdoInitDownloadReq( &__pUserList[6], 0x3240, 0x06, 4, (PUCHAR)&Data))
	if (ERROR_SUCCESS == Ecat64SdoInitDownloadResp(&__pUserList[6]))

 	Data = 0xFFFF;
	if (ERROR_SUCCESS == Ecat64SdoInitDownloadReq( &__pUserList[6], 0x3701, 0x00, 2, (PUCHAR)&Data))
	if (ERROR_SUCCESS == Ecat64SdoInitDownloadResp(&__pUserList[6]))

	//Set Input Special Configuration
 	Data = 1;
	if (ERROR_SUCCESS == Ecat64SdoInitDownloadReq( &__pUserList[6], 0x3240, 0x01, 4, (PUCHAR)&Data))
	if (ERROR_SUCCESS == Ecat64SdoInitDownloadResp(&__pUserList[6]))

	//Set Input Configuration
 	Data = 1;
//	if (ERROR_SUCCESS == Ecat64SdoInitDownloadReq( &__pUserList[6], 0x3240, 0x02, 4, (PUCHAR)&Data))
//	if (ERROR_SUCCESS == Ecat64SdoInitDownloadResp(&__pUserList[6]))

	//Set Output Configuration
 	Data = 0;
//	if (ERROR_SUCCESS == Ecat64SdoInitDownloadReq( &__pUserList[6], 0x3250, 0x02, 4, (PUCHAR)&Data))
//	if (ERROR_SUCCESS == Ecat64SdoInitDownloadResp(&__pUserList[6]))

	//Set outputs
 	Data = (1 << 17);
	if (ERROR_SUCCESS == Ecat64SdoInitDownloadReq( &__pUserList[6], 0x60FE, 0x01, 4, (PUCHAR)&Data))
	if (ERROR_SUCCESS == Ecat64SdoInitDownloadResp(&__pUserList[6]))

	//Set outputs
 	Data = 0x00000;
	if (ERROR_SUCCESS == Ecat64SdoInitDownloadReq( &__pUserList[6], 0x60FE, 0x01, 4, (PUCHAR)&Data))
	if (ERROR_SUCCESS == Ecat64SdoInitDownloadResp(&__pUserList[6]))

	//Read Inputs
 	Data = 0;
 	Size = 0;
	if (ERROR_SUCCESS == Ecat64SdoInitUploadReq( &__pUserList[6], 0x60FD, 0x00))
	if (ERROR_SUCCESS == Ecat64SdoInitUploadResp(&__pUserList[6], &Size, (PUCHAR)&Data))

	//Read Inputs
 	Data = 0;
 	Size = 0;
	if (ERROR_SUCCESS == Ecat64SdoInitUploadReq( &__pUserList[6], 0x60FD, 0x00))
	if (ERROR_SUCCESS == Ecat64SdoInitUploadResp(&__pUserList[6], &Size, (PUCHAR)&Data))
*/
	{
		//Do Homing
		pRxMap_PD4_0->PDO_607a = pTxMap_PD4_0->PDO_6064;
		pRxMap_PD4_0->PDO_6060 = 6;
		pRxMap_PD4_0->PDO_6040  = 0x80;		Sleep(100);
		pRxMap_PD4_0->PDO_6040  = 0x06;		Sleep(100);
		pRxMap_PD4_0->PDO_6040 |= 0x07;		Sleep(100);
		pRxMap_PD4_0->PDO_6040 |= 0x0F;		Sleep(100);
		pRxMap_PD4_0->PDO_6040 |= 0x1F;		Sleep(100);
	}
}


__inline void __EnableDrives(void)
{
	//Get mapped data pointers
	PTX_MAP_PD4_2407151442 pTxMap_PD4_0 = (PTX_MAP_PD4_2407151442)__GetMapDataByName(__pUserList, "PD4", 0, TRUE);		//1. Device
	PRX_MAP_PD4_2407151442 pRxMap_PD4_0 = (PRX_MAP_PD4_2407151442)__GetMapDataByName(__pUserList, "PD4", 0, FALSE);		//1. Device

	//Startup drive
	pRxMap_PD4_0->PDO_6060 = 8;
	pRxMap_PD4_0->PDO_607a = pTxMap_PD4_0->PDO_6064;
	pRxMap_PD4_0->PDO_6040  = 0x80;		Sleep(100);
	pRxMap_PD4_0->PDO_6040  = 0x06;		Sleep(100);
	pRxMap_PD4_0->PDO_6040 |= 0x07;		Sleep(100);
	pRxMap_PD4_0->PDO_6040 |= 0x0F;		Sleep(100);
	pRxMap_PD4_0->PDO_6040 |= 0x1F;		Sleep(100);
}


__inline void __DisableDrives(void)
{
	//Get mapped data pointers
	PTX_MAP_PD4_2407151442 pTxMap_PD4_0 = (PTX_MAP_PD4_2407151442)__GetMapDataByName(__pUserList, "PD4", 0, TRUE);		//1. Device
	PRX_MAP_PD4_2407151442 pRxMap_PD4_0 = (PRX_MAP_PD4_2407151442)__GetMapDataByName(__pUserList, "PD4", 0, FALSE);		//1. Device

	//Stop the drive (Power Off, Disable)
	pRxMap_PD4_0->PDO_6040 = 0x80;		Sleep(100);
}


void main(void)
{
//******************
#ifdef SEQ_DEBUG
	SEQ_ATTACH();						//Attach to sequence memory
	SEQ_RESET(TRUE, FALSE, NULL, 0);	//Reset/Init sequence memory
#endif
//******************

	printf("\n*** EtherCAT Code Type: [Expert DC] ***\n\n");

	//Set ethernet mode
	__EcatSetEthernetMode(0, REALTIME_PERIOD, SYNC_CYCLES, TRUE);

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
		printf("ECTCORE-DLL : %.2f\nECTCORE-DRV : %.2f\n", EcatParams.core_dll_ver           / (double)100, EcatParams.core_drv_ver / (double)100);
		printf("ETHCORE-DLL : %.2f\nETHCORE-DRV : %.2f\n", EcatParams.EthParams.core_dll_ver / (double)100, EcatParams.EthParams.core_drv_ver / (double)100);
		printf("SHA-LIB     : %.2f\nSHA-DRV     : %.2f\n", EcatParams.EthParams.sha_lib_ver  / (double)100, EcatParams.EthParams.sha_drv_ver / (double)100);
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

			//Reset devices
//			Ecat64ResetDevices();
//			Ecat64SendCommand(BWR_CMD, 0x0000,  0x300, 8, Data);

			//Set fixed station addresses and
			//Init FMMUs and SYNCMANs
			if (ERROR_SUCCESS == Ecat64InitStationAddresses(EcatParams.PhysAddr))
			if (ERROR_SUCCESS == Ecat64InitFmmus(EcatParams.LogicalAddr))
			if (ERROR_SUCCESS == Ecat64InitSyncManagers())
			{
				//Change state to PRE OPERATIONAL
				//Init PDO assignment
				if (ERROR_SUCCESS == Ecat64ChangeAllStates(AL_STATE_PRE_OP))
				if (ERROR_SUCCESS == __SdoControl())
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
						//Start cyclic operation
						//********************************************************
						if (ERROR_SUCCESS == Ecat64CyclicStateControl(&__StateObject, AL_STATE_SAFE_OP)) { Sleep(500);
						if (ERROR_SUCCESS == Ecat64CyclicStateControl(&__StateObject, AL_STATE_OP))      { Sleep(100);


								__Homing();

								//Startup drives
								__EnableDrives();
								//__HomingSdoDownload(&__pUserList[6]);

								//Set Logic Ready Flag
								__bLogicReady = TRUE;

								//Do a check loop
								printf("\nPress any key ...\n"); 
								while (!kbhit())
								{
									//Get mapped data pointers
									PTX_MAP_PD4_2407151442 pTxMap_PD4_0 = (PTX_MAP_PD4_2407151442)__GetMapDataByName(__pUserList, "PD4", 0, TRUE);		//1. Device
									PRX_MAP_PD4_2407151442 pRxMap_PD4_0 = (PRX_MAP_PD4_2407151442)__GetMapDataByName(__pUserList, "PD4", 0, FALSE);		//1. Device

									//Display drive information
									printf("6041:%04x, 6061:%02x, 6064:%08x, Update:%i\r",
											pTxMap_PD4_0->PDO_6041,
											pTxMap_PD4_0->PDO_6061,
											pTxMap_PD4_0->PDO_6064,
											__UpdateCnt);
		
									//Do some delay
									Sleep(100);
								}

								//Reset Logic Ready Flag
								__bLogicReady = FALSE;
								Sleep(100);

								//Stop drives
								__DisableDrives();

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
