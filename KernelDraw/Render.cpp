#include "Render.hpp"
#include "Utils.hpp"
#include "Imports.h"


typedef HDC(NTAPI* pfnNtUserGetDC)(HWND hWnd);
pfnNtUserGetDC NtUserGetDC = NULL;

typedef int (NTAPI* pfnNtUserReleaseDC)(HDC hDC);
pfnNtUserReleaseDC NtUserReleaseDC = NULL;

typedef BOOL(APIENTRY* pfnNtGdiPatBlt)(_In_ HDC hdcDest, _In_ INT x, _In_ INT y, _In_ INT cx, _In_ INT cy, _In_ DWORD dwRop);
pfnNtGdiPatBlt NtGdiPatBlt = NULL;

typedef HBRUSH(APIENTRY* pfnGreSelectBrush)(IN HDC hDC, IN HBRUSH hBrush);
pfnGreSelectBrush GreSelectBrush = NULL;

typedef HBRUSH(APIENTRY* pfnNtGdiCreateSolidBrush)(_In_ COLORREF cr, _In_opt_ HBRUSH hbr);
pfnNtGdiCreateSolidBrush NtGdiCreateSolidBrush = NULL;

typedef BOOL(APIENTRY* pfnNtGdiDeleteObjectApp)(HANDLE hobj);
pfnNtGdiDeleteObjectApp NtGdiDeleteObjectApp = NULL;

typedef BOOL(APIENTRY* pfnNtGdiExtTextOutW)(IN HDC hDC, IN INT XStart, IN INT YStart, IN UINT fuOptions, IN OPTIONAL LPRECT UnsafeRect, IN LPWSTR UnsafeString, IN INT Count, IN OPTIONAL LPINT UnsafeDx, IN DWORD dwCodePage);
pfnNtGdiExtTextOutW NtGdiExtTextOutW = NULL;

typedef HFONT(APIENTRY* pfnNtGdiHfontCreate)(IN PENUMLOGFONTEXDVW pelfw, IN ULONG cjElfw, IN DWORD lft, IN FLONG fl, IN PVOID pvCliData);
pfnNtGdiHfontCreate NtGdiHfontCreate = NULL;

typedef HFONT(APIENTRY* pfnNtGdiSelectFont)(_In_ HDC hdc, _In_ HFONT hf);;
pfnNtGdiSelectFont NtGdiSelectFont = NULL;


ULONG ThreadProcessOffset;
ULONG GetThreadProcessOffset()
{
	UNICODE_STRING FuncName = RTL_CONSTANT_STRING(L"PsGetThreadProcess");
	PVOID pfnPsGetThreadProcess = MmGetSystemRoutineAddress(&FuncName);
	if (!MmIsAddressValid(pfnPsGetThreadProcess))
		return 0;
	return *(PULONG)((PUCHAR)pfnPsGetThreadProcess + 3);
}

BOOLEAN IsInitialized = FALSE;
BOOLEAN Render::InitRender()
{
	if (IsInitialized)
	{
		return TRUE;
	}
	PVOID win32kase = Utils::GetModuleBase("win32kbase.sys");
	PVOID win32kfull = Utils::GetModuleBase("win32kfull.sys");

	KdPrint(("win32kase = %p\n", win32kase));
	KdPrint(("win32kfull = %p\n", win32kfull));

	if (!win32kase || !win32kfull)
	{
		KdPrint(("Could not find kernel module bases"));
		return FALSE;
	}

	if (!SpoofGuiThread())
	{
		return FALSE;
	}

	NtUserGetDC = (pfnNtUserGetDC)Utils::GetProcAddress(win32kase, "NtUserGetDC");
	NtUserReleaseDC = (pfnNtUserReleaseDC)Utils::GetProcAddress(win32kase, "NtUserReleaseDC");
	NtGdiPatBlt = (pfnNtGdiPatBlt)Utils::GetProcAddress(win32kfull, "NtGdiPatBlt");
	GreSelectBrush = (pfnGreSelectBrush)Utils::GetProcAddress(win32kase, "GreSelectBrush");
	NtGdiCreateSolidBrush = (pfnNtGdiCreateSolidBrush)Utils::GetProcAddress(win32kfull, "NtGdiCreateSolidBrush");
	NtGdiDeleteObjectApp = (pfnNtGdiDeleteObjectApp)Utils::GetProcAddress(win32kase, "NtGdiDeleteObjectApp");
	NtGdiExtTextOutW = (pfnNtGdiExtTextOutW)Utils::GetProcAddress(win32kfull, "NtGdiExtTextOutW");
	NtGdiHfontCreate = (pfnNtGdiHfontCreate)Utils::GetProcAddress(win32kfull, "NtGdiHfontCreate");
	NtGdiSelectFont = (pfnNtGdiSelectFont)Utils::GetProcAddress(win32kfull, "NtGdiSelectFont");

	UnspoofGuiThread();

	KdPrint(("NtUserGetDC = %p\n", NtUserGetDC));
	KdPrint(("NtUserReleaseDC = %p\n", NtUserReleaseDC));
	KdPrint(("NtGdiPatBlt = %p\n", NtGdiPatBlt));
	KdPrint(("GreSelectBrush = %p\n", GreSelectBrush));
	KdPrint(("NtGdiCreateSolidBrush = %p\n", NtGdiCreateSolidBrush));
	KdPrint(("NtGdiDeleteObjectApp = %p\n", NtGdiDeleteObjectApp));
	KdPrint(("NtGdiExtTextOutW = %p\n", NtGdiExtTextOutW));
	KdPrint(("NtGdiHfontCreate = %p\n", NtGdiHfontCreate));
	KdPrint(("NtGdiSelectFont = %p\n", NtGdiSelectFont));

	if (!NtUserGetDC || !NtGdiPatBlt || !GreSelectBrush ||
		!NtUserReleaseDC || !NtGdiCreateSolidBrush || !NtGdiDeleteObjectApp
		|| !NtGdiExtTextOutW || !NtGdiHfontCreate || !NtGdiSelectFont)
	{
		KdPrint(("Could not find kernel functions required for drawing"));
		return FALSE;
	}

	ThreadProcessOffset = GetThreadProcessOffset();

	IsInitialized = TRUE;
	return TRUE;
}


BOOLEAN Render::SpoofGuiThread()
{
	MaskProcess = Utils::GetProcessByName("dwm.exe");
	PETHREAD Thread = Utils::GetProcessMainThread(MaskProcess);
	MaskWin32Thread = PsGetThreadWin32Thread(Thread);

	if (!MaskWin32Thread)
	{
		KdPrint(("Failed to Get Win32Thread\n"));
		return FALSE;
	}

	PKTHREAD currentThread = KeGetCurrentThread();

	OriginalWin32Thread = PsGetCurrentThreadWin32Thread();
	OriginalProcess = PsGetThreadProcess(currentThread);

	KeStackAttachProcess(MaskProcess, &apc_state);

	PsSetThreadWin32Thread(currentThread, MaskWin32Thread, PsGetCurrentThreadWin32Thread());
	*(PEPROCESS*)((char*)currentThread + ThreadProcessOffset) = MaskProcess;

	return TRUE;
}

BOOLEAN Render::UnspoofGuiThread()
{
	PKTHREAD currentThread = KeGetCurrentThread();

	PsSetThreadWin32Thread(currentThread, OriginalWin32Thread, PsGetCurrentThreadWin32Thread());
	*(PEPROCESS*)((char*)currentThread + ThreadProcessOffset) = OriginalProcess;
	
	KeUnstackDetachProcess(&apc_state);
	return TRUE;
}


BOOLEAN Render::BeginDraw()
{
	if (!SpoofGuiThread())
	{
		return FALSE;
	}
	hdc = NtUserGetDC(0);
	if (!hdc)
	{
		KdPrint(("NtUserGetDC Failed\n"));
		return FALSE;
	}

	brush = NtGdiCreateSolidBrush(RGB(255, 0, 0), NULL);
	if (!brush)
	{
		KdPrint(("NtGdiCreateSolidBrush Failed\n"));
		NtUserReleaseDC(hdc);
		return FALSE;
	}
	return TRUE;
}

BOOLEAN Render::EndDraw()
{
	NtGdiDeleteObjectApp(brush);
	NtUserReleaseDC(hdc);

	UnspoofGuiThread();

	return TRUE;
}

BOOLEAN Render::DrawRect(RECT rect, int thickness)
{
	HBRUSH oldBrush = GreSelectBrush(hdc, brush);
	if (!oldBrush)
	{
		DbgPrint("failed to get brush");
		return FALSE;
	}

	NtGdiPatBlt(hdc, rect.left, rect.top, thickness, rect.bottom - rect.top, PATCOPY);
	NtGdiPatBlt(hdc, rect.right - thickness, rect.top, thickness, rect.bottom - rect.top, PATCOPY);
	NtGdiPatBlt(hdc, rect.left, rect.top, rect.right - rect.left, thickness, PATCOPY);
	NtGdiPatBlt(hdc, rect.left, rect.bottom - thickness, rect.right - rect.left, thickness, PATCOPY);

	GreSelectBrush(hdc, oldBrush);
	return TRUE;
}

/*
BOOLEAN Render::DrawLine(POINT start, POINT end, int thickness)
{
	HBRUSH oldBrush = GreSelectBrush(hdc, brush);
	if (!oldBrush)
	{
		DbgPrint("failed to get brush");
		return FALSE;
	}


	GreSelectBrush(hdc, oldBrush);
	return TRUE;
}

BOOLEAN Render::DrawText(POINT pos, PCHAR text)
{
	return BOOLEAN();
}
*/