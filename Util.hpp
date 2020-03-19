/*
 * Copyright (C) 2020 Richard Yu <yurichard3839@gmail.com>
 *
 * This file is part of ClashW.
 *
 * ClashW is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ClashW is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with ClashW.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

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
	auto guard = wil::CoInitializeEx();

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
