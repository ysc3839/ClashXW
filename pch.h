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

// rapidjson
#pragma warning(push)
#pragma warning(disable:5054)
#define RAPIDJSON_HAS_STDSTRING 1
#define RAPIDJSON_SSE2 1
#define RAPIDJSON_SSE42 1
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

RAPIDJSON_NAMESPACE_BEGIN
using WDocument = GenericDocument<UTF16<>>;
using WValue = GenericValue<UTF16<>>;
using WStringBuffer = GenericStringBuffer<UTF16<>>;
template<typename OutputStream>
using WWriter = Writer<OutputStream, UTF16<>, UTF16<>>;
RAPIDJSON_NAMESPACE_END

#pragma warning(pop)

// Edge WebView2
#include "WebView2.h"
#include "WebView2EnvironmentOptions.h"

// C++/WinRT
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>

#endif //PCH_H
