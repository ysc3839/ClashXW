#pragma once

struct Settings
{
	Settings() :
		systemProxy(false)
	{
		startAtLogin = fs::is_regular_file(GetKnownFolderFsPath(FOLDERID_Startup) / CLASHXW_LINK_NAME);
	}

	bool systemProxy;
	bool startAtLogin;
};

// Don't init before WinMain
std::optional<Settings> g_settings;

enum struct ClashProxyMode
{
	Global,
	Rule,
	Direct,
	Unknown
};

enum struct ClashLogLevel
{
	Error,
	Warning,
	Info,
	Debug,
	Silent,
	Unknown
};

struct ClashConfig
{
	uint16_t port;
	uint16_t socksPort;
	bool allowLan;
	ClashProxyMode mode;
	ClashLogLevel logLevel;
};

ClashConfig g_clashConfig = {};

void EnableStartAtLogin(bool enable)
{
	try
	{
		auto linkPath = GetKnownFolderFsPath(FOLDERID_Startup) / CLASHXW_LINK_NAME;
		if (enable)
			CreateShellLink(linkPath.c_str(), GetModuleFsPath(g_hInst).c_str());
		else
			fs::remove(linkPath);
		g_settings->startAtLogin = fs::is_regular_file(linkPath);
	}
	CATCH_LOG();
}

void EnableSystemProxy(bool enable)
{
	if (enable)
	{
		wchar_t server[sizeof("255.255.255.255:65535")];
		swprintf_s(server, L"127.0.0.1:%hu", g_clashConfig.port);
		SetSystemProxy(server);
	}
	else
		SetSystemProxy(nullptr);
	g_settings->systemProxy = enable;
}
