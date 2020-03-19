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

 // This file contains code from
 // https://github.com/Noisyfox/sysproxy/blob/master/sysproxy/main.c
 // which is licensed under the Apache License 2.0.
 // See licenses/sysproxy for more information.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wininet.h>
#include <ras.h>
#include <raserror.h>

#include <vector>

// wil
#ifndef _DEBUG
#define RESULT_DIAGNOSTICS_LEVEL 1
#endif
#include <wil/result.h>

void SetSystemProxy(const wchar_t* server, const wchar_t* bypass = L"<local>")
{
	std::vector<INTERNET_PER_CONN_OPTIONW> options;
	if (!server) // off
	{
		options.emplace_back(INTERNET_PER_CONN_OPTIONW{
			.dwOption = INTERNET_PER_CONN_FLAGS,
			.Value = { .dwValue = PROXY_TYPE_DIRECT }
		});
	}
	else // global
	{
		options.reserve(3);

		options.emplace_back(INTERNET_PER_CONN_OPTIONW{
			.dwOption = INTERNET_PER_CONN_FLAGS,
			.Value = { .dwValue = PROXY_TYPE_PROXY | PROXY_TYPE_DIRECT }
		});

		options.emplace_back(INTERNET_PER_CONN_OPTIONW{
			.dwOption = INTERNET_PER_CONN_PROXY_SERVER,
			.Value = { .pszValue = const_cast<LPWSTR>(server) }
		});

		options.emplace_back(INTERNET_PER_CONN_OPTIONW{
			.dwOption = INTERNET_PER_CONN_PROXY_BYPASS,
			.Value = { .pszValue = const_cast<LPWSTR>(bypass) }
		});
	}

	INTERNET_PER_CONN_OPTION_LISTW optList = {
		.dwSize = sizeof(optList),
		.dwOptionCount = static_cast<DWORD>(options.size()),
		.pOptions = options.data()
	};

	THROW_IF_WIN32_BOOL_FALSE(InternetSetOptionW(nullptr, INTERNET_OPTION_PER_CONNECTION_OPTION, &optList, sizeof(optList)));

	RASENTRYNAMEW entry;
	entry.dwSize = sizeof(entry);
	std::vector<RASENTRYNAMEW> entries;

	DWORD size = sizeof(entry), count;
	LPRASENTRYNAMEW entryAddr = &entry;

	auto ret = RasEnumEntriesW(nullptr, nullptr, entryAddr, &size, &count);
	if (ret == ERROR_BUFFER_TOO_SMALL)
	{
		entries.resize(count);
		entries[0].dwSize = sizeof(RASENTRYNAMEW);
		entryAddr = entries.data();
		ret = RasEnumEntriesW(nullptr, nullptr, entryAddr, &size, &count);
	}
	THROW_IF_WIN32_ERROR(ret);

	for (size_t i = 0; i < count; ++i)
	{
		optList.pszConnection = entryAddr[i].szEntryName;
		THROW_IF_WIN32_BOOL_FALSE(InternetSetOptionW(nullptr, INTERNET_OPTION_PER_CONNECTION_OPTION, &optList, sizeof(optList)));
	}

	THROW_IF_WIN32_BOOL_FALSE(InternetSetOptionW(nullptr, INTERNET_OPTION_PROXY_SETTINGS_CHANGED, nullptr, 0));
	THROW_IF_WIN32_BOOL_FALSE(InternetSetOptionW(nullptr, INTERNET_OPTION_REFRESH, nullptr, 0));
}
