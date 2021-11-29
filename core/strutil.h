//∙AML
// Copyright (C) 2016-2021 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "strcommon.h"

#include <string>
#include <string_view>
#include <utility>

namespace util {

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Конвертация строк Ansi/Wide
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Преобразует Ansi строку в строку Wide, используя текущую системную локаль
std::wstring FromAnsi(std::string_view str);

// Преобразует Ansi строку str в строку Unicode (Wide) и сохраняет её в buffer. Параметр bufferSize задаёт размер буфера
// (в символах). Если размер достаточен, то в буфер будет записана строка, а функция вернёт количество записанных символов.
// В случае ошибки или если размер буфера недостаточен, функция вернёт -1. Если buffer или bufferSize равен 0, то функция
// вернёт необходимый размер буфера (в символах). Функция не добавляет в buffer терминирующий 0
int FromAnsi(wchar_t* buffer, size_t bufferSize, std::string_view str);

// Преобразует Wide строку в строку Ansi, используя текущую системную локаль
std::string ToAnsi(std::wstring_view str);

// Преобразует Unicode (Wide) строку str в строку Ansi и сохраняет её в buffer. Параметр bufferSize задаёт размер буфера
// (в символах). Если размер достаточен, то в буфер будет записана строка, а функция вернёт количество записанных символов.
// В случае ошибки или если размер буфера недостаточен, функция вернёт -1. Если buffer или bufferSize равен 0, то функция
// вернёт необходимый размер буфера (в символах). Функция не добавляет в buffer терминирующий 0
int ToAnsi(char* buffer, size_t bufferSize, std::wstring_view str);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Конвертация строк UTF-8/Wide
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Преобразует UTF-8 строку в строку Wide
std::wstring FromUtf8(std::string_view str);

// Преобразует UTF-8 строку str в строку Unicode (Wide) и сохраняет её в buffer. Параметр bufferSize задаёт размер буфера
// (в символах). Если размер достаточен, то в буфер будет записана строка, а функция вернёт количество записанных символов.
// В случае ошибки или если размер буфера недостаточен, функция вернёт -1. Если buffer или bufferSize равен 0, то функция
// вернёт необходимый размер буфера (в символах). Функция не добавляет в buffer терминирующий 0
int FromUtf8(wchar_t* buffer, size_t bufferSize, std::string_view str);

// Преобразует Wide строку в строку UTF-8
std::string ToUtf8(std::wstring_view str);

// Преобразует Unicode (Wide) строку str в строку UTF-8 и сохраняет её в buffer. Параметр bufferSize задаёт размер буфера
// (в символах). Если размер достаточен, то в буфер будет записана строка, а функция вернёт количество записанных символов.
// В случае ошибки или если размер буфера недостаточен, функция вернёт -1. Если buffer или bufferSize равен 0, то функция
// вернёт необходимый размер буфера (в символах). Функция не добавляет в buffer терминирующий 0
int ToUtf8(char* buffer, size_t bufferSize, std::wstring_view str);

} // namespace util

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Отсутствующие в C++17 бинарные операторы + для классов std::[w]string и std::[w]string_view (-ish)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace std {

//--------------------------------------------------------------------------------------------------------------------------------
template<class CharT, class Traits, class Alloc, class StringViewIsh,
	class = util::IsStringViewIsh<StringViewIsh, CharT, Traits>>
inline basic_string<CharT, Traits, Alloc> operator +(
	const basic_string<CharT, Traits, Alloc>& lhs,
	const StringViewIsh& rhs)
{
	const basic_string_view<CharT, Traits> sv = rhs;
	basic_string<CharT, Traits, Alloc> result;
	result.reserve(lhs.size() + sv.size());
	result.assign(lhs);
	result.append(sv.data(), sv.size());
	return result;
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class CharT, class Traits, class Alloc, class StringViewIsh,
	class = util::IsStringViewIsh<StringViewIsh, CharT, Traits>>
inline basic_string<CharT, Traits, Alloc> operator +(
	const StringViewIsh& lhs,
	const basic_string<CharT, Traits, Alloc>& rhs)
{
	const basic_string_view<CharT, Traits> sv = lhs;
	basic_string<CharT, Traits, Alloc> result;
	result.reserve(sv.size() + rhs.size());
	result.assign(sv.data(), sv.size());
	result.append(rhs);
	return result;
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class CharT, class Traits, class Alloc, class StringViewIsh,
	class = util::IsStringViewIsh<StringViewIsh, CharT, Traits>>
inline basic_string<CharT, Traits, Alloc> operator +(
	basic_string<CharT, Traits, Alloc>&& lhs,
	const StringViewIsh& rhs)
{
	const basic_string_view<CharT, Traits> sv = rhs;
	return move(lhs.append(sv.data(), sv.size()));
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class CharT, class Traits, class Alloc, class StringViewIsh,
	class = util::IsStringViewIsh<StringViewIsh, CharT, Traits>>
inline basic_string<CharT, Traits, Alloc> operator +(
	const StringViewIsh& lhs,
	basic_string<CharT, Traits, Alloc>&& rhs)
{
	const basic_string_view<CharT, Traits> sv = lhs;
	return move(rhs.insert(0, sv.data(), sv.size()));
}

} // namespace std
