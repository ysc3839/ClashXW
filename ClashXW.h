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

#include "resource.h"
#include "boost/preprocessor/stringize.hpp"

namespace fs = std::filesystem;
namespace wrl = Microsoft::WRL;
using winrt::Windows::Foundation::IAsyncAction;
using winrt::Windows::Foundation::IAsyncOperation;
using nlohmann::json;
using namespace std::chrono_literals;

HINSTANCE g_hInst;
bool g_portableMode = false;
fs::path g_exePath;
fs::path g_dataPath;
fs::path g_configPath;
std::vector<fs::path> g_configFilesList;
bool g_clashRunning = false;
bool g_clashOnline = false;
std::string g_clashVersion;
HWND g_hWnd;
HWND g_hWndRemoteConfigDlg = nullptr;
IAsyncAction g_processMonitor = nullptr;

enum class BalloonClickAction
{
	None,
	ReloadConfig,
	ShowConsoleWindow
};
BalloonClickAction g_balloonClickAction = BalloonClickAction::None;

constexpr UINT WM_NOTIFYICON = WM_APP + 1;
constexpr UINT WM_RESUMECORO = WM_APP + 2; // wParam=coroutine_handle.address
constexpr UINT WM_CONFIGCHANGEDETECT = WM_APP + 3;
constexpr UINT WM_ADDREMOTECONFIG = WM_APP + 4;

// RemoteConfigDlg
constexpr UINT WM_REFRESHCONFIGS = WM_APP + 1;

#define CLASHXW_APP_ID L"com.ysc3839.clashxw"
constexpr auto CLASHXW_DATA_DIR = L"ClashXW";
constexpr auto CLASHXW_DATA_DIR_PORTABLE = L"Data";
constexpr auto CLASHXW_LINK_NAME = L"ClashXW.lnk";
constexpr auto CLASHXW_CONFIG_NAME = L"ClashXW.json";
constexpr auto CLASHXW_USERAGENT = L"ClashXW/" BOOST_PP_STRINGIZE(GIT_BASEVERSION_MAJOR) "." BOOST_PP_STRINGIZE(GIT_BASEVERSION_MINOR) "." BOOST_PP_STRINGIZE(GIT_BASEVERSION_PATCH);
constexpr auto CLASHXW_URL_FILEMAP_NAME = L"Local\\ClashXWURL";
constexpr auto CLASH_CONFIG_DIR_NAME = L"Config";
constexpr auto CLASH_ASSETS_DIR_NAME = L"ClashAssets";
constexpr auto CLASH_DASHBOARD_DIR_NAME = L"Dashboard";
constexpr auto CLASH_DEF_CONFIG_NAME = L"config.yaml";
constexpr auto CLASH_EXE_NAME = L"clash.exe";
constexpr auto CLASH_CTL_ADDR = L"127.0.0.1:9090";
constexpr auto CLASH_CTL_SECRET = L"";

#include "I18n.hpp"
#include "Coroutine.hpp"
#include "Util.hpp"
#include "SysProxy.hpp"
#include "SettingsUtil.hpp"
#include "ClashModel.hpp"
#include "ClashApi.hpp"
#include "ConfigFileManager.hpp"
#include "MenuUtil.hpp"
#include "ProcessManager.hpp"
#include "DarkMode.hpp"
#include "DialogUtil.hpp"
#include "OSLicensesDlg.hpp"
#include "TaskDialogUtil.hpp"
#include "RemoteConfigManager.hpp"
#include "RemoteConfigDlg.hpp"
#include "IconUtil.hpp"
#include "PortableModeUtil.hpp"
#include "RegistryUtil.hpp"
#include "URLProtocolUtil.hpp"

std::unique_ptr<ClashApi> g_clashApi;

#include "EdgeWebView2.hpp"
