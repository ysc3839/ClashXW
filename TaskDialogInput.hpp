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

void SetEditPos(HWND hWndEdit, HWND hWndProgressBarSink, HWND hWndDirectUI, wil::unique_hfont& hFont)
{
	RECT rect;
	GetClientRect(hWndProgressBarSink, &rect);
	MapWindowPoints(hWndProgressBarSink, hWndDirectUI, reinterpret_cast<LPPOINT>(&rect), 2);

	const auto width = rect.right - rect.left, height = rect.bottom - rect.top,
		center = rect.top + height / 2;

	auto hdc = wil::GetDC(nullptr);

	NONCLIENTMETRICSW ncm = { sizeof(ncm) };
	_SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0, _GetDpiForWindow(hWndEdit));

	hFont.reset(CreateFontIndirectW(&ncm.lfMessageFont));
	auto select = wil::SelectObject(hdc.get(), hFont.get());

	TEXTMETRICW tm;
	GetTextMetricsW(hdc.get(), &tm);

	const auto editHeight = tm.tmHeight + 4;
	rect.top = center - editHeight / 2;

	ShowWindow(hWndProgressBarSink, SW_HIDE);

	SetWindowFont(hWndEdit, hFont.get(), FALSE);
	SetWindowPos(hWndEdit, HWND_TOP, rect.left, rect.top, width, editHeight, 0);
}

HRESULT TaskDialogInput(HWND hWndOwner, HINSTANCE hInstance, PCWSTR windowTitle, PCWSTR mainInstruction, PCWSTR content, TASKDIALOG_COMMON_BUTTON_FLAGS commonButtons, PCWSTR icon, int* button, std::wstring& input)
{
	HWND hWndDirectUI = nullptr, hWndProgressBarSink = nullptr, hWndEdit = nullptr;
	wil::unique_hfont hFont;

	auto subclass = [&hWndDirectUI, &hWndProgressBarSink, &hWndEdit, &hFont](HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, [[maybe_unused]] UINT_PTR uIdSubclass) -> LRESULT {
		if (uMsg == WM_DPICHANGED)
		{
			const auto result = DefSubclassProc(hWnd, uMsg, wParam, lParam);
			SetEditPos(hWndEdit, hWndProgressBarSink, hWndDirectUI, hFont);
			return result;
		}
		return DefSubclassProc(hWnd, uMsg, wParam, lParam);
	};

	auto callback = [hInstance, &hWndDirectUI, &hWndProgressBarSink, &hWndEdit, &hFont, &subclass, &input](HWND hWnd, UINT msg, [[maybe_unused]] WPARAM wParam, [[maybe_unused]] LPARAM lParam) -> HRESULT {
		switch (msg)
		{
		case TDN_DIALOG_CONSTRUCTED:
		{
			hWndDirectUI = GetWindow(hWnd, GW_CHILD); // The only child of TaskDialog is a DirectUIHWND.
			if (hWndDirectUI)
			{
				// DirectUIHWND has some CtrlNotifySink children, each CtrlNotifySink contains a child.
				HWND hWndSink = GetWindow(hWndDirectUI, GW_CHILD);
				if (hWndSink)
				{
					// We search from bottom to top, because sink for progress bar is the -3rd children.
					hWndSink = GetWindow(hWndSink, GW_HWNDLAST);
					while (hWndSink)
					{
						const HWND hWndChild = GetWindow(hWndSink, GW_CHILD);
						if (hWndChild)
						{
							constexpr int len = static_cast<int>(std::size(PROGRESS_CLASSW));
							wchar_t className[len];
							if (GetClassNameW(hWndChild, className, len) == len - 1)
							{
								if (!memcmp(className, PROGRESS_CLASSW, len))
								{
									hWndProgressBarSink = hWndSink;
									break;
								}
							}
						}

						hWndSink = GetWindow(hWndSink, GW_HWNDPREV);
					}

					if (hWndProgressBarSink)
					{
						hWndEdit = CreateWindowExW(0, WC_EDITW, nullptr, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 0, 0, 0, 0, hWndDirectUI, nullptr, hInstance, nullptr);
						if (hWndEdit)
						{
							SetWindowSubclass(hWnd, [](HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) -> LRESULT {
								return (*reinterpret_cast<decltype(subclass)*>(dwRefData))(hWnd, uMsg, wParam, lParam, uIdSubclass);
							}, reinterpret_cast<UINT_PTR>(&subclass), reinterpret_cast<DWORD_PTR>(&subclass));

							SetWindowTextW(hWndEdit, input.c_str());
							SetEditPos(hWndEdit, hWndProgressBarSink, hWndDirectUI, hFont);
							SetFocus(hWndEdit);
						}
					}
				}
			}
		}
		break;
		case TDN_BUTTON_CLICKED:
			if (wParam == IDOK)
			{
				int len = GetWindowTextLengthW(hWndEdit);
				input.resize(static_cast<size_t>(len) + 1);
				len = GetWindowTextW(hWndEdit, input.data(), len + 1);
				input.resize(static_cast<size_t>(len));
			}
			break;
		}
		return S_OK;
	};

	TASKDIALOGCONFIG config = {
		.cbSize = sizeof(config),
		.hwndParent = hWndOwner,
		.hInstance = hInstance,
		.dwFlags = TDF_SHOW_PROGRESS_BAR,
		.dwCommonButtons = commonButtons,
		.pszWindowTitle = windowTitle,
		.pszMainIcon = icon,
		.pszMainInstruction = mainInstruction,
		.pszContent = content,
		.pfCallback = [](HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, LONG_PTR lpRefData) -> HRESULT {
			return (*reinterpret_cast<decltype(callback)*>(lpRefData))(hWnd, msg, wParam, lParam);
		},
		.lpCallbackData = reinterpret_cast<LONG_PTR>(&callback)
	};
	return TaskDialogIndirect(&config, button, nullptr, nullptr);
}
