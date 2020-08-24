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

	auto fileName = g_ConfigPath / CLASH_DEF_CONFIG_NAME;
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
		CreateDirectoryIgnoreExist(g_dataPath.c_str());
		CreateDirectoryIgnoreExist(g_ConfigPath.c_str());
		CopySampleConfigIfNeed();
	}
	CATCH_LOG();
}

std::vector<fs::path> GetConfigFilesList()
{
	std::vector<fs::path> list;
	for (auto& f : fs::directory_iterator(g_ConfigPath))
	{
		if (f.is_regular_file() && f.path().extension().native() == L".yaml")
			list.emplace_back(f.path().filename());
	}
	return list;
}
