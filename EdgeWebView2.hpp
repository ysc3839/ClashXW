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

void RespondJSMessage(ICoreWebView2* webView, std::wstring_view callbackId, rapidjson::WValue data)
{
	rapidjson::WStringBuffer buf;
	rapidjson::WWriter<rapidjson::WStringBuffer> writer(buf);

	writer.StartObject();
	writer.Key(L"responseId");
	writer.String(callbackId.data(), callbackId.length());
	writer.Key(L"responseData");
	data.Accept(writer);
	writer.EndObject();

	webView->PostWebMessageAsJson(buf.GetString());
}

template <typename T>
void RespondJSMessage(ICoreWebView2* webView, std::wstring_view callbackId, T data)
{
	RespondJSMessage(webView, callbackId, rapidjson::WValue(data));
}

void HandleJSMessage(std::wstring_view handlerName, std::wstring_view callbackId, rapidjson::WValue data, ICoreWebView2* webView)
{
	LOG_HR_MSG(E_NOTIMPL, "HandleJSMessage: %ls", handlerName.data());
	if (handlerName == L"ping")
	{
		// FIXME callHandler not implemented
		//bridge?.callHandler("pong")
		RespondJSMessage(webView, callbackId, true);
	}
	else if (handlerName == L"readConfigString")
	{
		// FIXME not implemented
		RespondJSMessage(webView, callbackId, L"");
	}
	else if (handlerName == L"getPasteboard")
	{
		// FIXME not implemented
		RespondJSMessage(webView, callbackId, L"");
	}
	else if (handlerName == L"apiInfo")
	{
		// FIXME not implemented
		rapidjson::WValue data(rapidjson::Type::kObjectType);
		rapidjson::WValue::AllocatorType allocator;
		data.AddMember(L"host", L"127.0.0.1", allocator);
		data.AddMember(L"port", 9090, allocator);
		data.AddMember(L"secret", L"", allocator);
		RespondJSMessage(webView, callbackId, std::move(data));
	}
	else if (handlerName == L"setPasteboard")
	{
		// FIXME not implemented
		RespondJSMessage(webView, callbackId, false);
	}
	else if (handlerName == L"writeConfigWithString")
	{
		// FIXME not implemented
		RespondJSMessage(webView, callbackId, false);
	}
	else if (handlerName == L"setSystemProxy")
	{
		// FIXME not implemented
		RespondJSMessage(webView, callbackId, false);
	}
	else if (handlerName == L"getStartAtLogin")
	{
		RespondJSMessage(webView, callbackId, g_settings->startAtLogin);
	}
	else if (handlerName == L"speedTest")
	{
		// FIXME not implemented
		RespondJSMessage(webView, callbackId, {});
	}
	else if (handlerName == L"setStartAtLogin")
	{
		// FIXME not implemented
		RespondJSMessage(webView, callbackId, false);
	}
	else if (handlerName == L"isSystemProxySet")
	{
		RespondJSMessage(webView, callbackId, g_settings->systemProxy);
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

				wil::com_ptr_nothrow<ICoreWebView2Settings> settings;
				RETURN_IF_FAILED(m_webView->get_Settings(&settings));
				settings->put_AreRemoteObjectsAllowed(FALSE);

				EventRegistrationToken token;
				RETURN_IF_FAILED(m_webView->add_FrameNavigationStarting(wrl::Callback<ICoreWebView2NavigationStartingEventHandler>(
					[](ICoreWebView2* sender, ICoreWebView2NavigationStartingEventArgs* args) {
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
					}).Get(), &token));

				(m_webView->add_WebMessageReceived(Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
					[](ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) {
						wil::unique_cotaskmem_string json;
						RETURN_IF_FAILED(args->get_WebMessageAsJson(&json));

						rapidjson::WDocument obj;
						RETURN_HR_IF(E_INVALIDARG, obj.Parse(json.get()).HasParseError());
						RETURN_HR_IF(E_INVALIDARG, !obj.IsObject());

						auto responseId = obj.FindMember(L"responseId");
						if (responseId != obj.MemberEnd() && responseId->value.IsString())
						{
							return E_NOTIMPL;
						}
						else
						{
							auto handlerName = obj.FindMember(L"handlerName");
							RETURN_HR_IF(E_INVALIDARG, handlerName == obj.MemberEnd() || !handlerName->value.IsString());
							auto callbackId = obj.FindMember(L"callbackId");
							RETURN_HR_IF(E_INVALIDARG, callbackId == obj.MemberEnd() || !callbackId->value.IsString());

							auto data = obj.FindMember(L"data");
							rapidjson::WValue value;
							if (data != obj.MemberEnd())
								value = data->value;

							std::wstring_view handlerNameView{ handlerName->value.GetString(), handlerName->value.GetStringLength() };
							std::wstring_view callbackIdView{ callbackId->value.GetString(), callbackId->value.GetStringLength() };
							HandleJSMessage(handlerNameView, callbackIdView, std::move(value), sender);
						}
						return S_OK;
					}).Get(), &token));

				RECT clientRc;
				GetClientRect(m_hWnd, &clientRc);
				m_webViewHost->put_Bounds(clientRc);

				m_webView->Navigate(L"http://127.0.0.1:9090/ui/");

				return S_OK;
			}).Get());
	}

	HWND m_hWnd;
	wil::com_ptr_nothrow<ICoreWebView2Host> m_webViewHost;
	wil::com_ptr_nothrow<ICoreWebView2> m_webView;
	inline static wil::com_ptr_nothrow<ICoreWebView2Environment> s_webViewEnv;
	inline static size_t s_instances = 0;
};
