//∙AML
// Copyright (C) 2017-2021 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "fasthash.h"

namespace hash {

// Функции GetFastHash используют алгоритм FNV-1a (32-битный хеш), который не уступает по скорости работы
// алгоритму SDBM. Но в отличие от SDBM, FNV-1a даёт лучшее распределение. Среди простых мультипликативных
// хеш-функций FNV-1a является одной из лучших. Но её использование целесообразно лишь для коротких строк

constexpr uint32_t FNV_SEED = 0x811c9dc5;
constexpr uint32_t FNV_PRIME = 0x01000193;

#define FNV_HASH_1B(V) hash = (hash ^ (V)) * FNV_PRIME
#define FNV_HASH_2B(V) { FNV_HASH_1B((V) & 0xff); FNV_HASH_1B((V) >> 8); }
#define FNV_HASH_4B(V) { FNV_HASH_2B((V) & 0xffff); V >>= 16; FNV_HASH_2B(V); }

const uint8_t loCaseTT[256] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
	32, 32, 32 };

//--------------------------------------------------------------------------------------------------------------------------------
unsigned GetFastHash(const char* str, bool toLower) noexcept
{
	uint32_t v, hash = FNV_SEED;
	if (auto p = reinterpret_cast<const uint8_t*>(str))
	{
		if (toLower)
		{
			while ((v = *p++) != 0)
			{
				v += loCaseTT[v];
				FNV_HASH_1B(v);
			}
		} else
		{
			while ((v = *p++) != 0)
				FNV_HASH_1B(v);
		}
	}
	return hash;
}

//--------------------------------------------------------------------------------------------------------------------------------
unsigned GetFastHash(std::string_view str, bool toLower) noexcept
{
	uint32_t v, hash = FNV_SEED;
	auto p = reinterpret_cast<const uint8_t*>(str.data());
	size_t count = str.size();

	if (toLower)
	{
		while (count--)
		{
			v = *p++;
			v += loCaseTT[v];
			FNV_HASH_1B(v);
		}
	} else
	{
		while (count--)
		{
			FNV_HASH_1B(*p++);
		}
	}

	return hash;
}

//--------------------------------------------------------------------------------------------------------------------------------
unsigned GetFastHash(const wchar_t* str, bool toLower) noexcept
{
	uint32_t v, hash = FNV_SEED;
	if constexpr (sizeof(wchar_t) == 2)
	{
		if (auto p = reinterpret_cast<const uint16_t*>(str))
		{
			if (toLower)
			{
				while ((v = *p++) != 0)
				{
					if (v <= 0x7f)
						v += loCaseTT[v];
					FNV_HASH_2B(v);
				}
			} else
			{
				while ((v = *p++) != 0)
					FNV_HASH_2B(v);
			}
		}
	}
	else if (auto p = reinterpret_cast<const uint32_t*>(str))
	{
		if (toLower)
		{
			while ((v = *p++) != 0)
			{
				if (v <= 0x7f)
					v += loCaseTT[v];
				FNV_HASH_4B(v);
			}
		} else
		{
			while ((v = *p++) != 0)
				FNV_HASH_4B(v);
		}
	}
	return hash;
}

//--------------------------------------------------------------------------------------------------------------------------------
unsigned GetFastHash(std::wstring_view str, bool toLower) noexcept
{
	uint32_t v, hash = FNV_SEED;
	using CharT = std::conditional_t<sizeof(wchar_t) == 2, uint16_t, uint32_t>;
	auto p = reinterpret_cast<const CharT*>(str.data());
	size_t count = str.size();

	if constexpr (sizeof(wchar_t) == 2)
	{
		if (toLower)
		{
			while (count--)
			{
				v = *p++;
				if (v <= 0x7f)
					v += loCaseTT[v];
				FNV_HASH_2B(v);
			}
		} else
		{
			while (count--)
			{
				v = *p++;
				FNV_HASH_2B(v);
			}
		}
	}
	else if (toLower)
	{
		while (count--)
		{
			v = *p++;
			if (v <= 0x7f)
				v += loCaseTT[v];
			FNV_HASH_4B(v);
		}
	} else
	{
		while (count--)
		{
			v = *p++;
			FNV_HASH_4B(v);
		}
	}

	return hash;
}

//--------------------------------------------------------------------------------------------------------------------------------
unsigned GetFastHash(const uint16_t* str) noexcept
{
	uint32_t v, hash = FNV_SEED;
	if (str)
	{
		while ((v = *str++) != 0)
			FNV_HASH_2B(v);
	}
	return hash;
}

//--------------------------------------------------------------------------------------------------------------------------------
unsigned GetFastHash(const uint16_t* str, size_t count) noexcept
{
	uint32_t v, hash = FNV_SEED;
	while (count--)
	{
		v = *str++;
		FNV_HASH_2B(v);
	}
	return hash;
}

//--------------------------------------------------------------------------------------------------------------------------------
unsigned GetFastHash(const void* data, size_t size, unsigned prevHash) noexcept
{
	uint32_t hash = prevHash;
	auto p = static_cast<const uint8_t*>(data);
	while (size--)
	{
		FNV_HASH_1B(*p++);
	}
	return hash;
}

} // namespace hash
