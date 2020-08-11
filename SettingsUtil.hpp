#pragma once

#define JSON_TO(k) j[#k] = value.k;
#define JSON_TRY_FROM(k) try { j.at(#k).get_to(value.k); } CATCH_LOG()

struct Settings
{
	bool systemProxy;
	std::wstring benchmarkUrl;

	friend void to_json(json& j, const Settings& value) {
		JSON_TO(systemProxy);
		try { j["benchmarkUrl"] = Utf16ToUtf8(value.benchmarkUrl); } CATCH_LOG();
	}

	friend void from_json(const json& j, Settings& value) {
		JSON_TRY_FROM(systemProxy);
		try { value.benchmarkUrl = Utf8ToUtf16(j.at("benchmarkUrl").get<std::string_view>()); } CATCH_LOG();
	}
};

Settings g_settings;

void DefaultSettings()
{
	g_settings.systemProxy = false;
	g_settings.benchmarkUrl = L"http://cp.cloudflare.com/generate_204";
}

void LoadSettings()
{
	try
	{
		DefaultSettings();

		std::ifstream i(g_dataPath / CLASHXW_CONFIG_NAME);
		json j;
		i >> j;
		j.get_to(g_settings);
	}
	CATCH_LOG();
}

void SaveSettings()
{
	try
	{
		std::ofstream o(g_dataPath / CLASHXW_CONFIG_NAME);
		json j = g_settings;
		o << j;
	}
	CATCH_LOG();
}

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
	g_settings.systemProxy = enable;
}
