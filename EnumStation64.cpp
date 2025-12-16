//*****************************************************************
//
//	This code is strictly reserved by SYBERA GmbH
//  Copyright (c) 2012 SYBERA
//
//*****************************************************************
//
// This sample demonstrates how to use enumerate ETHERCAT stations
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

//Declare global elements
PETH_STACK		__pUserStack = NULL;	//Ethernet Core Stack (used outside Realtime Task)
PETH_STACK		__pSystemStack = NULL;	//Ethernet Core Stack (used inside Realtime Task)
PSTATION_INFO	__pUserList = NULL;		//Station List (used outside Realtime Task)
PSTATION_INFO	__pSystemList = NULL;	//Station List (used inside Realtime Task)
USHORT			__StationNum = 0;		//Number of Stations
FP_ECAT_ENTER	__fpEcatEnter = NULL;	//Function pointer to Wrapper EcatEnter
FP_ECAT_EXIT	__fpEcatExit = NULL;	//Function pointer to Wrapper EcatExit
ULONG			__UpdateCnt = 0;		//Station Update Counter
ULONG			__CycleCnt = 0;			//Realtime Cycle Counter


void main(void)
{
	printf("\n*** EtherCAT Station Enumeration ***\n\n");

	//Required ECAT parameters
	ECAT_PARAMS EcatParams;
	memset(&EcatParams, 0, sizeof(ECAT_PARAMS));
	EcatParams.PhysAddr = DEFAULT_PHYSICAL_ADDRESS;
	EcatParams.LogicalAddr = DEFAULT_LOGICAL_ADDRESS;
	EcatParams.SyncCycles = 2;						//Set cycles for synchronisation interval
	EcatParams.EthParams.dev_num = 0;				//Set NIC index [0..7]
	EcatParams.EthParams.period = 1000;				//Set realtime period [µsec]
	EcatParams.EthParams.fpAppTask = NULL;

	//******************************
	//Create ECAT realtime core
	//******************************
	if (ERROR_SUCCESS == Sha64EcatCreate(&EcatParams))
	{
		//Init realtime elements
		__pUserStack	= EcatParams.EthParams.pUserStack;
		__pSystemStack	= EcatParams.EthParams.pSystemStack;
		__pUserList		= EcatParams.pUserList;
		__pSystemList	= EcatParams.pSystemList;
		__StationNum	= EcatParams.StationNum;
		__fpEcatEnter	= EcatParams.fpEcatEnter;
		__fpEcatExit	= EcatParams.fpEcatExit;

		//******************************
		//Enable Stations
		//******************************
		if (ERROR_SUCCESS == Sha64EcatEnable(&EcatParams))
		{
			//Display version information
			Sha64EcatGetVersion(&EcatParams);
			printf("ECTCORE-DLL : %.2f\nECTCORE-DRV : %.2f\n", EcatParams.core_dll_ver / (double)100, EcatParams.core_drv_ver / (double)100);
			printf("ETHCORE-DLL : %.2f\nETHCORE-DRV : %.2f\n", EcatParams.EthParams.core_dll_ver / (double)100, EcatParams.EthParams.core_drv_ver / (double)100);
			printf("SHA-LIB     : %.2f\nSHA-DRV     : %.2f\n", EcatParams.EthParams.sha_lib_ver / (double)100, EcatParams.EthParams.sha_drv_ver / (double)100);
			printf("\n");

			//Display station information
			for (int i=0; i<__StationNum; i++)
				printf("Station: %i, Name: %6s\n", i, __pUserList[i].szName);

			//Wait for key pressed
			printf("\nPress any key ...\n"); 
			while (!kbhit()) { Sleep(100); }

			//Disable Stations
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
	else												{ printf("\n*** Core Init Failed ***\n"); }

	//Wait for key press
	printf("\nPress any key ...\n"); 
	while (!kbhit()) { Sleep(100); }
}
