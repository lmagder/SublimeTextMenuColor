#include "stdafx.h"
#include "SublimeTextMenuColor.h"

typedef bool(*QueryBoolSettingFunc)(const wchar_t* strPtr);
typedef void(*QueryBinaryResourceFunc)(const wchar_t* strPtr, void** buffPointer, size_t* bufSize);

QueryBoolSettingFunc g_queryBoolSettingsFunc = nullptr;
QueryBinaryResourceFunc g_queryBinaryResourceFunc = nullptr;

PYTHON_CALLABLE void SetCallbacks(PrintFunc func, QueryBoolSettingFunc settingFunc, QueryBinaryResourceFunc resourceFunc)
{
	g_PrintFunc = func ? func : &DefaultPrintFunc;
  g_queryBoolSettingsFunc = settingFunc;
  g_queryBinaryResourceFunc = resourceFunc;
}

HANDLE g_sublimeProcess = INVALID_HANDLE_VALUE;

void Impl_PluginHost_PrintString(
  /* [string][in] */ const wchar_t *str)
{
  OutputDebugStringW(str);
  OutputDebugStringW(L"\r\n");
  g_PrintFunc(str);
}


boolean Impl_PluginHost_GetBoolSetting(
  /* [string][in] */ const wchar_t *str,
  /* [out] */ boolean *outValue)
{
  if (g_queryBoolSettingsFunc)
  {
    *outValue = g_queryBoolSettingsFunc(str);
    return true;
  }
  *outValue = false;
  return false;
}

boolean Impl_PluginHost_GetBinaryResource(
  /* [string][in] */ const wchar_t *str,
  /* [out] */ unsigned int *outDataSize,
  /* [out] */ unsigned char **outData)
{
  void* buffer = nullptr;
  size_t bufferSize = 0;
  if (g_queryBinaryResourceFunc)
  {
    g_queryBinaryResourceFunc(str, &buffer, &bufferSize);
    *outDataSize = (unsigned int)bufferSize;
    *outData = (unsigned char*)MIDL_user_allocate(bufferSize);
    if (buffer)
    {
      memcpy(*outData, buffer, bufferSize);
      HeapFree(GetProcessHeap(), 0, buffer);
    }
    return true;
  }
  *outData = nullptr;
  return false;
}

RPC_BINDING_HANDLE bindingHandle = 0;

PYTHON_CALLABLE bool LoadIntoMainProcess()
{
	if (g_sublimeProcess != INVALID_HANDLE_VALUE)
	{
		g_PrintFunc(L"Already loaded");
		return false;
	}

	g_PrintFuncF(L"Command line: %s", GetCommandLineW());

	int numArgs = 0;
	LPWSTR* args = CommandLineToArgvW(GetCommandLineW(), &numArgs);
	if (!args || numArgs < 2)
	{
		LocalFree(args);
		g_PrintFunc(L"Parsing args failed");
		return false;
	}

	std::wistringstream masterIdStr(args[1]);
	LocalFree(args);

	DWORD sublimeProcessID = 0;
	masterIdStr >> sublimeProcessID;

	if (!sublimeProcessID)
	{
		g_PrintFunc(L"Invalid pid");
		return false;
	}

	HMODULE myModule = 0;
	GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&LoadIntoMainProcess, &myModule);

	HMODULE kernel32Module = 0;
	GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, L"kernel32.dll", &kernel32Module);

	WCHAR myPath[MAX_PATH + 1];
	GetModuleFileNameW(myModule, myPath, MAX_PATH + 1);
	g_PrintFuncF(L"My path: %s", myPath);

	g_sublimeProcess = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION, FALSE, sublimeProcessID);
	if (g_sublimeProcess == INVALID_HANDLE_VALUE)
	{
		g_PrintFuncF(L"Can't open process %i", sublimeProcessID);
		return false;
	}

	LPVOID otherProcessBuffer = VirtualAllocEx(g_sublimeProcess, nullptr, MAX_PATH + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	WriteProcessMemory(g_sublimeProcess, otherProcessBuffer, myPath, MAX_PATH + 1, nullptr);
	HANDLE remoteThread = CreateRemoteThread(g_sublimeProcess, nullptr, 0, (LPTHREAD_START_ROUTINE)GetProcAddress(kernel32Module, "LoadLibraryW"), otherProcessBuffer, 0, nullptr);
	if (!remoteThread)
	{
		VirtualFreeEx(g_sublimeProcess, otherProcessBuffer, 0, MEM_RELEASE);
		g_PrintFuncF(L"Can't create thread in process %i", sublimeProcessID);
		return false;
	}
	WaitForSingleObject(remoteThread, INFINITE);
	CloseHandle(remoteThread);
	VirtualFreeEx(g_sublimeProcess, otherProcessBuffer, 0, MEM_RELEASE);

	std::wstring pipeName = GetPipeName(sublimeProcessID);
  wchar_t* stringBinding = nullptr;
  RPC_STATUS status = RpcStringBindingComposeW(nullptr, L"ncacn_np", nullptr, (RPC_WSTR)pipeName.c_str(), nullptr, &stringBinding);
  std::wstring stringBindingStr = stringBinding;
  status = RpcStringFreeW(&stringBinding);

  for (int tryc = 0; tryc < 100; tryc++)
  {
    RPC_BINDING_HANDLE clientHandle;
    if (RpcBindingFromStringBindingW((RPC_WSTR)stringBindingStr.c_str(), &clientHandle) == RPC_S_OK)
    {
      RpcBindingSetOption(clientHandle, RPC_C_OPT_BINDING_NONCAUSAL, FALSE);
      RpcBindingSetOption(clientHandle, RPC_C_OPT_DONT_LINGER, TRUE);
      bindingHandle = clientHandle;
      return true;
    }
    Sleep(100);
  }
	return true;
}

PYTHON_CALLABLE bool UnloadFromMainProcess()
{
	if (g_sublimeProcess == INVALID_HANDLE_VALUE)
	{
		g_PrintFunc(L"Not loaded");
		return false;
	}

  SetCallbacks(nullptr, nullptr, nullptr);

  HMODULE moduleHandle;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)&UnloadFromMainProcess, &moduleHandle);
  

  std::thread wrappedUpdate([moduleHandle]
  {
    while (true)
    {
      RpcTryExcept
      {
        SublimeProcess_Unload(bindingHandle);
      }
        RpcExcept(1)
      {
        if (RpcExceptionCode() == RPC_S_SERVER_TOO_BUSY)
          continue;
      }
      RpcEndExcept
        break;
    }

    RpcBindingFree(&bindingHandle);
    FreeLibrary(moduleHandle);
    FreeLibraryAndExitThread(moduleHandle, 0);
  });
  wrappedUpdate.detach();

  CloseHandle(g_sublimeProcess);
	g_sublimeProcess = INVALID_HANDLE_VALUE;


	return true;
}




PYTHON_CALLABLE bool FindTopLevelWindows()
{
	if (g_sublimeProcess == INVALID_HANDLE_VALUE)
	{
		g_PrintFunc(L"Not loaded");
		return false;
	}
  std::thread wrappedUpdate([]
  {
    while (true)
    {
      RpcTryExcept
      {
        SublimeProcess_FindNewWindows(bindingHandle);
      }
      RpcExcept(1)
      {
        if (RpcExceptionCode() == RPC_S_SERVER_TOO_BUSY)
          continue;
      }
      RpcEndExcept
      break;
    }
  });
  wrappedUpdate.detach();
	return true;
}

PYTHON_CALLABLE bool UpdateTheme(const wchar_t* theme)
{
  if (g_sublimeProcess == INVALID_HANDLE_VALUE)
  {
    g_PrintFunc(L"Not loaded");
    return false;
  }

  std::wstring themeStr = theme;
  std::thread wrappedUpdate([themeStr]
  {
    while (true)
    {
      RpcTryExcept
      {
        SublimeProcess_UpdateTheme(bindingHandle, themeStr.c_str());
      }
        RpcExcept(1)
      {
        if (RpcExceptionCode() == RPC_S_SERVER_TOO_BUSY)
          continue;
      }
      RpcEndExcept
      break;
    }
  });
  wrappedUpdate.detach();
	return true;
}
