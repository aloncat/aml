//∙AML
// Copyright (C) 2016-2021 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "crc32.h"

namespace hash {

// TODO: сейчас используется медленный алгоритм (обработка по 1 байту за цикл). У меня уже есть более быстрая реализация
// (см. crc32.cpp в директории old), но в ней есть ошибки на Big-endian платформах. Позже нужно будет перенести её сюда

constexpr uint32_t CRC32_POLYNOMIAL = 0xedb88320;

static uint32_t crc32Table[256];
static volatile bool crc32TableReady = false;

//--------------------------------------------------------------------------------------------------------------------------------
static AML_NOINLINE void InitCRC32Table()
{
	crc32Table[0] = 0;
	for (unsigned t = 1, i = 128; i; i >>= 1)
	{ 
		t = (t >> 1) ^ ((t & 1) * CRC32_POLYNOMIAL);
		crc32Table[i] = t;
	}
	for (unsigned i = 2; i <= 128; i <<= 1)
	{
		const auto t = crc32Table[i];
		for (unsigned j = 1; j < i; ++j)
		{
			crc32Table[i + j] = crc32Table[j] ^ t;
		}
	}

	// Перед установкой флага готовности мы должны быть уверены в том, что все элементы таблицы сохранены
	// в памяти. Иначе на платформах со слабой моделью памяти существует вероятность того, что другой
	// поток, проверив установленный флаг, сможет использовать частично проинициализированные данные
	std::atomic_thread_fence(std::memory_order_release);
	crc32TableReady = true;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE uint32_t GetCRC32(const void* data, size_t size, uint32_t prevHash) noexcept
{
	if (!crc32TableReady)
		InitCRC32Table();
	// Аналогично инициализации, здесь мы должны быть уверены, что
	// данные таблицы не будут прочитаны раньше, чем флаг готовности
	std::atomic_thread_fence(std::memory_order_acquire);

	size_t crc = prevHash ^ 0xffffffff;
	auto p = static_cast<const uint8_t*>(data);
	for (size_t count = size; count; --count)
	{
		crc = crc32Table[(crc ^ *p++) & 0xff] ^ (crc >> 8);
	}
	return static_cast<uint32_t>(~crc);
}

} // namespace hash
