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
#undef CreateWindow

void HandleJSMessage(std::string_view handlerName, std::string_view callbackId, json data, ICoreWebView2* webView)
{
	auto responseCallback = [callbackId = std::string(callbackId), webView = wil::com_ptr<ICoreWebView2>(webView)](json responseData) {
		json j = {
			{"responseId", callbackId},
			{"responseData", responseData}
		};
		webView->PostWebMessageAsJson(Utf8ToUtf16(j.dump()).c_str());
	};

	if (handlerName == "isSystemProxySet")
	{
		responseCallback(g_settings.systemProxy);
	}
	else if (handlerName == "setSystemProxy")
	{
		if (data.is_boolean())
		{
			EnableSystemProxy(data.get<bool>());
			responseCallback(true);
		}
		else
			responseCallback(false);
	}
	else if (handlerName == "getStartAtLogin")
	{
		responseCallback(StartAtLogin::IsEnabled());
	}
	else if (handlerName == "setStartAtLogin")
	{
		if (data.is_boolean())
		{
			StartAtLogin::SetEnable(data.get<bool>());
			responseCallback(true);
		}
		else
			responseCallback(false);
	}
	else if (handlerName == "getBreakConnections")
	{
		// FIXME not implemented
		responseCallback(false);
	}
	else if (handlerName == "setBreakConnections")
	{
		// FIXME not implemented
		responseCallback(false);
	}
	else if (handlerName == "speedTest")
	{
		// Web message callback is run in UI thread, so run GetProxyDelay in background thread.
		[](std::string name, auto responseCallback) -> winrt::fire_and_forget {
			co_await winrt::resume_background();
			u16milliseconds delay;
			try
			{
				delay = g_clashApi->GetProxyDelay(name);
			}
			catch (...)
			{
				LOG_CAUGHT_EXCEPTION();
				delay = {};
			}
			responseCallback(delay.count());
		}(data.get<std::string>(), responseCallback);
	}
	else if (handlerName == "apiInfo")
	{
		// FIXME not implemented
		responseCallback({
			{"host", "127.0.0.1"},
			{"port", 9090},
			{"secret", ""}
		});
	}
	else if (handlerName == "ping")
	{
		// FIXME callHandler not implemented
		//bridge?.callHandler("pong")
		responseCallback(true);
	}
}

constexpr auto OLD_PROTOCOL_SCHEME = L"wvjbscheme";
constexpr auto NEW_PROTOCOL_SCHEME = L"https";
constexpr auto BRIDGE_LOADED = L"__bridge_loaded__";
// Minified version of EdgeWebView2JavascriptBridge.js
constexpr auto JS_BRIDGE_CODE = LR"(!function(){function e(e,a){if(a){var r="cb_"+ ++i+"_"+ +new Date;n[r]=a,e.callbackId=r}window.chrome.webview.postMessage(e)}if(!window.WebViewJavascriptBridge){var a={},n={},i=0;window.chrome.webview.addEventListener("message",function(i){var r,t=i.data;if(t.responseId){if(r=n[t.responseId],!r)return;r(t.responseData),delete n[t.responseId]}else{if(t.callbackId){var o=t.callbackId;r=function(a){e({handlerName:t.handlerName,responseId:o,responseData:a})}}var s=a[t.handlerName];s?s(t.data,r):console.log("WebViewJavascriptBridge: WARNING: no handler for message from native:",t)}}),window.WebViewJavascriptBridge={registerHandler:function(e,n){a[e]=n},callHandler:function(a,n,i){2==arguments.length&&"function"==typeof n&&(i=n,n=null),e({handlerName:a,data:n},i)},disableJavscriptAlertBoxSafetyTimeout:function(){}},setTimeout(function(){var e=window.WVJBCallbacks;if(e){delete window.WVJBCallbacks;for(var a=0;a<e.length;++a)e[a](WebViewJavascriptBridge)}},0)}}();)";

bool IsSchemeMatch(IUri* uri)
{
	wil::unique_bstr scheme;
	if (FAILED(uri->GetSchemeName(&scheme)))
		return false;
	return (_wcsicmp(scheme.get(), NEW_PROTOCOL_SCHEME) == 0) || (_wcsicmp(scheme.get(), OLD_PROTOCOL_SCHEME) == 0);
}

bool IsBridgeLoadedURI(IUri* uri)
{
	wil::unique_bstr host;
	if (FAILED(uri->GetHost(&host)))
		return false;
	return IsSchemeMatch(uri) && (_wcsicmp(host.get(), BRIDGE_LOADED) == 0);
}

HRESULT LoadWebView2LoaderDLL()
{
	PEXCEPTION_POINTERS info = nullptr;
	__try
	{
		__HrLoadAllImportsForDll("WebView2Loader.dll"); // Case sensitive
	}
	__except (info = GetExceptionInformation(), GetExceptionCode() == VcppException(ERROR_SEVERITY_ERROR, ERROR_MOD_NOT_FOUND) ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
	{
		auto dli = reinterpret_cast<PDelayLoadInfo>(info->ExceptionRecord->ExceptionInformation[0]);
		return HRESULT_FROM_WIN32(dli->dwLastError);
	}
	return S_OK;
}

class EdgeWebView2
{
public:
	static void Create(HWND hWndParent)
	{
		if (s_webViewEnv)
		{
			CreateWindow(hWndParent);
		}
		else
		{
			HRESULT hr = S_OK;
			do
			{
				hr = LoadWebView2LoaderDLL();
				if (FAILED(hr))
					break;

				auto options = wrl::Make<CoreWebView2EnvironmentOptions>();
				options->put_AdditionalBrowserArguments(LR"(--user-agent="ClashX Runtime")");
				hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, g_dataPath.c_str(), options.Get(),
					wrl::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
						[hWndParent](HRESULT result, ICoreWebView2Environment* env) {
							RETURN_IF_FAILED(result);
							s_webViewEnv = env;
							CreateWindow(hWndParent);
							return S_OK;
						}).Get());
			} while (0);

			if (FAILED(hr))
			{
				LOG_HR(hr);
				TaskDialog(nullptr, nullptr, _(L"Error"), nullptr, _(L"Failed to load Edge WebView2."), TDCBF_OK_BUTTON, TD_ERROR_ICON, nullptr);
			}
		}
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

	static LRESULT CALLBACK WebViewWndProcStatic(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		EdgeWebView2* self = nullptr;
		if (message == WM_NCCREATE)
		{
			self = new EdgeWebView2(hWnd);
			SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
		}
		else
			self = reinterpret_cast<EdgeWebView2*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
		if (self)
		{
			auto result = self->WndProc(hWnd, message, wParam, lParam);
			if (message == WM_NCDESTROY)
			{
				SetWindowLongPtr(hWnd, GWLP_USERDATA, NULL);
				delete self;
			}
			return result;
		}
		return DefWindowProcW(hWnd, message, wParam, lParam);
	}

	LRESULT CALLBACK WndProc(HWND hWnd, UINT message, [[maybe_unused]] WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
		case WM_CREATE:
		{
			int width = 920, height = 580;
			CenterWindow(hWnd, width, height, WS_OVERLAPPEDWINDOW, 0);

			HRESULT hr = CreateWebViewController();
			if (FAILED(hr))
			{
				LOG_HR(hr);
				return -1;
			}
		}
		case WM_DESTROY:
			if (m_webViewController)
				m_webViewController->Close();
			break;
		case WM_SIZE:
		{
			if (m_webViewController)
			{
				const int clientWidth = GET_X_LPARAM(lParam), clientHeight = GET_Y_LPARAM(lParam);
				m_webViewController->put_Bounds({ 0, 0, clientWidth, clientHeight });
			}
		}
		break;
		case WM_MOVE:
		case WM_MOVING:
			if (m_webViewController)
				m_webViewController->NotifyParentWindowPositionChanged();
			break;
		default:
			return DefWindowProcW(hWnd, message, wParam, lParam);
		}
		return 0;
	}

	static void CreateWindow(HWND hWndParent)
	{
		static ATOM classAtom = 0;
		if (!classAtom)
		{
			WNDCLASSEXW wcex = {
				.cbSize = sizeof(wcex),
				.lpfnWndProc = WebViewWndProcStatic,
				.hInstance = g_hInst,
				.hIcon = LoadIconW(g_hInst, MAKEINTRESOURCEW(IDI_CLASHXW)),
				.hCursor = LoadCursorW(nullptr, IDC_ARROW),
				.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1),
				.lpszClassName = L"ClashXW_WebView",
				.hIconSm = wcex.hIcon
			};
			classAtom = RegisterClassExW(&wcex);
		}

		auto hWnd = CreateWindowW(L"ClashXW_WebView", _(L"Dashboard"), WS_OVERLAPPEDWINDOW, 0, 0, 0, 0, hWndParent, nullptr, g_hInst, nullptr);
		if (hWnd)
		{
			ShowWindow(hWnd, SW_SHOW);
			UpdateWindow(hWnd);
		}
	}

	HRESULT OnFrameNavigationStarting(ICoreWebView2* sender, ICoreWebView2NavigationStartingEventArgs* args)
	{
		wil::unique_cotaskmem_string uriStr;
		RETURN_IF_FAILED(args->get_Uri(&uriStr));

		wil::com_ptr_nothrow<IUri> uri;
		RETURN_IF_FAILED(CreateUri(uriStr.get(), Uri_CREATE_CANONICALIZE | Uri_CREATE_NO_DECODE_EXTRA_INFO, 0, &uri));

		if (IsBridgeLoadedURI(uri.get()))
		{
			args->put_Cancel(TRUE);
			sender->ExecuteScript(JS_BRIDGE_CODE, nullptr);
		}

		return S_OK;
	}

	HRESULT OnWebMessageReceived(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) noexcept try
	{
		wil::unique_cotaskmem_string jsonStr;
		THROW_IF_FAILED(args->get_WebMessageAsJson(&jsonStr));

		json j = json::parse(std::wstring_view(jsonStr.get()));

		auto& responseId = j["responseId"];
		if (responseId.is_string())
		{
			THROW_HR(E_NOTIMPL);
		}
		else
		{
			HandleJSMessage(j["handlerName"].get<std::string_view>(), j["callbackId"].get<std::string_view>(), j["data"], sender);
		}
		return S_OK;
	}
	CATCH_LOG_RETURN_HR(S_OK);

	HRESULT CreateWebViewController()
	{
		return s_webViewEnv->CreateCoreWebView2Controller(m_hWnd, wrl::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
			[this](HRESULT result, ICoreWebView2Controller* controller) {
				RETURN_IF_FAILED(result);
				m_webViewController = controller;

				RETURN_IF_FAILED(m_webViewController->get_CoreWebView2(&m_webView));

				wil::com_ptr_nothrow<ICoreWebView2Settings> settings;
				RETURN_IF_FAILED(m_webView->get_Settings(&settings));
				settings->put_AreHostObjectsAllowed(FALSE);

				EventRegistrationToken token;
				RETURN_IF_FAILED(m_webView->add_FrameNavigationStarting(wrl::Callback<ICoreWebView2NavigationStartingEventHandler>(
					this, &EdgeWebView2::OnFrameNavigationStarting).Get(), &token));

				RETURN_IF_FAILED(m_webView->add_WebMessageReceived(Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
					this, &EdgeWebView2::OnWebMessageReceived).Get(), &token));

				RECT clientRc;
				GetClientRect(m_hWnd, &clientRc);
				m_webViewController->put_Bounds(clientRc);

				m_webView->Navigate(L"http://127.0.0.1:9090/ui/");

				return S_OK;
			}).Get());
	}

	HWND m_hWnd;
	wil::com_ptr_nothrow<ICoreWebView2Controller> m_webViewController;
	wil::com_ptr_nothrow<ICoreWebView2> m_webView;
	inline static wil::com_ptr_nothrow<ICoreWebView2Environment> s_webViewEnv;
	inline static size_t s_instances = 0;
};
