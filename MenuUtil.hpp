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

HMENU g_hTopMenu;
HMENU g_hContextMenu;
HMENU g_hProxyModeMenu;
HMENU g_hLogLevelMenu;
HMENU g_hPortsMenu;

BOOL SetMenuItemText(HMENU hMenu, UINT pos, const wchar_t* text) noexcept
{
	MENUITEMINFOW mii = {
		.cbSize = sizeof(mii),
		.fMask = MIIM_STRING,
		.dwTypeData = const_cast<LPWSTR>(text)
	};
	return SetMenuItemInfoW(hMenu, pos, TRUE, &mii);
}

void SetupMenu() noexcept
{
	try {
		g_hTopMenu = LoadMenuW(g_hInst, MAKEINTRESOURCEW(IDC_MENU));
		THROW_LAST_ERROR_IF_NULL(g_hTopMenu);

		g_hContextMenu = GetSubMenu(g_hTopMenu, 0);
		THROW_LAST_ERROR_IF_NULL(g_hContextMenu);

		g_hProxyModeMenu = GetSubMenu(g_hContextMenu, 0);
		THROW_LAST_ERROR_IF_NULL(g_hProxyModeMenu);

		HMENU hHelpMenu = GetSubMenu(g_hContextMenu, 13);
		THROW_LAST_ERROR_IF_NULL(hHelpMenu);

		g_hLogLevelMenu = GetSubMenu(hHelpMenu, 2);
		THROW_LAST_ERROR_IF_NULL(g_hLogLevelMenu);

		g_hPortsMenu = GetSubMenu(hHelpMenu, 3);
		THROW_LAST_ERROR_IF_NULL(g_hPortsMenu);

		SetMenuItemText(g_hContextMenu, 3, _(L"Copy shell command\tCtrl+C"));

		CheckMenuItem(g_hContextMenu, IDM_EXPERIMENTAL_OPENDASHBOARDINBROWSER, MF_BYCOMMAND | (g_settings.openDashboardInBrowser ? MF_CHECKED : MF_UNCHECKED));
	}
	CATCH_FAIL_FAST();
}

void ShowContextMenu(HWND hWnd, int x, int y) noexcept
{
	try {
		THROW_IF_WIN32_BOOL_FALSE(SetForegroundWindow(hWnd));

		UINT flags = TPM_RIGHTBUTTON;
		if (GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0)
			flags |= TPM_RIGHTALIGN;
		else
			flags |= TPM_LEFTALIGN;

		THROW_IF_WIN32_BOOL_FALSE(TrackPopupMenuEx(g_hContextMenu, flags, x, y, hWnd, nullptr));
	}
	CATCH_LOG_RETURN();
}

void UpdateContextMenu() noexcept
{
	CheckMenuItem(g_hContextMenu, 2, MF_BYPOSITION | (g_settings.systemProxy ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(g_hContextMenu, 5, MF_BYPOSITION | (StartAtLogin::IsEnabled() ? MF_CHECKED : MF_UNCHECKED));
	CheckMenuItem(g_hContextMenu, 7, MF_BYPOSITION | (g_clashConfig.allowLan ? MF_CHECKED : MF_UNCHECKED));
}

void UpdateProxyModeMenu()
{
	const wchar_t* modeText;
	switch (g_clashConfig.mode)
	{
	case ClashProxyMode::Global:
		modeText = _(L"Global");
		break;
	case ClashProxyMode::Rule:
		modeText = _(L"Rule");
		break;
	case ClashProxyMode::Direct:
		modeText = _(L"Direct");
		break;
	default:
		modeText = _(L"Unknown");
		break;
	}
	wchar_t text[32];
	swprintf_s(text, _(L"Proxy Mode (%s)"), modeText);
	SetMenuItemText(g_hContextMenu, 0, text);
	CheckMenuRadioItem(g_hProxyModeMenu, 0, 2, static_cast<UINT>(g_clashConfig.mode) - 1, MF_BYPOSITION);
}

void UpdateLogLevelMenu() noexcept
{
	CheckMenuRadioItem(g_hLogLevelMenu, 0, 4, static_cast<UINT>(g_clashConfig.logLevel), MF_BYPOSITION);
}

void UpdatePortsMenu()
{
	wchar_t text[32];
	swprintf_s(text, _(L"Http Port: %hu"), g_clashConfig.port);
	SetMenuItemText(g_hPortsMenu, 0, text);

	swprintf_s(text, _(L"Socks Port: %hu"), g_clashConfig.socksPort);
	SetMenuItemText(g_hPortsMenu, 1, text);

	swprintf_s(text, _(L"Mixed Port: %hu"), g_clashConfig.mixedPort);
	SetMenuItemText(g_hPortsMenu, 2, text);
}

void UpdateMenus()
{
	UpdateContextMenu();
	UpdateProxyModeMenu();
	UpdateLogLevelMenu();
	UpdatePortsMenu();
}
