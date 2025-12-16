// ProcTask64.cpp : Defines the entry point for the console application.
//

#include <Windows.h>
#include "c:\eth\Eth64CoreDef.h"
#include "c:\eth\Eth64Macros.h"
#include "c:\ect\Ecat64CoreDef.h"
#include "c:\ect\Ecat64Macros.h"
#include "c:\ect\Sha64EcatCore.h"

#define CVI_DLL_EXPORT	extern "C" __declspec(dllexport)

//Declare global elements
ECAT_PARAMS		__EcatParams;
PETH_STACK		__pUserStack = NULL;	//Ethernet Core Stack (used outside Realtime Task)
PETH_STACK		__pSystemStack = NULL;	//Ethernet Core Stack (used inside Realtime Task)
PSTATION_INFO	__pUserList = NULL;		//Station List (used outside Realtime Task)
PSTATION_INFO	__pSystemList = NULL;	//Station List (used inside Realtime Task)
USHORT			__StationNum = 0;		//Number of Stations
FP_ECAT_ENTER	__fpEcatEnter = NULL;	//Function pointer to Wrapper EcatEnter
FP_ECAT_EXIT	__fpEcatExit = NULL;	//Function pointer to Wrapper EcatExit
ULONG			__EcatState = 0;		//Initial Wrapper State
ULONG			__ReadyCnt = 0;			//Ready state counter
BOOLEAN			__bInitDone = FALSE;	//Initialization ready flag

CVI_DLL_EXPORT ULONG __LoopCnt = 0;
CVI_DLL_EXPORT ULONG __UpdateCnt = 0;
CVI_DLL_EXPORT UCHAR __DioIn  = 0;
CVI_DLL_EXPORT UCHAR __DioOut = 0;


void static AppTask(PVOID)
{
	//Check if initialization has been done
	if (__bInitDone == FALSE)
		return;

	//Call enter wrapper function
	__EcatState = __fpEcatEnter(
							__pSystemStack,
							__pSystemList,
							(USHORT)__StationNum,
							NULL);

	//Check operation state and decrease ready count
	if (__EcatState == ECAT_STATE_READY) { __ReadyCnt--; }
	else								 { __ReadyCnt = __EcatParams.SyncCycles; }

	//Check ready count
	if (__ReadyCnt == 1)
	{
		//DBG_INITIAL_BREAK();

		//*********************************
		//Do the logical station operation

		//Set Outputs
		__pSystemList[2].TxTel.s.data[0] = __DioIn  = __pSystemList[1].RxTel.s.data[0];
		//*********************************

		__UpdateCnt++;
	}

	//Call exit function
	__fpEcatExit();
	
	//Increase loop count
	__LoopCnt++;
}


CVI_DLL_EXPORT BOOLEAN CviEcatCreate(
								ULONG SyncCycles,
								ULONG Period)
{
	//Required ECAT parameters
	memset(&__EcatParams, 0, sizeof(ECAT_PARAMS));
	__EcatParams.PhysAddr = DEFAULT_PHYSICAL_ADDRESS;
	__EcatParams.LogicalAddr = DEFAULT_LOGICAL_ADDRESS;
	__EcatParams.EthParams.dev_num = 0;							//Set NIC index [0..7]
	__EcatParams.SyncCycles = SyncCycles;						//Set cycles for synchronisation interval
	__EcatParams.EthParams.period = Period;						//Set realtime period [µsec]
	__EcatParams.EthParams.fpAppTask = AppTask;

	if (ERROR_SUCCESS == Sha64EcatCreate(&__EcatParams))
	{
		//Init global elements
		__pUserStack	= __EcatParams.EthParams.pUserStack;
		__pSystemStack	= __EcatParams.EthParams.pSystemStack;
		__pUserList		= __EcatParams.pUserList;
		__pSystemList	= __EcatParams.pSystemList;
		__StationNum	= __EcatParams.StationNum;
		__fpEcatEnter	= __EcatParams.fpEcatEnter;
		__fpEcatExit	= __EcatParams.fpEcatExit;

		//Set initialization done flag
		__bInitDone = TRUE;

		//******************************
		//Enable Stations
		//******************************
		if (ERROR_SUCCESS == Sha64EcatEnable(&__EcatParams))
			return TRUE;
	}

	//Something failed
	return FALSE;
}


CVI_DLL_EXPORT void CviEcatDestroy(void)
{
	if (__bInitDone)
	{
		//Cleanup
		Sha64EcatDisable(&__EcatParams);
		Sha64EcatDestroy(&__EcatParams);
	}
}
 