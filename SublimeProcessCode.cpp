#include "stdafx.h"
#include "SublimeTextMenuColor.h"
#include "ThemeDef.h"


void PipePrintFunc(const wchar_t* strPtr)
{
  OutputDebugStringW(strPtr);
  OutputDebugStringW(L"\r\n");
  RpcTryExcept
  {
    PluginHost_PrintString(strPtr);
  }
  RpcExcept(1)
  {

  }
  RpcEndExcept
}

bool QueryBoolSetting(const wchar_t* setting)
{
  boolean outVal = false;
  RpcTryExcept
  {
    if (PluginHost_GetBoolSetting(setting, &outVal))
    {
      return outVal != 0;
    }
  }
  RpcExcept(1)
  {

  }
  RpcEndExcept
  return false;
}

bool QueryBinaryResource(
  /* [string][in] */ const wchar_t *str,
  /* [out] */ unsigned int *outDataSize,
  /* [out] */ unsigned char **outData)
{
  RpcTryExcept
  {
    return PluginHost_GetBinaryResource(str, outDataSize, outData) != 0;
  }
  RpcExcept(1)
  {

  }
  RpcEndExcept

  *outData = nullptr;
  return false;
}

std::shared_mutex hookedWindowsCS;
std::unordered_map<HWND, WNDPROC> hookedWindows;
std::recursive_mutex g_themeDefCS;
std::unique_ptr<ThemeDef> g_themeDef;

static bool ApplyMenuChanges(HMENU menu, bool setOwnerDraw, HBRUSH bgBrush);

static LRESULT CallBaseProc(
	_In_ HWND   hwnd,
	_In_ UINT   uMsg,
	_In_ WPARAM wParam,
	_In_ LPARAM lParam
)
{
  WNDPROC oldProc = nullptr;
  {
    std::shared_lock<std::shared_mutex> lock(hookedWindowsCS);
    auto it = hookedWindows.find(hwnd);
    if (it != hookedWindows.end())
    {
      oldProc = it->second;
      if (uMsg == WM_DESTROY)
      {
        lock.unlock();
        std::unique_lock<std::shared_mutex> lock2(hookedWindowsCS);
        hookedWindows.erase(hwnd);
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
    {
      std::lock_guard<std::recursive_mutex> lock(g_themeDefCS);
      if (g_themeDef)
        ApplyMenuChanges((HMENU)wParam, true, g_themeDef->GetBGBrush());
    }
		return ret;
	}

  if (uMsg == WM_NCPAINT || uMsg == WM_PAINT || uMsg == WM_ACTIVATE)
  {
    LRESULT ret = CallBaseProc(hwnd, uMsg, wParam, lParam);
    {
      std::lock_guard<std::recursive_mutex> lock(g_themeDefCS);
      if (g_themeDef)
      {
        MENUBARINFO mbi;
        mbi.cbSize = sizeof(mbi);
        GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi);
        HDC hdc = GetWindowDC(hwnd);
        {
          Gdiplus::Graphics graphics(hdc);
          RECT windowRect;
          GetWindowRect(hwnd, &windowRect);
          OffsetRect(&mbi.rcBar, -windowRect.left, -windowRect.top);
          graphics.FillRectangle(g_themeDef->GetBGBrushGDIP(), mbi.rcBar.left, mbi.rcBar.bottom, mbi.rcBar.right - mbi.rcBar.left, 1);
        }
        ReleaseDC(hwnd, hdc);
      }
    }
    return TRUE;
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
      return ret;
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
      return ret;
		}
	}
	return CallBaseProc(hwnd, uMsg, wParam, lParam);
}

static bool ApplyMenuChanges(HMENU menu, bool setOwnerDraw, HBRUSH bgBrush)
{
  bool anyChanged = false;
  MENUINFO menuInfo;
  memset(&menuInfo, 0, sizeof(menuInfo));
  menuInfo.cbSize = sizeof(menuInfo);
  menuInfo.fMask = MIM_BACKGROUND;
  menuInfo.hbrBack = setOwnerDraw ? bgBrush : 0;
  SetMenuInfo(menu, &menuInfo);

	int menuItemCount = GetMenuItemCount(menu);
	for (int i = 0; i < menuItemCount; i++)
	{
		MENUITEMINFOW menuItemInfo;
		memset(&menuItemInfo, 0, sizeof(menuItemInfo));
		menuItemInfo.cbSize = sizeof(menuItemInfo);
		menuItemInfo.fMask = MIIM_FTYPE | MIIM_SUBMENU;
		BOOL b = GetMenuItemInfoW(menu, i, TRUE, &menuItemInfo);

    auto oldType = menuItemInfo.fType;
		if (setOwnerDraw)
			menuItemInfo.fType |= MFT_OWNERDRAW;
		else
			menuItemInfo.fType &= ~MFT_OWNERDRAW;

		menuItemInfo.fMask = MIIM_FTYPE;
    if (oldType != menuItemInfo.fType)
    {
      b = SetMenuItemInfoW(menu, i, TRUE, &menuItemInfo);
      anyChanged = true;
    }

		menuItemInfo.fMask = MIIM_FTYPE | MIIM_SUBMENU;
		b = GetMenuItemInfoW(menu, i, TRUE, &menuItemInfo);
		
		if (menuItemInfo.hSubMenu != 0)
		{
      anyChanged |= ApplyMenuChanges(menuItemInfo.hSubMenu, setOwnerDraw, bgBrush);
		}
	}
  return anyChanged;
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
      wchar_t className[1024];
      GetClassNameW(hwnd, className, 1024);
      if (_wcsicmp(className, L"PX_WINDOW_CLASS") == 0)
      {
        {
          std::shared_lock<std::shared_mutex> lock(hookedWindowsCS);
          //one of ours
          if (hookedWindows.count(hwnd) == 0)
          {
            //it's one of the main windows
            lock.unlock();
            std::unique_lock<std::shared_mutex> lock2(hookedWindowsCS);
            hookedWindows[hwnd] = (WNDPROC)SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)&WindowProcFunc);
          }
        }

        {
          std::lock_guard<std::recursive_mutex> lock2(g_themeDefCS);
          if (g_themeDef)
            ApplyMenuChanges(menuHandle, true, g_themeDef->GetBGBrush());
          else
            ApplyMenuChanges(menuHandle, false, 0);
        }
        DrawMenuBar(hwnd);
        RedrawWindow(hwnd, nullptr, 0, RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW);
        //RedrawWindow(hwnd, nullptr, 0, RDW_ERASE);
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
  {
    std::shared_lock<std::shared_mutex> lock(hookedWindowsCS);
    for (auto it : hookedWindows)
    {
      SetWindowLongPtrW(it.first, GWLP_WNDPROC, (LONG_PTR)it.second);
      HMENU menuHandle = GetMenu(it.first);
      if (menuHandle != 0)
      {
        ApplyMenuChanges(menuHandle, false, 0);

        DrawMenuBar(it.first);
        RedrawWindow(it.first, nullptr, 0, RDW_INVALIDATE | RDW_FRAME | RDW_UPDATENOW);
      }
    }
  }

  {
    std::unique_lock<std::shared_mutex> lock(hookedWindowsCS);
    hookedWindows.clear();
  }
}


void Impl_SublimeProcess_Unload(/* [in] */ handle_t IDL_handle)
{
  RpcMgmtStopServerListening(nullptr);
}

void Impl_SublimeProcess_FindNewWindows(/* [in] */ handle_t IDL_handle)
{
  HandleFindNewWindows();
}

void Impl_SublimeProcess_UpdateTheme(/* [in] */ handle_t IDL_handle, /* [string][in] */ const wchar_t *str)
{
  UnhookAllWindows();
  std::unique_ptr<ThemeDef> newDef = std::make_unique<ThemeDef>(str);
  {
    std::lock_guard<std::recursive_mutex> lock(g_themeDefCS);
    if (newDef->IsValid())
      g_themeDef = std::move(newDef);
    else
      g_themeDef.reset();
  }
  HandleFindNewWindows();
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

  RPC_STATUS status = RpcServerUseProtseqEpW(L"ncacn_np", 1, (RPC_WSTR)pipeName.c_str(), nullptr);
  if (status != RPC_S_OK && status != RPC_S_DUPLICATE_ENDPOINT)
  {
    FreeLibraryAndExitThread(localArgs.myModule, 0);
  }

  status = RpcServerRegisterIfEx(Impl_SublimeProcess_v0_0_s_ifspec, nullptr, nullptr, RPC_IF_ALLOW_LOCAL_ONLY, 1, nullptr);

  g_PrintFunc = &PipePrintFunc;

  Gdiplus::GdiplusStartupInput gdiplusStartupInput;
  ULONG_PTR gdiplusToken;

  Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

  status = RpcServerListen(1, 1, FALSE);

  UnhookAllWindows();
  g_PrintFunc = &DefaultPrintFunc;

  {
    std::lock_guard<std::recursive_mutex> lock(g_themeDefCS);
    g_themeDef.reset();
  }

  Gdiplus::GdiplusShutdown(gdiplusToken);

  status = RpcServerUnregisterIf(Impl_SublimeProcess_v0_0_s_ifspec, nullptr, TRUE);

  FreeLibrary(localArgs.myModule);
	FreeLibraryAndExitThread(localArgs.myModule, 0);
}
