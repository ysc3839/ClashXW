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

struct Response
{
	uint32_t statusCode;
	std::string data;
};

using u16milliseconds = std::chrono::duration<uint16_t, std::milli>;

class ClashApi
{
public:
	ClashApi(std::wstring hostName, INTERNET_PORT port) :
		m_hostName(hostName), m_port(port) {}

	Response Request(const wchar_t* path, const wchar_t* method = L"GET", json parameters = nullptr)
	{
		try
		{
			Connect();

			wil::unique_winhttp_hinternet hRequest(WinHttpOpenRequest(m_hConnect.get(), method, path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_ESCAPE_PERCENT));
			THROW_LAST_ERROR_IF_NULL(hRequest);

			if (!parameters.is_null())
			{
				auto data = parameters.dump(); // must remains valid until after WinHttpWriteData completes
				const auto length = static_cast<DWORD>(data.size());
				THROW_IF_WIN32_BOOL_FALSE(WinHttpSendRequest(hRequest.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, length, 0));
				DWORD written;
				THROW_IF_WIN32_BOOL_FALSE(WinHttpWriteData(hRequest.get(), data.data(), length, &written));
			}
			else
				THROW_IF_WIN32_BOOL_FALSE(WinHttpSendRequest(hRequest.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0));

			THROW_IF_WIN32_BOOL_FALSE(WinHttpReceiveResponse(hRequest.get(), nullptr));

			DWORD statusCode;
			DWORD statusCodeSize = sizeof(statusCode);
			THROW_IF_WIN32_BOOL_FALSE(WinHttpQueryHeaders(hRequest.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX));

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

			return { static_cast<uint32_t>(statusCode), std::move(data) };
		}
		catch (...)
		{
			Cleanup();
			throw;
		}
	}

	std::string GetVersion()
	{
		auto res = Request(L"/version");

		THROW_HR_IF(HTTP_E_STATUS_UNEXPECTED, res.statusCode != 200);

		return json::parse(res.data).at("version").get<std::string>();
	}

	ClashConfig GetConfig()
	{
		auto res = Request(L"/configs");

		THROW_HR_IF(HTTP_E_STATUS_UNEXPECTED, res.statusCode != 200);

		return json::parse(res.data).get<ClashConfig>();
	}

	std::optional<std::wstring> RequestConfigUpdate(fs::path configPath)
	{
		auto u8path = configPath.u8string();
		std::string_view path(reinterpret_cast<const char*>(u8path.c_str()), u8path.size());
		auto res = Request(L"/configs", L"PUT", { {"path", path} });
		if (res.statusCode != 204)
		{
			std::wstring errorDesp = _(L"Error occoured, Please try to fix it by restarting ClashXW.");
			try
			{
				errorDesp = Utf8ToUtf16(json::parse(res.data).at("message").get<std::string_view>());
			}
			CATCH_LOG();
			return errorDesp;
		}
		return std::nullopt;
	}

	bool UpdateProxyMode(ClashProxyMode mode)
	{
		auto res = Request(L"/configs", L"PATCH", { {"mode", mode} });
		return res.statusCode == 204; // HTTP 204 No Content
	}

	bool UpdateLogLevel(ClashLogLevel level)
	{
		auto res = Request(L"/configs", L"PATCH", { {"log-level", level} });
		return res.statusCode == 204; // HTTP 204 No Content
	}

	bool UpdateAllowLan(bool allow)
	{
		auto res = Request(L"/configs", L"PATCH", { {"allow-lan", allow} });
		return res.statusCode == 204; // HTTP 204 No Content
	}

	u16milliseconds GetProxyDelay(std::string_view proxyName)
	{
		std::wstring path = L"/proxies/";
		path.append(Utf8ToUtf16(proxyName));
		path.append(L"/delay"); // "/proxies/\(proxyName.encoded)/delay"

		path.append(L"?timeout=5000&url=");
		path.append(g_settings.benchmarkUrl);

		auto res = Request(path.c_str());

		THROW_HR_IF(HTTP_E_STATUS_UNEXPECTED, res.statusCode != 200);

		return u16milliseconds(json::parse(res.data).at("delay").get<uint16_t>());
	}

	ClashProxies GetProxies()
	{
		auto res = Request(L"/proxies");

		THROW_HR_IF(HTTP_E_STATUS_UNEXPECTED, res.statusCode != 200);

		return json::parse(res.data).get<ClashProxies>();
	}

	bool UpdateProxyGroup(std::string_view group, std::string_view selectProxy)
	{
		std::wstring path = L"/proxies/";
		path.append(Utf8ToUtf16(group));
		auto res = Request(path.c_str(), L"PUT", { {"name", selectProxy} });
		return res.statusCode == 204; // HTTP 204 No Content
	}

private:
	void Connect()
	{
		if (!m_hSession)
		{
			m_hSession.reset(WinHttpOpen(CLASHXW_USERAGENT, WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
			THROW_LAST_ERROR_IF_NULL(m_hSession);
		}
		if (!m_hConnect)
		{
			m_hConnect.reset(WinHttpConnect(m_hSession.get(), m_hostName.c_str(), m_port, 0));
			THROW_LAST_ERROR_IF_NULL(m_hConnect);
		}
	}

	void Cleanup()
	{
		m_hConnect.reset();
		m_hSession.reset();
	}

	std::wstring m_hostName;
	INTERNET_PORT m_port;
	wil::unique_winhttp_hinternet m_hSession;
	wil::unique_winhttp_hinternet m_hConnect;
};
