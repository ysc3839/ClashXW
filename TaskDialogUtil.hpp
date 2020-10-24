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

using DownloadCompleted = wistd::function<bool(HWND hWnd, LPCWSTR fileName)>;

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
					it->second = GetWindowString(hWndEdit);
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

void ShowTaskDialogErrorInfo(HWND hWnd, HRESULT hr, LPCWSTR text) noexcept
{
	SendMessageW(hWnd, TDM_SET_PROGRESS_BAR_STATE, PBST_ERROR, 0);
	SendMessageW(hWnd, TDM_UPDATE_ELEMENT_TEXT, TDE_MAIN_INSTRUCTION, reinterpret_cast<LPARAM>(text));
	wchar_t msg[32];
	swprintf_s(msg, _(L"Error code: 0x%X"), hr);
	SendMessageW(hWnd, TDM_UPDATE_ELEMENT_TEXT, TDE_CONTENT, reinterpret_cast<LPARAM>(msg));
}

TASKDIALOGCONFIG MakeTaskDialogDownloadPage(PCWSTR mainInstruction, LPCWSTR serverName, INTERNET_PORT port, LPCWSTR path, bool secure, LPCWSTR fileName, DownloadCompleted&& downloadCompleted)
{
	struct DownloadInfo
	{
		std::thread thread;
		LPCWSTR serverName = nullptr;
		INTERNET_PORT port = 0;
		LPCWSTR path = nullptr;
		bool secure = false;
		LPCWSTR fileName = nullptr;
		DownloadCompleted downloadCompleted;
		bool downloadStarted = false;
		std::atomic_bool cancel = false;
		DWORD length = 0;
		std::atomic<DWORD> dlLength = 0;
		DWORD lastDlLength = 0;
	};

	auto dlInfo = new DownloadInfo{
		.serverName = serverName,
		.port = port,
		.path = path,
		.secure = secure,
		.fileName = fileName,
		.downloadCompleted = std::move(downloadCompleted),
	};
	return {
		.cbSize = sizeof(TASKDIALOGCONFIG),
		.hInstance = g_hInst,
		.dwFlags = TDF_SHOW_MARQUEE_PROGRESS_BAR | TDF_CALLBACK_TIMER,
		.dwCommonButtons = TDCBF_CANCEL_BUTTON,
		.pszWindowTitle = L"Download",
		.pszMainInstruction = mainInstruction,
		.pszContent = L" ",
		.pfCallback = [](HWND hWnd, UINT msg, WPARAM wParam, LPARAM, LONG_PTR lpRefData) -> HRESULT {
			auto info = reinterpret_cast<decltype(dlInfo)>(lpRefData);
			switch (msg)
			{
			case TDN_CREATED:
			case TDN_NAVIGATED: // For use with TDM_NAVIGATE_PAGE
				SendMessageW(hWnd, TDM_SET_PROGRESS_BAR_MARQUEE, TRUE, 0);
				info->thread = std::thread([hWnd, info] {
					try
					{
						// FIXME: Proxy support
						wil::unique_winhttp_hinternet hSession(WinHttpOpen(CLASHXW_USERAGENT, WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
						THROW_LAST_ERROR_IF_NULL(hSession);

						DWORD value = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
						if (!LOG_IF_WIN32_BOOL_FALSE(WinHttpSetOption(hSession.get(), WINHTTP_OPTION_SECURE_PROTOCOLS, &value, sizeof(value))))
						{
							value &= ~WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
							LOG_IF_WIN32_BOOL_FALSE(WinHttpSetOption(hSession.get(), WINHTTP_OPTION_SECURE_PROTOCOLS, &value, sizeof(value)));
						}

						value = WINHTTP_DECOMPRESSION_FLAG_ALL;
						LOG_IF_WIN32_BOOL_FALSE(WinHttpSetOption(hSession.get(), WINHTTP_OPTION_DECOMPRESSION, &value, sizeof(value)));

						value = WINHTTP_PROTOCOL_FLAG_HTTP2;
						LOG_IF_WIN32_BOOL_FALSE(WinHttpSetOption(hSession.get(), WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL, &value, sizeof(value)));

						wil::unique_winhttp_hinternet hConnect(WinHttpConnect(hSession.get(), info->serverName, info->port, 0));
						THROW_LAST_ERROR_IF_NULL(hConnect);

						wil::unique_winhttp_hinternet hRequest(WinHttpOpenRequest(hConnect.get(), L"GET", info->path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, info->secure ? WINHTTP_FLAG_SECURE : 0));
						THROW_LAST_ERROR_IF_NULL(hRequest);

						THROW_IF_WIN32_BOOL_FALSE(WinHttpSendRequest(hRequest.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0));
						THROW_IF_WIN32_BOOL_FALSE(WinHttpReceiveResponse(hRequest.get(), nullptr));

						DWORD statusCode;
						DWORD size = sizeof(statusCode);
						THROW_IF_WIN32_BOOL_FALSE(WinHttpQueryHeaders(hRequest.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &statusCode, &size, WINHTTP_NO_HEADER_INDEX));

						THROW_HR_IF(HTTP_E_STATUS_UNEXPECTED, statusCode != 200);

						if (WinHttpQueryHeaders(hRequest.get(), WINHTTP_QUERY_CONTENT_LENGTH | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &info->length, &size, WINHTTP_NO_HEADER_INDEX))
						{
							SendMessageW(hWnd, TDM_SET_MARQUEE_PROGRESS_BAR, FALSE, 0);
							info->downloadStarted = true;
						}
						else
						{
							LOG_LAST_ERROR();
							info->length = 0;
						}

						{
							wil::unique_hfile hFile(CreateFileW(info->fileName, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr));
							THROW_LAST_ERROR_IF(!hFile);

							while (true)
							{
								if (info->cancel)
									THROW_HR(HRESULT_FROM_WIN32(ERROR_CANCELLED));

								constexpr DWORD BUFSIZE = 8192;
								uint8_t buf[BUFSIZE];
								DWORD read;
								THROW_IF_WIN32_BOOL_FALSE(WinHttpReadData(hRequest.get(), buf, BUFSIZE, &read));
								if (read == 0)
									break;

								DWORD written;
								THROW_IF_WIN32_BOOL_FALSE(WriteFile(hFile.get(), buf, read, &written, nullptr));
								THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_DISK_FULL), written != read);

								info->dlLength += read;
							}
						}

						info->cancel = true;
						SendMessageW(hWnd, TDM_SET_PROGRESS_BAR_POS, 100, 0);

						if (info->downloadCompleted)
							if (info->downloadCompleted(hWnd, info->fileName))
								return; // Prevet closing dialog
					}
					catch (const wil::ResultException& ex)
					{
						LOG_CAUGHT_EXCEPTION();
						auto hr = ex.GetErrorCode();
						if (hr != HRESULT_FROM_WIN32(ERROR_CANCELLED))
						{
							info->cancel = true;
							ShowTaskDialogErrorInfo(hWnd, hr, _(L"Download failed"));
							return; // Prevet closing dialog
						}
						PostMessageW(hWnd, TDM_CLICK_BUTTON, 1025, 0); // Close the dialog with special cancel id
					}
					CATCH_LOG();
					PostMessageW(hWnd, TDM_CLICK_BUTTON, 1024, 0); // Close the dialog
				});
				break;
			case TDN_DESTROYED:
				info->thread.join();
				delete info;
				break;
			case TDN_BUTTON_CLICKED:
				if (wParam == IDCANCEL)
				{
					info->cancel = true;
					if (WaitForSingleObject(info->thread.native_handle(), 0) != WAIT_OBJECT_0) // Thread not exited
						return S_FALSE;
				}
				break;
			case TDN_TIMER:
				if (!info->cancel)
				{
					const auto timeDelta = static_cast<DWORD>(wParam);
					if (timeDelta > 400) // milliseconds
					{
						const auto dlLength = info->dlLength.load();
						const auto dlDelta = dlLength - info->lastDlLength;
						info->lastDlLength = dlLength;

						auto unit = _(L"B/s");
						auto speed = static_cast<float>(static_cast<uint64_t>(dlDelta) * 1000 / timeDelta);
						if (speed > 1024)
						{
							speed /= 1024; // KiB
							unit = _(L"KiB/s");
							if (speed > 1024)
							{
								speed /= 1024; // MiB
								unit = _(L"MiB/s");
							}
						}
						wchar_t speedText[32];
						swprintf_s(speedText, L"%.1f %s", speed, unit);
						SendMessageW(hWnd, TDM_UPDATE_ELEMENT_TEXT, TDE_CONTENT, reinterpret_cast<LPARAM>(speedText));

						if (info->length != 0)
						{
							const auto progress = static_cast<uint64_t>(dlLength) * 100 / info->length;
							SendMessageW(hWnd, TDM_SET_PROGRESS_BAR_POS, static_cast<WPARAM>(progress), 0);
						}

						return S_FALSE; // Reset timer tick
					}
				}
				break; // Continue counting
			}
			return S_OK;
		},
		.lpCallbackData = reinterpret_cast<LONG_PTR>(dlInfo)
	};
}
