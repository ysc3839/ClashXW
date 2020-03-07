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

size_t g_edgeWebView2Count = 0;

class EdgeWebView2
{
public:
	static HWND Create(HWND hWndParent)
	{
		DialogTemplate dlgTmpl = { {
			.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_VISIBLE,
			.dwExtendedStyle = WS_EX_DLGMODALFRAME,
			.cx = 500,
			.cy = 400
		} };
		return CreateDialogIndirectParamW(g_hInst, &dlgTmpl.tmpl, hWndParent, DlgProcStatic, 0);
	}

	static auto GetCount() { return s_instances; }

private:
	EdgeWebView2(HWND hWnd) : m_hWnd(hWnd) { ++s_instances; }
	~EdgeWebView2()
	{
		--s_instances;
		if (s_instances == 0)
		{
			s_webViewEnv.reset();
		}
	}

	static INT_PTR CALLBACK DlgProcStatic(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
	{
		EdgeWebView2* self = nullptr;
		if (message == WM_INITDIALOG)
		{
			self = new EdgeWebView2(hDlg);
			SetWindowLongPtr(hDlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
		}
		else
			self = reinterpret_cast<EdgeWebView2*>(GetWindowLongPtrW(hDlg, GWLP_USERDATA));
		if (self)
		{
			auto result = self->DlgProc(hDlg, message, wParam, lParam);
			if (message == WM_NCDESTROY)
			{
				SetWindowLongPtr(hDlg, GWLP_USERDATA, NULL);
				delete self;
			}
			return result;
		}
		return static_cast<INT_PTR>(FALSE);
	}

	INT_PTR CALLBACK DlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
	{
		UNREFERENCED_PARAMETER(hDlg);
		UNREFERENCED_PARAMETER(wParam);
		UNREFERENCED_PARAMETER(lParam);
		switch (message)
		{
		case WM_INITDIALOG:
		{
			CreateWebView();
			return static_cast<INT_PTR>(TRUE);
		}
		case WM_CLOSE:
			DestroyWindow(hDlg);
			break;
		case WM_DESTROY:
			m_webViewHost->Close();
			break;
		case WM_SIZE:
		{
			int clientWidth = GET_X_LPARAM(lParam), clientHeight = GET_Y_LPARAM(lParam);
			m_webViewHost->put_Bounds({ 0, 0, clientWidth, clientHeight });
		}
		break;
		case WM_MOVE:
		case WM_MOVING:
			m_webViewHost->NotifyParentWindowPositionChanged();
			break;
		}
		return static_cast<INT_PTR>(FALSE);
	}

	void CreateWebView()
	{
		if (s_webViewEnv)
		{
			CreateWebViewHost();
		}
		else
		{
			CreateCoreWebView2EnvironmentWithDetails(nullptr, g_dataPath.c_str(), LR"(--user-agent="ClashX Runtime")",
				wrl::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
					[this](HRESULT result, ICoreWebView2Environment* env) {
						RETURN_IF_FAILED(result);
						RETURN_IF_FAILED(env->QueryInterface(IID_PPV_ARGS(&s_webViewEnv)));
						CreateWebViewHost();
						return S_OK;
					}).Get());
		}
	}

	void CreateWebViewHost()
	{
		s_webViewEnv->CreateCoreWebView2Host(m_hWnd, wrl::Callback<ICoreWebView2CreateCoreWebView2HostCompletedHandler>(
			[this](HRESULT result, ICoreWebView2Host* host) {
				RETURN_IF_FAILED(result);
				RETURN_IF_FAILED(host->QueryInterface(IID_PPV_ARGS(&m_webViewHost)));

				ICoreWebView2* webView;
				RETURN_IF_FAILED(m_webViewHost->get_CoreWebView2(&webView));
				RETURN_IF_FAILED(webView->QueryInterface(IID_PPV_ARGS(&m_webView)));

				RECT clientRc;
				GetClientRect(m_hWnd, &clientRc);
				m_webViewHost->put_Bounds(clientRc);

				m_webView->Navigate(L"chrome://version");

				return S_OK;
			}).Get());
	}

	HWND m_hWnd;
	wil::com_ptr_nothrow<ICoreWebView2Host> m_webViewHost;
	wil::com_ptr_nothrow<ICoreWebView2> m_webView;
	inline static wil::com_ptr_nothrow<ICoreWebView2Environment> s_webViewEnv;
	inline static size_t s_instances = 0;
};
