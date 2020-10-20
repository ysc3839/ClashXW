#pragma once

namespace URLProtocolUtil
{
	namespace
	{
		void CreateURLProtocol(HKEY hClassesKey, const wchar_t* scheme)
		{
			using namespace std::string_literals;

			wil::unique_hkey hKey;
			DWORD disposition;
			THROW_IF_WIN32_ERROR(RegCreateKeyExW(hClassesKey, scheme, 0, nullptr, REG_OPTION_VOLATILE, KEY_WRITE, nullptr, &hKey, &disposition));
			THROW_HR_IF(HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS), disposition == REG_OPENED_EXISTING_KEY);

			THROW_IF_WIN32_ERROR(RegUtil::RegSetStringValue(hKey.get(), nullptr, L"URL:"s + scheme));
			THROW_IF_WIN32_ERROR(RegUtil::RegSetStringValue(hKey.get(), L"URL Protocol", nullptr));
			THROW_IF_WIN32_ERROR(RegUtil::RegSetDWORDValue(hKey.get(), L"UseOriginalUrlEncoding", 1));

			wil::unique_hkey hCommandKey;
			THROW_IF_WIN32_ERROR(RegCreateKeyExW(hKey.get(), LR"(shell\open\command)", 0, nullptr, REG_OPTION_VOLATILE, KEY_WRITE, nullptr, &hCommandKey, nullptr));

			std::wstring command(LR"(")");
			command.append(GetModuleFsPath(g_hInst));
			command.append(LR"(" --url=%1)");
			THROW_IF_WIN32_ERROR(RegUtil::RegSetStringValue(hCommandKey.get(), nullptr, command));
		}
	}

	int HandleURL(std::wstring_view urlStr)
	{
		try
		{
			skyr::url url(urlStr);
			if (!url.scheme().starts_with("clash")) return EXIT_FAILURE;
			if (url.hostname() == "install-config")
			{
				auto configUrl = url.search_parameters().get("url").value();
				auto name = url.search_parameters().get("name").value_or("");

				auto size = (configUrl.size() + 1) + (name.size() + 1);
				wil::unique_handle hFileMapping(CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, static_cast<DWORD>(size), CLASHXW_URL_FILEMAP_NAME));
				THROW_LAST_ERROR_IF_NULL(hFileMapping);
				auto error = GetLastError();
				if (error == ERROR_ALREADY_EXISTS) THROW_WIN32(error);

				wil::unique_mapview_ptr<char> buffer(reinterpret_cast<char*>(MapViewOfFile(hFileMapping.get(), FILE_MAP_WRITE, 0, 0, 0)));
				THROW_LAST_ERROR_IF_NULL(buffer);

				auto urlPtr = buffer.get();
				auto i = configUrl.copy(urlPtr, decltype(configUrl)::npos);
				urlPtr[i] = 0;

				auto namePtr = buffer.get() + i + 1;
				i = name.copy(namePtr, decltype(name)::npos);
				namePtr[i] = 0;

				auto hWnd = FindWindowW(L"ClashXW", L"Global");
				THROW_LAST_ERROR_IF_NULL(hWnd);

				SendMessageW(hWnd, WM_ADDREMOTECONFIG, 0, 0);
			}
		}
		catch (...)
		{
			LOG_CAUGHT_EXCEPTION();
			return EXIT_FAILURE;
		}
		return 0;
	}

	void RemoveURLProtocols() noexcept
	{
		RegDeleteTreeW(HKEY_CURRENT_USER, LR"(Software\Classes\clash)");
		RegDeleteTreeW(HKEY_CURRENT_USER, LR"(Software\Classes\clashxw)");
	}

	void CreateURLProtocols()
	{
		RemoveURLProtocols();

		wil::unique_hkey hKey;
		THROW_IF_WIN32_ERROR(RegOpenKeyExW(HKEY_CURRENT_USER, LR"(Software\Classes)", 0, KEY_WRITE, &hKey));

		CreateURLProtocol(hKey.get(), L"clash");
		CreateURLProtocol(hKey.get(), L"clashxw");
	}
}
