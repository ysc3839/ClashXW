#pragma once

using ClashProxyName = std::string; // UTF-8

enum struct ClashProxyType
{
	Unknown,
	URLTest,
	Fallback,
	LoadBalance,
	Selector,
	Direct,
	Reject,
	Shadowsocks,
	ShadowsocksR,
	Socks5,
	Http,
	Vmess,
	Snell,
	Trojan,
	Relay,
};

NLOHMANN_JSON_SERIALIZE_ENUM(ClashProxyType, {
	{ClashProxyType::Unknown, nullptr},
	{ClashProxyType::URLTest, "URLTest"},
	{ClashProxyType::Fallback, "Fallback"},
	{ClashProxyType::LoadBalance, "LoadBalance"},
	{ClashProxyType::Selector, "Selector"},
	{ClashProxyType::Direct, "Direct"},
	{ClashProxyType::Reject, "Reject"},
	{ClashProxyType::Shadowsocks, "Shadowsocks"},
	{ClashProxyType::ShadowsocksR, "ShadowsocksR"},
	{ClashProxyType::Socks5, "Socks5"},
	{ClashProxyType::Http, "Http"},
	{ClashProxyType::Vmess, "Vmess"},
	{ClashProxyType::Snell, "Snell"},
	{ClashProxyType::Trojan, "Trojan"},
	{ClashProxyType::Relay, "Relay"},
});

struct ClashProxy
{
	ClashProxyName name;
	ClashProxyType type;
	std::vector<ClashProxyName> all;
	// ClashProxySpeedHistory history; // FIXME
	std::optional<ClashProxyName> now;

	friend void from_json(const json& j, ClashProxy& value) {
		JSON_FROM(name);
		JSON_FROM(type);
		JSON_TRY_FROM(all);
		try { value.now.emplace(j.at("now").get<decltype(now)::value_type>()); } CATCH_LOG();
	}

	friend void from_json(const json& j, std::unique_ptr<ClashProxy>& ptr) {
		if (!ptr)
			ptr = std::make_unique<ClashProxy>();
		from_json(j, *ptr);
	}
};

struct ClashProxies
{
	std::unordered_map<ClashProxyName, std::unique_ptr<ClashProxy>> proxiesMap;

	friend void from_json(const json& j, ClashProxies& value) {
		j.at("proxies").get_to(value.proxiesMap);
	}
};
