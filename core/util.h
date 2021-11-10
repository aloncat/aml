﻿//∙AML
// Copyright (C) 2016-2021 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "platform.h"

#include <stdlib.h>
#include <string.h>
#include <type_traits>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Макросы
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
#define AML_NONCOPYABLE(NAME) \
	NAME(const NAME&) = delete; \
	NAME& operator =(const NAME&) = delete;

//--------------------------------------------------------------------------------------------------------------------------------
#define AML_SAFE_DELETE(P) \
	((void)(P && (delete P, P = nullptr, 0)))

//--------------------------------------------------------------------------------------------------------------------------------
#define AML_SAFE_DELETEA(P) \
	((void)(P && (delete[] P, P = nullptr, 0)))

//--------------------------------------------------------------------------------------------------------------------------------
#define AML_SAFE_RELEASE(P) \
	((void)(P && (P->Release(), P = nullptr, 0)))

//--------------------------------------------------------------------------------------------------------------------------------
#define AML_FILLA(P, V, COUNT) \
	memset(P, V, sizeof(P[0]) * (COUNT))

//--------------------------------------------------------------------------------------------------------------------------------
#define AML_WSTRING(STRING) AML_WIDE_STRING(STRING)
#define AML_WIDE_STRING(STRING) L##STRING

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Функция CheckMinimalRequirements
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace util {

// Выполняет минимальную проверку совместимости компилятора/RTL/системы. Если компилятор, RTL или
// система окажутся несовместимыми, функция вернёт false. Если параметр terminateIfFailed равен
// true, то работа приложения будет аварийно завершена вызовом abort
bool CheckMinimalRequirements(bool terminateIfFailed = true);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Функции Is*Build для проверки параметров сборки в run-time
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
constexpr bool IsX64Build() noexcept
{
	return sizeof(size_t) == 8;
}

//--------------------------------------------------------------------------------------------------------------------------------
constexpr bool IsDebugBuild() noexcept
{
	#if AML_DEBUG
		return true;
	#else
		return false;
	#endif
}

//--------------------------------------------------------------------------------------------------------------------------------
constexpr bool IsProductionBuild() noexcept
{
	#if AML_PRODUCTION
		return true;
	#else
		return false;
	#endif
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Макросы AML_TO_LE*, функции CastTo и ByteSwap*
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
#if AML_LITTLE_ENDIAN
	// NB: использование функции CastTo (неявное преобразование) в сравнении явным с преобразованием значения
	// с помощью static_cast имеет преимущество: в случаях, когда размер типа переданного значения больше 16 бит
	// для макроса _LE16 (32 бита для макроса _LE32), компилятор выдаст предупреждение о возможной потере данных
	#define AML_TO_LE16(V) util::CastTo<uint16_t>(V)
	#define AML_TO_LE32(V) util::CastTo<uint32_t>(V)
	#define AML_TO_LE64(V) util::CastTo<uint64_t>(V)
#elif AML_BIG_ENDIAN
	#define AML_TO_LE16(V) util::ByteSwap16(V)
	#define AML_TO_LE32(V) util::ByteSwap32(V)
	#define AML_TO_LE64(V) util::ByteSwap64(V)
#else
	#error Unrecognized architecture
#endif

//--------------------------------------------------------------------------------------------------------------------------------
template<class T>
constexpr const T& CastTo(const T& value) noexcept
{
	return value;
}

//--------------------------------------------------------------------------------------------------------------------------------
constexpr uint16_t ByteSwap16(uint16_t value) noexcept
{
	return (value >> 8) | (value << 8);
}

//--------------------------------------------------------------------------------------------------------------------------------
inline uint32_t ByteSwap32(uint32_t value) noexcept
{
	#if AML_OS_WINDOWS && !AML_DEBUG
		// Этот интринсик в MSVC2017 генерирует в отладочных конфигурациях
		// более медленный код, поэтому используем его только в Release
		return _byteswap_ulong(value);
	#else
		return (value >> 24) | ((value >> 8) & 0xff00) | ((value & 0xff00) << 8) | (value << 24);
	#endif
}

//--------------------------------------------------------------------------------------------------------------------------------
inline uint64_t ByteSwap64(uint64_t value) noexcept
{
	#if AML_OS_WINDOWS && !AML_DEBUG
		// Этот интринсик в MSVC2017 генерирует в отладочных конфигурациях
		// более медленный код, поэтому используем его только в Release
		return _byteswap_uint64(value);
	#else
		const uint32_t lo = static_cast<uint32_t>(value);
		const uint32_t hi = value >> 32;

		return (static_cast<uint64_t>(
			(lo >> 24) | ((lo >> 8) & 0xff00) | ((lo & 0xff00) << 8) | (lo << 24)) << 32) |
			((hi >> 24) | ((hi >> 8) & 0xff00) | ((hi & 0xff00) << 8) | (hi << 24));
	#endif
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Функция CountOf
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Возвращает количество элементов в массиве

//--------------------------------------------------------------------------------------------------------------------------------
template<class T, size_t arraySize>
constexpr size_t CountOf(const T (&)[arraySize]) noexcept
{
	return arraySize;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Функция Clamp
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Возвращает значение параметра v, если оно лежит внутри диапазона [lo, hi].
// Иначе возвращает значение lo (если v < lo) либо значение hi (если v > hi)

//--------------------------------------------------------------------------------------------------------------------------------
template<class T>
constexpr const T& Clamp(const T& v, const T& lo, const T& hi)
{
	return (v < lo) ? lo : (hi < v) ? hi : v;
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class T, class U, class = std::enable_if_t<std::is_convertible_v<U, T>>>
constexpr T Clamp(const T& v, const U& lo, const U& hi)
{
	return (v < lo) ? lo : (hi < v) ? hi : v;
}

} // namespace util
