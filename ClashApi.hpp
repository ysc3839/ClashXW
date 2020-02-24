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

class ClashApi
{
public:
	ClashApi(std::wstring hostName, INTERNET_PORT port) :
		m_hostName(hostName), m_port(port) {}

	std::string Request(const wchar_t* path, const wchar_t* method = L"GET")
	{
		try
		{
			Connect();

			wil::unique_winhttp_hinternet hRequest(WinHttpOpenRequest(m_hConnect.get(), method, path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0));
			THROW_LAST_ERROR_IF_NULL(hRequest);

			THROW_IF_WIN32_BOOL_FALSE(WinHttpSendRequest(hRequest.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0));

			THROW_IF_WIN32_BOOL_FALSE(WinHttpReceiveResponse(hRequest.get(), nullptr));

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
		catch (...)
		{
			Cleanup();
			throw;
		}
	}

private:
	void Connect()
	{
		if (!m_hSession)
		{
			m_hSession.reset(WinHttpOpen(L"ClashW/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
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
