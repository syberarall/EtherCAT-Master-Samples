//*****************************************************************
#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include "c:\ect\Sha64EcatCore.h"
#include "c:\ect\Ecat64CoreDef.h"
#include "c:\ect\Ecat64EoeDef.h"
#include "c:\ect\Ecat64Control.h"
#include <powrprof.h>


#pragma warning(disable:4996)
#pragma warning(disable:4748)
#pragma comment(lib, "Powrprof.lib")

#pragma comment( lib, "c:\\sha\\sha64dll.lib" )
#pragma comment( lib, "c:\\eth\\sha64ethcore.lib" )
#pragma comment( lib, "c:\\ect\\sha64ecatcore.lib" )

//***************************
//SEQ_INIT_ELEMENTS(); //Init sequencing elements
//***************************

//Declare global elements
ULONG64				__CpuFreq = 0;          //CPU frequency
PETH_STACK			__pUserStack = NULL;	//Ethernet Core Stack (used outside Realtime Task)
PETH_STACK			__pSystemStack = NULL;	//Ethernet Core Stack (used inside Realtime Task)
PSTATION_INFO		__pUserList = NULL;		//Station List (used outside Realtime Task)
PSTATION_INFO		__pSystemList = NULL;	//Station List (used inside Realtime Task)
USHORT				__StationNum = 0;		//Number of Stations
FP_ECAT_ENTER		__fpEcatEnter = NULL;	//Function pointer to Wrapper EcatEnter
FP_ECAT_EXIT		__fpEcatExit = NULL;	//Function pointer to Wrapper EcatExit
STATE_OBJECT		__StateObject = { 0 };	//Cyclic state object
ULONG				__EcatState = 0;		//Initial Wrapper State
ULONG64				__UpdateCnt = 0;		//Station Update Counter
ULONG				__ReadyCnt = 0;			//Ready state counter
BOOLEAN				__bInitDone = FALSE;	//Initialization ready flag
	

#define REALTIME_PERIOD		1000
#define	SYNC_CYCLES			2

//Get externals
extern BOOLEAN EoeEnter(PSTATION_INFO);
extern void    EoeExit(void);

//Structur for CPU frequency
typedef struct _PROCESSOR_POWER_INFORMATION
{
	ULONG Number;
	ULONG MaxMhz;
	ULONG CurrentMhz; //Frequenz
	ULONG MhzLimit;
	ULONG MaxIdleState;
	ULONG CurrentIdleState;

} PROCESSOR_POWER_INFORMATION, * PPROCESSOR_POWER_INFORMATION;



//******* Realtime ********************************************
void DoLogic(void)
{
}


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
							&__StateObject);

	//Check operation state and increase ready count
	if (__EcatState == ECAT_STATE_READY) { __ReadyCnt--; }
	else								 { __ReadyCnt = SYNC_CYCLES; }

	//Check ready count for cycle operation
	if (__ReadyCnt == 1)
	{
		//*********************************
		//Do logic
		DoLogic();
		//*********************************
				
		__UpdateCnt++;
	}

	//Call exit function
	__fpEcatExit();
}


__inline ULONG64  __GetCpuFrequency(void)
{
	//Get system information
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	//Allocate buffer for processor information
	ULONG BufferSize = sizeof(PROCESSOR_POWER_INFORMATION) * si.dwNumberOfProcessors;
	PUCHAR pBuffer = (PUCHAR)malloc(BufferSize);

	//Get the system power information
	NTSTATUS NtStatus = ::CallNtPowerInformation(ProcessorInformation, NULL, 0, pBuffer, BufferSize);
	PPROCESSOR_POWER_INFORMATION pProcPowerInfo = (PPROCESSOR_POWER_INFORMATION)pBuffer;

	//Get CPU frequency
	ULONG64 CpuFreq = pProcPowerInfo->CurrentMhz * 1000000;
	free(pBuffer);

	//Return CPU frequency
	return CpuFreq;
}


int main(void)
{
//******************
//SEQ_ATTACH();					//Attach to sequence memory
//SEQ_RESET(TRUE, TRUE, NULL, 0);	//Reset/Init sequence memory
//******************

	//Set ethernet mode for Non-DC
	printf("SetEthernetMode...\n");
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

	printf("Sha64EcatCreate...\n");
	if (ERROR_SUCCESS == Sha64EcatCreate(&EcatParams))
	{
		//Init global elements
		__pUserStack = EcatParams.EthParams.pUserStack;
		__pSystemStack = EcatParams.EthParams.pSystemStack;
		__pUserList = EcatParams.pUserList;
		__pSystemList = EcatParams.pSystemList;
		__StationNum = EcatParams.StationNum;
		__fpEcatEnter = EcatParams.fpEcatEnter;
		__fpEcatExit = EcatParams.fpEcatExit;
			
		//Set initialization done flag
		__bInitDone = TRUE;

		//Display version information
		printf("Sha64EcatGetVersion...\n");
		Sha64EcatGetVersion(&EcatParams);
		printf("ECTCORE-DLL : %.2f\nECTCORE-DRV : %.2f\n", EcatParams.core_dll_ver / (double)100, EcatParams.core_drv_ver / (double)100);
		printf("ETHCORE-DLL : %.2f\nETHCORE-DRV : %.2f\n", EcatParams.EthParams.core_dll_ver / (double)100, EcatParams.EthParams.core_drv_ver / (double)100);
		printf("SHA-LIB     : %.2f\nSHA-DRV     : %.2f\n", EcatParams.EthParams.sha_lib_ver / (double)100, EcatParams.EthParams.sha_drv_ver / (double)100);

		//******************************
		//Enable Stations
		//******************************

		//Change state to INIT
		if (ERROR_SUCCESS == Ecat64ChangeAllStates(AL_STATE_INIT))
		{
			//Set fixed station addresses and
			//Init FMMUs and SYNCMANs
			if (ERROR_SUCCESS == Ecat64InitStationAddresses(EcatParams.PhysAddr))
			if (ERROR_SUCCESS == Ecat64InitFmmus(EcatParams.LogicalAddr))
			if (ERROR_SUCCESS == Ecat64InitSyncManagers())
			{
				//Display station information
				for (int i = 0; i < __StationNum; i++)
					printf("Station: %i, Name: %6s, Addr:0x%04x\n",
							i, __pUserList[i].szName, __pUserList[i].PhysAddr);

				//Change state to PRE OPERATIONAL
				if (ERROR_SUCCESS == Ecat64ChangeAllStates(AL_STATE_PRE_OP))
				{
					//Init PDO assignment
					Ecat64PdoAssignment();						

					//Init process telegrams (required for AL_STATE_SAFE_OP)
					Ecat64InitProcessTelegrams();

					//Change state to SAFE OPERATIONAL
					//Change state to OPERATIONAL
					if (ERROR_SUCCESS == Ecat64CyclicStateControl(&__StateObject, AL_STATE_SAFE_OP)) { Sleep(500);
					if (ERROR_SUCCESS == Ecat64CyclicStateControl(&__StateObject, AL_STATE_OP))      { Sleep(100);

							//***************************************************
							EoeEnter(&__pUserList[0]);
							Sleep(100);
							//***************************************************

							//Do a check loop
							printf("System running...\n");
							while (!kbhit())
							{
								//Display TX and RX information
								printf("Update Count: %I64i, CPU Freq : %I64i\r", __UpdateCnt, __CpuFreq);

								//Do some delay [msec]																																			
								Sleep(100);

								//FreeConsole();
								//ShowWindow(::GetConsoleWindow(), SW_HIDE); //Close windows
							}

							//**********************************
							EoeExit();
							Sleep(100);
							//**********************************

							//Change state to SAFE OPERATIONAL then to INIT
							Ecat64CyclicStateControl(&__StateObject, AL_STATE_INIT);
						}
					}
				}
			}
		}
		//Destroy ECAT core
		Sha64EcatDestroy(&EcatParams);

		//Check Status
		if      (EcatParams.EthParams.pUserStack == NULL) { printf("\n*** Ethernet Stack Failed ***\n"); }
		else if (EcatParams.EthParams.err_cnts.Phy != 0) { printf("\n*** No Link ***\n"); }
		else if (EcatParams.pUserList == NULL) { printf("\n*** No Station List ***\n"); }
		else if (EcatParams.StationNum == 0) { printf("\n*** No Station Found ***\n"); }
		else    { printf("\n*** OK ***\n"); }
	}

//******************
//SEQ_DETACH(); //Detach from sequence memory
//******************

	return 0;
}
