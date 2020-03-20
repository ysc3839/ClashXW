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

void EnableStartAtLogin(bool enable)
{
	try
	{
		auto linkPath = GetKnownFolderFsPath(FOLDERID_Startup) / CLASHW_LINK_NAME;
		if (enable)
			CreateShellLink(linkPath.c_str(), GetModuleFsPath(g_hInst).c_str());
		else
			fs::remove(linkPath);
		g_settings->startAtLogin = fs::is_regular_file(linkPath);
	}
	CATCH_LOG();
}
