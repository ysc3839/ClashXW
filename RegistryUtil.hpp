#pragma once

namespace RegUtil
{
	inline auto RegSetStringValue(HKEY hKey, LPCWSTR lpValueName, std::wstring_view str)
	{
		return RegSetValueExW(hKey, lpValueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(str.data()), str.data() ? static_cast<DWORD>((str.size() + 1) * sizeof(wchar_t)) : 0);
	}

	inline auto RegSetDWORDValue(HKEY hKey, LPCWSTR lpValueName, DWORD value)
	{
		return RegSetValueExW(hKey, lpValueName, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
	}
}
