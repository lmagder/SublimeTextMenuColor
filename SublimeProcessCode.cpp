#include "stdafx.h"
#include "SublimeTextMenuColor.h"

static HANDLE g_pipeHandle = 0;

void PipePrintFunc(const wchar_t* strPtr)
{
	static std::vector<uint8_t> tempBuffer(1024, 0);
	size_t strLen = wcslen(strPtr) + 1; //include null
	size_t requiredSize = strLen * sizeof(wchar_t) + sizeof(PipeCommand);

	if (tempBuffer.size() < requiredSize)
	{
		tempBuffer.resize(tempBuffer.size() * 2);
	}

	uint8_t* pointer = tempBuffer.data();
	*((PipeCommand*)pointer) = PipeCommand::PRINT;
	memcpy(pointer + sizeof(PipeCommand), strPtr, strLen * sizeof(wchar_t));
	DWORD bytesWritten = 0;
	WriteFile(g_pipeHandle, pointer, (DWORD)requiredSize, &bytesWritten, nullptr);
}

CRITICAL_SECTION hookedWindowsCS;
std::unordered_map<HWND, WNDPROC> hookedWindows;
HBRUSH debugBrush = 0;
HBRUSH debugBrush2 = 0;

static void ApplyMenuChanges(HMENU menu, bool setOwnerDraw);

static LRESULT CallBaseProc(
	_In_ HWND   hwnd,
	_In_ UINT   uMsg,
	_In_ WPARAM wParam,
	_In_ LPARAM lParam
)
{
	EnterCriticalSection(&hookedWindowsCS);
	auto it = hookedWindows.find(hwnd);
	if (it != hookedWindows.end())
	{
		WNDPROC oldProc = it->second;
		if (uMsg == WM_DESTROY)
		{
			hookedWindows.erase(it);
		}
		LeaveCriticalSection(&hookedWindowsCS);
		return CallWindowProcW(oldProc, hwnd, uMsg, wParam, lParam);
	}
	LeaveCriticalSection(&hookedWindowsCS);
	return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK WindowProcFunc(
	_In_ HWND   hwnd,
	_In_ UINT   uMsg,
	_In_ WPARAM wParam,
	_In_ LPARAM lParam
)
{
	if (uMsg == WM_INITMENU || uMsg == WM_INITMENUPOPUP)
	{
		//These dynamically add items to we need to set them to ownerdraw also
		LRESULT ret = CallBaseProc(hwnd, uMsg, wParam, lParam);
		ApplyMenuChanges((HMENU)wParam, true);
		return ret;
	}

	if (uMsg == WM_DRAWITEM) //if 0 it's a menus
	{
		LPDRAWITEMSTRUCT diStruct = (LPDRAWITEMSTRUCT)lParam;
		if (diStruct->CtlType == ODT_MENU)
		{
			FillRect(diStruct->hDC, &diStruct->rcItem, debugBrush2);
			WCHAR temp[1024];
			GetMenuStringW(GetMenu(hwnd), diStruct->itemID, temp, 1024, MF_BYCOMMAND);
			SetBkMode(diStruct->hDC, TRANSPARENT);
			DrawTextW(diStruct->hDC, temp, (int)wcslen(temp), &diStruct->rcItem, DT_LEFT);
			OutputDebugStringW(temp);
			OutputDebugStringW(L"\n");
			return TRUE;
		}
	}
	else if (uMsg == WM_MEASUREITEM)
	{
		LPMEASUREITEMSTRUCT miStruct = (LPMEASUREITEMSTRUCT)lParam;
		if (miStruct->CtlType == ODT_MENU)
		{
			WCHAR temp[1024];
			GetMenuStringW(GetMenu(hwnd), miStruct->itemID, temp, 1024, MF_BYCOMMAND);
			HDC dc = GetDC(hwnd);
			SIZE sz;
			GetTextExtentPoint32(dc, temp, (int)wcslen(temp), &sz);
			miStruct->itemWidth = sz.cx;
			miStruct->itemHeight = sz.cy;
			ReleaseDC(hwnd, dc);
			return TRUE;
		}
	}
	return CallBaseProc(hwnd, uMsg, wParam, lParam);
}

static void ApplyMenuChanges(HMENU menu, bool setOwnerDraw)
{
	MENUINFO menuInfo;
	memset(&menuInfo, 0, sizeof(menuInfo));
	menuInfo.cbSize = sizeof(menuInfo);
	menuInfo.fMask = MIM_BACKGROUND;
	BOOL b = GetMenuInfo(menu, &menuInfo);
	menuInfo.hbrBack = setOwnerDraw ? debugBrush : 0;
	if (setOwnerDraw)
		menuInfo.dwStyle |= MF_OWNERDRAW;
	else
		menuInfo.dwStyle &= ~MF_OWNERDRAW;

	b = SetMenuInfo(menu, &menuInfo);
	int menuItemCount = GetMenuItemCount(menu);
	for (int i = 0; i < menuItemCount; i++)
	{
		WCHAR temp[1024];
		GetMenuStringW(menu, i, temp, 1024, MF_BYPOSITION);

		MENUITEMINFOW menuItemInfo;
		memset(&menuItemInfo, 0, sizeof(menuItemInfo));
		menuItemInfo.cbSize = sizeof(menuItemInfo);
		menuItemInfo.fMask = MIIM_FTYPE | MIIM_SUBMENU;
		b = GetMenuItemInfoW(menu, i, TRUE, &menuItemInfo);
		if (setOwnerDraw)
			menuItemInfo.fType |= MFT_OWNERDRAW;
		else
			menuItemInfo.fType &= ~MFT_OWNERDRAW;

		menuItemInfo.fMask = MIIM_FTYPE;
		b = SetMenuItemInfoW(menu, i, TRUE, &menuItemInfo);

		menuItemInfo.fMask = MIIM_FTYPE | MIIM_SUBMENU;
		b = GetMenuItemInfoW(menu, i, TRUE, &menuItemInfo);
		
		if (menuItemInfo.hSubMenu != 0)
		{
			ApplyMenuChanges(menuItemInfo.hSubMenu, setOwnerDraw);
		}
	}
}

static BOOL CALLBACK EnumWindowsProcFunc(
	_In_ HWND   hwnd,
	_In_ LPARAM lParam
)
{
	DWORD processID;
	DWORD threadID = GetWindowThreadProcessId(hwnd, &processID);
	if (processID == GetCurrentProcessId())
	{
		HMENU menuHandle = GetMenu(hwnd);
		if (menuHandle != 0)
		{
			EnterCriticalSection(&hookedWindowsCS);
			//one of ours
			if (hookedWindows.count(hwnd) == 0)
			{
				wchar_t className[1024];
				GetClassNameW(hwnd, className, 1024);
				if (_wcsicmp(className, L"PX_WINDOW_CLASS") == 0)
				{
					//it's one of the main windows
					hookedWindows[hwnd] = (WNDPROC)GetWindowLongPtrW(hwnd, GWLP_WNDPROC);
					LeaveCriticalSection(&hookedWindowsCS);

					SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)&WindowProcFunc);
					ApplyMenuChanges(menuHandle, true);
					DrawMenuBar(hwnd);
				}
				else
				{
					LeaveCriticalSection(&hookedWindowsCS);
				}
			}
			else
			{
				LeaveCriticalSection(&hookedWindowsCS);
			}
		}
	}
	return TRUE;
}

static void HandleFindNewWindows()
{
	EnumWindows(&EnumWindowsProcFunc, 0);
}

static void UnhookAllWindows()
{
	for (auto it : hookedWindows)
	{
		SetWindowLongPtrW(it.first, GWLP_WNDPROC, (LONG_PTR)it.second);
		HMENU menuHandle = GetMenu(it.first);
		if (menuHandle != 0)
		{
			ApplyMenuChanges(menuHandle, false);
		}
	}
}

void MainThreadInSublimeProcess(void* voidArgs)
{
	MainThreadInSublimeProcessArgs localArgs;
	{
		MainThreadInSublimeProcessArgs* args = reinterpret_cast<MainThreadInSublimeProcessArgs*>(voidArgs);
		localArgs = *args;
		delete args;
	}

	InitializeCriticalSection(&hookedWindowsCS);
	debugBrush = CreateSolidBrush(RGB(255, 0, 255));
	debugBrush2 = CreateSolidBrush(RGB(0, 255, 0));

	std::wstring pipeName = GetPipeName(GetCurrentProcessId());
	g_pipeHandle = CreateNamedPipeW(pipeName.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_WAIT, 1, 0, 0, 500, nullptr);
	if (!g_pipeHandle)
	{
		FreeLibraryAndExitThread(localArgs.myModule, 0);
	}
	ConnectNamedPipe(g_pipeHandle, nullptr);

	g_PrintFunc = &PipePrintFunc;

	std::vector<uint8_t> tempBuffer(1024, 0);
	while (true)
	{
		DWORD bytesRead = 0;
		BOOL ret = ReadFile(g_pipeHandle, tempBuffer.data(), (DWORD)tempBuffer.size(), &bytesRead, nullptr);
		if (!ret)
		{
			if (GetLastError() == ERROR_MORE_DATA)
			{
				tempBuffer.resize(tempBuffer.size() * 2);
			}
			else
			{
				break;
			}
		}

		PipeCommand command = *((PipeCommand*)tempBuffer.data());
		if (command == PipeCommand::QUIT)
		{
			break;
		}
		else if (command == PipeCommand::PRINT)
		{
			MessageBoxW(0, (const wchar_t*)(tempBuffer.data() + sizeof(PipeCommand)), L"Print", 0);
		}
		else if (command == PipeCommand::FIND_NEW_WINDOWS)
		{
			HandleFindNewWindows();
		}
	}
	UnhookAllWindows();
	g_PrintFunc = &DefaultPrintFunc;

	CloseHandle(g_pipeHandle);
	g_pipeHandle = 0;

	DeleteCriticalSection(&hookedWindowsCS);
	DeleteObject(debugBrush);
	DeleteObject(debugBrush2);
	//There is an extra handle
	FreeLibrary(localArgs.myModule);
	FreeLibraryAndExitThread(localArgs.myModule, 0);
}
