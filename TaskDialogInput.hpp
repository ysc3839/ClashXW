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

using LabelTextVector = std::vector<std::pair<LPCWSTR, std::wstring&>>;
using ControlsVector = std::vector<std::pair<HWND, HWND>>;
// void EditTextChanged(const ControlsVector& hWndControls, WPARAM wParam, LPARAM lParam)
using EditTextChanged = wistd::function<void(const ControlsVector&, WPARAM, LPARAM)>;

void SetControlPos(const ControlsVector& hWndControls, HWND hWndProgressBarSink, HWND hWndParent, wil::unique_hfont& hFont, const LabelTextVector& input)
{
	RECT rect;
	GetClientRect(hWndProgressBarSink, &rect);
	MapWindowRect(hWndProgressBarSink, hWndParent, &rect);

	const auto width = rect.right - rect.left, height = rect.bottom - rect.top,
		center = rect.top + height / 2;

	auto hdc = wil::GetDC(hWndParent);

	const auto dpi = _GetDpiForWindow(hWndParent);

	NONCLIENTMETRICSW ncm = { sizeof(ncm) };
	_SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0, dpi);

	hFont.reset(CreateFontIndirectW(&ncm.lfMessageFont));
	auto select = wil::SelectObject(hdc.get(), hFont.get());

	TEXTMETRICW tm;
	GetTextMetricsW(hdc.get(), &tm);

	constexpr long editPadding = 2;
	const auto controlHeight = editPadding + tm.tmHeight + editPadding;
	rect.top = center - controlHeight / 2;

	ShowWindow(hWndProgressBarSink, SW_HIDE);

	long maxLabelWidth = 0;
	for (auto [label, _] : input)
	{
		if (label && *label)
		{
			RECT rc = {};
			DrawTextW(hdc.get(), label, -1, &rc, DT_CALCRECT | DT_SINGLELINE);

			if (rc.right > maxLabelWidth)
				maxLabelWidth = rc.right;
		}
	}

	const long lineMargin = Scale(10, dpi);
	auto i = hWndControls.size();
	for (auto [hWndStatic, hWndEdit] : hWndControls)
	{
		--i;

		const auto top = rect.top - static_cast<long>(i) * (controlHeight + lineMargin);
		if (hWndStatic)
		{
			SetWindowFont(hWndStatic, hFont.get(), FALSE);
			SetWindowPos(hWndStatic, HWND_TOP, rect.left, top + editPadding, maxLabelWidth, controlHeight, 0);
		}

		constexpr long margin = 4;
		SetWindowFont(hWndEdit, hFont.get(), FALSE);
		SetWindowPos(hWndEdit, HWND_TOP, rect.left + maxLabelWidth + margin, top, width - maxLabelWidth - margin, controlHeight, 0);
	}
}

HRESULT TaskDialogInput(HWND hWndOwner, HINSTANCE hInstance, PCWSTR windowTitle, PCWSTR mainInstruction, PCWSTR content, TASKDIALOG_COMMON_BUTTON_FLAGS commonButtons, PCWSTR icon, int* button, const LabelTextVector& input, EditTextChanged editTextChanged)
{
	HWND hWndDirectUI = nullptr, hWndProgressBarSink = nullptr;
	ControlsVector hWndControls;
	wil::unique_hfont hFont;

	auto subclass = [&hWndDirectUI, &hWndProgressBarSink, &hWndControls, &hFont, &input, &editTextChanged](HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, [[maybe_unused]] UINT_PTR uIdSubclass) -> LRESULT {
		switch (uMsg)
		{
		case WM_DPICHANGED:
		{
			const auto result = DefSubclassProc(hWnd, uMsg, wParam, lParam);
			SetControlPos(hWndControls, hWndProgressBarSink, hWndDirectUI, hFont, input);
			return result;
		}
		case WM_CTLCOLORSTATIC:
		{
			HDC hdc = reinterpret_cast<HDC>(wParam);
			SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
			SetBkColor(hdc, GetSysColor(COLOR_WINDOW));
			SetBkMode(hdc, TRANSPARENT);
			return reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_WINDOW));
		}
		case WM_COMMAND:
			if (HIWORD(wParam) == EN_CHANGE && editTextChanged)
				editTextChanged(hWndControls, wParam, lParam);
			break;
		}
		return DefSubclassProc(hWnd, uMsg, wParam, lParam);
	};

	auto callback = [hInstance, &hWndDirectUI, &hWndProgressBarSink, &hWndControls, &hFont, &subclass, &input](HWND hWnd, UINT msg, [[maybe_unused]] WPARAM wParam, [[maybe_unused]] LPARAM lParam) -> HRESULT {
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
						hWndControls.reserve(input.size());

						size_t i = 1;
						for (auto [label, _] : input)
						{
							HWND hWndStatic = nullptr;
							if (label && *label)
								hWndStatic = CreateWindowW(WC_STATICW, label, WS_CHILD | WS_VISIBLE | SS_RIGHT, 0, 0, 0, 0, hWnd, reinterpret_cast<HMENU>(i), hInstance, nullptr);

							HWND hWndEdit = CreateWindowExW(0, WC_EDITW, nullptr, WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, 0, 0, 0, 0, hWnd, reinterpret_cast<HMENU>(i), hInstance, nullptr);

							hWndControls.emplace_back(hWndStatic, hWndEdit);
							++i;
						}

						SetWindowSubclass(hWnd, [](HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) -> LRESULT {
							return (*reinterpret_cast<decltype(subclass)*>(dwRefData))(hWnd, uMsg, wParam, lParam, uIdSubclass);
						}, reinterpret_cast<UINT_PTR>(&subclass), reinterpret_cast<DWORD_PTR>(&subclass));

						auto it = input.begin();
						for (auto [_, hWndEdit] : hWndControls)
						{
							SetWindowTextW(hWndEdit, it->second.c_str()); // Trigger EN_CHANGE
							++it;
						}

						SetControlPos(hWndControls, hWndProgressBarSink, hWnd, hFont, input);
					}
				}
			}
		}
		break;
		case TDN_BUTTON_CLICKED:
			if (wParam == IDOK)
			{
				auto it = input.begin();
				for (auto [_, hWndEdit] : hWndControls)
				{
					int len = GetWindowTextLengthW(hWndEdit);
					it->second.resize(static_cast<size_t>(len) + 1);
					len = GetWindowTextW(hWndEdit, it->second.data(), len + 1);
					it->second.resize(static_cast<size_t>(len));
					++it;
				}
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

HRESULT TaskDialogInput(HWND hWndOwner, HINSTANCE hInstance, PCWSTR windowTitle, PCWSTR mainInstruction, PCWSTR content, TASKDIALOG_COMMON_BUTTON_FLAGS commonButtons, PCWSTR icon, int* button, std::wstring& input)
{
	LabelTextVector labelText = { {nullptr, input} };
	return TaskDialogInput(hWndOwner, hInstance, windowTitle, mainInstruction, content, commonButtons, icon, button, labelText, {});
}
