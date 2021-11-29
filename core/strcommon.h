//∙AML
// Copyright (C) 2019-2021 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "platform.h"

#include <string>
#include <string_view>

namespace util {

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   BasicZStringView - view-контейнер для строк, гарантирующий null-терминированность хранимого view
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Шаблонный класс BasicZStringView - это упрощённый аналог std::basic_string_view, который хранит вью исключительно
// null-терминированных строк. Поэтому его можно проинициализировать только из указателя (в т.ч. строкового литерала)
// или строки std::[w]string. Этот класс реализует только самый простой функционал (функции empty, size, data/c_str)

//--------------------------------------------------------------------------------------------------------------------------------
template<class CharT, class Traits = std::char_traits<CharT>>
class BasicZStringView
{
public:
	using traits_type = Traits;
	using const_pointer = const CharT*;
	using size_type = size_t;

	// Инициализирует view пустой null-terminated строкой
	constexpr BasicZStringView() noexcept
		: m_Data(&s_Zero)
		, m_Size(0)
	{
	}

	// Инициализирует view из указателя на null-terminated строку, в том числе
	// из строкового литерала. Для обычных указателей вычисляется длина строки
	constexpr BasicZStringView(const_pointer str) noexcept
		: m_Data(str)
		, m_Size(traits_type::length(str))
	{
	}

	// Инициализирует view из строки std::[w]string
	BasicZStringView(const std::basic_string<CharT>& str) noexcept
		: m_Data(str.c_str())
		, m_Size(str.size())
	{
	}

	constexpr BasicZStringView& operator =(const_pointer str) noexcept
	{
		m_Data = str;
		m_Size = traits_type::length(str);
		return *this;
	}

	BasicZStringView& operator =(const std::basic_string<CharT>& str) noexcept
	{
		m_Data = str.c_str();
		m_Size = str.size();
		return *this;
	}

	// Возвращает указатель на начало строки
	constexpr const_pointer data() const noexcept
	{
		return m_Data;
	}

	// Возвращает указатель на null-terminated строку
	constexpr const_pointer c_str() const noexcept
	{
		return m_Data;
	}

	// Возвращает длину строки в символах
	constexpr size_type size() const noexcept
	{
		return m_Size;
	}

	// Возвращает true, если строка пуста
	constexpr bool empty() const noexcept
	{
		return !m_Size;
	}

	// Неявно приводит контейнер к типу "std::[w]string_view"
	constexpr operator std::basic_string_view<CharT>() const noexcept
	{
		return std::basic_string_view<CharT>(m_Data, m_Size);
	}

protected:
	constexpr BasicZStringView(const_pointer str, size_type size) noexcept
		: m_Data(str)
		, m_Size(size)
	{
	}

	static inline const CharT s_Zero = 0;

	const_pointer m_Data;	// Указатель на первый символ null-terminated строки
	size_type m_Size;		// Количество символов в строке
};

using ZStringView = BasicZStringView<char>;
using WZStringView = BasicZStringView<wchar_t>;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   EMPTY - универсальная константа пустой строки
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
struct EmptyStringContainer final
{
	// NB: согласно стандарту, дефолтный конструктор, если он удовлетворяет требованиям constexpr,
	// автоматически будет constexpr. Однако явное указание спецификатора заставит компилятор
	// сгенерировать ошибку, если эти требования окажутся по какой-то причине неудовлетворены
	constexpr EmptyStringContainer() = default;

	constexpr const std::string& Ansi() const noexcept { return s_String; }
	constexpr const std::string& Utf8() const noexcept { return s_String; }
	constexpr const std::wstring& Wide() const noexcept { return s_WString; }

	constexpr operator const char*() const noexcept { return ""; }
	constexpr operator const wchar_t*() const noexcept { return L""; }
	constexpr operator ZStringView() const noexcept { return ZStringView(); }
	constexpr operator WZStringView() const noexcept { return WZStringView(); }
	constexpr operator const std::string&() const noexcept { return s_String; }
	constexpr operator const std::wstring&() const noexcept { return s_WString; }
	constexpr operator std::string_view() const noexcept { return std::string_view("", 0); }
	constexpr operator std::wstring_view() const noexcept { return std::wstring_view(L"", 0); }

private:
	const void* m_Dummy = nullptr;

	static const std::string s_String;
	static const std::wstring s_WString;
};

extern const EmptyStringContainer EMPTY;

} // namespace util
