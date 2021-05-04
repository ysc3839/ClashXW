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
HMENU g_hConfigMenu;
HMENU g_hLogLevelMenu;
HMENU g_hPortsMenu;

struct AccelKey
{
	uint8_t flags;
	uint16_t key;

	bool operator==(const AccelKey&) const = default;
};

namespace std
{
	template<> struct hash<AccelKey>
	{
		size_t operator()(const AccelKey& k) const noexcept
		{
			return std::hash<uint32_t>{}(k.key << 16 | k.flags);
		}
	};
}

const std::unordered_map<AccelKey, WORD> MenuAccel = {
//	{ {flags, key}, cmd },
	{ {FALT, 'G'}, IDM_MODE_GLOBAL },
	{ {FALT, 'R'}, IDM_MODE_RULE },
	{ {FALT, 'D'}, IDM_MODE_DIRECT },
	{ {FCONTROL, 'S'}, IDM_SYSTEMPROXY },
	{ {FCONTROL, 'C'}, IDM_COPYCOMMAND },
	{ {FCONTROL | FALT, 'C'}, IDM_COPYCOMMAND_EXTERNAL },
	{ {FCONTROL, 'T'}, IDM_BENCHMARK },
	{ {FCONTROL, 'D'}, IDM_DASHBOARD },
	{ {FCONTROL, 'O'}, IDM_CONFIG_OPENFOLDER },
	{ {FCONTROL, 'R'}, IDM_CONFIG_RELOAD },
	{ {FCONTROL, 'M'}, IDM_REMOTECONFIG_MANAGE },
	{ {FCONTROL, 'U'}, IDM_REMOTECONFIG_UPDATE },
	{ {FCONTROL, 'Q'}, IDM_QUIT },
};

namespace MenuUtil
{
	BOOL SetMenuItemText(HMENU hMenu, UINT pos, const wchar_t* text) noexcept
	{
		MENUITEMINFOW mii = {
			.cbSize = sizeof(mii),
			.fMask = MIIM_STRING,
			.dwTypeData = const_cast<LPWSTR>(text)
		};
		return SetMenuItemInfoW(hMenu, pos, TRUE, &mii);
	}

	namespace
	{
		struct MenuProxyGroup
		{
			ClashProxy* model;
			std::vector<ClashProxy*> allProxies;
			HMENU hMenu;
		};

		std::vector<MenuProxyGroup> s_menuProxyGroups;
		ClashProxies::MapType s_menuProxies;
		ClashProxyMode s_menuProxyMode = ClashProxyMode::Unknown;

		std::unordered_map<HWND, bool> s_openedMenus; // bool = isScrollable
		std::unordered_map<HWND, int> s_menuScrollDelta;

		bool ProcessAccelerator(LPMSG msg)
		{
			if (msg->message == WM_KEYDOWN || msg->message == WM_SYSKEYDOWN)
			{
				uint8_t flags = 0;
				if (GetKeyState(VK_CONTROL) & 0x8000) flags |= FCONTROL;
				if (GetKeyState(VK_MENU) & 0x8000) flags |= FALT;

				if (flags != 0) // Currently all accelerators have modifier key
				{
					AccelKey key = { flags, static_cast<uint16_t>(msg->wParam) };
					auto it = MenuAccel.find(key);
					if (it != MenuAccel.end())
					{
						PostMessageW(g_hWnd, WM_COMMAND, MAKEWPARAM(it->second, 1), 0);
						return true;
					}
				}
			}
			return false;
		}

		void UpdateContextMenu() noexcept
		{
			CheckMenuItem(g_hContextMenu, IDM_SYSTEMPROXY, g_settings.systemProxy ? MF_CHECKED : MF_UNCHECKED);
			CheckMenuItem(g_hContextMenu, IDM_STARTATLOGIN, StartAtLogin::IsEnabled() ? MF_CHECKED : MF_UNCHECKED);
			CheckMenuItem(g_hContextMenu, IDM_ALLOWFROMLAN, g_clashConfig.allowLan ? MF_CHECKED : MF_UNCHECKED);
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

		void UpdateConfigMenu()
		{
			g_configFilesList = GetConfigFilesList();

			// Remove items before separator
			while (WI_IsFlagClear(GetMenuState(g_hConfigMenu, 0, MF_BYPOSITION), MF_SEPARATOR))
				RemoveMenu(g_hConfigMenu, 0, MF_BYPOSITION);

			UINT i = 0;
			for (const auto& n : g_configFilesList)
			{
				auto stem = n.stem();
				MENUITEMINFOW mii = {
					.cbSize = sizeof(mii),
					.fMask = MIIM_FTYPE | MIIM_STATE | MIIM_STRING,
					.fType = MFT_RADIOCHECK,
					.fState = static_cast<UINT>(n == g_settings.configFile ? MFS_CHECKED : MFS_UNCHECKED),
					.dwTypeData = const_cast<LPWSTR>(stem.c_str())
				};
				InsertMenuItemW(g_hConfigMenu, i, TRUE, &mii);
				++i;
			}
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

		void AddMenuProxyGroup(ClashProxy& group)
		{
			std::vector<ClashProxy*> allProxies;
			allProxies.reserve(group.all.size());
			for (const auto& key : group.all)
			{
				auto it = s_menuProxies.find(key);
				if (it == s_menuProxies.end())
				{
					// FIXME: If a proxy is not found, it may come from proxy provider.
					// Currently add a dummy proxy to allow it shows in menu.
					// Should load proxy provider data.
					ClashProxy p;
					p.name = key;
					auto [newIt, _] = s_menuProxies.emplace(key, std::move(p));
					it = std::move(newIt);
				}
				//auto& proxy = s_menuProxies.at(key);
				allProxies.emplace_back(&it->second);
			}
			s_menuProxyGroups.emplace_back(&group, std::move(allProxies));
		}

		void BuildMenuProxyGroups()
		{
			s_menuProxyGroups.clear();
			s_menuProxies = std::move(g_clashProxies.proxies);

			auto& global = s_menuProxies.at("GLOBAL");
			if (g_clashConfig.mode == ClashProxyMode::Global)
			{
				AddMenuProxyGroup(global);
			}
			else
			{
				static const std::unordered_set<ClashProxy::Name> unusedProxy = { "DIRECT", "REJECT", "GLOBAL" };
				for (const auto& key : global.all)
				{
					if (unusedProxy.contains(key))
						continue;
					auto& proxy = s_menuProxies.at(key);
					if (proxy.type == ClashProxy::Type::URLTest ||
						proxy.type == ClashProxy::Type::Fallback ||
						proxy.type == ClashProxy::Type::LoadBalance ||
						proxy.type == ClashProxy::Type::Selector) // group type
					{
						AddMenuProxyGroup(proxy);
					}
				}
			}

			s_menuProxyMode = g_clashConfig.mode;
		}

		void DeleteProxyGroupsMenu()
		{
			const size_t count = s_menuProxyGroups.size() + (s_menuProxyGroups.empty() ? 0 : 1);
			for (size_t i = 0; i < count; ++i)
				DeleteMenu(g_hContextMenu, 2, MF_BYPOSITION);
		}

		void RebuildProxyGroupsMenu()
		{
			UINT i = 2;
			for (auto& group : s_menuProxyGroups)
			{
				wil::unique_hmenu hMenu(CreatePopupMenu());
				THROW_LAST_ERROR_IF_NULL(hMenu);

				for (auto proxy : group.allProxies)
				{
					bool selected = group.model->now && *group.model->now == proxy->name;
					std::wstring text = Utf8ToUtf16(proxy->name);
					if (!proxy->history.empty())
					{
						text += L'\t';
						auto delay = proxy->history.back().delay;
						if (delay == 0)
						{
							text += L"fail";
						}
						else
						{
							text += std::to_wstring(delay);
							text += L"ms";
						}
					}
					AppendMenuW(hMenu.get(), selected ? MF_CHECKED : 0, 0, text.c_str());
				}

				group.hMenu = hMenu.release();
				InsertMenuW(g_hContextMenu, i, MF_BYPOSITION | MF_POPUP, reinterpret_cast<UINT_PTR>(group.hMenu), Utf8ToUtf16(group.model->name).c_str());
				++i;
			}

			if (!s_menuProxyGroups.empty())
			{
				InsertMenuW(g_hContextMenu, i, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
			}
		}

		void UpdateProxyGroupsMenu()
		{
			if (g_clashProxies.proxies.empty())
				return;

			bool rebuild = (s_menuProxyMode != g_clashConfig.mode) || (s_menuProxies.size() != g_clashProxies.proxies.size());
			if (!rebuild)
			{
				for (auto& [name, proxy] : s_menuProxies)
				{
					auto it = g_clashProxies.proxies.find(name);
					if (it == g_clashProxies.proxies.end())
					{
						rebuild = true;
						break;
					}
					auto& newProxy = it->second;
					proxy.history = newProxy.history; // copy
					proxy.now = newProxy.now;
				}
			}

			if (rebuild)
			{
				DeleteProxyGroupsMenu();
				BuildMenuProxyGroups();
				RebuildProxyGroupsMenu();
			}
		}

		void UpdateMenuAltText()
		{
			static bool altState = false; // true=down
			bool currentAltState = (GetKeyState(VK_MENU) & 0x8000);
			if (altState != currentAltState)
			{
				altState = currentAltState;
				auto text = currentAltState ?
					_(L"Copy shell command (External IP)\tCtrl+Alt+C") :
					_(L"Copy shell command\tCtrl+C");
				MenuUtil::SetMenuItemText(g_hContextMenu, 3, text);
			}
		}

		LRESULT MenuSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, [[maybe_unused]] UINT_PTR uIdSubclass, [[maybe_unused]] DWORD_PTR dwRefData)
		{
			switch (uMsg)
			{
			case WM_NCDESTROY:
				s_openedMenus.erase(hWnd);
				s_menuScrollDelta.erase(hWnd);
				RemoveWindowSubclass(hWnd, MenuSubclassProc, uIdSubclass);
				break;
			case WM_MOUSEWHEEL:
			{
				UINT scrollLines;
				if (!SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &scrollLines, 0)) {
					scrollLines = 3;
				}
				if (scrollLines == 0)
					break;
				if (scrollLines == WHEEL_PAGESCROLL)
					scrollLines = 20;

				auto& lastDelta = s_menuScrollDelta[hWnd];
				int delta = GET_WHEEL_DELTA_WPARAM(wParam) + lastDelta;
				int ticks = delta * static_cast<int>(scrollLines) / WHEEL_DELTA;
				lastDelta = delta - ticks * WHEEL_DELTA / static_cast<int>(scrollLines);

				if (ticks == 0)
					break;

				RECT rc;
				GetWindowRect(hWnd, &rc);
				LPARAM pos;
				if (ticks < 0)
				{
					ticks = -ticks;
					pos = MAKELPARAM(rc.left + 6, rc.bottom - 6);
				}
				else
					pos = MAKELPARAM(rc.left + 6, rc.top + 6);

				for (int i = 0; i < ticks; ++i)
				{
					PostMessageW(hWnd, WM_LBUTTONDOWN, 0, pos);
					PostMessageW(hWnd, WM_LBUTTONUP, 0, pos);
				}
				PostMessageW(hWnd, WM_MOUSEMOVE, 0, lParam);
			}
			break;
			}
			return DefSubclassProc(hWnd, uMsg, wParam, lParam);
		}
	}

	void SetupMenu() noexcept
	{
		try {
			g_hTopMenu = LoadMenuW(g_hInst, MAKEINTRESOURCEW(IDC_MENU));
			THROW_LAST_ERROR_IF_NULL(g_hTopMenu);

			g_hContextMenu = GetSubMenu(g_hTopMenu, 0);
			THROW_LAST_ERROR_IF_NULL(g_hContextMenu);

			MENUINFO mi = {
					.cbSize = sizeof(mi),
					.fMask = MIM_STYLE,
					.dwStyle = MNS_NOTIFYBYPOS
			};
			SetMenuInfo(g_hContextMenu, &mi);

			g_hProxyModeMenu = GetSubMenu(g_hContextMenu, 0);
			THROW_LAST_ERROR_IF_NULL(g_hProxyModeMenu);

			g_hConfigMenu = GetSubMenu(g_hContextMenu, 12);
			THROW_LAST_ERROR_IF_NULL(g_hConfigMenu);

			HMENU hHelpMenu = GetSubMenu(g_hContextMenu, 13);
			THROW_LAST_ERROR_IF_NULL(hHelpMenu);

			g_hLogLevelMenu = GetSubMenu(hHelpMenu, 2);
			THROW_LAST_ERROR_IF_NULL(g_hLogLevelMenu);

			g_hPortsMenu = GetSubMenu(hHelpMenu, 3);
			THROW_LAST_ERROR_IF_NULL(g_hPortsMenu);

			SetMenuItemText(g_hContextMenu, 3, _(L"Copy shell command\tCtrl+C"));
		}
		CATCH_FAIL_FAST();
	}

	void ShowContextMenu(HWND hWnd, int x, int y) noexcept
	{
		static wil::unique_hhook hMouseHook;
		hMouseHook.reset(SetWindowsHookExW(WH_MOUSE_LL, [](int code, WPARAM wParam, LPARAM lParam) -> LRESULT {
			if (code == HC_ACTION && wParam == WM_MOUSEWHEEL)
			{
				auto info = reinterpret_cast<LPMSLLHOOKSTRUCT>(lParam);
				HWND hWnd = WindowFromPhysicalPoint(info->pt);
				auto it = s_openedMenus.find(hWnd);
				if (it != s_openedMenus.end() && it->second)
				{
					return PostMessageW(hWnd, WM_MOUSEWHEEL, MAKEWPARAM(0, GET_WHEEL_DELTA_WPARAM(info->mouseData)), POINTTOPOINTS(info->pt));
				}
			}
			return CallNextHookEx(hMouseHook.get(), code, wParam, lParam);
		}, nullptr, 0));

		static wil::unique_hhook hMenuHook;
		hMenuHook.reset(SetWindowsHookExW(WH_MSGFILTER, [](int code, WPARAM wParam, LPARAM lParam) -> LRESULT {
			if (code == MSGF_MENU)
			{
				auto msg = reinterpret_cast<LPMSG>(lParam);
				if (msg->message == WM_SYSKEYDOWN && msg->wParam == VK_MENU)
					return TRUE; // Prevent menu closing
				if (ProcessAccelerator(msg))
					msg->wParam = VK_ESCAPE; // Force menu to close
			}
			return CallNextHookEx(hMenuHook.get(), code, wParam, lParam);
		}, nullptr, GetCurrentThreadId()));

		try
		{
			THROW_IF_WIN32_BOOL_FALSE(SetForegroundWindow(hWnd));

			UINT flags = TPM_RIGHTBUTTON;
			if (GetSystemMetrics(SM_MENUDROPALIGNMENT) != 0)
				flags |= TPM_RIGHTALIGN;
			else
				flags |= TPM_LEFTALIGN;

			THROW_IF_WIN32_BOOL_FALSE(TrackPopupMenuEx(g_hContextMenu, flags, x, y, hWnd, nullptr));
		}
		CATCH_LOG();

		hMenuHook.reset();
		hMouseHook.reset();
		s_openedMenus.clear();
		s_menuScrollDelta.clear();
	}

	void UpdateMenus()
	{
		UpdateContextMenu();
		UpdateProxyModeMenu();
		UpdateConfigMenu();
		UpdateLogLevelMenu();
		UpdatePortsMenu();
		UpdateProxyGroupsMenu();
	}

	void PostCommandByMenuIndex(HWND hWnd, HMENU hMenu, size_t i)
	{
		UINT id = GetMenuItemID(hMenu, static_cast<int>(i));
		if (id != UINT_ERROR)
			PostMessageW(hWnd, WM_COMMAND, MAKEWPARAM(id, 1), 0);
	}

	bool HandleProxyGroupsItemChoose(HMENU hMenu, size_t i)
	{
		auto it = std::find_if(s_menuProxyGroups.begin(), s_menuProxyGroups.end(), [hMenu](const MenuProxyGroup& g) { return g.hMenu == hMenu; });
		if (it != s_menuProxyGroups.end())
		{
			// FIXME: check if type is selector
			MenuProxyGroup& group = *it;
			if (i < group.allProxies.size())
			{
				[](HMENU hMenu, size_t i, MenuProxyGroup& group) -> winrt::fire_and_forget {
					co_await winrt::resume_background();
					try
					{
						if (!g_clashApi->UpdateProxyGroup(group.model->name, group.allProxies[i]->name))
							co_return;
					}
					catch (...)
					{
						LOG_CAUGHT_EXCEPTION();
						co_return;
					}

					group.model->now = group.allProxies[i]->name;

					co_await ResumeForeground();
					DWORD ret;
					UINT j = 0;
					do
					{
						ret = CheckMenuItem(hMenu, j, MF_BYPOSITION | MF_UNCHECKED);
						++j;
					} while (ret != DWORD_ERROR);
					CheckMenuItem(hMenu, static_cast<UINT>(i), MF_BYPOSITION | MF_CHECKED);
				}(hMenu, i, group);
			}
			return true;
		}
		return false;
	}

	void OnMenuEnterIdle(HWND hWndMenu)
	{
		if (!s_openedMenus.contains(hWndMenu))
		{
			// Get menu non-client size
			RECT rc;
			GetWindowRect(hWndMenu, &rc);
			POINT pt = { rc.left, rc.top };
			ScreenToClient(hWndMenu, &pt);
			auto x = -pt.x, y = -pt.y;

			bool isScrollable = y > x * 2; // x = 3, y = 25, scrollable
			s_openedMenus.emplace(hWndMenu, isScrollable);

			if (isScrollable)
			{
				SetWindowSubclass(hWndMenu, MenuSubclassProc, 0, 0);
			}
		}
		UpdateMenuAltText();
	}
}
