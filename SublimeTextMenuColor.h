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
  REQUEST_BOOL_SETTING = 5,
  BOOL_SETTING = 6,
  REQUEST_BINARY_RESOURCE = 7,
  BINARY_RESOURCE = 8,
};

typedef void(*PrintFunc)(const wchar_t* strPtr);
void DefaultPrintFunc(const wchar_t* strPtr);
extern PrintFunc g_PrintFunc;
void g_PrintFuncF(const wchar_t* format, ...);

std::wstring GetPipeName(DWORD pid);

void MainThreadInSublimeProcess(void* voidArgs);
bool QueryBoolSetting(const wchar_t* setting);
std::vector<uint8_t> QueryBinaryResource(const wchar_t* setting);
