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

#include "pch.h"
#include "ClashXW.h"

LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);
	UNREFERENCED_PARAMETER(nCmdShow);

	g_hInst = hInstance;

	winrt::init_apartment(winrt::apartment_type::single_threaded);

	SetCurrentProcessExplicitAppUserModelID(CLASHXW_APP_ID);

	if (!CheckOnlyOneInstance(CLASHXW_MUTEX_NAME))
		return EXIT_FAILURE;

	g_exePath = GetModuleFsPath(hInstance).remove_filename();
	SetCurrentDirectoryW(g_exePath.c_str());
	g_dataPath = GetKnownFolderFsPath(FOLDERID_RoamingAppData) / CLASHXW_DIR_NAME;
	g_configPath = g_dataPath / CLASH_CONFIG_DIR_NAME;

	LoadTranslateData();
	InitDarkMode();
	InitDPIAPI();

	LoadSettings();

	WNDCLASSEXW wcex = {
		.cbSize = sizeof(wcex),
		.lpfnWndProc = WndProc,
		.hInstance = hInstance,
		.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_CLASHXW)),
		.hCursor = LoadCursorW(nullptr, IDC_ARROW),
		.lpszClassName = L"ClashXW",
		.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_CLASHXW))
	};

	RegisterClassExW(&wcex);

	auto hWnd = CreateWindowExW(WS_EX_TOPMOST, L"ClashXW", nullptr, WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, hInstance, nullptr);
	if (!hWnd)
		return EXIT_FAILURE;

	MSG msg;
	while (GetMessageW(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	return static_cast<int>(msg.wParam);
}

void ShowBalloon(const wchar_t* info, const wchar_t* title, DWORD flag = NIIF_INFO)
{
	NOTIFYICONDATAW nid = {
		.cbSize = sizeof(nid),
		.hWnd = g_hWnd,
		.uFlags = NIF_INFO | NIF_SHOWTIP,
		.dwInfoFlags = flag | NIIF_RESPECT_QUIET_TIME
	};
	if (title)
		wcscpy_s(nid.szInfoTitle, title);
	if (info)
		wcscpy_s(nid.szInfo, info);
	LOG_IF_WIN32_BOOL_FALSE(Shell_NotifyIconW(NIM_MODIFY, &nid));
}

winrt::fire_and_forget StartClash()
{
	if (g_processManager->Start())
	{
		for (size_t i = 0; i < 3; ++i)
		{
			co_await winrt::resume_after(1s);
			try
			{
				g_clashVersion = g_clashApi->GetVersion();
			}
			catch (...)
			{
				LOG_CAUGHT_EXCEPTION();
				continue;
			}
			g_clashOnline = true;
			co_return;
		}
		ShowBalloon(_(L"Clash started, but failed to communiacte.\nEnsure `external-controller` is set to `127.0.0.1:9090` in config.yaml."), _(L"Error"), NIIF_ERROR);
	}
	else
	{
		ShowBalloon(_(L"Failed to start clash."), _(L"Error"), NIIF_ERROR);
	}
}

winrt::fire_and_forget UpdateConfigFile(fs::path name, bool showSuccess = false)
{
	co_await winrt::resume_background();
	std::optional<std::wstring> errorDesp;
	try
	{
		if (name.empty())
			errorDesp = g_clashApi->RequestConfigUpdate(g_configPath / g_settings.configFile);
		else
		{
			auto config = g_configPath / name;
			errorDesp = g_clashApi->RequestConfigUpdate(config);
			if (!errorDesp.has_value())
			{
				g_settings.configFile = name.native();
				g_processManager->SetConfigFile(config);
			}
		}
	}
	catch (...)
	{
		LOG_CAUGHT_EXCEPTION();
		co_return;
	}
	co_await ResumeForeground();
	if (errorDesp.has_value())
		ShowBalloon(errorDesp->c_str(), _(L"Reload Config Fail"), NIIF_ERROR);
	else if (showSuccess)
		ShowBalloon(_(L"Success"), _(L"Reload Config Succeed"));
	UpdateMenus();
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static UINT WM_TASKBAR_CREATED = 0;
	static NOTIFYICONDATAW nid = {
		.cbSize = sizeof(nid),
		.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP,
		.uCallbackMessage = WM_NOTIFYICON,
		.uVersion = NOTIFYICON_VERSION_4
	};
	static bool altState = false; // true=down, used by WM_ENTERIDLE
	static bool configChangeDetected = false;
	switch (message)
	{
	case WM_CREATE:
	{
		g_hWnd = hWnd;

		SetupMenu();

		nid.hWnd = hWnd;
		nid.hIcon = LoadIconW(g_hInst, MAKEINTRESOURCEW(IDI_CLASHXW));
		wcscpy_s(nid.szTip, _(L"ClashXW"));

		if (Shell_NotifyIconW(NIM_ADD, &nid))
		{
			FAIL_FAST_IF_WIN32_BOOL_FALSE(Shell_NotifyIconW(NIM_SETVERSION, &nid));
		}
		else
		{
			LOG_LAST_ERROR();
		}

		WM_TASKBAR_CREATED = RegisterWindowMessageW(L"TaskbarCreated");
		LOG_LAST_ERROR_IF(WM_TASKBAR_CREATED == 0);

		auto assetsDir = g_exePath / CLASH_ASSETS_DIR_NAME;
		g_processManager = std::make_unique<ProcessManager>(assetsDir / CLASH_EXE_NAME, assetsDir, g_configPath / g_settings.configFile, assetsDir / CLASH_DASHBOARD_DIR_NAME, CLASH_CTL_ADDR, CLASH_CTL_SECRET);
		g_clashApi = std::make_unique<ClashApi>(L"127.0.0.1", static_cast<INTERNET_PORT>(9090));

		SetupDataDirectory();
		StartClash();

		EnableSystemProxy(g_settings.systemProxy);
		CheckMenuItem(g_hContextMenu, IDM_EXPERIMENTAL_OPENDASHBOARDINBROWSER, MF_BYCOMMAND | (g_settings.openDashboardInBrowser ? MF_CHECKED : MF_UNCHECKED));

		WatchConfigFile();
	}
	break;
	case WM_COMMAND:
	{
		WORD wmId = LOWORD(wParam);
		auto type = static_cast<MenuIdType>(wmId >> 14);
		if (type == MenuIdType::Command)
		{
			switch (wmId)
			{
			case IDM_MODE_GLOBAL:
			case IDM_MODE_RULE:
			case IDM_MODE_DIRECT:
			{
				ClashProxyMode mode = static_cast<ClashProxyMode>(wmId - IDM_MODE_GLOBAL + 1);
				[mode]() -> winrt::fire_and_forget {
					co_await winrt::resume_background();
					try
					{
						if (!g_clashApi->UpdateProxyMode(mode))
							co_return;
						g_clashConfig = g_clashApi->GetConfig();
					}
					catch (...)
					{
						LOG_CAUGHT_EXCEPTION();
						co_return;
					}
					co_await ResumeForeground();
					UpdateMenus();
				}();
			}
			break;
			case IDM_ALLOWFROMLAN:
			{
				[]() -> winrt::fire_and_forget {
					co_await winrt::resume_background();
					try
					{
						if (!g_clashApi->UpdateAllowLan(!g_clashConfig.allowLan))
							co_return;
						g_clashConfig = g_clashApi->GetConfig();
					}
					catch (...)
					{
						LOG_CAUGHT_EXCEPTION();
						co_return;
					}
					co_await ResumeForeground();
					UpdateMenus();
				}();
			}
			break;
			case IDM_LOGLEVEL_ERROR:
			case IDM_LOGLEVEL_WARNING:
			case IDM_LOGLEVEL_INFO:
			case IDM_LOGLEVEL_DEBUG:
			case IDM_LOGLEVEL_SILENT:
			{
				ClashLogLevel level = static_cast<ClashLogLevel>(wmId - IDM_LOGLEVEL_ERROR + 1);
				[level]() -> winrt::fire_and_forget {
					co_await winrt::resume_background();
					try
					{
						if (!g_clashApi->UpdateLogLevel(level))
							co_return;
						g_clashConfig = g_clashApi->GetConfig();
					}
					catch (...)
					{
						LOG_CAUGHT_EXCEPTION();
						co_return;
					}
					co_await ResumeForeground();
					UpdateMenus();
				}();
			}
			break;
			case IDM_SYSTEMPROXY:
				EnableSystemProxy(!g_settings.systemProxy);
				break;
			case IDM_STARTATLOGIN:
				StartAtLogin::SetEnable(!StartAtLogin::IsEnabled());
				break;
			case IDM_HELP_ABOUT:
			{
				static bool opened = false;
				if (!opened)
				{
					opened = true;
					DialogBoxW(g_hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
					opened = false;
				}
			}
			break;
			case IDM_DASHBOARD:
				if (g_settings.openDashboardInBrowser)
					ShellExecuteW(hWnd, nullptr, L"http://127.0.0.1:9090/ui/", nullptr, nullptr, SW_SHOWNORMAL);
				else
				{
					if (EdgeWebView2::GetCount() == 0)
						EdgeWebView2::Create(nullptr);
				}
				break;
			case IDM_CONFIG_OPENFOLDER:
				ShellExecuteW(hWnd, nullptr, g_configPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
				break;
			case IDM_CONFIG_RELOAD:
				UpdateConfigFile({});
				break;
			case IDM_EXPERIMENTAL_SETBENCHURL:
			{
				auto benchmarkUrl = g_settings.benchmarkUrl;
				int button = 0;
				auto hr = TaskDialogInput(hWnd, g_hInst, _(L"Set benchmark url"), nullptr, _(L"Set benchmark url"), TDCBF_OK_BUTTON | TDCBF_CANCEL_BUTTON, MAKEINTRESOURCEW(IDI_CLASHXW), &button, benchmarkUrl);
				if (SUCCEEDED(hr) && button == IDOK)
				{
					if (IsUrlVaild(benchmarkUrl.c_str()))
						g_settings.benchmarkUrl = benchmarkUrl;
					else
						TaskDialog(hWnd, nullptr, _(L"Warning"), nullptr, _(L"URL is not valid"), TDCBF_OK_BUTTON, TD_WARNING_ICON, nullptr);
				}
			}
			break;
			case IDM_EXPERIMENTAL_OPENDASHBOARDINBROWSER:
				g_settings.openDashboardInBrowser = !g_settings.openDashboardInBrowser;
				CheckMenuItem(g_hContextMenu, IDM_EXPERIMENTAL_OPENDASHBOARDINBROWSER, MF_BYCOMMAND | (g_settings.openDashboardInBrowser ? MF_CHECKED : MF_UNCHECKED));
				break;
			case IDM_QUIT:
				DestroyWindow(hWnd);
				break;
			default:
				return DefWindowProcW(hWnd, message, wParam, lParam);
			}
		}
		else if (type == MenuIdType::Config)
		{
			size_t i = wmId & 0x3FFF;
			if (i < g_configFilesList.size())
			{
				UpdateConfigFile(g_configFilesList[i]);
			}
		}
	}
	break;
	case WM_DESTROY:
		StopWatchConfigFile();
		g_hMenuHook.reset();
		g_hMenuAccel.reset();
		g_processManager->ForceStop();
		SaveSettings();
		Shell_NotifyIconW(NIM_DELETE, &nid);
		PostQuitMessage(0);
		break;
	case WM_ENTERIDLE:
		if (wParam == MSGF_MENU)
		{
			bool currentAltState = (GetKeyState(VK_MENU) & 0x8000);
			if (altState != currentAltState)
			{
				altState = currentAltState;
				auto text = currentAltState ?
					_(L"Copy shell command (External IP)\tCtrl+Alt+C") :
					_(L"Copy shell command\tCtrl+C");
				SetMenuItemText(g_hContextMenu, 3, text);
			}
		}
		break;
	case WM_NOTIFYICON:
		switch (LOWORD(lParam))
		{
		case NIN_SELECT:
		case NIN_KEYSELECT:
		case WM_CONTEXTMENU:
			[](HWND hWnd, WPARAM wParam) -> winrt::fire_and_forget {
				co_await winrt::resume_background();
				[]() -> IAsyncAction {
					co_await winrt::resume_background();
					try
					{
						g_clashConfig = g_clashApi->GetConfig();
					}
					CATCH_LOG();
				}().wait_for(500ms);

				co_await ResumeForeground();
				UpdateMenus();
				ShowContextMenu(hWnd, GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam));
			}(hWnd, wParam);
			break;
		case NIN_BALLOONUSERCLICK:
			if (configChangeDetected)
				UpdateConfigFile({}, true);
			[[fallthrough]];
		case NIN_BALLOONTIMEOUT:
			configChangeDetected = false;
			break;
		}
		break;
	case WM_PROCESSNOTIFY:
		LOG_HR_MSG(E_FAIL, "Clash crashed with exit code: %d", wParam);
		StartClash();
		break;
	case WM_RESUMECORO:
		std::experimental::coroutine_handle<>::from_address(reinterpret_cast<void*>(wParam)).resume();
		break;
	case WM_CONFIGCHANGEDETECT:
		if (!configChangeDetected)
		{
			configChangeDetected = true;
			ShowBalloon(_(L"Tap to reload config"), _(L"Config file have been changed"));
		}
		break;
	default:
		if (WM_TASKBAR_CREATED && message == WM_TASKBAR_CREATED)
		{
			if (!Shell_NotifyIconW(NIM_MODIFY, &nid))
			{
				FAIL_FAST_IF_WIN32_BOOL_FALSE(Shell_NotifyIconW(NIM_ADD, &nid));
				FAIL_FAST_IF_WIN32_BOOL_FALSE(Shell_NotifyIconW(NIM_SETVERSION, &nid));
			}
		}
		return DefWindowProcW(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	static wil::unique_hbrush hDarkBrush;
	switch (message)
	{
	case WM_INITDIALOG:
		if (g_darkModeSupported)
		{
			AllowDarkModeForWindow(hDlg, true);
			RefreshTitleBarThemeColor(hDlg);
		}
		return (INT_PTR)TRUE;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	case WM_DESTROY:
		hDarkBrush.reset();
		break;
	case WM_NOTIFY:
	{
		auto nml = reinterpret_cast<PNMLINK>(lParam);
		if (nml->hdr.code == NM_CLICK)
		{
			if (*nml->item.szUrl)
			{
				ShellExecuteW(hDlg, nullptr, nml->item.szUrl, nullptr, nullptr, SW_SHOWNORMAL);
			}
			else if (!wcscmp(nml->item.szID, L"license"))
			{
				ShowRichEditDialog(hDlg, _(L"ClashXW License"), 1);
			}
			else if (!wcscmp(nml->item.szID, L"os_license"))
			{
				ShowOpenSourceLicensesDialog(hDlg);
			}
		}
	}
	break;
	case WM_CTLCOLORDLG:
	case WM_CTLCOLORSTATIC:
	{
		bool darkModeEnabled = ShouldAppsUseDarkMode() && !IsHighContrast();
		if (message == WM_CTLCOLORSTATIC)
		{
			HDC hdc = reinterpret_cast<HDC>(wParam);
			COLORREF textColor = DarkWindowTextColor;
			COLORREF bkColor = DarkWindowBkColor;

			if (!darkModeEnabled)
			{
				textColor = GetSysColor(COLOR_WINDOWTEXT);
				bkColor = GetSysColor(COLOR_WINDOW);
			}

			SetTextColor(hdc, textColor);
			SetBkColor(hdc, bkColor);
			SetBkMode(hdc, TRANSPARENT);
		}
		if (darkModeEnabled)
		{
			if (!hDarkBrush)
				hDarkBrush.reset(CreateSolidBrush(DarkWindowBkColor));
			return reinterpret_cast<INT_PTR>(hDarkBrush.get());
		}
		return reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_WINDOW));
	}
	case WM_SETTINGCHANGE:
	{
		if (g_darkModeSupported && IsColorSchemeChangeMessage(lParam))
		{
			RefreshTitleBarThemeColor(hDlg);
			RedrawWindow(hDlg, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE);
		}
	}
	break;
	}
	return (INT_PTR)FALSE;
}
