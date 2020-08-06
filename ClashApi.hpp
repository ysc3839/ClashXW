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

			wil::unique_winhttp_hinternet hRequest(WinHttpOpenRequest(m_hConnect.get(), method, path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0));
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

		if (res.statusCode != 200)
			throw res.statusCode;

		return json::parse(res.data).at("version").get<std::string>();
	}

	ClashConfig GetConfig()
	{
		auto res = Request(L"/configs");

		if (res.statusCode != 200)
			throw res.statusCode;

		return json::parse(res.data).get<ClashConfig>();
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

private:
	void Connect()
	{
		if (!m_hSession)
		{
			m_hSession.reset(WinHttpOpen(L"ClashXW/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
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
