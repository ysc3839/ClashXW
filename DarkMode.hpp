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
#include "IatHook.hpp"
#include "delayimp.h"

constexpr COLORREF DarkWindowBkColor = 0x383838;
constexpr COLORREF DarkWindowTextColor = 0xFFFFFF;

enum IMMERSIVE_HC_CACHE_MODE
{
	IHCM_USE_CACHED_VALUE,
	IHCM_REFRESH
};

// 1903 18362
enum class PreferredAppMode
{
	Default,
	AllowDark,
	ForceDark,
	ForceLight,
	Max
};

enum WINDOWCOMPOSITIONATTRIB
{
	WCA_UNDEFINED = 0,
	WCA_NCRENDERING_ENABLED = 1,
	WCA_NCRENDERING_POLICY = 2,
	WCA_TRANSITIONS_FORCEDISABLED = 3,
	WCA_ALLOW_NCPAINT = 4,
	WCA_CAPTION_BUTTON_BOUNDS = 5,
	WCA_NONCLIENT_RTL_LAYOUT = 6,
	WCA_FORCE_ICONIC_REPRESENTATION = 7,
	WCA_EXTENDED_FRAME_BOUNDS = 8,
	WCA_HAS_ICONIC_BITMAP = 9,
	WCA_THEME_ATTRIBUTES = 10,
	WCA_NCRENDERING_EXILED = 11,
	WCA_NCADORNMENTINFO = 12,
	WCA_EXCLUDED_FROM_LIVEPREVIEW = 13,
	WCA_VIDEO_OVERLAY_ACTIVE = 14,
	WCA_FORCE_ACTIVEWINDOW_APPEARANCE = 15,
	WCA_DISALLOW_PEEK = 16,
	WCA_CLOAK = 17,
	WCA_CLOAKED = 18,
	WCA_ACCENT_POLICY = 19,
	WCA_FREEZE_REPRESENTATION = 20,
	WCA_EVER_UNCLOAKED = 21,
	WCA_VISUAL_OWNER = 22,
	WCA_HOLOGRAPHIC = 23,
	WCA_EXCLUDED_FROM_DDA = 24,
	WCA_PASSIVEUPDATEMODE = 25,
	WCA_USEDARKMODECOLORS = 26,
	WCA_LAST = 27
};

struct WINDOWCOMPOSITIONATTRIBDATA
{
	WINDOWCOMPOSITIONATTRIB Attrib;
	PVOID pvData;
	SIZE_T cbData;
};

extern "C" {
	NTSYSAPI VOID NTAPI RtlGetNtVersionNumbers(_Out_ LPDWORD major, _Out_ LPDWORD minor, _Out_ LPDWORD build);
	WINUSERAPI BOOL WINAPI SetWindowCompositionAttribute(_In_ HWND hWnd, _In_ WINDOWCOMPOSITIONATTRIBDATA*);

	// 1809 17763
	THEMEAPI_(bool) ShouldAppsUseDarkMode(); // ordinal 132
	THEMEAPI_(bool) AllowDarkModeForWindow(HWND hWnd, bool allow); // ordinal 133
	THEMEAPI_(bool) AllowDarkModeForApp(bool allow); // ordinal 135, in 1809
	THEMEAPI_(VOID) FlushMenuThemes(); // ordinal 136
	THEMEAPI_(VOID) RefreshImmersiveColorPolicyState(); // ordinal 104
	THEMEAPI_(bool) IsDarkModeAllowedForWindow(HWND hWnd); // ordinal 137
	THEMEAPI_(bool) GetIsImmersiveColorUsingHighContrast(IMMERSIVE_HC_CACHE_MODE mode); // ordinal 106
	THEMEAPI_(HTHEME) OpenNcThemeData(HWND hWnd, LPCWSTR pszClassList); // ordinal 49

	// 1903 18362
	THEMEAPI_(bool) ShouldSystemUseDarkMode(); // ordinal 138
	//THEMEAPI_(PreferredAppMode) SetPreferredAppMode(PreferredAppMode appMode); // ordinal 135, in 1903
	THEMEAPI_(bool) IsDarkModeAllowedForApp(); // ordinal 139
}

using fnSetPreferredAppMode = PreferredAppMode (WINAPI*)(PreferredAppMode appMode); // ordinal 135, in 1903

bool g_darkModeSupported = false;
DWORD g_buildNumber = 0;

bool IsHighContrast()
{
	HIGHCONTRASTW highContrast = { sizeof(highContrast) };
	if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(highContrast), &highContrast, FALSE))
		return highContrast.dwFlags & HCF_HIGHCONTRASTON;
	return false;
}

void RefreshTitleBarThemeColor(HWND hWnd)
{
	BOOL dark = FALSE;
	if (IsDarkModeAllowedForWindow(hWnd) &&
		ShouldAppsUseDarkMode() &&
		!IsHighContrast())
	{
		dark = TRUE;
	}
	if (g_buildNumber < 18362)
		SetPropW(hWnd, L"UseImmersiveDarkModeColors", reinterpret_cast<HANDLE>(static_cast<INT_PTR>(dark)));
	else
	{
		WINDOWCOMPOSITIONATTRIBDATA data = { WCA_USEDARKMODECOLORS, &dark, sizeof(dark) };
		SetWindowCompositionAttribute(hWnd, &data);
	}
}

bool IsColorSchemeChangeMessage(LPARAM lParam)
{
	bool is = false;
	if (lParam && CompareStringOrdinal(reinterpret_cast<LPCWCH>(lParam), -1, L"ImmersiveColorSet", -1, TRUE) == CSTR_EQUAL)
	{
		RefreshImmersiveColorPolicyState();
		is = true;
	}
	GetIsImmersiveColorUsingHighContrast(IHCM_REFRESH);
	return is;
}

bool IsColorSchemeChangeMessage(UINT message, LPARAM lParam)
{
	if (message == WM_SETTINGCHANGE)
		return IsColorSchemeChangeMessage(lParam);
	return false;
}

void SetAppDarkMode(bool allowDark)
{
	if (g_buildNumber < 18362)
		AllowDarkModeForApp(allowDark);
	else
		reinterpret_cast<fnSetPreferredAppMode>(AllowDarkModeForApp)(allowDark ? PreferredAppMode::AllowDark : PreferredAppMode::Default);
}

void FixDarkScrollBar()
{
	// Must loaded because it's static imported.
	HMODULE hComctl = GetModuleHandleW(L"comctl32.dll");
	if (hComctl)
	{
		auto addr = FindDelayLoadThunkInModule(hComctl, "uxtheme.dll", 49); // OpenNcThemeData
		if (addr)
		{
			DWORD oldProtect;
			if (VirtualProtect(addr, sizeof(IMAGE_THUNK_DATA), PAGE_READWRITE, &oldProtect))
			{
				auto MyOpenThemeData = [](HWND hWnd, LPCWSTR classList) -> HTHEME {
					if (wcscmp(classList, L"ScrollBar") == 0)
					{
						hWnd = nullptr;
						classList = L"Explorer::ScrollBar";
					}
					return OpenNcThemeData(hWnd, classList);
				};

				addr->u1.Function = reinterpret_cast<ULONG_PTR>(static_cast<decltype(OpenNcThemeData)*>(MyOpenThemeData));
				VirtualProtect(addr, sizeof(IMAGE_THUNK_DATA), oldProtect, &oldProtect);
			}
		}
	}
}

constexpr bool CheckBuildNumber(DWORD buildNumber)
{
	return (buildNumber == 17763 || // 1809
		buildNumber == 18362 || // 1903
		buildNumber == 18363 || // 1909
		buildNumber == 19041); // 2004
}

void InitDarkMode()
{
	DWORD major, minor;
	RtlGetNtVersionNumbers(&major, &minor, &g_buildNumber);
	g_buildNumber = static_cast<DWORD>(LOWORD(g_buildNumber));
	if (major == 10 && minor == 0 && CheckBuildNumber(g_buildNumber))
	{
		__try
		{
			__HrLoadAllImportsForDll("UxTheme.dll"); // Case sensitive
		}
		__except (GetExceptionCode() == VcppException(ERROR_SEVERITY_ERROR, ERROR_PROC_NOT_FOUND) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
		{
			return;
		}

		g_darkModeSupported = true;

		SetAppDarkMode(true);
		RefreshImmersiveColorPolicyState();

		FixDarkScrollBar();
	}
}

void InitListView(HWND hListView)
{
	HWND hHeader = ListView_GetHeader(hListView);

	if (g_darkModeSupported)
	{
		AllowDarkModeForWindow(hListView, true);
		AllowDarkModeForWindow(hHeader, true);

		SetWindowSubclass(hListView, [](HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR /*uIdSubclass*/, DWORD_PTR dwRefData) -> LRESULT {
			switch (uMsg)
			{
			case WM_NOTIFY:
			{
				if (reinterpret_cast<LPNMHDR>(lParam)->code == NM_CUSTOMDRAW)
				{
					LPNMCUSTOMDRAW nmcd = reinterpret_cast<LPNMCUSTOMDRAW>(lParam);
					switch (nmcd->dwDrawStage)
					{
					case CDDS_PREPAINT:
						return CDRF_NOTIFYITEMDRAW;
					case CDDS_ITEMPREPAINT:
					{
						auto headerTextColor = reinterpret_cast<COLORREF*>(dwRefData);
						SetTextColor(nmcd->hdc, *headerTextColor);
						return CDRF_DODEFAULT;
					}
					}
				}
			}
			break;
			case WM_THEMECHANGED:
			{
				HWND hHeader = ListView_GetHeader(hWnd);
				HTHEME hTheme = OpenThemeData(nullptr, L"ItemsView");
				if (hTheme)
				{
					COLORREF color;
					if (SUCCEEDED(GetThemeColor(hTheme, 0, 0, TMT_TEXTCOLOR, &color)))
					{
						ListView_SetTextColor(hWnd, color);
					}
					if (SUCCEEDED(GetThemeColor(hTheme, 0, 0, TMT_FILLCOLOR, &color)))
					{
						ListView_SetTextBkColor(hWnd, color);
						ListView_SetBkColor(hWnd, color);
					}
					CloseThemeData(hTheme);
				}

				hTheme = OpenThemeData(hHeader, L"Header");
				if (hTheme)
				{
					auto headerTextColor = reinterpret_cast<COLORREF*>(dwRefData);
					GetThemeColor(hTheme, HP_HEADERITEM, 0, TMT_TEXTCOLOR, headerTextColor);
					CloseThemeData(hTheme);
				}

				SendMessageW(hHeader, WM_THEMECHANGED, wParam, lParam);

				RedrawWindow(hWnd, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE);
			}
			break;
			case WM_NCDESTROY:
			{
				auto headerTextColor = reinterpret_cast<COLORREF*>(dwRefData);
				delete headerTextColor;
			}
			break;
			}
			return DefSubclassProc(hWnd, uMsg, wParam, lParam);
			}, 0, reinterpret_cast<DWORD_PTR>(new COLORREF{}));
	}

	ListView_SetExtendedListViewStyle(hListView, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_HEADERDRAGDROP);

	// Hide focus dots
	SendMessageW(hListView, WM_CHANGEUISTATE, MAKELONG(UIS_SET, UISF_HIDEFOCUS), 0);

	SetWindowTheme(hHeader, L"ItemsView", nullptr); // DarkMode
	SetWindowTheme(hListView, L"ItemsView", nullptr); // DarkMode
}

void UpdateRichEditColor(HWND hWnd)
{
	wil::unique_htheme hTheme(OpenThemeData(hWnd, L"Edit"));
	if (hTheme)
	{
		COLORREF color;
		if (SUCCEEDED(GetThemeColor(hTheme.get(), EP_EDITTEXT, ETS_NORMAL, TMT_TEXTCOLOR, &color)))
		{
			CHARFORMATW cf;
			cf.cbSize = sizeof(cf);
			cf.dwMask = CFM_COLOR;
			cf.dwEffects = 0;
			cf.crTextColor = color;
			SendMessageW(hWnd, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&cf));
		}
		if (SUCCEEDED(GetThemeColor(hTheme.get(), EP_BACKGROUND, ETS_NORMAL, TMT_FILLCOLOR, &color)))
		{
			SendMessageW(hWnd, EM_SETBKGNDCOLOR, FALSE, color);
		}
	}

	SendMessageW(hWnd, WM_THEMECHANGED, 0, 0);
	RedrawWindow(hWnd, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE);
}

// Polyfill for ShouldSystemUseDarkMode, because it's unavailable in 1809.
bool ShouldSystemUseLightTheme()
{
	DWORD value = 1, cbValue = sizeof(value);
	if (RegGetValueW(HKEY_CURRENT_USER, LR"(Software\Microsoft\Windows\CurrentVersion\Themes\Personalize)", L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &value, &cbValue) != ERROR_SUCCESS)
		value = 1;
	return value != 0;
}
