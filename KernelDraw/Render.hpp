#pragma once
#include "includes.h"

namespace Render
{
	namespace {
		PVOID OriginalWin32Thread;
		PEPROCESS OriginalProcess;

		PVOID MaskWin32Thread;
		PEPROCESS MaskProcess;

		KAPC_STATE apc_state;

		HDC hdc;
		HBRUSH brush;
	}

	BOOLEAN InitRender();

	BOOLEAN SpoofGuiThread();

	BOOLEAN UnspoofGuiThread();

	BOOLEAN BeginDraw();

	BOOLEAN EndDraw();

	BOOLEAN DrawLine(POINT start, POINT end, int thickness);
	
	BOOLEAN DrawRect(RECT rect, int thickness);

	BOOLEAN DrawText(POINT pos, PCHAR text);
};

