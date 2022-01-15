//∙AML
// Copyright (C) 2019-2021 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "platform.h"

#include <string>
#include <string.h>
#include <string_view>
#include <type_traits>

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
	static_assert(std::is_same_v<CharT, typename Traits::char_type>, "Bad Traits::char_type");

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
	explicit constexpr BasicZStringView(bool) noexcept {}
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
//   ZString - расширенная версия BasicZStringView с возможностью инициализации из std::[w]string_view
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Класс ZString имеет внутренний буфер небольшого размера (задаётся параметром ssoSize шаблона), в который может быть
// помещена строка при инициализации из std::[w]string_view или пары <указатель, длина>. Если эта строка исходного вью
// длиннее, чем ssoSize, то в куче выделяется массив нужной длины и строка копируется в него. При инициализации из
// указателя или строки std::[w]string буфер не используется (в этом случае ZString работает как BasicZStringView)

//--------------------------------------------------------------------------------------------------------------------------------
template<class CharT, size_t ssoSize = 16>
class ZString : public BasicZStringView<CharT>
{
	static_assert(ssoSize >= 8, "SSO buffer's size must be >= 8");

public:
	using const_pointer = const CharT*;
	using size_type = size_t;

	// Инициализирует view пустой null-terminated строкой
	constexpr ZString() noexcept
	{
		m_Buffer = nullptr;
	}

	// При инициализации из другого view мы выполняем глубокое копирование с выделением памяти
	// при необходимости. После инициализации объект that никак не будет связан с нашим view
	ZString(const ZString& that)
		: Base(that)
	{
		m_Buffer = nullptr;
		if (Base::m_Data == that.m_Inner || that.m_Buffer)
			Base::m_Data = InitCopy(Base::m_Data, Base::m_Size);
	}

	ZString(ZString&& that) noexcept
		: Base(false)
	{
		that.MoveTo(*this);
	}

	// Инициализирует view из указателя на null-terminated строку, в том числе из строкового
	// литерала (что обходится очень дёшево). Для обычных указателей вычисляется длина строки
	constexpr ZString(const_pointer str) noexcept
		: Base(str)
	{
		m_Buffer = nullptr;
	}

	// Инициализирует view из указателя и количества символов. Как и при иницилизации из std::[w]string_view
	// приводит к копированию символов (во внутренний буфер, если строка короче ssoSize) и выделению памяти
	// в куче (если длина исходной строки равна ssoSize символов или больше)
	ZString(const_pointer str, size_type count)
		: Base(InitCopy(str, count), count)
	{
	}

	// Инициализирует view из std::[w]string_view. Самый дорогой способ инициализации, который всегда
	// приводит к копированию символов (во внутренний буфер, если строка короче ssoSize) и выделению
	// памяти в куче (если длина исходной строки равна ssoSize символов или больше)
	explicit ZString(std::basic_string_view<CharT> str)
		: ZString(str.data(), str.size())
	{
	}

	// Инициализирует view из строки std::[w]string (обходится очень дёшево)
	ZString(const std::basic_string<CharT>& str) noexcept
		: Base(str)
	{
		m_Buffer = nullptr;
	}

	~ZString() noexcept
	{
		Tidy();
	}

	// При копировании из другого view мы выполняем глубокое копирование с выделением памяти
	// при необходимости. После копирования объект that никак не будет связан с нашим view
	ZString& operator =(const ZString& that)
	{
		if (this != &that)
		{
			auto oldBuffer = m_Buffer;
			if (that.m_Data == that.m_Inner || that.m_Buffer)
				Base::m_Data = InitCopy(that.m_Data, that.m_Size);
			else
			{
				Base::m_Data = that.m_Data;
				m_Buffer = nullptr;
			}
			if (Base::m_Size >= ssoSize && oldBuffer)
				delete[] oldBuffer;
			Base::m_Size = that.m_Size;
		}
		return *this;
	}

	ZString& operator =(ZString&& that) noexcept
	{
		if (this != &that)
		{
			Tidy();
			that.MoveTo(*this);
		}
		return *this;
	}

	ZString& operator =(const_pointer str)
	{
		Tidy();
		Base::m_Data = str;
		Base::m_Size = Base::traits_type::length(str);
		m_Buffer = nullptr;

		return *this;
	}

	ZString& operator =(std::basic_string_view<CharT> str)
	{
		auto oldBuffer = m_Buffer;
		const size_type newSize = str.size();
		Base::m_Data = InitCopy(str.data(), newSize);
		if (Base::m_Size >= ssoSize && oldBuffer)
			delete[] oldBuffer;
		Base::m_Size = newSize;

		return *this;
	}

	ZString& operator =(const std::basic_string<CharT>& str) noexcept
	{
		Tidy();
		Base::m_Data = str.c_str();
		Base::m_Size = str.size();
		m_Buffer = nullptr;

		return *this;
	}

protected:
	using Base = BasicZStringView<CharT>;

	void Tidy() noexcept
	{
		if (Base::m_Size >= ssoSize && m_Buffer)
			delete[] m_Buffer;
	}

	AML_NOINLINE const_pointer InitCopy(const_pointer str, size_type count)
	{
		CharT* out = m_Inner;
		if (count >= ssoSize)
		{
			out = new CharT[count + 1];
			m_Buffer = out;
		}
		memcpy(out, str, count * sizeof(CharT));
		out[count] = 0;
		return out;
	}

	AML_NOINLINE void MoveTo(ZString& dest) const noexcept
	{
		dest.m_Size = Base::m_Size;
		if (Base::m_Data != m_Inner)
		{
			dest.m_Data = Base::m_Data;
			dest.m_Buffer = m_Buffer;

			m_Buffer = nullptr;
			Base::m_Data = Base::s_Zero;
			Base::m_Size = 0;
		} else
		{
			dest.m_Data = dest.m_Inner;
			// NB: если размер m_Inner кратен 8 байтам, то быстрее копировать весь массив целиком. В
			// этом случае компилятор заменит вызов memcpy всего несколькими ассемблерными командами
			memcpy(dest.m_Inner, m_Inner, (sizeof(m_Inner) <= 48) ?
				sizeof(m_Inner) : (Base::m_Size + 1) * sizeof(CharT));
		}
	}

protected:
	union {
		CharT* m_Buffer;		// Указатель на массив, выделенный в куче (если m_Data != m_Inner)
		CharT m_Inner[ssoSize];	// Внутренний буфер SSO
	};
};

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
