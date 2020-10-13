#pragma once

void AddOrUpdateRemoteConfigItem(HWND hListView, const RemoteConfig& config, int index = -1)
{
	LVITEMW item;
	item.iItem = INT_MAX;
	item.iSubItem = 0;
	item.mask = LVIF_TEXT;
	item.pszText = const_cast<LPWSTR>(config.url.c_str());
	if (index != -1)
	{
		item.iItem = index;
		ListView_SetItem(hListView, &item);
	}
	else
		item.iItem = ListView_InsertItem(hListView, &item);

	item.iSubItem = 1;
	item.pszText = const_cast<LPWSTR>(config.name.c_str());
	ListView_SetItem(hListView, &item);

	auto dateStr = config.GetTimeString();
	item.iSubItem = 2;
	item.pszText = const_cast<LPWSTR>(dateStr.c_str());
	ListView_SetItem(hListView, &item);
}

void AddRemoteConfigItems(HWND hListView)
{
	SetWindowRedraw(hListView, FALSE);

	for (const auto& config : g_settings.remoteConfig)
		AddOrUpdateRemoteConfigItem(hListView, *config);

	SetWindowRedraw(hListView, TRUE);
}

void UpdateRemoteConfigItems(HWND hListView)
{
	SetWindowRedraw(hListView, FALSE);

	int i = 0;
	for (const auto& config : g_settings.remoteConfig)
		AddOrUpdateRemoteConfigItem(hListView, *config, i++);

	SetWindowRedraw(hListView, TRUE);
}

bool CheckUrl(const wchar_t* url, BSTR* host)
{
	try
	{
		wil::com_ptr_nothrow<IUri> uri;
		THROW_IF_FAILED(CreateUri(url, Uri_CREATE_CANONICALIZE | Uri_CREATE_NO_DECODE_EXTRA_INFO, 0, &uri));

		DWORD scheme = URL_SCHEME_UNKNOWN;
		THROW_IF_FAILED(uri->GetScheme(&scheme));

		if (scheme == URL_SCHEME_HTTP || scheme == URL_SCHEME_HTTPS)
		{
			if (host)
				THROW_IF_FAILED(uri->GetHost(host));
			return true;
		}
	}
	catch (...) {}
	return false;
}

bool EditRemoteConfig(HWND hWnd, std::wstring& url, std::wstring& configName)
{
	int button = 0;
	LabelTextVector labelText = { {_(L"Url:"), url}, {_(L"Config Name:"), configName} };
	auto hr = TaskDialogInput(hWnd, g_hInst, _(L"Remote Config"), nullptr, L" ", TDCBF_OK_BUTTON | TDCBF_CANCEL_BUTTON, MAKEINTRESOURCEW(IDI_CLASHXW), &button, labelText,
		[]([[maybe_unused]] const ControlsVector& hWndControls, WPARAM wParam, LPARAM lParam) {
			if (LOWORD(wParam) == 1) // The first (url) edit is changed
			{
				HWND hWndEdit2 = hWndControls[1].second;
				auto url = GetWindowString(reinterpret_cast<HWND>(lParam));
				if (!url.empty())
				{
					wil::unique_bstr host;
					if (CheckUrl(url.c_str(), &host))
					{
						Edit_SetCueBannerTextFocused(hWndEdit2, host.get(), TRUE);
						return;
					}
				}
				Edit_SetCueBannerText(hWndEdit2, L"");
			}
		});
	if (SUCCEEDED(hr) && button == IDOK)
	{
		if (!url.empty())
		{
			if (configName.empty())
			{
				wil::unique_bstr host;
				if (!CheckUrl(url.c_str(), &host))
					return false;
				configName = host.get();
			}
			else if (!CheckUrl(url.c_str(), nullptr))
				return false;

			constexpr auto notAllowedChars = LR"(\/:*?"<>|)";
			if (configName.find_first_of(notAllowedChars) == std::wstring::npos)
				return true;
		}
	}
	return false;
}

INT_PTR CALLBACK RemoteConfigDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	static wil::unique_hbrush hDarkBrush;
	switch (message)
	{
	case WM_INITDIALOG:
	{
		g_hWndRemoteConfigDlg = hDlg;

		HWND hListView = GetDlgItem(hDlg, IDC_REMOTECONFIG_LISTVIEW);
		InitListView(hListView);
		ListView_SetExtendedListViewStyle(hListView, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_AUTOSIZECOLUMNS);

		LVCOLUMNW lvc;
		lvc.mask = LVCF_WIDTH | LVCF_TEXT;
		lvc.cx = INT_MAX;
		lvc.pszText = const_cast<LPWSTR>(_(L"Url"));
		ListView_InsertColumn(hListView, 0, &lvc);

		lvc.pszText = const_cast<LPWSTR>(_(L"Config Name"));
		ListView_InsertColumn(hListView, 1, &lvc);

		lvc.pszText = const_cast<LPWSTR>(_(L"Update Time"));
		ListView_InsertColumn(hListView, 2, &lvc);

		AddRemoteConfigItems(hListView);

		if (g_darkModeSupported)
		{
			AllowDarkModeForWindow(hDlg, true);
			RefreshTitleBarThemeColor(hDlg);

			HWND hAddButton = GetDlgItem(hDlg, IDC_REMOTECONFIG_ADD),
				hDeleteButton = GetDlgItem(hDlg, IDC_REMOTECONFIG_DELETE),
				hUpdateButton = GetDlgItem(hDlg, IDC_REMOTECONFIG_UPDATE);

			AllowDarkModeForWindow(hAddButton, true);
			SetWindowTheme(hAddButton, L"Explorer", nullptr);

			AllowDarkModeForWindow(hDeleteButton, true);
			SetWindowTheme(hDeleteButton, L"Explorer", nullptr);

			AllowDarkModeForWindow(hUpdateButton, true);
			SetWindowTheme(hUpdateButton, L"Explorer", nullptr);
		}

		RECT rc;
		GetClientRect(hDlg, &rc);
		SendMessageW(hDlg, WM_SIZE, 0, MAKELPARAM(rc.right - rc.left, rc.bottom - rc.top));

		return (INT_PTR)TRUE;
	}
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		//case IDOK:
		case IDCANCEL:
			EndDialog(hDlg, 0);
			return (INT_PTR)TRUE;
		case IDC_REMOTECONFIG_ADD:
		{
			auto config = std::make_unique<RemoteConfig>();
			if (EditRemoteConfig(hDlg, config->url, config->name))
				AddOrUpdateRemoteConfigItem(GetDlgItem(hDlg, IDC_REMOTECONFIG_LISTVIEW), *g_settings.remoteConfig.emplace_back(std::move(config)));
			else
				TaskDialog(hDlg, nullptr, _(L"Warning"), nullptr, _(L"Invalid input"), TDCBF_OK_BUTTON, TD_WARNING_ICON, nullptr);
		}
		break;
		case IDC_REMOTECONFIG_DELETE:
		{
			HWND hListView = GetDlgItem(hDlg, IDC_REMOTECONFIG_LISTVIEW);
			int i = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
			if (i != -1)
			{
				ListView_DeleteItem(hListView, i);
				g_settings.remoteConfig.erase(g_settings.remoteConfig.begin() + i);
			}
		}
		break;
		case IDC_REMOTECONFIG_UPDATE:
		{
			HWND hListView = GetDlgItem(hDlg, IDC_REMOTECONFIG_LISTVIEW);
			int i = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
			if (i != -1)
			{
				RemoteConfigManager::UpdateConfig(*g_settings.remoteConfig[i]).Completed([hListView, i](const IAsyncOperation<bool>& op, auto) {
					if (!g_hWndRemoteConfigDlg) return;
					if (!op.GetResults())
					{
						TaskDialog(g_hWndRemoteConfigDlg, nullptr, _(L"Error"), nullptr, _(L"Remote Config Update Fail"), TDCBF_OK_BUTTON, TD_ERROR_ICON, nullptr);
					}
					AddOrUpdateRemoteConfigItem(hListView, *g_settings.remoteConfig[i], i);
				});
			}
		}
		break;
		}
		break;
	case WM_SIZE:
	{
		const auto width = GET_X_LPARAM(lParam), height = GET_Y_LPARAM(lParam);
		HWND hListView = GetDlgItem(hDlg, IDC_REMOTECONFIG_LISTVIEW),
			hAddButton = GetDlgItem(hDlg, IDC_REMOTECONFIG_ADD),
			hDeleteButton = GetDlgItem(hDlg, IDC_REMOTECONFIG_DELETE),
			hUpdateButton = GetDlgItem(hDlg, IDC_REMOTECONFIG_UPDATE);

		RECT rect;
		GetWindowRect(hListView, &rect);
		POINT listViewPos = { rect.left, rect.top };
		ScreenToClient(hDlg, &listViewPos); // Don't use MapWindowPoints because the result is client area, without the border.

		RECT buttonPos;
		GetClientRect(hAddButton, &buttonPos);
		MapWindowRect(hAddButton, hDlg, &buttonPos);

		const auto buttonHeight = buttonPos.bottom - buttonPos.top;

		HDWP hDWP = BeginDeferWindowPos(4);
		if (hDWP)
		{
			DeferWindowPos(hDWP, hListView, 0, 0, 0, width - listViewPos.x * 2, height - listViewPos.y * 3 - buttonHeight, SWP_NOMOVE | SWP_NOZORDER);
			DeferWindowPos(hDWP, hAddButton, 0, buttonPos.left, height - listViewPos.y - buttonHeight, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

			POINT pos = { 0, 0 };
			MapWindowPoints(hDeleteButton, hDlg, &pos, 1);
			DeferWindowPos(hDWP, hDeleteButton, 0, pos.x, height - listViewPos.y - buttonHeight, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

			pos = { 0, 0 };
			MapWindowPoints(hUpdateButton, hDlg, &pos, 1);
			DeferWindowPos(hDWP, hUpdateButton, 0, pos.x, height - listViewPos.y - buttonHeight, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

			EndDeferWindowPos(hDWP);
		}
	}
	break;
	case WM_NOTIFY:
		switch ((reinterpret_cast<LPNMHDR>(lParam))->code)
		{
		case LVN_ITEMCHANGED:
		{
			auto nmv = reinterpret_cast<LPNMLISTVIEW>(lParam);
			if ((nmv->uChanged & LVIF_STATE) && ((nmv->uNewState ^ nmv->uOldState) & LVIS_SELECTED))
			{
				BOOL enable = FALSE, selected = FALSE;
				if (nmv->uNewState & LVIS_SELECTED)
				{
					if (nmv->iItem >= 0 && g_settings.remoteConfig.size() > static_cast<size_t>(nmv->iItem))
					{
						const auto& config = g_settings.remoteConfig[nmv->iItem];
						if (!config->updating)
							enable = TRUE;
						selected = TRUE;
					}
				}
				EnableWindow(GetDlgItem(hDlg, IDC_REMOTECONFIG_DELETE), enable);
				EnableWindow(GetDlgItem(hDlg, IDC_REMOTECONFIG_UPDATE), selected);
			}
		}
		break;
		case LVN_ITEMACTIVATE:
		{
			auto nmia = reinterpret_cast<LPNMITEMACTIVATE>(lParam);
			if (nmia->iItem >= 0 && g_settings.remoteConfig.size() > static_cast<size_t>(nmia->iItem))
			{
				auto& config = g_settings.remoteConfig[nmia->iItem];
				std::wstring url = config->url, name = config->name;
				if (EditRemoteConfig(hDlg, url, name))
				{
					config->url = std::move(url);
					config->name = std::move(name);
					AddOrUpdateRemoteConfigItem(nmia->hdr.hwndFrom, *config, nmia->iItem);
				}
				else
					TaskDialog(hDlg, nullptr, _(L"Warning"), nullptr, _(L"Invalid input"), TDCBF_OK_BUTTON, TD_WARNING_ICON, nullptr);
			}
		}
		break;
		}
		break;
	case WM_CTLCOLORDLG:
		if (ShouldAppsUseDarkMode() && !IsHighContrast())
		{
			if (!hDarkBrush)
				hDarkBrush.reset(CreateSolidBrush(DarkWindowBkColor));
			return reinterpret_cast<INT_PTR>(hDarkBrush.get());
		}
		break;
	case WM_SETTINGCHANGE:
		if (g_darkModeSupported && IsColorSchemeChangeMessage(lParam))
		{
			RefreshTitleBarThemeColor(hDlg);
			SendDlgItemMessageW(hDlg, IDC_REMOTECONFIG_LISTVIEW, WM_THEMECHANGED, 0, 0);
			SendDlgItemMessageW(hDlg, IDC_REMOTECONFIG_ADD, WM_THEMECHANGED, 0, 0);
			SendDlgItemMessageW(hDlg, IDC_REMOTECONFIG_DELETE, WM_THEMECHANGED, 0, 0);
			SendDlgItemMessageW(hDlg, IDC_REMOTECONFIG_UPDATE, WM_THEMECHANGED, 0, 0);
			RedrawWindow(hDlg, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE);
		}
		break;
	case WM_REFRESHCONFIGS:
		UpdateRemoteConfigItems(GetDlgItem(hDlg, IDC_REMOTECONFIG_LISTVIEW));
		break;
	}
	return (INT_PTR)FALSE;
}

void ShowRemoteConfigDialog(HWND hWndParent)
{
	if (!g_hWndRemoteConfigDlg)
	{
		DialogBoxW(g_hInst, MAKEINTRESOURCEW(IDD_REMOTECONFIG), hWndParent, RemoteConfigDlgProc);
		g_hWndRemoteConfigDlg = nullptr;
	}
}
