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

wil::unique_hmenu g_hTopMenu;
HMENU g_hContextMenu;
HMENU g_hProxyModeMenu;

BOOL SetMenuItemText(HMENU hMenu, UINT pos, const wchar_t* text)
{
	MENUITEMINFOW mii = {
		.cbSize = sizeof(mii),
		.fMask = MIIM_STRING,
		.dwTypeData = const_cast<LPWSTR>(text)
	};
	return SetMenuItemInfoW(hMenu, pos, TRUE, &mii);
}

void SetupMenu()
{
	try {
		g_hTopMenu.reset(LoadMenuW(g_hInst, MAKEINTRESOURCEW(IDC_MENU)));
		THROW_LAST_ERROR_IF_NULL(g_hTopMenu);

		g_hContextMenu = GetSubMenu(g_hTopMenu.get(), 0);
		THROW_LAST_ERROR_IF_NULL(g_hContextMenu);

		g_hProxyModeMenu = GetSubMenu(g_hContextMenu, 0);
		THROW_LAST_ERROR_IF_NULL(g_hProxyModeMenu);

		SetMenuItemText(g_hContextMenu, 0, _(L"Proxy Mode (FIXME)")); // FIXME
		SetMenuItemText(g_hContextMenu, 3, _(L"Copy shell command\tCtrl+C"));
	}
	CATCH_FAIL_FAST();
}

void ShowContextMenu(HWND hWnd, int x, int y)
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
