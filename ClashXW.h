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

namespace fs = std::filesystem;
namespace wrl = Microsoft::WRL;

HINSTANCE g_hInst;
fs::path g_exePath;
fs::path g_dataPath;
bool g_clashOnline = false;
std::string g_clashVersion;
HWND g_hWnd;

constexpr UINT WM_NOTIFYICON = WM_APP + 1;
constexpr UINT WM_PROCESSNOTIFY = WM_APP + 2; // wParam=exitCode
constexpr UINT WM_RESUMECORO = WM_APP + 3; // wParam=coroutine_handle.address

constexpr auto CLASHXW_APP_ID = L"com.ysc3839.clashxw";
constexpr auto CLASHXW_MUTEX_NAME = L"Local\\com.ysc3839.clashxw";
constexpr auto CLASHXW_DIR_NAME = L"ClashXW";
constexpr auto CLASHXW_LINK_NAME = L"ClashXW.lnk";
constexpr auto CLASH_CONFIG_DIR_NAME = L"Config";
constexpr auto CLASH_ASSETS_DIR_NAME = L"ClashAssets";
constexpr auto CLASH_DEF_CONFIG_NAME = L"config.yaml";
constexpr auto CLASH_EXE_NAME = L"clash.exe";

#include "Coroutine.hpp"
#include "Util.hpp"
#include "SysProxy.hpp"
#include "SettingsUtil.hpp"
#include "I18n.hpp"
#include "ClashApi.hpp"
#include "MenuUtil.hpp"
#include "ProcessManager.hpp"
#include "ConfigFileManager.hpp"
#include "DarkMode.hpp"
#include "DialogUtil.hpp"
#include "OSLicensesDlg.hpp"
#include "EdgeWebView2.hpp"

std::unique_ptr<ProcessManager> g_processManager;
std::unique_ptr<ClashApi> g_clashApi;
