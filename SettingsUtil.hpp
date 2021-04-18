#pragma once

struct RemoteConfig
{
	std::wstring url;
	std::wstring name;
	std::optional<std::chrono::system_clock::time_point> updateTime;
	bool updating = false;

	std::wstring GetTimeString() const
	{
		if (updating)
			return _(L"Updating");
		if (!updateTime)
			return _(L"Never");

		try
		{
			FILETIME fileTime = winrt::clock::to_FILETIME(winrt::clock::from_sys(*updateTime));
			SYSTEMTIME utcTime, localTime;
			THROW_IF_WIN32_BOOL_FALSE(FileTimeToSystemTime(&fileTime, &utcTime));
			THROW_IF_WIN32_BOOL_FALSE(SystemTimeToTzSpecificLocalTime(nullptr, &utcTime, &localTime));

			std::wstring str(12, L'\0');
			// return value including the terminating null character
			auto i = GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &localTime, L"MM-dd", str.data(), static_cast<int>(str.size()), nullptr);
			THROW_LAST_ERROR_IF(i == 0);

			str[i - 1] = L' ';

			i = GetTimeFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &localTime, L"HH:mm", str.data() + i, static_cast<int>(str.size()) - i);
			THROW_LAST_ERROR_IF(i == 0);

			return str;
		}
		CATCH_LOG();
		return {};
	}

	friend void to_json(json& j, const RemoteConfig& value) {
		try { j["url"] = Utf16ToUtf8(value.url); } CATCH_LOG();
		try { j["name"] = Utf16ToUtf8(value.name); } CATCH_LOG();
		if (value.updateTime)
			j["updateTime"] = std::chrono::system_clock::to_time_t(*value.updateTime);
	}

	friend void to_json(json& j, const std::unique_ptr<RemoteConfig>& ptr) {
		if (ptr)
			to_json(j, *ptr);
	}

	friend void from_json(const json& j, RemoteConfig& value) {
		try { value.url = Utf8ToUtf16(j.at("url").get<std::string_view>()); } CATCH_LOG();
		try { value.name = Utf8ToUtf16(j.at("name").get<std::string_view>()); } CATCH_LOG();
		try { value.updateTime.emplace(std::chrono::system_clock::from_time_t(j.at("updateTime").get<time_t>())); } CATCH_LOG();
	}

	friend void from_json(const json& j, std::unique_ptr<RemoteConfig>& ptr) {
		if (!ptr)
			ptr = std::make_unique<RemoteConfig>();
		from_json(j, *ptr);
	}
};

struct Settings
{
	bool systemProxy;
	std::wstring benchmarkUrl;
	bool openDashboardInBrowser;
	std::wstring configFile;
	std::vector<std::unique_ptr<RemoteConfig>> remoteConfig;
	bool configAutoUpdate;
	std::wstring bypassRules;

	friend void to_json(json& j, const Settings& value) {
		JSON_TO(systemProxy);
		try { j["benchmarkUrl"] = Utf16ToUtf8(value.benchmarkUrl); } CATCH_LOG();
		JSON_TO(openDashboardInBrowser);
		try { j["configFile"] = Utf16ToUtf8(value.configFile); } CATCH_LOG();
		JSON_TO(remoteConfig);
		JSON_TO(configAutoUpdate);
		try { j["bypassRules"] = Utf16ToUtf8(value.bypassRules); } CATCH_LOG();
	}

	friend void from_json(const json& j, Settings& value) {
		JSON_TRY_FROM(systemProxy);
		try { value.benchmarkUrl = Utf8ToUtf16(j.at("benchmarkUrl").get<std::string_view>()); } CATCH_LOG();
		JSON_TRY_FROM(openDashboardInBrowser);
		try { value.configFile = Utf8ToUtf16(j.at("configFile").get<std::string_view>()); } CATCH_LOG();
		JSON_TRY_FROM(remoteConfig);
		JSON_TRY_FROM(configAutoUpdate);
		try { value.bypassRules = Utf8ToUtf16(j.at("bypassRules").get<std::string_view>()); } CATCH_LOG();
	}
};

Settings g_settings;

void DefaultSettings()
{
	g_settings.systemProxy = false;
	g_settings.benchmarkUrl = L"http://cp.cloudflare.com/generate_204";
	g_settings.openDashboardInBrowser = false;
	g_settings.configFile = CLASH_DEF_CONFIG_NAME;
	g_settings.configAutoUpdate = false;
	g_settings.bypassRules = L"<local>";
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
		if (g_portableMode) return false;
		return fs::is_regular_file(GetKnownFolderFsPath(FOLDERID_Startup) / CLASHXW_LINK_NAME);
	}

	void SetEnable(bool enable)
	{
		if (g_portableMode) return;
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
	auto port = g_clashConfig.port;
	if (port == 0)
	{
		port = g_clashConfig.mixedPort;
		if (port == 0)
			enable = false;
	}

	if (enable)
	{
		wchar_t server[sizeof("255.255.255.255:65535")];
		swprintf_s(server, L"127.0.0.1:%hu", port);
		SetSystemProxy(server, g_settings.bypassRules.c_str());
	}
	else
		SetSystemProxy(nullptr);
	g_settings.systemProxy = enable;
}
