//***************************************
//  Automated generated code template ***
//  EtherCAT Code Type : [Expert]     ***
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
#include "DevConfig_S6A_2204121233.h"

#pragma comment( lib, "c:\\sha\\sha64dll.lib" )
#pragma comment( lib, "c:\\eth\\sha64ethcore.lib" )
#pragma comment( lib, "c:\\ect\\sha64ecatcore.lib" )

#pragma warning(disable:4996)
#pragma warning(disable:4748)


#define REALTIME_PERIOD	   1000		//Realtime sampling period [usec]
#define SYNC_CYCLES           2

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
BOOLEAN         __bDriveReady = FALSE;		//Drive ready flag


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
	PTX_MAP_S6A_2204121233 pTxMap_S6A_0 = (PTX_MAP_S6A_2204121233)__GetMapDataByName(__pSystemList, (char*)"S6A", 0, TRUE);    //1. Device
	PRX_MAP_S6A_2204121233 pRxMap_S6A_0 = (PRX_MAP_S6A_2204121233)__GetMapDataByName(__pSystemList, (char*)"S6A", 0, FALSE);   //1. Device
	//PTX_MAP_S6A_2204121233 pTxMap_S6A_1 = (PTX_MAP_S6A_2204121233)__GetMapDataByName(__pSystemList, "S6A", 1, TRUE);    //2. Device
	//PRX_MAP_S6A_2204121233 pRxMap_S6A_1 = (PRX_MAP_S6A_2204121233)__GetMapDataByName(__pSystemList, "S6A", 1, FALSE);   //2. Device
	//PTX_MAP_S6A_2204121233 pTxMap_S6A_2 = (PTX_MAP_S6A_2204121233)__GetMapDataByName(__pSystemList, "S6A", 2, TRUE);    //3. Device
	//PRX_MAP_S6A_2204121233 pRxMap_S6A_2 = (PRX_MAP_S6A_2204121233)__GetMapDataByName(__pSystemList, "S6A", 2, FALSE);   //3. Device
	//... more devices

	//Increase target position
//	pRxMap_S6A_0->PDO_607a += 10;
//	pRxMap_S6A_0->PDO_6042 = 500;

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
		if (__bDriveReady) { DoLogic(); }
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
		if (strstr(pStation->szName, "S6A")) { if (__SdoControl_S6A_2204121233(pStation) == FALSE) { return (-1); } }
	}

	//Do default PDO assignment
	return Ecat64PdoAssignment();
}


__inline void __EnableDrives(void)
{
	PTX_MAP_S6A_2204121233 pTxMap_S6A_0 = (PTX_MAP_S6A_2204121233)__GetMapDataByName(__pUserList, (char*)"S6A", 0, TRUE);    //1. Device
	PRX_MAP_S6A_2204121233 pRxMap_S6A_0 = (PRX_MAP_S6A_2204121233)__GetMapDataByName(__pUserList, (char*)"S6A", 0, FALSE);   //1. Device

	//Enable homing
//	pRxMap_S6A_0->PDO_6060 = 6;
//	pRxMap_S6A_0->PDO_6040  = 0x80; Sleep(100);
//	pRxMap_S6A_0->PDO_6040 |= 0x06; Sleep(100);
//	pRxMap_S6A_0->PDO_6040 |= 0x07; Sleep(100);
//	pRxMap_S6A_0->PDO_6040 |= 0x0F; Sleep(100);
//	pRxMap_S6A_0->PDO_6040 |= 0x1F; Sleep(100);

	//Startup drive
	pRxMap_S6A_0->PDO_6060 = 2;
	pRxMap_S6A_0->PDO_6040  = 0x80; Sleep(100);
	pRxMap_S6A_0->PDO_6040 |= 0x06; Sleep(100);
	pRxMap_S6A_0->PDO_6040 |= 0x07; Sleep(100);
	pRxMap_S6A_0->PDO_6040 |= 0x0F; Sleep(100);
	pRxMap_S6A_0->PDO_6040 |= 0x1F; Sleep(100);

	//Set target position
	pRxMap_S6A_0->PDO_6081 = 100;
	pRxMap_S6A_0->PDO_6082 = 100;
	pRxMap_S6A_0->PDO_2314 = 500;
	pRxMap_S6A_0->PDO_6040 = 0x0F; Sleep(100);
	pRxMap_S6A_0->PDO_6040 = 0x1F; Sleep(100);

	//Start logic drive operation
	__bDriveReady = TRUE;
}

__inline void __DisableDrives(void)
{
	PTX_MAP_S6A_2204121233 pTxMap_S6A_0 = (PTX_MAP_S6A_2204121233)__GetMapDataByName(__pUserList, (char*)"S6A", 0, TRUE);    //1. Device
	PRX_MAP_S6A_2204121233 pRxMap_S6A_0 = (PRX_MAP_S6A_2204121233)__GetMapDataByName(__pUserList, (char*)"S6A", 0, FALSE);   //1. Device

	//Stop the drive (Power Off, Disable)
	pRxMap_S6A_0->PDO_6040  = 0x80; Sleep(100);
	
	//Stop logic drive operation
	Sleep(100);
	__bDriveReady = FALSE;
	Sleep(100);
}


void main(void)
{
//******************
#ifdef SEQ_DEBUG
	SEQ_ATTACH();						//Attach to sequence memory
	SEQ_RESET(TRUE, FALSE, NULL, 0);	//Reset/Init sequence memory
#endif
//******************

	printf("\n*** EtherCAT Code Type: [Expert] ***\n\n");


	//Set ethernet mode
	__EcatSetEthernetMode(0, REALTIME_PERIOD, SYNC_CYCLES, FALSE);

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
				if (ERROR_SUCCESS == __SdoControl())
				{
					//Init process telegrams (required for AL_STATE_SAFE_OP)
					Ecat64InitProcessTelegrams();

					//********************************************************
					//Start cyclic operation
					//********************************************************
					if (ERROR_SUCCESS == Ecat64CyclicStateControl(&__StateObject, AL_STATE_SAFE_OP)) { Sleep(500);
					if (ERROR_SUCCESS == Ecat64CyclicStateControl(&__StateObject, AL_STATE_OP))      { Sleep(100);

							//Startup drives
							__EnableDrives();

							PTX_MAP_S6A_2204121233 pTxMap_S6A_0 = (PTX_MAP_S6A_2204121233)__GetMapDataByName(__pUserList, (char*)"S6A", 0, TRUE);    //1. Device
							PRX_MAP_S6A_2204121233 pRxMap_S6A_0 = (PRX_MAP_S6A_2204121233)__GetMapDataByName(__pUserList, (char*)"S6A", 0, FALSE);   //1. Device

							//Do a check loop
							printf("\nPress any key ...\n"); 
							while (!kbhit())
							{
								//Display TX and RX information
								printf("Update:%i, S0:0x%04x P0:0x%08x\r",
										__UpdateCnt,
										(USHORT)pTxMap_S6A_0->PDO_6041,
										(ULONG) pTxMap_S6A_0->PDO_6064);

								//Display TX and RX information
								printf("Update Count: %i\r", __UpdateCnt);
	
								//Do some delay
								Sleep(100);
							}

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
