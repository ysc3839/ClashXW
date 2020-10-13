#pragma once

void ShowBalloon(const wchar_t* info, const wchar_t* title, DWORD flag);

namespace RemoteConfigManager
{
	namespace
	{
		wil::unique_threadpool_timer_nowait timer;

		std::string GetRemoteConfigData(const RemoteConfig& config)
		{
			auto urlSize = static_cast<DWORD>(config.url.size());
			auto hostName = std::make_unique<wchar_t[]>(urlSize);
			URL_COMPONENTS urlComp = {
				.dwStructSize = sizeof(urlComp),
				.lpszHostName = hostName.get(),
				.dwHostNameLength = urlSize,
				.dwUrlPathLength = MAXDWORD,
				.dwExtraInfoLength = MAXDWORD
			};
			THROW_IF_WIN32_BOOL_FALSE(WinHttpCrackUrl(config.url.c_str(), urlSize, ICU_REJECT_USERPWD, &urlComp));

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

			// FIXME: handle IDNs
			wil::unique_winhttp_hinternet hConnect(WinHttpConnect(hSession.get(), urlComp.lpszHostName, urlComp.nPort, 0));
			THROW_LAST_ERROR_IF_NULL(hConnect);

			wil::unique_winhttp_hinternet hRequest(WinHttpOpenRequest(hConnect.get(), L"GET", urlComp.lpszUrlPath, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, urlComp.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0));
			THROW_LAST_ERROR_IF_NULL(hRequest);

			THROW_IF_WIN32_BOOL_FALSE(WinHttpSendRequest(hRequest.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0));

			THROW_IF_WIN32_BOOL_FALSE(WinHttpReceiveResponse(hRequest.get(), nullptr));

			DWORD statusCode;
			DWORD statusCodeSize = sizeof(statusCode);
			THROW_IF_WIN32_BOOL_FALSE(WinHttpQueryHeaders(hRequest.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX));

			THROW_HR_IF(HTTP_E_STATUS_UNEXPECTED, statusCode != 200);

			std::string data;
			while (true)
			{
				DWORD size;
				THROW_IF_WIN32_BOOL_FALSE(WinHttpQueryDataAvailable(hRequest.get(), &size));
				if (size == 0)
					break;

				auto offset = data.size();
				data.resize(offset + static_cast<size_t>(size));

				DWORD read;
				THROW_IF_WIN32_BOOL_FALSE(WinHttpReadData(hRequest.get(), data.data() + offset, size, &read));
				data.resize(offset + static_cast<size_t>(read));
			}

			return data;
		}
	}

	IAsyncOperation<bool> UpdateConfig(RemoteConfig& config)
	{
		co_await winrt::resume_background();

		std::string configString;
		try
		{
			configString = GetRemoteConfigData(config);
		}
		catch (...)
		{
			LOG_CAUGHT_EXCEPTION();
			co_return false;
		}

		wil::unique_file file;
		_wfopen_s(&file, (g_configPath / (config.name + L".yaml")).c_str(), L"wb");
		fwrite(configString.c_str(), sizeof(decltype(configString)::value_type), configString.size(), file.get());

		config.updating = false;
		config.updateTime = std::chrono::system_clock::now();

		co_return true;
	}

	void CheckUpdate(bool ignoreTimeLimit = false)
	{
		std::vector<IAsyncOperation<bool>> operations;
		operations.reserve(g_settings.remoteConfig.size());

		for (const auto& config : g_settings.remoteConfig)
		{
			// 48 hours check
			bool timeLimitNoMantians = (std::chrono::system_clock::now() - config->updateTime.value_or(std::chrono::system_clock::time_point())) < 48h;
			if (timeLimitNoMantians && !ignoreTimeLimit)
				continue;
			operations.emplace_back(UpdateConfig(*config));
		}

		bool success = true;
		for (const auto& operation : operations)
		{
			if (!operation.get())
				success = false;
		}

		if (!success)
			ShowBalloon(_(L"Remote Config Update Fail"), _(L"Error"), NIIF_ERROR);

		if (g_hWndRemoteConfigDlg)
			PostMessageW(g_hWndRemoteConfigDlg, WM_REFRESHCONFIGS, 0, 0);
	}

	void SetupAutoUpdateTimer()
	{
		timer.reset(CreateThreadpoolTimer([](auto, auto, auto) {
			CheckUpdate();
		}, nullptr, nullptr));
		THROW_LAST_ERROR_IF_NULL(timer);

		constexpr std::chrono::duration<DWORD, std::milli> interval = 3h, tolerance = 1h;

		FILETIME dueTime = {};
		SetThreadpoolTimer(timer.get(), &dueTime, interval.count(), tolerance.count());
	}
}
