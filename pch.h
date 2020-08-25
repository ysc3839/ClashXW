// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

// add headers that you want to pre-compile here
#include "framework.h"

// wil
#ifndef _DEBUG
#define RESULT_DIAGNOSTICS_LEVEL 1
#endif

#include <wil/common.h>
#include <wil/resource.h>
#include <wil/win32_helpers.h>
#include <wil/result.h>
#include <wil/com.h>
#include <wil/filesystem.h>

// nlohmann-json
#define JSON_USE_IMPLICIT_CONVERSIONS 0
#include "nlohmann/json.hpp"

// Edge WebView2
#include "WebView2.h"
#include "WebView2EnvironmentOptions.h"

// C++/WinRT
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>

#endif //PCH_H
