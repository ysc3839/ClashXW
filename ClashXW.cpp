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

	SetCurrentProcessExplicitAppUserModelID(CLASHXW_APP_ID);

	if (!CheckOnlyOneInstance(CLASHXW_MUTEX_NAME))
		return EXIT_FAILURE;

	g_exePath = GetModuleFsPath(hInstance).remove_filename();
	SetCurrentDirectoryW(g_exePath.c_str());
	g_dataPath = GetKnownFolderFsPath(FOLDERID_RoamingAppData) / CLASHXW_DIR_NAME;

	LoadTranslateData();
	InitDarkMode();

	// FIXME: move to LoadSettings()
	g_settings.emplace();

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

	auto hWnd = CreateWindowW(L"ClashXW", nullptr, WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, hInstance, nullptr);
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
		.uFlags = NIF_INFO,
		.dwInfoFlags = flag | NIIF_RESPECT_QUIET_TIME
	};
	wcscpy_s(nid.szInfoTitle, title);
	wcscpy_s(nid.szInfo, info);
	LOG_IF_WIN32_BOOL_FALSE(Shell_NotifyIconW(NIM_MODIFY, &nid));
}

winrt::fire_and_forget StartClash()
{
	if (g_processManager->Start())
	{
		for (size_t i = 0; i < 3; ++i)
		{
			using namespace std::chrono_literals;
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

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	static UINT WM_TASKBAR_CREATED = 0;
	static NOTIFYICONDATAW nid = {
		.cbSize = sizeof(nid),
		.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP,
		.uCallbackMessage = WM_NOTIFYICON,
		.uVersion = NOTIFYICON_VERSION_4
	};
	static bool shiftState = false; // true=down, used by WM_MENUCHAR and WM_ENTERIDLE
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
		g_processManager = std::make_unique<ProcessManager>(g_exePath / CLASH_EXE_NAME, assetsDir, g_dataPath / CLASH_CONFIG_DIR_NAME / CLASH_DEF_CONFIG_NAME, assetsDir / CLASH_DASHBOARD_DIR_NAME, CLASH_CTL_ADDR, CLASH_CTL_SECRET);
		g_clashApi = std::make_unique<ClashApi>(L"127.0.0.1", static_cast<INTERNET_PORT>(9090));

		SetupDataDirectory();
		StartClash();
	}
	break;
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_SYSTEMPROXY:
			EnableSystemProxy(!g_settings->systemProxy);
			break;
		case IDM_STARTATLOGIN:
			EnableStartAtLogin(!g_settings->startAtLogin);
			break;
		case IDM_HELP_ABOUT:
			ShowOpenSourceLicensesDialog(hWnd);
			//DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_DASHBOARD:
			if (EdgeWebView2::GetCount() == 0)
				EdgeWebView2::Create(nullptr);
			break;
		case IDM_CONFIG_OPENFOLDER:
			ShellExecuteW(hWnd, nullptr, (g_dataPath / CLASH_CONFIG_DIR_NAME).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
			break;
		case IDM_QUIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProcW(hWnd, message, wParam, lParam);
		}
	}
	break;
	case WM_DESTROY:
		g_processManager->ForceStop();
		Shell_NotifyIconW(NIM_DELETE, &nid);
		PostQuitMessage(0);
		break;
	case WM_MENUCHAR:
	{
		auto charCode = toupper(LOWORD(wParam));
		int id = 0;
		switch (charCode)
		{
		case 'G':
			if (shiftState) id = IDM_MODE_GLOBAL; break;
		case 'R':
			if (shiftState) id = IDM_MODE_RULE; break;
		case 'D':
			if (shiftState) id = IDM_MODE_DIRECT; break;
		case 0x13: // ^S
			if (!shiftState) id = IDM_SYSTEMPROXY; break;
		case 0x03: // ^C
			if (shiftState)
				id = IDM_COPYCOMMAND_EXTERNAL;
			else
				id = IDM_COPYCOMMAND;
			break;
		case 0x14: // ^T
			if (!shiftState) id = IDM_BENCHMARK; break;
		case 0x04: // ^D
			if (!shiftState) id = IDM_DASHBOARD; break;
		case 0x0F: // ^O
			if (!shiftState) id = IDM_CONFIG_OPENFOLDER; break;
		case 0x12: // ^R
			if (!shiftState) id = IDM_CONFIG_RELOAD; break;
		case 0x0D: // ^M
			if (!shiftState) id = IDM_REMOTECONFIG_MANAGE; break;
		case 0x15: // ^U
			if (!shiftState) id = IDM_REMOTECONFIG_UPDATE; break;
		case 0x0C: // ^L
			if (!shiftState) id = IDM_HELP_SHOWLOG; break;
		case 0x11: // ^Q
			if (!shiftState) id = IDM_QUIT; break;
		}
		if (id)
		{
			PostMessageW(hWnd, WM_COMMAND, id, 0);
			return MAKELRESULT(0, MNC_CLOSE);
		}
		return MAKELRESULT(0, MNC_IGNORE);
	}
	break;
	case WM_ENTERIDLE:
		if (wParam == MSGF_MENU)
		{
			bool currentShiftState = (GetKeyState(VK_SHIFT) & 0x8000);
			if (shiftState != currentShiftState)
			{
				shiftState = currentShiftState;
				auto text = currentShiftState ?
					_(L"Copy shell command (External IP)\tCtrl+Shift+C") :
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
			g_clashApi->GetConfigs();
			UpdateMenus();
			ShowContextMenu(hWnd, GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam));
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
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
