// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define RPC_USE_NATIVE_WCHAR
// Windows Header Files:
#include <windows.h>
#include <stdint.h>
#include <shellapi.h>
#include <process.h>
#include <Uxtheme.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <set>
#include <algorithm>
#include <array>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <deque>
#include <thread>
#include <objidl.h>
#include <gdiplus.h>
#include <Shlwapi.h>
#include <atlbase.h>
#include <rpc.h>

#undef min
#undef max
#define RAPIDJSON_HAS_STDSTRING 1
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"


#define PYTHON_CALLABLE extern "C" __declspec(dllexport)

#ifdef _WIN64
#include "RPCInterface_x64_h.h"
#else
#include "RPCInterface_Win32_h.h"
#endif
