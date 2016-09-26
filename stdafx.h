// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#include <stdint.h>
#include <shellapi.h>
#include <process.h>
#include <Uxtheme.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <unordered_map>

#define PYTHON_CALLABLE extern "C" __declspec(dllexport)
// TODO: reference additional headers your program requires here
