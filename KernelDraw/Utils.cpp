#include "Utils.hpp"
#include "Imports.h"

PVOID Utils::GetModuleBase(PCHAR szModuleName)
{
	PVOID result = 0;
	ULONG length = 0;

	ZwQuerySystemInformation(SystemModuleInformation, &length, 0, &length);
	if (!length) return result;

	const unsigned long tag = 'MEM';
	PSYSTEM_MODULE_INFORMATION system_modules = (PSYSTEM_MODULE_INFORMATION)ExAllocatePoolWithTag(NonPagedPool, length, tag);
	if (!system_modules) return result;

	NTSTATUS status = ZwQuerySystemInformation(SystemModuleInformation, system_modules, length, 0);
	if (NT_SUCCESS(status))
	{
		for (size_t i = 0; i < system_modules->ulModuleCount; i++)
		{
			char* fileName = (char*)system_modules->Modules[i].ImageName + system_modules->Modules[i].ModuleNameOffset;
			if (!strcmp(fileName, szModuleName))
			{
				result = system_modules->Modules[i].Base;
				break;
			}
		}
	}
	ExFreePoolWithTag(system_modules, tag);
	return result;
}

PVOID Utils::GetProcAddress(PVOID ModuleBase, PCHAR szFuncName)
{
	return RtlFindExportedRoutineByName(ModuleBase, szFuncName);
}

ULONG Utils::GetActiveProcessLinksOffset()
{
    UNICODE_STRING FunName = { 0 };
    RtlInitUnicodeString(&FunName, L"PsGetProcessId");

    /*
    .text:000000014007E054                   PsGetProcessId  proc near
    .text:000000014007E054
    .text:000000014007E054 48 8B 81 80 01 00+                mov     rax, [rcx+180h]
    .text:000000014007E054 00
    .text:000000014007E05B C3                                retn
    .text:000000014007E05B                   PsGetProcessId  endp
    */

    PUCHAR pfnPsGetProcessId = (PUCHAR)MmGetSystemRoutineAddress(&FunName);
    if (pfnPsGetProcessId && MmIsAddressValid(pfnPsGetProcessId) && MmIsAddressValid(pfnPsGetProcessId + 0x7))
    {
        for (size_t i = 0; i < 0x7; i++)
        {
            if (pfnPsGetProcessId[i] == 0x48 && pfnPsGetProcessId[i + 1] == 0x8B)
            {
                return *(PULONG)(pfnPsGetProcessId + i + 3) + 8;
            }
        }
    }
    return 0;
}

HANDLE Utils::GetProcessIdByName(PCHAR szName)
{
    PEPROCESS Process = GetProcessByName(szName);
    if (Process)
    {
        return PsGetProcessId(Process);
    }
    return NULL;
}

PEPROCESS Utils::GetProcessByName(PCHAR szName)
{
    PEPROCESS Process = NULL;
    PCHAR ProcessName = NULL;
    PLIST_ENTRY pHead = NULL;
    PLIST_ENTRY pNode = NULL;

    ULONG64 ActiveProcessLinksOffset = GetActiveProcessLinksOffset();
    //KdPrint(("ActiveProcessLinksOffset = %llX\n", ActiveProcessLinksOffset));
    if (!ActiveProcessLinksOffset)
    {
        KdPrint(("GetActiveProcessLinksOffset failed\n"));
        return NULL;
    }
    Process = PsGetCurrentProcess();

    pHead = (PLIST_ENTRY)((ULONG64)Process + ActiveProcessLinksOffset);
    pNode = pHead;

    do
    {
        Process = (PEPROCESS)((ULONG64)pNode - ActiveProcessLinksOffset);
        ProcessName = PsGetProcessImageFileName(Process);
        //KdPrint(("%s\n", ProcessName));
        if (!strcmp(szName, ProcessName))
        {
            return Process;
        }

        pNode = pNode->Flink;
    } while (pNode != pHead);

    return NULL;
}


PETHREAD Utils::GetProcessMainThread(PEPROCESS Process)
{
    PETHREAD ethread = NULL;

    KAPC_STATE kApcState = { 0 };

    KeStackAttachProcess(Process, &kApcState);

    HANDLE hThread = NULL;

    NTSTATUS status = ZwGetNextThread(NtCurrentProcess(), NULL, THREAD_ALL_ACCESS,
        OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, 0, &hThread);

    if (NT_SUCCESS(status))
    {

        status = ObReferenceObjectByHandle(hThread, THREAD_ALL_ACCESS,
            *PsThreadType, KernelMode, (PVOID*)&ethread, NULL);
        NtClose(hThread);

        if (!NT_SUCCESS(status))
        {
            ethread = NULL;
        }
    }

    KeUnstackDetachProcess(&kApcState);
    return ethread;
}


ULONG64 Utils::FindPattern(ULONG64 base, SIZE_T size, PCHAR pattern, PCHAR mask)
{
    const auto patternSize = strlen(mask);

    for (size_t i = 0; i < size - patternSize; i++) {
        for (size_t j = 0; j < patternSize; j++) {
            if (mask[j] != '?' && *reinterpret_cast<PBYTE>(base + i + j) != static_cast<BYTE>(pattern[j]))
                break;

            if (j == patternSize - 1)
                return (ULONG64)base + i;
        }
    }
    return 0;
}


ULONG64 Utils::GetImageSectionByName(ULONG64 imageBase, PCHAR sectionName, SIZE_T* sizeOut)
{
    if (reinterpret_cast<PIMAGE_DOS_HEADER>(imageBase)->e_magic != 0x5A4D)
        return 0;

    const auto ntHeader = reinterpret_cast<PIMAGE_NT_HEADERS64>(
        imageBase + reinterpret_cast<PIMAGE_DOS_HEADER>(imageBase)->e_lfanew);
    const auto sectionCount = ntHeader->FileHeader.NumberOfSections;

#define IMAGE_FIRST_SECTION( ntheader ) ((PIMAGE_SECTION_HEADER)        \
    ((ULONG_PTR)(ntheader) +                                            \
     FIELD_OFFSET(IMAGE_NT_HEADERS, OptionalHeader) +                   \
     ((ntheader))->FileHeader.SizeOfOptionalHeader))

    auto sectionHeader = IMAGE_FIRST_SECTION(ntHeader);
    for (size_t i = 0; i < sectionCount; ++i, ++sectionHeader) {
        if (!strcmp(sectionName, reinterpret_cast<const char*>(sectionHeader->Name))) {
            if (sizeOut)
                *sizeOut = sectionHeader->Misc.VirtualSize;
            return imageBase + sectionHeader->VirtualAddress;
        }
    }
    return 0;
}

PSERVICE_DESCRIPTOR_TABLE Utils::GetKeServiceDescriptorTableShadow()
{
    uintptr_t ntoskrnlBase = (uintptr_t)GetModuleBase("ntoskrnl.exe");

    size_t ntoskrnlTextSize = 0;
    const auto ntoskrnlText = GetImageSectionByName(ntoskrnlBase, ".text", &ntoskrnlTextSize);
    if (!ntoskrnlText)
        return 0;

    auto keServiceDescriptorTableShadow = FindPattern(ntoskrnlText, ntoskrnlTextSize,
        "\xC1\xEF\x07\x83\xE7\x20\x25\xFF\x0F", "xxxxxxxxx");

    if (!keServiceDescriptorTableShadow)
        return 0;

    keServiceDescriptorTableShadow += 21;
    keServiceDescriptorTableShadow += *reinterpret_cast<int*>(keServiceDescriptorTableShadow) + sizeof(int);

    return (PSERVICE_DESCRIPTOR_TABLE)keServiceDescriptorTableShadow;
}

PVOID Utils::GetServiceFunctionByIndex(PSYSTEM_SERVICE_TABLE ServiceTable, ULONG ServiceId)
{
    PULONG ServiceTableBase = (PULONG)ServiceTable->ServiceTable;
    if (!MmIsAddressValid(ServiceTableBase))
        return NULL;
    return (PVOID)((ULONG64)(ServiceTableBase) + (ServiceTableBase[ServiceId & 0xFFF] >> 4));
}
