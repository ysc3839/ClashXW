#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
#define NOMINMAX
// Windows Header Files
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <windowsx.h>
#include <shobjidl_core.h>
#include <shlobj_core.h>
#include <winhttp.h>
#include <uxtheme.h>
#include <richedit.h>
#include <vssym32.h>
#include <wrl.h>
#include <shlwapi.h>
#include <propvarutil.h>
#include <propkey.h>
#include <wincodec.h>

// C RunTime Header Files
#include <cstdlib>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include <string>
#include <filesystem>
#include <thread>
#include <array>
#include <optional>
#include <exception>
#include <experimental/coroutine>
#include <fstream>
#include <conio.h>
#include <unordered_set>

#include <delayimp.h>
