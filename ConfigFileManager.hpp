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

void CopySampleConfigIfNeed()
{
	constexpr auto sampleConfig = R"(port: 7890
socks-port: 7891
allow-lan: false
mode: Rule
log-level: info
external-controller: 127.0.0.1:9090
external-ui: Dashboard

Proxy:

Proxy Group:

Rule:
- DOMAIN-SUFFIX,google.com,DIRECT
- DOMAIN-KEYWORD,google,DIRECT
- DOMAIN,google.com,DIRECT
- DOMAIN-SUFFIX,ad.com,REJECT
- GEOIP,CN,DIRECT
- MATCH,DIRECT
)";

	auto fileName = g_configPath / CLASH_DEF_CONFIG_NAME;
	if (!fs::exists(fileName))
	{
		wil::unique_file file;
		_wfopen_s(&file, fileName.c_str(), L"wb");
		THROW_HR_IF_NULL(E_ACCESSDENIED, file);
		fputs(sampleConfig, file.get());
	}
}

void SetupDataDirectory()
{
	try
	{
		CreateDirectoryIgnoreExist(g_configPath.c_str());
		CopySampleConfigIfNeed();
	}
	CATCH_LOG();
}

std::vector<fs::path> GetConfigFilesList()
{
	std::vector<fs::path> list;
	for (auto& f : fs::directory_iterator(g_configPath))
	{
		if (f.is_regular_file() && f.path().extension().native() == L".yaml")
			list.emplace_back(f.path().filename());
	}
	return list;
}

wil::unique_folder_change_reader reader = nullptr;
wil::unique_threadpool_timer timer = nullptr;
bool pause = false;

void WatchConfigFile()
{
	timer.reset(CreateThreadpoolTimer([](PTP_CALLBACK_INSTANCE, PVOID, PTP_TIMER) {
		PostMessageW(g_hWnd, WM_CONFIGCHANGEDETECT, 0, 0);
		pause = false;
	}, nullptr, nullptr));
	THROW_LAST_ERROR_IF_NULL(timer);

	reader = wil::make_folder_change_reader(g_configPath.c_str(), false,
		wil::FolderChangeEvents::FileName | wil::FolderChangeEvents::LastWriteTime,
		[](wil::FolderChangeEvent event, PCWSTR fileName) {
			if (!pause && (event == wil::FolderChangeEvent::Added ||
				event == wil::FolderChangeEvent::Modified ||
				event == wil::FolderChangeEvent::RenameNewName) &&
				g_settings.configFile == fileName)
			{
				pause = true;
				constexpr winrt::Windows::Foundation::TimeSpan latency = 300ms;
				int64_t relative_count = -latency.count();
				SetThreadpoolTimer(timer.get(), reinterpret_cast<PFILETIME>(&relative_count), 0, 0);
			}
		});
}

void StopWatchConfigFile()
{
	reader.reset();
	timer.reset();
	pause = false;
}
