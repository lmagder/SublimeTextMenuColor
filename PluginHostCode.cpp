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
HANDLE g_sublimePipe = 0;

void PipeWrite(PipeCommand cmd, const void* strPtr, size_t strLen)
{
	static std::vector<uint8_t> tempBuffer(1024, 0);
	size_t requiredSize = strLen + sizeof(PipeCommand);

	while (tempBuffer.size() < requiredSize)
	{
		tempBuffer.resize(tempBuffer.size() * 2);
	}

	uint8_t* pointer = tempBuffer.data();
	*((PipeCommand*)pointer) = cmd;
  if (strLen > 0)
	  memcpy(pointer + sizeof(PipeCommand), strPtr, strLen);

	DWORD bytesWritten = 0;
	WriteFile(g_sublimePipe, pointer, (DWORD)requiredSize, &bytesWritten, nullptr);
}

void PipeWriteString(PipeCommand cmd, const wchar_t* strPtr)
{
  PipeWrite(cmd, strPtr, (wcslen(strPtr) + 1) * sizeof(wchar_t));
}

void PollForInput(void* param)
{
  if (g_sublimeProcess == INVALID_HANDLE_VALUE)
  {
    g_PrintFunc(L"Not loaded");
    return;
  }

  std::vector<uint8_t> tempBuffer(1024, 0);
  DWORD totalRead = 0;
  while (true)
  {
    DWORD bytesRead = 0;
    BOOL ret = ReadFile(g_sublimePipe, tempBuffer.data() + totalRead, (DWORD)tempBuffer.size() - totalRead, &bytesRead, nullptr);
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
        break;
      }
    }

    totalRead = 0;

    PipeCommand command = *((PipeCommand*)tempBuffer.data());
    if (command == PipeCommand::QUIT)
    {
      break;
    }
    else if (command == PipeCommand::PRINT)
    {
      g_PrintFunc((const wchar_t*)(tempBuffer.data() + sizeof(PipeCommand)));
    }
    else if (command == PipeCommand::REQUEST_BOOL_SETTING)
    {
      auto settingName = (const wchar_t*)(tempBuffer.data() + sizeof(PipeCommand));
      uint8_t result = g_queryBoolSettingsFunc ? (g_queryBoolSettingsFunc(settingName) ? 1 : 0) : 0;
      PipeWrite(PipeCommand::BOOL_SETTING, &result, 1);
    }
    else if (command == PipeCommand::REQUEST_BINARY_RESOURCE)
    {
      auto resName = (const wchar_t*)(tempBuffer.data() + sizeof(PipeCommand));
      void* buffer = nullptr;
      size_t bufferSize = 0;
      if (g_queryBinaryResourceFunc)
      {
        g_queryBinaryResourceFunc(resName, &buffer, &bufferSize);
      }
      PipeWrite(PipeCommand::BINARY_RESOURCE, buffer, bufferSize);
      if (buffer)
      {
        HeapFree(GetProcessHeap(), 0, buffer);
      }
    }
  }
}

HANDLE g_threadHandle = INVALID_HANDLE_VALUE;

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
	for (int tries = 0; tries < 10; tries++)
	{
		if (WaitNamedPipeW(pipeName.c_str(), NMPWAIT_WAIT_FOREVER))
		{
			break;
		}
		Sleep(250);
	}

	g_sublimePipe = CreateFileW(pipeName.c_str(), GENERIC_WRITE | GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
	if (!remoteThread)
	{
		g_PrintFuncF(L"Can't CreateFileW pipe %s in process %i", pipeName.c_str(), sublimeProcessID);
		return false;
	}

	DWORD dwMode = PIPE_READMODE_MESSAGE;
	SetNamedPipeHandleState(
		g_sublimePipe,    // pipe handle 
		&dwMode,  // new pipe mode 
		NULL,     // don't set maximum bytes 
		NULL);    // don't set maximum time 

  g_threadHandle = (HANDLE)_beginthread(&PollForInput, 0, nullptr);
	//PipeWriteString(PipeCommand::PRINT, L"We are in!");

	return true;
}

PYTHON_CALLABLE bool UnloadFromMainProcess()
{
	if (g_sublimeProcess == INVALID_HANDLE_VALUE)
	{
		g_PrintFunc(L"Not loaded");
		return false;
	}

	PipeCommand quit = PipeCommand::QUIT;
	DWORD written = 0;
	WriteFile(g_sublimePipe, &quit, sizeof(quit), &written, nullptr);

	CloseHandle(g_sublimePipe);
	CloseHandle(g_sublimeProcess);
	g_sublimeProcess = INVALID_HANDLE_VALUE;

  WaitForSingleObject(g_threadHandle, INFINITE);
  g_threadHandle = INVALID_HANDLE_VALUE;

	return true;
}




PYTHON_CALLABLE bool FindTopLevelWindows()
{
	if (g_sublimeProcess == INVALID_HANDLE_VALUE)
	{
		g_PrintFunc(L"Not loaded");
		return false;
	}

	PipeCommand quit = PipeCommand::FIND_NEW_WINDOWS;
	DWORD written = 0;
	WriteFile(g_sublimePipe, &quit, sizeof(quit), &written, nullptr);
	return true;
}

PYTHON_CALLABLE bool UpdateTheme(const wchar_t* theme)
{
	if (g_sublimeProcess == INVALID_HANDLE_VALUE)
	{
		g_PrintFunc(L"Not loaded");
		return false;
	}

	PipeWriteString(PipeCommand::THEME_UPDATE, theme);

	return true;
}
