#define RPC_USE_NATIVE_WCHAR

#ifdef _WIN64
#include "RPCInterface_x64_s.c"
#else
#include "RPCInterface_Win32_s.c"
#endif
