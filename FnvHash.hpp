#pragma once
#include <climits>

constexpr uint32_t FNV1_32_INIT = 0x811c9dc5;
constexpr uint32_t FNV_32_PRIME = 0x01000193;

template <typename CharT>
constexpr uint32_t fnv1a_32(const CharT* str, const size_t len, uint32_t hval = FNV1_32_INIT)
{
	for (size_t i = 0; i < len; ++i)
	{
		for (size_t j = 0; j < sizeof(CharT); ++j)
		{
			hval ^= ((str[i] >> (j * CHAR_BIT)) & 0xff); // little endian
			hval *= FNV_32_PRIME;
		}
	}
	return hval;
}

template <typename CharT, size_t N>
constexpr uint32_t fnv1a_32(const CharT (&str)[N])
{
	return fnv1a_32(str, N - 1);
}
