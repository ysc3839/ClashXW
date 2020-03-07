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

struct DialogTemplate
{
	DLGTEMPLATE tmpl;
	WORD menu;
	WORD className;
	WORD title;
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
