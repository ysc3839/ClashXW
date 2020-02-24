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

#include "pch.h"
#include "ClashW.h"

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

	if (!CheckOnlyOneInstance(L"Local\\com.ysc3839.clashw"))
		return EXIT_FAILURE;

	SetCurrentProcessExplicitAppUserModelID(L"com.ysc3839.clashw");

	g_exePath = GetModuleFsPath(hInstance);
	SetCurrentDirectoryW(g_exePath.c_str());
	g_dataPath = GetAppDataPath() / L"ClashW" / L"";

	LoadTranslateData();

	WNDCLASSEXW wcex = {
		.cbSize = sizeof(wcex),
		.lpfnWndProc = WndProc,
		.hInstance = hInstance,
		.hIcon = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_CLASHW)),
		.hCursor = LoadCursorW(nullptr, IDC_ARROW),
		.lpszClassName = L"ClashW",
		.hIconSm = LoadIconW(hInstance, MAKEINTRESOURCEW(IDI_CLASHW))
	};

	RegisterClassExW(&wcex);

	g_hWnd = CreateWindowW(L"ClashW", nullptr, WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, hInstance, nullptr);
	if (!g_hWnd)
		return EXIT_FAILURE;

	MSG msg;
	while (GetMessageW(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	return static_cast<int>(msg.wParam);
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
		SetupMenu();

		nid.hWnd = hWnd;
		nid.hIcon = LoadIconW(g_hInst, MAKEINTRESOURCEW(IDI_CLASHW));
		wcscpy_s(nid.szTip, _(L"ClashW"));

		FAIL_FAST_IF_WIN32_BOOL_FALSE(Shell_NotifyIconW(NIM_ADD, &nid));
		FAIL_FAST_IF_WIN32_BOOL_FALSE(Shell_NotifyIconW(NIM_SETVERSION, &nid));

		WM_TASKBAR_CREATED = RegisterWindowMessageW(L"TaskbarCreated");
		LOG_LAST_ERROR_IF(WM_TASKBAR_CREATED == 0);
	}
	break;
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_HELP_ABOUT:
			DialogBox(g_hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
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
			ShowContextMenu(hWnd, GET_X_LPARAM(wParam), GET_Y_LPARAM(wParam));
			break;
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
