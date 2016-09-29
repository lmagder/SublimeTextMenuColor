#include "stdafx.h"
#include "SublimeTextMenuColor.h"

PYTHON_CALLABLE void SetPrintCallback(PrintFunc func)
{
	g_PrintFunc = func ? func : &DefaultPrintFunc;
}

HANDLE g_sublimeProcess = INVALID_HANDLE_VALUE;
HANDLE g_sublimePipe = 0;

void PipeWriteString(PipeCommand cmd, const wchar_t* strPtr)
{
	static std::vector<uint8_t> tempBuffer(1024, 0);
	size_t strLen = wcslen(strPtr) + 1; //include null
	size_t requiredSize = strLen * sizeof(wchar_t) + sizeof(PipeCommand);

	while (tempBuffer.size() < requiredSize)
	{
		tempBuffer.resize(tempBuffer.size() * 2);
	}

	uint8_t* pointer = tempBuffer.data();
	*((PipeCommand*)pointer) = cmd;
	memcpy(pointer + sizeof(PipeCommand), strPtr, strLen * sizeof(wchar_t));
	DWORD bytesWritten = 0;
	WriteFile(g_sublimePipe, pointer, (DWORD)requiredSize, &bytesWritten, nullptr);
}

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

	g_sublimePipe = CreateFileW(pipeName.c_str(), GENERIC_WRITE | GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, 0);
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