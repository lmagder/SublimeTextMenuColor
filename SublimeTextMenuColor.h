#pragma once

struct MainThreadInSublimeProcessArgs
{
  HMODULE myModule;
};

enum class PipeCommand : uint32_t
{
  QUIT = 1,
  PRINT = 2,
  FIND_NEW_WINDOWS = 3,
  THEME_UPDATE = 4,
};

typedef void(*PrintFunc)(const wchar_t* strPtr);
void DefaultPrintFunc(const wchar_t* strPtr);
extern PrintFunc g_PrintFunc;
void g_PrintFuncF(const wchar_t* format, ...);

std::wstring GetPipeName(DWORD pid);

void MainThreadInSublimeProcess(void* voidArgs);
