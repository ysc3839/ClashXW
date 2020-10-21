#pragma once

namespace PortableModeUtil
{
	void CheckAndSetDataPath()
	{
		auto portableDataPath = g_exePath / CLASHXW_DATA_DIR_PORTABLE;
		if (fs::is_directory(portableDataPath))
		{
			g_portableMode = true;
			g_dataPath = std::move(portableDataPath);
		}
		else
			g_dataPath = GetKnownFolderFsPath(FOLDERID_RoamingAppData) / CLASHXW_DATA_DIR;
		g_configPath = g_dataPath / CLASH_CONFIG_DIR_NAME;
	}

	bool CheckOnlyOneInstance() noexcept
	{
		auto hFile = CreateFileW(g_dataPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
		if (hFile == INVALID_HANDLE_VALUE && GetLastError() != ERROR_FILE_NOT_FOUND)
			return false;
		// Hold this handle
		return true;
	}

	void SetAppId()
	{
		if (g_portableMode)
		{
			wchar_t appId[std::size(CLASHXW_APP_ID) + guidSize] = CLASHXW_APP_ID L"/"; // Both size includes the null terminator
			GUID guid = {};
			THROW_IF_FAILED(CoCreateGuid(&guid));
			THROW_HR_IF(E_OUTOFMEMORY, StringFromGUID2(guid, appId + std::size(CLASHXW_APP_ID), guidSize) != guidSize);
			THROW_IF_FAILED(SetCurrentProcessExplicitAppUserModelID(appId));
		}
		else
			THROW_IF_FAILED(SetCurrentProcessExplicitAppUserModelID(CLASHXW_APP_ID));
	}

	void AskForPortableMode()
	{
		if (g_portableMode) return;
		if (fs::is_directory(g_dataPath)) return;

		int button;
		if (SUCCEEDED(TaskDialog(nullptr, g_hInst, _(L"Portable Mode"), nullptr, _(L"Do you want to enable portable mode?\nEnabling portable mode will disable non-portable features like start at login and URL protocol."), TDCBF_YES_BUTTON | TDCBF_NO_BUTTON, MAKEINTRESOURCEW(IDI_CLASHXW), &button)) && button == IDYES)
		{
			g_portableMode = true;
			g_dataPath = std::move(g_exePath / CLASHXW_DATA_DIR_PORTABLE);
			g_configPath = g_dataPath / CLASH_CONFIG_DIR_NAME;
		}
	}
}
