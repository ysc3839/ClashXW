/*
 * Copyright (C) 2020 Richard Yu <yurichard3839@gmail.com>
 *
 * This file is part of ClashXW.
 *
 * ClashXW is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ClashXW is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with ClashXW.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

 // https://msdn.microsoft.com/en-us/magazine/mt763237
std::wstring Utf8ToUtf16(std::string_view utf8)
{
	if (utf8.empty())
	{
		return {};
	}

	constexpr DWORD kFlags = MB_ERR_INVALID_CHARS;
	const int utf8Length = static_cast<int>(utf8.length());
	const int utf16Length = MultiByteToWideChar(
		CP_UTF8,
		kFlags,
		utf8.data(),
		utf8Length,
		nullptr,
		0
	);
	THROW_LAST_ERROR_IF(utf16Length == 0);

	std::wstring utf16(utf16Length, L'\0');
	const int result = MultiByteToWideChar(
		CP_UTF8,
		kFlags,
		utf8.data(),
		utf8Length,
		utf16.data(),
		utf16Length
	);
	THROW_LAST_ERROR_IF(result == 0);

	return utf16;
}

std::string Utf16ToUtf8(std::wstring_view utf16)
{
	if (utf16.empty())
	{
		return {};
	}

	constexpr DWORD kFlags = WC_ERR_INVALID_CHARS;
	const int utf16Length = static_cast<int>(utf16.length());
	const int utf8Length = WideCharToMultiByte(
		CP_UTF8,
		kFlags,
		utf16.data(),
		utf16Length,
		nullptr,
		0,
		nullptr, nullptr
	);
	THROW_LAST_ERROR_IF(utf8Length == 0);

	std::string utf8(utf8Length, '\0');
	const int result = WideCharToMultiByte(
		CP_UTF8,
		kFlags,
		utf16.data(),
		utf16Length,
		utf8.data(),
		utf8Length,
		nullptr, nullptr
	);
	THROW_LAST_ERROR_IF(result == 0);

	return utf8;
}

// https://docs.microsoft.com/en-us/windows/uwp/cpp-and-winrt-apis/author-coclasses#add-helper-types-and-functions
// License: see the https://github.com/MicrosoftDocs/windows-uwp/blob/docs/LICENSE-CODE file
auto GetModuleFsPath(HMODULE hModule)
{
	std::wstring path(MAX_PATH, L'\0');
	DWORD actualSize;

	while(1)
	{
		actualSize = GetModuleFileNameW(hModule, path.data(), static_cast<DWORD>(path.size()));

		if (static_cast<size_t>(actualSize) + 1 > path.size())
			path.resize(path.size() * 2);
		else
			break;
	}

	path.resize(actualSize);
	return fs::path(path);
}

bool CheckOnlyOneInstance(const wchar_t* mutexName)
{
	CreateMutex(nullptr, FALSE, mutexName);
	return (GetLastError() != ERROR_ALREADY_EXISTS);
}

auto GetKnownFolderFsPath(REFKNOWNFOLDERID rfid)
{
	wil::unique_cotaskmem_string path;
	SHGetKnownFolderPath(rfid, 0, nullptr, &path);
	return fs::path(path.get()).concat(L"\\");
}

void CreateDirectoryIgnoreExist(const wchar_t* path)
{
	if (!CreateDirectoryW(path, nullptr))
	{
		auto lastErr = GetLastError();
		if (lastErr != ERROR_ALREADY_EXISTS)
			THROW_WIN32(lastErr);
	}
}

void CreateShellLink(const wchar_t* linkPath, const wchar_t* target)
{
	auto shellLink = wil::CoCreateInstance<IShellLinkW>(CLSID_ShellLink);
	THROW_IF_FAILED(shellLink->SetPath(target));

	auto persistFile = shellLink.query<IPersistFile>();
	THROW_IF_FAILED(persistFile->Save(linkPath, TRUE));
}

void SetClipboardText(std::wstring_view text)
{
	try
	{
		THROW_IF_WIN32_BOOL_FALSE(OpenClipboard(g_hWnd));
		THROW_IF_WIN32_BOOL_FALSE(EmptyClipboard());

		auto hGlobal = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t));
		THROW_LAST_ERROR_IF_NULL(hGlobal);

		{
			wil::unique_hglobal_locked ptr(hGlobal);
			THROW_LAST_ERROR_IF_NULL(ptr.get());

			auto str = static_cast<wchar_t*>(ptr.get());
			auto len = text.copy(str, text.size());
			str[len] = 0;
		}

		SetClipboardData(CF_UNICODETEXT, hGlobal);
	}
	CATCH_LOG();
	CloseClipboard();
}

std::wstring GetClipboardText()
{
	std::wstring result;
	try
	{
		if (!IsClipboardFormatAvailable(CF_UNICODETEXT))
			return result;

		THROW_IF_WIN32_BOOL_FALSE(OpenClipboard(g_hWnd));

		auto hGlobal = GetClipboardData(CF_UNICODETEXT);
		THROW_LAST_ERROR_IF_NULL(hGlobal);

		wil::unique_hglobal_locked ptr(hGlobal);
		THROW_LAST_ERROR_IF_NULL(ptr.get());

		result = static_cast<wchar_t*>(ptr.get());
	}
	CATCH_LOG();
	CloseClipboard();
	return result;
}

bool IsUrlVaild(const wchar_t* url)
{
	try
	{
		if (*url == 0)
			return false;

		wil::com_ptr<IUri> uri;
		THROW_IF_FAILED(CreateUri(url, Uri_CREATE_CANONICALIZE | Uri_CREATE_NO_DECODE_EXTRA_INFO, 0, &uri));

		wil::unique_bstr host;
		THROW_IF_FAILED(uri->GetHost(&host));

		if (*host.get() == 0)
			return false;

		DWORD scheme = URL_SCHEME_UNKNOWN;
		THROW_IF_FAILED(uri->GetScheme(&scheme));

		return scheme == URL_SCHEME_HTTP || scheme == URL_SCHEME_HTTPS;
	}
	CATCH_LOG();
	return false;
}

std::wstring GetWindowString(HWND hWnd)
{
	std::wstring str;
	int len = GetWindowTextLengthW(hWnd);
	if (len != 0)
	{
		str.resize(static_cast<size_t>(len) + 1);
		len = GetWindowTextW(hWnd, str.data(), len + 1);
		str.resize(static_cast<size_t>(len));
	}
	return str;
}
