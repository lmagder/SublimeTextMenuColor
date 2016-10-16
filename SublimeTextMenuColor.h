#pragma once

struct MainThreadInSublimeProcessArgs
{
  HMODULE myModule;
};


typedef void(*PrintFunc)(const wchar_t* strPtr);
void DefaultPrintFunc(const wchar_t* strPtr);
extern PrintFunc g_PrintFunc;
void g_PrintFuncF(const wchar_t* format, ...);

std::wstring GetPipeName(DWORD pid);

void MainThreadInSublimeProcess(void* voidArgs);
bool QueryBoolSetting(const wchar_t* setting, bool def = false);
std::wstring QueryStringSetting(const wchar_t* setting, const std::wstring& def = std::wstring());
std::vector<std::wstring> QueryStringArraySetting(const wchar_t* setting, const std::vector<std::wstring>& def = std::vector<std::wstring>());

double QueryNumberSetting(const wchar_t* setting, double def = 0.0);
std::vector<double> QueryNumberArraySetting(const wchar_t* setting, const std::vector<double>& def = std::vector<double>());
bool QueryBinaryResource(
  /* [string][in] */ const wchar_t *str,
  /* [out] */ unsigned int *outDataSize,
  /* [out] */ unsigned char **outData);
