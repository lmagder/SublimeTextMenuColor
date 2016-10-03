#include "stdafx.h"
#include "SublimeTextMenuColor.h"
#include "ThemeDef.h"

static HANDLE g_pipeHandle = 0;

void WriteToPipe(PipeCommand cmd, const wchar_t* strPtr)
{
	static std::vector<uint8_t> tempBuffer(1024, 0);
	size_t strLen = strPtr ? (wcslen(strPtr) + 1) : 0; //include null
	size_t requiredSize = strLen * sizeof(wchar_t) + sizeof(PipeCommand);

	while (tempBuffer.size() < requiredSize)
	{
		tempBuffer.resize(tempBuffer.size() * 2);
	}

	uint8_t* pointer = tempBuffer.data();
	*((PipeCommand*)pointer) = cmd;
  if (strLen > 0)
	  memcpy(pointer + sizeof(PipeCommand), strPtr, strLen * sizeof(wchar_t));

	DWORD bytesWritten = 0;
	WriteFile(g_pipeHandle, pointer, (DWORD)requiredSize, &bytesWritten, nullptr);
}

void PipePrintFunc(const wchar_t* strPtr)
{
  WriteToPipe(PipeCommand::PRINT, strPtr);
}

std::deque<std::pair<PipeCommand, std::vector<uint8_t>>> g_inputCommandQueue;
std::recursive_mutex g_inputCommandQueueCS;

bool PumpInputCommandQueue()
{
  static std::vector<uint8_t> tempBuffer(1024, 0);
  DWORD totalRead = 0;
  while (true)
  {
    DWORD bytesRead = 0;
    BOOL ret = ReadFile(g_pipeHandle, tempBuffer.data() + totalRead, (DWORD)tempBuffer.size() - totalRead, &bytesRead, nullptr);
    if (!ret)
    {
      if (GetLastError() == ERROR_MORE_DATA)
      {
        tempBuffer.resize(tempBuffer.size() * 2);
        totalRead += bytesRead;
        continue;
      }
      else
      {
        return false;
      }
    }
    totalRead += bytesRead;
    if (totalRead > 0)
    {
      PipeCommand command = *((PipeCommand*)tempBuffer.data());
      std::lock_guard<std::recursive_mutex> queueLock(g_inputCommandQueueCS);
      g_inputCommandQueue.emplace_back(command, std::vector<uint8_t>(tempBuffer.data() + sizeof(command), tempBuffer.data() + totalRead));
      totalRead = 0;
      return true;
    }
  }
}

bool QueryBoolSetting(const wchar_t* setting)
{
  WriteToPipe(PipeCommand::REQUEST_BOOL_SETTING, setting);
  while (PumpInputCommandQueue())
  {
    std::lock_guard<std::recursive_mutex> queueLock(g_inputCommandQueueCS);
    for (auto it = g_inputCommandQueue.begin(); it != g_inputCommandQueue.end(); it++)
    {
      if (it->first == PipeCommand::BOOL_SETTING)
      {
        bool ret = it->second[0] != 0;
        g_inputCommandQueue.erase(it);
        return ret;
      }
    }
  }
  return false;
}

std::vector<uint8_t> QueryBinaryResource(const wchar_t* setting)
{
  WriteToPipe(PipeCommand::REQUEST_BINARY_RESOURCE, setting);
  while (PumpInputCommandQueue())
  {
    std::lock_guard<std::recursive_mutex> queueLock(g_inputCommandQueueCS);
    for (auto it = g_inputCommandQueue.begin(); it != g_inputCommandQueue.end(); it++)
    {
      if (it->first == PipeCommand::BINARY_RESOURCE)
      {
        std::vector<uint8_t> ret = std::move(it->second);
        g_inputCommandQueue.erase(it);
        return std::move(ret);
      }
    }
  }
  return std::vector<uint8_t>();
}

std::recursive_mutex hookedWindowsCS;
std::unordered_map<HWND, WNDPROC> hookedWindows;
std::recursive_mutex g_themeDefCS;
std::unique_ptr<ThemeDef> g_themeDef;

static void ApplyMenuChanges(HMENU menu, bool setOwnerDraw, HBRUSH bgBrush);

static LRESULT CallBaseProc(
	_In_ HWND   hwnd,
	_In_ UINT   uMsg,
	_In_ WPARAM wParam,
	_In_ LPARAM lParam
)
{
  WNDPROC oldProc = nullptr;
  {
    std::lock_guard<std::recursive_mutex> lock(hookedWindowsCS);
    auto it = hookedWindows.find(hwnd);
    if (it != hookedWindows.end())
    {
      oldProc = it->second;
      if (uMsg == WM_DESTROY)
      {
        hookedWindows.erase(it);
      }
    }
  }
	
  if (oldProc)
    return CallWindowProcW(oldProc, hwnd, uMsg, wParam, lParam);
  else
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
    std::lock_guard<std::recursive_mutex> lock(g_themeDefCS);
		ApplyMenuChanges((HMENU)wParam, true, g_themeDef->GetBGBrush());
		return ret;
	}

	if (uMsg == WM_DRAWITEM) //if 0 it's a menus
	{
		LPDRAWITEMSTRUCT diStruct = (LPDRAWITEMSTRUCT)lParam;
		if (diStruct->CtlType == ODT_MENU)
		{
			BOOL ret = FALSE;
      std::lock_guard<std::recursive_mutex> lock(g_themeDefCS);
			if (g_themeDef)
			{
				g_themeDef->DrawItem(hwnd, diStruct);
				ret = true;
			}
			//FillRect(diStruct->hDC, &diStruct->rcItem, debugBrush2);
			//WCHAR temp[1024];
			//GetMenuStringW(GetMenu(hwnd), diStruct->itemID, temp, 1024, MF_BYCOMMAND);
			//SetBkMode(diStruct->hDC, TRANSPARENT);
			//DrawTextW(diStruct->hDC, temp, (int)wcslen(temp), &diStruct->rcItem, DT_LEFT);
			//OutputDebugStringW(temp);
			//OutputDebugStringW(L"\n");
			if (ret)
			{
				return TRUE;
			}
		}
	}
	else if (uMsg == WM_MEASUREITEM)
	{
		LPMEASUREITEMSTRUCT miStruct = (LPMEASUREITEMSTRUCT)lParam;
		if (miStruct->CtlType == ODT_MENU)
		{
			BOOL ret = FALSE;
      std::lock_guard<std::recursive_mutex> lock(g_themeDefCS);
			if (g_themeDef)
			{
				g_themeDef->MeasureItem(hwnd, miStruct);
				ret = true;
			}
			//WCHAR temp[1024];
			//GetMenuStringW(GetMenu(hwnd), miStruct->itemID, temp, 1024, MF_BYCOMMAND);
			//HDC dc = GetDC(hwnd);
			//SIZE sz;
			//GetTextExtentPoint32(dc, temp, (int)wcslen(temp), &sz);
			//miStruct->itemWidth = sz.cx;
			//miStruct->itemHeight = sz.cy;
			//ReleaseDC(hwnd, dc);
			if (ret)
			{
				return TRUE;
			}
		}
	}
	return CallBaseProc(hwnd, uMsg, wParam, lParam);
}

static void ApplyMenuChanges(HMENU menu, bool setOwnerDraw, HBRUSH bgBrush)
{
	MENUINFO menuInfo;
	memset(&menuInfo, 0, sizeof(menuInfo));
	menuInfo.cbSize = sizeof(menuInfo);
	menuInfo.fMask = MIM_BACKGROUND;
	BOOL b = GetMenuInfo(menu, &menuInfo);
	menuInfo.hbrBack = setOwnerDraw ? bgBrush : 0;
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
			ApplyMenuChanges(menuItemInfo.hSubMenu, setOwnerDraw, bgBrush);
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
      bool hookedWindow = false;
      {
        std::lock_guard<std::recursive_mutex> lock(hookedWindowsCS);
        //one of ours
        if (hookedWindows.count(hwnd) == 0)
        {
          wchar_t className[1024];
          GetClassNameW(hwnd, className, 1024);
          if (_wcsicmp(className, L"PX_WINDOW_CLASS") == 0)
          {
            //it's one of the main windows
            hookedWindows[hwnd] = (WNDPROC)SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)&WindowProcFunc);
            hookedWindow = true;
          }
        }
      }

      if (hookedWindow)
      {
        {
          std::lock_guard<std::recursive_mutex> lock2(g_themeDefCS);
          ApplyMenuChanges(menuHandle, true, g_themeDef->GetBGBrush());
        }
        DrawMenuBar(hwnd);
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
  std::lock_guard<std::recursive_mutex> lock(hookedWindowsCS);
	for (auto it : hookedWindows)
	{
		SetWindowLongPtrW(it.first, GWLP_WNDPROC, (LONG_PTR)it.second);
		HMENU menuHandle = GetMenu(it.first);
		if (menuHandle != 0)
		{
			ApplyMenuChanges(menuHandle, false, 0);
		}
	}
  hookedWindows.clear();
}

void MainThreadInSublimeProcess(void* voidArgs)
{
	MainThreadInSublimeProcessArgs localArgs;
	{
		MainThreadInSublimeProcessArgs* args = reinterpret_cast<MainThreadInSublimeProcessArgs*>(voidArgs);
		localArgs = *args;
		delete args;
	}

	std::wstring pipeName = GetPipeName(GetCurrentProcessId());
	g_pipeHandle = CreateNamedPipeW(pipeName.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_MESSAGE | PIPE_WAIT | PIPE_READMODE_MESSAGE, 1, 10 * 1024*1024, 10 * 1024 * 1024, 500, nullptr);
	if (!g_pipeHandle)
	{
		FreeLibraryAndExitThread(localArgs.myModule, 0);
	}
	ConnectNamedPipe(g_pipeHandle, nullptr);

	g_PrintFunc = &PipePrintFunc;

  Gdiplus::GdiplusStartupInput gdiplusStartupInput;
  ULONG_PTR gdiplusToken;

  Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

	while (PumpInputCommandQueue())
	{
    std::lock_guard<std::recursive_mutex> queueLock(g_inputCommandQueueCS);
    while (g_inputCommandQueue.size() > 0)
    {
      auto frontCommand = std::move(g_inputCommandQueue.front());
      g_inputCommandQueue.pop_front();

      PipeCommand command = frontCommand.first;
      const auto& data = frontCommand.second;
      if (command == PipeCommand::QUIT)
      {
        break;
      }
      else if (command == PipeCommand::PRINT)
      {
        MessageBoxW(0, (const wchar_t*)data.data(), L"Print", 0);
      }
      else if (command == PipeCommand::FIND_NEW_WINDOWS)
      {
        HandleFindNewWindows();
      }
      else if (command == PipeCommand::THEME_UPDATE)
      {
        std::unique_ptr<ThemeDef> newDef = std::make_unique<ThemeDef>((const wchar_t*)data.data());
        bool newIsValid = newDef->IsValid();
        bool oldIsValid;
        {
          std::lock_guard<std::recursive_mutex> lock(g_themeDefCS);
          oldIsValid = g_themeDef ? g_themeDef->IsValid() : false;
          g_themeDef = std::move(newDef);
        }

        if (oldIsValid != newIsValid)
        {
          if (!newIsValid)
          {
            UnhookAllWindows();
          }
          else
          {
            HandleFindNewWindows();
          }
        }
        else
        {
          std::unordered_map<HWND, WNDPROC> hookedWindowCopy;
          {
            std::lock_guard<std::recursive_mutex> lock(hookedWindowsCS);
            hookedWindowCopy = hookedWindows;
          }

          for (auto it : hookedWindowCopy)
          {
            DrawMenuBar(it.first);
          }
        }
      }
    }
	}
	UnhookAllWindows();
	g_PrintFunc = &DefaultPrintFunc;

	CloseHandle(g_pipeHandle);
	g_pipeHandle = 0;

  {
    std::lock_guard<std::recursive_mutex> lock(g_themeDefCS);
    g_themeDef.reset();
  }

  Gdiplus::GdiplusShutdown(gdiplusToken);

  FreeLibrary(localArgs.myModule);
	FreeLibraryAndExitThread(localArgs.myModule, 0);
}
