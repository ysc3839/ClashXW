#pragma once

struct Settings
{
	bool systemProxy;
	std::wstring benchmarkUrl;
};

// Don't init before WinMain
std::optional<Settings> g_settings;



enum struct ClashProxyMode
{
	Unknown,
	Global,
	Rule,
	Direct
};

NLOHMANN_JSON_SERIALIZE_ENUM(ClashProxyMode, {
	{ClashProxyMode::Unknown, nullptr},
	{ClashProxyMode::Global, "global"},
	{ClashProxyMode::Rule, "rule"},
	{ClashProxyMode::Direct, "direct"},
})

enum struct ClashLogLevel
{
	Unknown,
	Error,
	Warning,
	Info,
	Debug,
	Silent
};

NLOHMANN_JSON_SERIALIZE_ENUM(ClashLogLevel, {
	{ClashLogLevel::Unknown, nullptr},
	{ClashLogLevel::Error, "error"},
	{ClashLogLevel::Warning, "warning"},
	{ClashLogLevel::Info, "info"},
	{ClashLogLevel::Debug, "debug"},
	{ClashLogLevel::Silent, "silent"},
})

struct ClashConfig
{
	uint16_t port;
	uint16_t socksPort;
	uint16_t mixedPort;
	bool allowLan;
	ClashProxyMode mode;
	ClashLogLevel logLevel;

	friend void from_json(const json& j, ClashConfig& c) {
		j.at("port").get_to(c.port);
		j.at("socks-port").get_to(c.socksPort);
		j.at("mixed-port").get_to(c.mixedPort);
		j.at("allow-lan").get_to(c.allowLan);
		j.at("mode").get_to(c.mode);
		j.at("log-level").get_to(c.logLevel);
	}
};

ClashConfig g_clashConfig = {};

namespace StartAtLogin
{
	bool IsEnabled()
	{
		return fs::is_regular_file(GetKnownFolderFsPath(FOLDERID_Startup) / CLASHXW_LINK_NAME);
	}

	void SetEnable(bool enable)
	{
		try
		{
			auto linkPath = GetKnownFolderFsPath(FOLDERID_Startup) / CLASHXW_LINK_NAME;
			if (enable)
				CreateShellLink(linkPath.c_str(), GetModuleFsPath(g_hInst).c_str());
			else
				fs::remove(linkPath);
		}
		CATCH_LOG();
	}
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
