#include "stdafx.h"
#include "SublimeTextMenuColor.h"

void DefaultPrintFunc(const wchar_t* strPtr)
{
  std::wcout << strPtr << std::endl;
}

PrintFunc g_PrintFunc = &DefaultPrintFunc;

void g_PrintFuncF(const wchar_t* format, ...)
{
  static std::vector<wchar_t> tempBuffer(1024, 0);
  va_list args;
  va_start(args, format);
  while (_vsnwprintf_s(tempBuffer.data(), tempBuffer.size(), _TRUNCATE, format, args) < 0)
  {
    tempBuffer.resize(tempBuffer.size() * 2);
  }
  g_PrintFunc(tempBuffer.data());
  va_end(args);
}

std::wstring GetPipeName(DWORD pid)
{
  std::wostringstream ss;
  ss << L"\\pipe\\MenuColorPipe_" << pid;
  return ss.str();
}

static bool StringEndsWith(const wchar_t * str, const wchar_t * suffix)
{
  size_t str_len = wcslen(str);
  size_t suffix_len = wcslen(suffix);

  return
    (str_len >= suffix_len) &&
    (0 == _wcsicmp(str + (str_len - suffix_len), suffix));
}

BOOL APIENTRY DllMain(HMODULE hModule,
  DWORD  ul_reason_for_call,
  LPVOID lpReserved
)
{
  WCHAR myEXEPath[MAX_PATH + 1];
  switch (ul_reason_for_call)
  {
  case DLL_PROCESS_ATTACH:
    GetModuleFileNameW(nullptr, myEXEPath, MAX_PATH + 1);
    if (!StringEndsWith(myEXEPath, L"plugin_host.exe"))
    {
      MainThreadInSublimeProcessArgs* args = new MainThreadInSublimeProcessArgs();
      args->myModule = hModule;
      _beginthread(&MainThreadInSublimeProcess, 0, args);
    }
    break;
  case DLL_THREAD_ATTACH:
  case DLL_THREAD_DETACH:
  case DLL_PROCESS_DETACH:
    break;
  }
  return TRUE;
}


void __RPC_FAR * __RPC_USER midl_user_allocate(size_t cBytes)
{
  return((void __RPC_FAR *)HeapAlloc(GetProcessHeap(), 0, cBytes));
}

void __RPC_USER midl_user_free(void __RPC_FAR * p)
{
  HeapFree(GetProcessHeap(), 0, p);
}
