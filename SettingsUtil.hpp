#pragma once

struct Settings
{
	Settings() :
		systemProxy(false)
	{
		startAtLogin = fs::is_regular_file(GetKnownFolderFsPath(FOLDERID_Startup) / CLASHW_LINK_NAME);
	}

	bool systemProxy;
	bool startAtLogin;
};

// Don't init before WinMain
std::optional<Settings> g_settings;
