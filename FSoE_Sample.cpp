//***************************************
//  Automated generated code template ***
//  EtherCAT Code Type : [Beginner]   ***
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
#include "c:\ect\Ecat64Fsoe.h"

#pragma comment( lib, "c:\\sha\\sha64dll.lib" )
#pragma comment( lib, "c:\\eth\\sha64ethcore.lib" )
#pragma comment( lib, "c:\\ect\\sha64ecatcore.lib" )

#pragma warning(disable:4996)
#pragma warning(disable:4748)

//************************************
#define SEQ_ENABLE			1
//************************************

#define REALTIME_PERIOD	   2000		//Realtime sampling period [usec]
#define SYNC_CYCLES           1


//Structs need to have 1 byte alignment
#pragma pack(push, old_alignment)
#pragma pack(1)

typedef struct _MAP_EL1904
{
	struct
	{
		UCHAR cmd;
		UCHAR data;
		USHORT crc;
		USHORT con_id;

	} fsoe;

} MAP_EL1904, *PMAP_EL1904;


typedef struct _MAP_EL2904
{
	struct
	{
		UCHAR cmd;
		UCHAR data;
		USHORT crc;
		USHORT con_id;

	} fsoe;

	USHORT do_data;

} MAP_EL2904, *PMAP_EL2904;

//Set back old alignment
#pragma pack(pop, old_alignment)


//Declare global elements
PETH_STACK		__pUserStack = NULL;		//Ethernet Core Stack (used outside Realtime Task)
PETH_STACK		__pSystemStack = NULL;		//Ethernet Core Stack (used inside Realtime Task)
PSTATION_INFO	__pUserList = NULL;			//Station List (used outside Realtime Task)
PSTATION_INFO	__pSystemList = NULL;		//Station List (used inside Realtime Task)
USHORT			__StationNum = 0;			//Number of Stations
FP_ECAT_ENTER	__fpEcatEnter = NULL;		//Function pointer to Wrapper EcatEnter
FP_ECAT_EXIT	__fpEcatExit = NULL;		//Function pointer to Wrapper EcatExit
FP_FSOE_HANDLER __fpFsoeHandler = NULL;		//Function pointer to FSoE Handler
ULONG			__EcatState = 0;			//Initial Wrapper State
ULONG			__UpdateCnt = 0;			//Station Update Counter
ULONG			__LoopCnt = 0;				//Realtime Cycle Counter
ULONG			__ReadyCnt = 0;				//Ready state counter
STATE_OBJECT    __StateObject = { 0 };		//Cyclic state object
BOOLEAN			__bLogicReady = FALSE;		//Logic Ready Flag


//***************************
#ifdef SEQ_ENABLE
	SEQ_INIT_ELEMENTS(); //Init sequencing elements
#endif
//***************************


__inline PSTATION_INFO __GetStationByName(char* szName, int Index)
{
	int cnt = 0;

	//Loop through station list
	//Check station name
	//Check station index
	//Return station pointer
	for (USHORT i=0; i<__StationNum;i++)
		if (strstr(__pSystemList[i].szName, szName))
			if (cnt++ == Index)
				return &__pSystemList[i];

	//Station not found
	return NULL;
}


void static FsoeLogic()
{
	//Get station pointer
	PSTATION_INFO pEl1904 = __GetStationByName("EL1904", 0);
	PSTATION_INFO pEl2904 = __GetStationByName("EL2904", 0);

	//Get PDO map
	PMAP_EL1904 pMapEl1904 = (PMAP_EL1904)pEl1904->RxTel.s.data;
	PMAP_EL2904 pMapEl2904 = (PMAP_EL2904)pEl2904->TxTel.s.data;

	//Just set FSoE inputs to outputs
	pMapEl2904->fsoe.data = pMapEl1904->fsoe.data;

	//Call handler for each FSoE station
	__FsoeSynchHandler(pEl1904, __fpFsoeHandler);
	__FsoeSynchHandler(pEl2904, __fpFsoeHandler);
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
		if (__bLogicReady)
		{
			//Do the FSoE logic
			FsoeLogic();
		}
		//**********************************

		//Update counter
		__UpdateCnt++;
	}

	//Call exit function
	__fpEcatExit();
	
	//Increase loop count
	__LoopCnt++;
}


void main(void)
{
//******************
#ifdef SEQ_ENABLE
	SEQ_ATTACH();						//Attach to sequence memory
	SEQ_RESET(TRUE, FALSE, NULL, 0);	//Reset/Init sequence memory
#endif
//******************

	printf("\n*** FSoE Sample with EL1904 and EL2904 ***\n\n");

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
		__fpFsoeHandler = EcatParams.fpFsoeHandler;

		//*********************
		//Reset FSoE stations
		//*********************
		for (ULONG i=0; i<__StationNum; i++)
			__ResetFSoeStation(&__pUserList[i]);

		//******************
		//Enable Stations
		//******************
		if (ERROR_SUCCESS == Sha64EcatEnable(&EcatParams))
		{
			//Display version information
			Sha64EcatGetVersion(&EcatParams);
			printf("ECTCORE-DLL : %.2f\nECTCORE-DRV : %.2f\n", EcatParams.core_dll_ver           / (double)100, EcatParams.core_drv_ver / (double)100);
			printf("ETHCORE-DLL : %.2f\nETHCORE-DRV : %.2f\n", EcatParams.EthParams.core_dll_ver / (double)100, EcatParams.EthParams.core_drv_ver / (double)100);
			printf("SHA-LIB     : %.2f\nSHA-DRV     : %.2f\n", EcatParams.EthParams.sha_lib_ver  / (double)100, EcatParams.EthParams.sha_drv_ver / (double)100);
			printf("\n");

			//Display station information
			for (int i=0; i<__StationNum; i++)
				printf("Station: %i, Name: %6s\n", i, __pUserList[i].szName);

			//Set Logic Ready Flag
			__bLogicReady = TRUE;

			//Do a check loop
			printf("\nPress any key ...\n"); 
			while (!kbhit())
			{
				//Display TX and RX information
				printf("Update Count: %i\r", __UpdateCnt);

				//Do some delay
				Sleep(100);
			}

			//Reset Logic Ready Flag
			__bLogicReady = FALSE;
			Sleep(100);
			
			//*******************
			//Disable Stations
			//*******************
			Sha64EcatDisable(&EcatParams);
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
#ifdef SEQ_ENABLE
	SEQ_DETACH(); //Detach from sequence memory
#endif
//******************
}
