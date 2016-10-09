#define RPC_USE_NATIVE_WCHAR

#ifdef _WIN64
#include "RPCInterface_x64_c.c"
#else
#include "RPCInterface_Win32_c.c"
#endif
