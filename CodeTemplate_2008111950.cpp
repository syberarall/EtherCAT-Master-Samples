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
#include "c:\ect\Ecat64EoeDef.h"
#include "c:\ect\Ecat64Macros.h"
#include "c:\ect\Ecat64Control.h"
#include "c:\ect\Ecat64SilentDef.h"

//Configuration Header Includes
#include "DevConfig_EMCA_2104151712.h"

#pragma comment( lib, "c:\\sha\\sha64dll.lib" )
#pragma comment( lib, "c:\\eth\\sha64ethcore.lib" )
#pragma comment( lib, "c:\\ect\\sha64ecatcore.lib" )

#pragma warning(disable:4996)
#pragma warning(disable:4748)

#define MAX_SYSTIME_DIFF	 500			//[nsec]
#define REALTIME_PERIOD	   2000		//Realtime sampling period [usec]
#define SYNC_CYCLES           1
#define POS_STEP			 50


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
BOOLEAN			__bPositive = TRUE;
LONG			__PosStep = POS_STEP;
ULONG			__DriveIndex = 1;


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
	PTX_MAP_EMCA_2104151712 pTxMap_EMCA_0 = (PTX_MAP_EMCA_2104151712)__GetMapDataByName(__pSystemList, "EMCA", __DriveIndex, TRUE);    //1. Device
	PRX_MAP_EMCA_2104151712 pRxMap_EMCA_0 = (PRX_MAP_EMCA_2104151712)__GetMapDataByName(__pSystemList, "EMCA", __DriveIndex, FALSE);   //1. Device
	//PTX_MAP_EMCA_2104151712 pTxMap_EMCA_1 = (PTX_MAP_EMCA_2104151712)__GetMapDataByName(__pSystemList, "EMCA", 1, TRUE);    //2. Device
	//PRX_MAP_EMCA_2104151712 pRxMap_EMCA_1 = (PRX_MAP_EMCA_2104151712)__GetMapDataByName(__pSystemList, "EMCA", 1, FALSE);   //2. Device
	//PTX_MAP_EMCA_2104151712 pTxMap_EMCA_2 = (PTX_MAP_EMCA_2104151712)__GetMapDataByName(__pSystemList, "EMCA", 2, TRUE);    //3. Device
	//PRX_MAP_EMCA_2104151712 pRxMap_EMCA_2 = (PRX_MAP_EMCA_2104151712)__GetMapDataByName(__pSystemList, "EMCA", 2, FALSE);   //3. Device
	//... more devices

	//Toggle direction
	if ((__PosStep > 0) && (pTxMap_EMCA_0->PDO_6064 > 0x30000)) { __PosStep = -POS_STEP; }
	if ((__PosStep < 0) && (pTxMap_EMCA_0->PDO_6064 < 0x1000))  { __PosStep =  POS_STEP; }

	//Increase position
	pRxMap_EMCA_0->PDO_607a += __PosStep;

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


__inline void __EnableDrive(void)
{
	//Get mapped data pointers
	PTX_MAP_EMCA_2104151712 pTxMap_EMCA_0 = (PTX_MAP_EMCA_2104151712)__GetMapDataByName(__pUserList, "EMCA", __DriveIndex, TRUE);    //1. Device
	PRX_MAP_EMCA_2104151712 pRxMap_EMCA_0 = (PRX_MAP_EMCA_2104151712)__GetMapDataByName(__pUserList, "EMCA", __DriveIndex, FALSE);   //1. Device

	for (;;)
	{
		//Enable drive
		pRxMap_EMCA_0->PDO_6040  = 0x06;	Sleep(100);
		pRxMap_EMCA_0->PDO_6040  = 0x07;	Sleep(100);
		pRxMap_EMCA_0->PDO_6040  = 0x0F;	Sleep(100);

		//Check for homing done
		if ((pTxMap_EMCA_0->PDO_6041 & (1<<1)) &&
			(pTxMap_EMCA_0->PDO_6041 & (1<<2)))
			break;

		//Check for fault
		if (pTxMap_EMCA_0->PDO_6041 & (1<<3))
			pRxMap_EMCA_0->PDO_6040  = 0x80;
		
		Sleep(100);
	}
}


__inline void __DisableDrive(void)
{
	//Get mapped data pointers
	PTX_MAP_EMCA_2104151712 pTxMap_EMCA_0 = (PTX_MAP_EMCA_2104151712)__GetMapDataByName(__pUserList, "EMCA", __DriveIndex, TRUE);    //1. Device
	PRX_MAP_EMCA_2104151712 pRxMap_EMCA_0 = (PRX_MAP_EMCA_2104151712)__GetMapDataByName(__pUserList, "EMCA", __DriveIndex, FALSE);   //1. Device

	//Stop the drive (Power Off, Disable)
	pRxMap_EMCA_0->PDO_6040 = 0x06;		Sleep(100);
}


__inline BOOLEAN __Homing(void)
{
	ULONG Data;
	ULONG SdoSize = 0;
	UCHAR SdoBuffer[MAX_ECAT_DATA] = { 0 };


	for (int i=0; i<__StationNum; i++)
	{
		PSTATION_INFO pStation = (PSTATION_INFO)&__pUserList[i];

		 //Do Device SDO control
		if (strstr(pStation->szName, "EMCA"))
		{
			//Homing method
			Data  = 1;
			if (ERROR_SUCCESS != Ecat64SdoInitDownloadReq( pStation, 0x6098, 0x00, 1, (PUCHAR)&Data)) { return FALSE; }
			if (ERROR_SUCCESS != Ecat64SdoInitDownloadResp(pStation)) { return FALSE; }
			Sleep(100);
	
			//Set Homing Position
			//Data  = 0;
			//if (ERROR_SUCCESS != Ecat64SdoInitDownloadReq( pStation, 0x301B, 0x16, 4, (PUCHAR)&Data)) { return FALSE; }
			//if (ERROR_SUCCESS != Ecat64SdoInitDownloadResp(pStation)) {	return FALSE; }
			//Sleep(100);
		}
	}

	//Get mapped data pointers
	PTX_MAP_EMCA_2104151712 pTxMap_EMCA_0 = (PTX_MAP_EMCA_2104151712)__GetMapDataByName(__pUserList, "EMCA", __DriveIndex, TRUE);    //1. Device
	PRX_MAP_EMCA_2104151712 pRxMap_EMCA_0 = (PRX_MAP_EMCA_2104151712)__GetMapDataByName(__pUserList, "EMCA", __DriveIndex, FALSE);   //1. Device

	//Start homing
	pRxMap_EMCA_0->PDO_6040 = 0x1F;
	Sleep(100);

	//Wait for homing completed
	time_t TimeoutTime = clock() + 30000;
	while (clock() < TimeoutTime)
	{
		//Check if homing is done
		if ((pTxMap_EMCA_0->PDO_6041 & (1<<10)) &&
			(pTxMap_EMCA_0->PDO_6041 & (1<<12)) &&
			(pTxMap_EMCA_0->PDO_6041 & (1<<15)))
			return TRUE;
	}

	//Something failed
	return FALSE;
}


__inline void __SetOpMode(UCHAR OpMode)
{
	//Get mapped data pointers
	PTX_MAP_EMCA_2104151712 pTxMap_EMCA_0 = (PTX_MAP_EMCA_2104151712)__GetMapDataByName(__pUserList, "EMCA", __DriveIndex, TRUE);    //1. Device
	PRX_MAP_EMCA_2104151712 pRxMap_EMCA_0 = (PRX_MAP_EMCA_2104151712)__GetMapDataByName(__pUserList, "EMCA", __DriveIndex, FALSE);   //1. Device

	//Set position to homing
	if (OpMode == 8) { pRxMap_EMCA_0->PDO_607a  = pTxMap_EMCA_0->PDO_6064; }

	//Set OP mode
	pRxMap_EMCA_0->PDO_6060 = OpMode;
	Sleep(100);
}



__inline ULONG __SdoControl(void)
{
	//Loop through station list
	for (USHORT i=0; i<__StationNum;i++)
	{
		PSTATION_INFO pStation = (PSTATION_INFO)&__pUserList[i];

		 //Do Device SDO control
		if (strstr(pStation->szName, "EMCA"))
		{
			//Send EoE data
			//For this drive the SDO list has to be sent twice
			if (__SdoControl_EMCA_2104151712(pStation) == FALSE) { return (-1); }
		}
	}

	//Do default PDO assignment
	return Ecat64PdoAssignment();
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

								//Set OP mode
								__SetOpMode(6);

								//Enable drives
								__EnableDrive();

								//Do homing
								if (__Homing())
								{
									//Set OP mode
									__SetOpMode(8);

									//Set Logic Ready Flag
									__bLogicReady = TRUE;

									//Do a check loop
									printf("\nPress any key ...\n"); 
									while (!kbhit())
									{
										//Get mapped data pointers
										PTX_MAP_EMCA_2104151712 pTxMap_EMCA_0 = (PTX_MAP_EMCA_2104151712)__GetMapDataByName(__pUserList, "EMCA", __DriveIndex, TRUE);    //1. Device
										PRX_MAP_EMCA_2104151712 pRxMap_EMCA_0 = (PRX_MAP_EMCA_2104151712)__GetMapDataByName(__pUserList, "EMCA", __DriveIndex, FALSE);   //1. Device

										//Display drive information
										printf("6041:[%04x], 6064:[%08x], Update:%i\r",
												pTxMap_EMCA_0->PDO_6041,
												pTxMap_EMCA_0->PDO_6064,
												__UpdateCnt);
		
										//Do some delay
										Sleep(100);
									}

									//Reset Logic Ready Flag
									__bLogicReady = FALSE;
									Sleep(100);
								}

								//Set OP mode
								__SetOpMode(1);

								//Disable drives
								__DisableDrive();
								Sleep(100);
		
								//********************************************************
								//Stop cyclic operation
								//********************************************************
								Ecat64CyclicStateControl(&__StateObject, AL_STATE_INIT);
								Sleep(100);
							}
						}
					}
				}
			}
		}

		USHORT AlStatus = 0;
		Ecat64SendCommand(APRD_CMD, 0x0000, 0x0134, 2, (PUCHAR)&AlStatus);

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
