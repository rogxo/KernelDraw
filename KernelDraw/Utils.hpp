#pragma once
#include "includes.h"
#include "NativeStructs.h"


namespace Utils
{
	PVOID GetModuleBase(PCHAR szModuleName);

	PVOID GetProcAddress(PVOID ModuleBase, PCHAR szFuncName);

	ULONG GetActiveProcessLinksOffset();

	HANDLE GetProcessIdByName(PCHAR szName);

	PEPROCESS GetProcessByName(PCHAR szName);

	PETHREAD GetProcessMainThread(PEPROCESS Process);

	ULONG64 FindPattern(ULONG64 base, SIZE_T size, PCHAR pattern, PCHAR mask);

	ULONG64 GetImageSectionByName(ULONG64 imageBase, PCHAR sectionName, SIZE_T* sizeOut);

	PSERVICE_DESCRIPTOR_TABLE GetKeServiceDescriptorTableShadow();

	PVOID GetServiceFunctionByIndex(PSYSTEM_SERVICE_TABLE, ULONG ServiceId);
};

