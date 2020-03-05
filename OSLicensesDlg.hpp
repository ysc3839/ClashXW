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
#include "HiDPI.hpp"
#include "licenses/generated/LicensesList.hpp"

struct DialogTemplate
{
	DLGTEMPLATE tmpl;
	// It seems that DialogBoxIndirectParam will access data out-of-bounds even when using a non-EX template, add this padding to fix it.
	uint8_t padding[64];
};

void CenterWindow(HWND hWnd, int& width, int& height)
{
	InitDPIAPI();
	auto dpi = _GetDpiForWindow(hWnd);

	width = Scale(width, dpi);
	height = Scale(height, dpi);
	const int cxScreen = GetSystemMetrics(SM_CXSCREEN), cyScreen = GetSystemMetrics(SM_CYSCREEN); // not influenced by dpi

	RECT rc = { (cxScreen - width) / 2, (cyScreen - height) / 2 };
	rc.right = rc.left + width;
	rc.bottom = rc.top + height;

	_AdjustWindowRectExForDpi(&rc, WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME, FALSE, WS_EX_DLGMODALFRAME, dpi);
	SetWindowPos(hWnd, nullptr, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_NOZORDER | SWP_NOACTIVATE);
}

struct TitleAndContent
{
	const wchar_t* title;
	const char8_t* content;
};

INT_PTR CALLBACK RichEditDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(wParam);
	UNREFERENCED_PARAMETER(lParam);
	static HWND hRichEdit = nullptr;
	switch (message)
	{
	case WM_INITDIALOG:
	{
		auto titleAndContent = reinterpret_cast<const TitleAndContent*>(lParam);

		SetWindowTextW(hDlg, titleAndContent->title);

		int width = 600, height = 500;
		CenterWindow(hDlg, width, height);

		static HMODULE hMsftedit = nullptr;
		if (!hMsftedit)
			hMsftedit = LoadLibraryExW(L"msftedit.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);

		hRichEdit = CreateWindowExW(0, MSFTEDIT_CLASS, nullptr, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_NOIME | ES_NOOLEDRAGDROP | ES_MULTILINE | ES_READONLY, 0, 0, width, height, hDlg, nullptr, g_hInst, nullptr);

		SETTEXTEX st = { ST_DEFAULT, CP_UTF8 };
		SendMessageW(hRichEdit, EM_SETTEXTEX, reinterpret_cast<WPARAM>(&st), reinterpret_cast<LPARAM>(titleAndContent->content));

		CHARFORMATW cf;
		cf.cbSize = sizeof(cf);
		cf.dwMask = CFM_FACE;
		wcscpy_s(cf.szFaceName, L"Consolas");
		SendMessageW(hRichEdit, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&cf));

		SetFocus(hRichEdit);

		if (g_darkModeSupported)
		{
			SetWindowTheme(hRichEdit, L"Explorer", nullptr); // DarkMode

			UpdateDarkModeEnabled();
			_AllowDarkModeForWindow(hDlg, g_darkModeEnabled);
			RefreshTitleBarThemeColor(hDlg);
			UpdateRichEditColor(hRichEdit);
		}

		return static_cast<INT_PTR>(FALSE);
	}
	case WM_SIZE:
	{
		int clientWidth = GET_X_LPARAM(lParam), clientHeight = GET_Y_LPARAM(lParam);
		SetWindowPos(hRichEdit, nullptr, 0, 0, clientWidth, clientHeight, SWP_NOZORDER);
	}
	break;
	case WM_SETTINGCHANGE:
	{
		if (g_darkModeSupported && IsColorSchemeChangeMessage(lParam))
		{
			UpdateDarkModeEnabled();
			RefreshTitleBarThemeColor(hDlg);
			UpdateRichEditColor(hRichEdit);
		}
	}
	break;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return static_cast<INT_PTR>(TRUE);
		}
		break;
	}
	return static_cast<INT_PTR>(FALSE);
}

void ShowRichEditDialog(HWND hWndParent, const wchar_t* title, const char8_t* content)
{
	static bool opened = false;
	if (!opened)
	{
		opened = true;

		TitleAndContent titleAndContent = { title, content };
		DialogTemplate dlgTmpl = { {
			.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
			.dwExtendedStyle = WS_EX_DLGMODALFRAME
		} };
		DialogBoxIndirectParamW(g_hInst, &dlgTmpl.tmpl, hWndParent, RichEditDlgProc, reinterpret_cast<LPARAM>(&titleAndContent));

		opened = false;
	}
}

INT_PTR CALLBACK OSLDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	static HWND hListView = nullptr;
	switch (message)
	{
	case WM_INITDIALOG:
	{
		SetWindowTextW(hDlg, _(L"Open source licenses"));

		int width = 200, height = 200;
		CenterWindow(hDlg, width, height);

		hListView = CreateWindowW(WC_LISTVIEWW, nullptr, WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_NOCOLUMNHEADER | LVS_SHOWSELALWAYS | LVS_SINGLESEL, 0, 0, width, height, hDlg, nullptr, g_hInst, nullptr);

		SetWindowTheme(hListView, L"ItemsView", nullptr); // DarkMode
		ListView_SetExtendedListViewStyle(hListView, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_AUTOSIZECOLUMNS | LVS_EX_ONECLICKACTIVATE | LVS_EX_ONECLICKACTIVATE | LVS_EX_UNDERLINEHOT);
		SendMessageW(hListView, WM_CHANGEUISTATE, MAKEWPARAM(UIS_SET, UISF_HIDEFOCUS), 0);

		if (g_darkModeSupported)
		{
			UpdateDarkModeEnabled();
			_AllowDarkModeForWindow(hDlg, g_darkModeEnabled);
			RefreshTitleBarThemeColor(hDlg);
			UpdateListViewColor(hListView);
		}

		LVCOLUMNW lvc;
		lvc.mask = LVCF_WIDTH;
		lvc.cx = INT_MAX;
		ListView_InsertColumn(hListView, 0, &lvc);

		LVITEMW lvi;
		lvi.mask = LVIF_TEXT;
		lvi.iItem = INT_MAX;
		lvi.iSubItem = 0;
		for (auto name : licenseList)
		{
			lvi.pszText = const_cast<LPWSTR>(name);
			ListView_InsertItem(hListView, &lvi);
		}

		return static_cast<INT_PTR>(TRUE);
	}
	case WM_SIZE:
	{
		int clientWidth = GET_X_LPARAM(lParam), clientHeight = GET_Y_LPARAM(lParam);
		SetWindowPos(hListView, nullptr, 0, 0, clientWidth, clientHeight, SWP_NOZORDER);
	}
	break;
	case WM_NOTIFY:
	{
		auto nmia = reinterpret_cast<LPNMITEMACTIVATE>(lParam);
		if (nmia->hdr.code == LVN_ITEMACTIVATE)
		{
			auto i = static_cast<size_t>(nmia->iItem);
			if (i < licenseList.size())
			{
				auto hRes = FindResourceW(g_hInst, MAKEINTRESOURCEW(i + 1), L"TEXT");
				if (hRes)
				{
					auto hResData = LoadResource(g_hInst, hRes);
					if (hResData)
						ShowRichEditDialog(hDlg, licenseList[i], reinterpret_cast<const char8_t*>(hResData));
				}
			}
		}
	}
	break;
	case WM_SETTINGCHANGE:
	{
		if (g_darkModeSupported && IsColorSchemeChangeMessage(lParam))
		{
			UpdateDarkModeEnabled();
			RefreshTitleBarThemeColor(hDlg);
			UpdateListViewColor(hListView);
		}
	}
	break;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return static_cast<INT_PTR>(TRUE);
		}
		break;
	}
	return static_cast<INT_PTR>(FALSE);
}

void ShowOpenSourceLicensesDialog(HWND hWndParent)
{
	static bool opened = false;
	if (!opened)
	{
		opened = true;

		DialogTemplate dlgTmpl = { {
			.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
			.dwExtendedStyle = WS_EX_DLGMODALFRAME
		} };
		DialogBoxIndirectW(g_hInst, &dlgTmpl.tmpl, hWndParent, OSLDlgProc);

		opened = false;
	}
}
