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

	Response Request(const wchar_t* path, const wchar_t* method = L"GET")
	{
		try
		{
			Connect();

			wil::unique_winhttp_hinternet hRequest(WinHttpOpenRequest(m_hConnect.get(), method, path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0));
			THROW_LAST_ERROR_IF_NULL(hRequest);

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

	uint32_t PatchRequest(const wchar_t* path, std::string_view data, const wchar_t* method = L"PATCH")
	{
		try
		{
			Connect();

			wil::unique_winhttp_hinternet hRequest(WinHttpOpenRequest(m_hConnect.get(), method, path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0));
			THROW_LAST_ERROR_IF_NULL(hRequest);

			THROW_IF_WIN32_BOOL_FALSE(WinHttpSendRequest(hRequest.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, static_cast<DWORD>(data.size()), 0));
			DWORD written;
			THROW_IF_WIN32_BOOL_FALSE(WinHttpWriteData(hRequest.get(), data.data(), static_cast<DWORD>(data.size()), &written));
			THROW_IF_WIN32_BOOL_FALSE(WinHttpReceiveResponse(hRequest.get(), nullptr));

			DWORD statusCode;
			DWORD statusCodeSize = sizeof(statusCode);
			THROW_IF_WIN32_BOOL_FALSE(WinHttpQueryHeaders(hRequest.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &statusCode, &statusCodeSize, WINHTTP_NO_HEADER_INDEX));

			return static_cast<uint32_t>(statusCode);
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

		rapidjson::Document json;
		json.Parse(res.data);
		THROW_HR_IF(E_INVALIDARG, json.HasParseError());

		auto it = json.FindMember("version");
		THROW_HR_IF(E_INVALIDARG, it == json.MemberEnd() || !it->value.IsString());

		return { it->value.GetString(), it->value.GetStringLength() };
	}

	void GetConfigs()
	{
		auto res = Request(L"/configs");

		if (res.statusCode != 200)
			return;

		rapidjson::Document json;
		json.Parse(res.data);
		if (json.HasParseError() || !json.IsObject())
			return;

		for (auto it = json.MemberBegin(); it != json.MemberEnd(); ++it)
		{
			if (it->name == "port" && it->value.IsUint())
				g_clashConfig.port = static_cast<uint16_t>(it->value.GetUint());
			else if (it->name == "socks-port" && it->value.IsUint())
				g_clashConfig.socksPort = static_cast<uint16_t>(it->value.GetUint());
			else if (it->name == "allow-lan" && it->value.IsBool())
				g_clashConfig.allowLan = it->value.GetBool();
			else if (it->name == "mode" && it->value.IsString())
			{
				if (it->value == "global")
					g_clashConfig.mode = ClashProxyMode::Global;
				else if (it->value == "rule")
					g_clashConfig.mode = ClashProxyMode::Rule;
				else if (it->value == "direct")
					g_clashConfig.mode = ClashProxyMode::Direct;
				else
					g_clashConfig.mode = ClashProxyMode::Unknown;
			}
			else if (it->name == "log-level" && it->value.IsString())
			{
				if (it->value == "error")
					g_clashConfig.logLevel = ClashLogLevel::Error;
				else if (it->value == "warning")
					g_clashConfig.logLevel = ClashLogLevel::Warning;
				else if (it->value == "info")
					g_clashConfig.logLevel = ClashLogLevel::Info;
				else if (it->value == "debug")
					g_clashConfig.logLevel = ClashLogLevel::Debug;
				else if (it->value == "silent")
					g_clashConfig.logLevel = ClashLogLevel::Silent;
				else
					g_clashConfig.logLevel = ClashLogLevel::Unknown;
			}
		}
	}

	bool PatchConfigs(const rapidjson::Value& json)
	{
		rapidjson::StringBuffer buf;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
		json.Accept(writer);

		std::string_view data(buf.GetString(), buf.GetLength());

		return PatchRequest(L"/configs", data) == 204; // HTTP 204 No Content
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
