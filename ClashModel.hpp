#pragma once

struct ClashProxy
{
	using Name = std::string; // UTF-8

	enum struct Type
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

	Name name;
	Type type;
	std::vector<Name> all;
	// ClashProxySpeedHistory history; // FIXME
	std::optional<Name> now;

	friend void from_json(const json& j, ClashProxy& value) {
		JSON_FROM(name);
		JSON_FROM(type);
		JSON_TRY_FROM(all);
		try { value.now.emplace(j.at("now").get<decltype(now)::value_type>()); } CATCH_LOG();
	}
};

NLOHMANN_JSON_SERIALIZE_ENUM(ClashProxy::Type, {
	{ClashProxy::Type::Unknown, nullptr},
	{ClashProxy::Type::URLTest, "URLTest"},
	{ClashProxy::Type::Fallback, "Fallback"},
	{ClashProxy::Type::LoadBalance, "LoadBalance"},
	{ClashProxy::Type::Selector, "Selector"},
	{ClashProxy::Type::Direct, "Direct"},
	{ClashProxy::Type::Reject, "Reject"},
	{ClashProxy::Type::Shadowsocks, "Shadowsocks"},
	{ClashProxy::Type::ShadowsocksR, "ShadowsocksR"},
	{ClashProxy::Type::Socks5, "Socks5"},
	{ClashProxy::Type::Http, "Http"},
	{ClashProxy::Type::Vmess, "Vmess"},
	{ClashProxy::Type::Snell, "Snell"},
	{ClashProxy::Type::Trojan, "Trojan"},
	{ClashProxy::Type::Relay, "Relay"},
});

struct ClashProxies
{
	std::map<ClashProxy::Name, ClashProxy> proxiesMap;

	friend void from_json(const json& j, ClashProxies& value) {
		j.at("proxies").get_to(value.proxiesMap);
	}
};

struct ClashProvider
{
	using Name = std::string; // UTF-8

	enum struct Type
	{
		Unknown,
		Proxy,
		String,
	};

	enum struct VehicleType
	{
		Unknown,
		HTTP,
		File,
		Compatible,
	};

	Name name;
	std::vector<ClashProxy> proxies;
	Type type;
	VehicleType vehicleType;
};

NLOHMANN_JSON_SERIALIZE_ENUM(ClashProvider::Type, {
	{ClashProvider::Type::Unknown, nullptr},
	{ClashProvider::Type::Proxy, "Proxy"},
	{ClashProvider::Type::String, "String"},
});

NLOHMANN_JSON_SERIALIZE_ENUM(ClashProvider::VehicleType, {
	{ClashProvider::VehicleType::Unknown, nullptr},
	{ClashProvider::VehicleType::HTTP, "HTTP"},
	{ClashProvider::VehicleType::File, "File"},
	{ClashProvider::VehicleType::Compatible, "Compatible"},
});

struct ClashProviders
{
	std::map<ClashProvider::Name, ClashProxy> allProviders;

	friend void from_json(const json& j, ClashProviders& value) {
		j.at("providers").get_to(value.allProviders);
	}
};
