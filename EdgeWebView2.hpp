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

constexpr auto OLD_PROTOCOL_SCHEME = "wvjbscheme";
constexpr auto NEW_PROTOCOL_SCHEME = "https";
constexpr auto BRIDGE_LOADED = "__bridge_loaded__";
// Minified version of EdgeWebView2JavascriptBridge.js
constexpr auto JS_BRIDGE_CODE = LR"(!function(){function e(e,a){if(a){var r="cb_"+ ++i+"_"+ +new Date;n[r]=a,e.callbackId=r}window.chrome.webview.postMessage(e)}if(!window.WebViewJavascriptBridge){var a={},n={},i=0;window.chrome.webview.addEventListener("message",function(i){var r,t=i.data;if(t.responseId){if(r=n[t.responseId],!r)return;r(t.responseData),delete n[t.responseId]}else{if(t.callbackId){var o=t.callbackId;r=function(a){e({handlerName:t.handlerName,responseId:o,responseData:a})}}var s=a[t.handlerName];s?s(t.data,r):console.log("WebViewJavascriptBridge: WARNING: no handler for message from native:",t)}}),window.WebViewJavascriptBridge={registerHandler:function(e,n){a[e]=n},callHandler:function(a,n,i){2==arguments.length&&"function"==typeof n&&(i=n,n=null),e({handlerName:a,data:n},i)},disableJavscriptAlertBoxSafetyTimeout:function(){}},setTimeout(function(){var e=window.WVJBCallbacks;if(e){delete window.WVJBCallbacks;for(var a=0;a<e.length;++a)e[a](WebViewJavascriptBridge)}},0)}}();)";

inline bool IsSchemeMatch(const skyr::url& url)
{
	return url.scheme() == NEW_PROTOCOL_SCHEME || url.scheme() == OLD_PROTOCOL_SCHEME;
}

bool IsBridgeLoadedURI(std::wstring_view urlStr)
{
	try
	{
		skyr::url url(urlStr);
		return IsSchemeMatch(url) && url.hostname() == BRIDGE_LOADED;
	}
	catch (...) {}
	return false;
}

bool ShowWebViewFailedDialog(HWND hWndParent, bool failedToLoad)
{
	std::wstring content;
	if (failedToLoad)
		content = _(L"It looks like Edge WebView is not installed.");
	else // Failed in callback
		content = _(L"It looks like Edge WebView is installed but doesn't work properly.");
	content.append(L"\n");
	content.append(_(L"Do you want install Edge WebView or open in browser?"));

	constexpr int ID_DOWNLOAD_AND_INSTALL = 100;
	constexpr int ID_OPEN_IN_BROWSER = 101;
	TASKDIALOG_BUTTON buttons[] = { {
		.nButtonID = ID_DOWNLOAD_AND_INSTALL,
		.pszButtonText = _(L"Download and install Edge WebView runtime")
	}, {
		.nButtonID = ID_OPEN_IN_BROWSER,
		.pszButtonText = _(L"Open Dashboard in browser\nYou can turn off this behavior in Experimental menu")
	} };

	auto fileName = fs::temp_directory_path() / L"MicrosoftEdgeWebview2Setup.exe";
	auto callback = [&fileName](HWND hWnd, UINT msg, WPARAM wParam, LPARAM) -> HRESULT {
		switch (msg)
		{
		case TDN_DIALOG_CONSTRUCTED:
			SendMessageW(hWnd, TDM_SET_BUTTON_ELEVATION_REQUIRED_STATE, ID_DOWNLOAD_AND_INSTALL, TRUE);
			break;
		case TDN_BUTTON_CLICKED:
			if (wParam == ID_DOWNLOAD_AND_INSTALL)
			{
				auto config = MakeTaskDialogDownloadPage(_(L"Downloading Edge WebView"), L"go.microsoft.com", 443, L"/fwlink/p/?LinkId=2124703", true, fileName.c_str(), [](HWND hWnd, LPCWSTR fileName) {
					SendMessageW(hWnd, TDM_SET_MARQUEE_PROGRESS_BAR, TRUE, 0);
					SendMessageW(hWnd, TDM_UPDATE_ELEMENT_TEXT, TDE_MAIN_INSTRUCTION, reinterpret_cast<LPARAM>(_(L"Installing Edge WebView")));
					SendMessageW(hWnd, TDM_UPDATE_ELEMENT_TEXT, TDE_CONTENT, reinterpret_cast<LPARAM>(L" "));

					bool preventClosingDialog = false;

					SHELLEXECUTEINFOW sei = {
						.cbSize = sizeof(sei),
						.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_NOZONECHECKS,
						.hwnd = hWnd,
						.lpVerb = L"runas",
						.lpFile = fileName,
						.lpParameters = L"/install",
						.nShow = SW_SHOW,
					};
					if (ShellExecuteExW(&sei))
					{
						wil::unique_process_handle hProcess(sei.hProcess);
						if (hProcess)
						{
							SendMessageW(hWnd, TDM_ENABLE_BUTTON, IDCANCEL, FALSE);
							WaitForSingleObject(hProcess.get(), INFINITE);
						}
					}
					else
					{
						auto error = LOG_LAST_ERROR();
						ShowTaskDialogErrorInfo(hWnd, HRESULT_FROM_WIN32(error), _(L"Install failed"));
						preventClosingDialog = true;
					}

					DeleteFileW(fileName);

					return preventClosingDialog;
				});
				SendMessageW(hWnd, TDM_NAVIGATE_PAGE, 0, reinterpret_cast<LPARAM>(&config));
				return S_FALSE;
			}
			break;
		}
		return S_OK;
	};

	TASKDIALOGCONFIG config = {
		.cbSize = sizeof(config),
		.hwndParent = hWndParent,
		.hInstance = g_hInst,
		.dwFlags = TDF_USE_COMMAND_LINKS,
		.dwCommonButtons = TDCBF_CANCEL_BUTTON,
		.pszWindowTitle = _(L"Dashboard"),
		.pszMainIcon = MAKEINTRESOURCEW(IDI_CLASHXW),
		.pszMainInstruction = _(L"Failed to load Edge WebView"),
		.pszContent = content.c_str(),
		.cButtons = static_cast<UINT>(std::size(buttons)),
		.pButtons = buttons,
		.nDefaultButton = IDCANCEL,
		.pfCallback = [](HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, LONG_PTR lpRefData) -> HRESULT {
			return (*reinterpret_cast<decltype(callback)*>(lpRefData))(hWnd, msg, wParam, lParam);
		},
		.lpCallbackData = reinterpret_cast<LONG_PTR>(&callback)
	};
	int button = 0;
	if (SUCCEEDED(TaskDialogIndirect(&config, &button, nullptr, nullptr)))
	{
		switch (button)
		{
		case ID_OPEN_IN_BROWSER:
			g_settings.openDashboardInBrowser = true;
			CheckMenuItem(g_hContextMenu, IDM_EXPERIMENTAL_OPENDASHBOARDINBROWSER, MF_BYCOMMAND | (g_settings.openDashboardInBrowser ? MF_CHECKED : MF_UNCHECKED));
			ShellExecuteW(hWndParent, nullptr, L"http://127.0.0.1:9090/ui/", nullptr, nullptr, SW_SHOWNORMAL);
			break;
		case 1024: // Download page
			return true; // Try create webview again
		}
	}
	return false;
}

class EdgeWebView2
{
public:
	static void Create()
	{
		bool tryAgain = false;
		do
		{
			tryAgain = false;

			if (s_webViewEnv)
			{
				CreateWindow(nullptr);
			}
			else
			{
				auto options = wrl::Make<CoreWebView2EnvironmentOptions>();
				options->put_AdditionalBrowserArguments(LR"(--user-agent="ClashX Runtime")");
				HRESULT callbackResult = E_FAIL;
				HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, g_dataPath.c_str(), options.Get(),
					wrl::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
						[&callbackResult](HRESULT result, ICoreWebView2Environment* env) {
							callbackResult = result;
							RETURN_IF_FAILED(result);
							s_webViewEnv = env;
							return S_OK;
						}).Get());

				if (SUCCEEDED_LOG(hr) && SUCCEEDED_LOG(callbackResult))
					CreateWindow(nullptr);
				else
					tryAgain = ShowWebViewFailedDialog(g_hWnd, FAILED(hr));
			}
		} while (tryAgain);
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
		break;
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
			SetPreventPinningForWindow(hWnd);
			ShowWindow(hWnd, SW_SHOW);
			UpdateWindow(hWnd);
		}
	}

	HRESULT OnFrameNavigationStarting(ICoreWebView2* sender, ICoreWebView2NavigationStartingEventArgs* args)
	{
		wil::unique_cotaskmem_string uriStr;
		RETURN_IF_FAILED(args->get_Uri(&uriStr));

		if (IsBridgeLoadedURI(uriStr.get()))
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
