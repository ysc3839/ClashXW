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
#include "HiDPI.hpp"

struct DialogTemplate
{
	DLGTEMPLATE tmpl;
	WORD menu;
	WORD className;
	WORD title;
};

void CenterWindow(HWND hWnd, int& width, int& height, DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME, DWORD exStyle = WS_EX_DLGMODALFRAME)
{
	auto dpi = _GetDpiForWindow(hWnd);

	width = Scale(width, dpi);
	height = Scale(height, dpi);
	const int cxScreen = GetSystemMetrics(SM_CXSCREEN), cyScreen = GetSystemMetrics(SM_CYSCREEN); // not influenced by dpi

	RECT rc = { (cxScreen - width) / 2, (cyScreen - height) / 2 };
	rc.right = rc.left + width;
	rc.bottom = rc.top + height;

	_AdjustWindowRectExForDpi(&rc, style, FALSE, exStyle, dpi);
	SetWindowPos(hWnd, nullptr, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, SWP_NOZORDER | SWP_NOACTIVATE);
}

HRESULT SetPreventPinningForWindow(HWND hWnd)
{
	wil::com_ptr_nothrow<IPropertyStore> ps;
	RETURN_IF_FAILED(SHGetPropertyStoreForWindow(hWnd, IID_PPV_ARGS(&ps)));

	PROPVARIANT var;
	InitPropVariantFromBoolean(TRUE, &var);
	RETURN_IF_FAILED(ps->SetValue(PKEY_AppUserModel_PreventPinning, var));

	return S_OK;
}
