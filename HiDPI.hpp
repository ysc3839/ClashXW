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

using fnAdjustWindowRectExForDpi = decltype(AdjustWindowRectExForDpi)*;
using fnGetDpiForWindow = decltype(GetDpiForWindow)*;
using fnGetSystemMetricsForDpi = decltype(GetSystemMetricsForDpi)*;
using fnSystemParametersInfoForDpi = decltype(SystemParametersInfoForDpi)*;

fnAdjustWindowRectExForDpi _fnAdjustWindowRectExForDpi = nullptr;
fnGetDpiForWindow _fnGetDpiForWindow = nullptr;
fnGetSystemMetricsForDpi _fnGetSystemMetricsForDpi = nullptr;
fnSystemParametersInfoForDpi _fnSystemParametersInfoForDpi = nullptr;

void InitDPIAPI()
{
	static bool init = false;
	if (!init)
	{
		auto hUser = GetModuleHandleW(L"user32.dll");
		if (hUser)
		{
			_fnAdjustWindowRectExForDpi = GetProcAddressByFunctionDeclaration(hUser, AdjustWindowRectExForDpi);
			_fnGetDpiForWindow = GetProcAddressByFunctionDeclaration(hUser, GetDpiForWindow);
			//_fnGetSystemMetricsForDpi = GetProcAddressByFunctionDeclaration(hUser, GetSystemMetricsForDpi);
			//_fnSystemParametersInfoForDpi = GetProcAddressByFunctionDeclaration(hUser, SystemParametersInfoForDpi);

			init = true;
		}
	}
}

UINT _GetDpiForWindow(HWND hWnd)
{
	if (_fnGetDpiForWindow)
		return _fnGetDpiForWindow(hWnd);

	UINT dpi = 0;

	auto hdc = wil::GetDC(hWnd);
	if (hdc)
		dpi = GetDeviceCaps(hdc.get(), LOGPIXELSX);

	return dpi;
}

BOOL _AdjustWindowRectExForDpi(LPRECT lpRect, DWORD dwStyle, BOOL bMenu, DWORD dwExStyle, UINT dpi)
{
	if (_fnAdjustWindowRectExForDpi)
		return _fnAdjustWindowRectExForDpi(lpRect, dwStyle, bMenu, dwExStyle, dpi);
	return AdjustWindowRectEx(lpRect, dwStyle, bMenu, dwExStyle);
}

int _GetSystemMetricsForDpi(int nIndex, UINT dpi)
{
	if (_fnGetSystemMetricsForDpi)
		return _fnGetSystemMetricsForDpi(nIndex, dpi);
	return GetSystemMetrics(nIndex);
}

BOOL _SystemParametersInfoForDpi(UINT uiAction, UINT uiParam, PVOID pvParam, UINT fWinIni, UINT dpi)
{
	if (_fnSystemParametersInfoForDpi)
		return _fnSystemParametersInfoForDpi(uiAction, uiParam, pvParam, fWinIni, dpi);
	return SystemParametersInfoW(uiAction, uiParam, pvParam, fWinIni);
}

int Scale(int i, uint32_t dpi)
{
	if (dpi == 0)
		return i;
	else
		return MulDiv(i, dpi, USER_DEFAULT_SCREEN_DPI);
}
