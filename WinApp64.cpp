//*****************************************************************
//
//	This code is strictly reserved by SYBERA GmbH
//  Copyright (c) 2012 SYBERA
//
//*****************************************************************
//
// This sample demonstrates the Application process of a 2-process solution
//
//*****************************************************************

#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include "c:\sha\sha64exp.h"
#include "..\inc\common.h"

#pragma warning(disable:4996)
#pragma warning(disable:4748)


__inline void __ProcessInput(PMEM_INFO pUserMem)
{
	//Check, if DI data is valid
	if (__pUserMem->bDiValid == FALSE)
		return;

	//Get input data
	printf("DiData: [%02x]\n", pUserMem->DiData);
	
	//Reset DI valid flag
	__pUserMem->bDiValid = FALSE;
}


__inline void __ProcessOutput(PMEM_INFO pUserMem, UCHAR c)
{
	//Check, if DO data has been sent
	if (__pUserMem->bDoValid == TRUE)
		return;

	//Set output data
	pUserMem->DoData = c;
	
	//Set DO valid flag
	__pUserMem->bDoValid = TRUE;
}


void main(void)
{
	HANDLE hMem = NULL;
	PMEM_INFO pUserMem;
	PMEM_INFO pSystemMem;
	ULONG MemSize;
	UCHAR c;
	DWORD Error;

	//Try to attach to BOOT memory
	Error = Sha64AttachMemWithTag(
							BOOT_MEM_TAG,			//Memory tag
							&MemSize,				//Memory size
							(void**)&pUserMem,		//Pointer to memory for Windows access
							(PVOID*)&pSystemMem,	//Pointer to memory for Realtime access
							NULL,					//Physical memory address
							&hMem);					//Handle to memory device

	if (Error == ERROR_SUCCESS)
	{
		do
		{
			//Process DI data
			__ProcessInput(pUserMem);
			
			//Read byte
			c = getch();

			//Process DO data
			__ProcessOutput(pUserMem, c);
		}
		while (c != 'q');

		//Detach from BOOT memory
		Sha64DetachMem(hMem);
	}
}
