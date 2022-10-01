#include "Render.hpp"
#include "includes.h"
#include "Utils.hpp"
#include "Imports.h"

void MainThread()
{
	while (true)
	{
		Render::BeginDraw();
		
		Render::DrawRect({ 100, 100, 200, 200 }, 3);
		Render::DrawRect({ 500, 500, 700, 700 }, 3);

		Render::EndDraw();

		YieldProcessor();
	}
	
	PsTerminateSystemThread(STATUS_SUCCESS);
}

NTSTATUS CreateThread(PVOID entry)
{
	HANDLE threadHandle = NULL;
	NTSTATUS status = PsCreateSystemThread(&threadHandle, NULL, NULL, NULL, NULL, (PKSTART_ROUTINE)entry, NULL);

	if (!NT_SUCCESS(status))
	{
		KdPrint(("failed to create system thread, %x", status));
		return status;
	}

	ZwClose(threadHandle);
	return status;
}


EXTERN_C NTSTATUS DriverEntry()
{
	KdPrint(("DriverEntry"));

	Render::InitRender();

	CreateThread(MainThread);

	return STATUS_SUCCESS;
}
