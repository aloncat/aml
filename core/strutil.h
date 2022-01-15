//∙AML
// Copyright (C) 2016-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "exception.h"
#include "platform.h"
#include "util.h"

#include <string>
#include <string.h>
#include <string_view>
#include <type_traits>
#include <utility>

namespace util {

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Вспомогательные шаблоны
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
template<class StringViewIsh, class CharT, class Traits = std::char_traits<CharT>>
using IsStringViewIsh = std::enable_if_t<std::conjunction_v<
	std::is_convertible<const StringViewIsh&, std::basic_string_view<CharT, Traits>>,
	std::negation<std::is_convertible<const StringViewIsh&, const CharT*>>>>;

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

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   StringWriter - вывод символьных данных в строку
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Класс StringWriter - простой контейнер, предназначенный для вывода данных в строку. Хранит данные в виде массива.
// Имеет внутренний буфер небольшого размера (размер в байтах задаётся параметром innerBuf шаблона). Автоматически
// увеличивает свой размер по мере добавления новых данных

//--------------------------------------------------------------------------------------------------------------------------------
template<class CharT, size_t innerBuf = 640>
class StringWriter
{
	AML_NONCOPYABLE(StringWriter)

public:
	constexpr StringWriter() noexcept
		: m_Data(m_Buffer)
		, m_Capacity(INNER_CAPACITY)
	{
	}

	~StringWriter() noexcept
	{
		if (m_Capacity > INNER_CAPACITY)
			delete[] m_Data;
	}

	// Возвращает указатель на первый символ накопленных данных (начало
	// строки с результатом, строка не имеет нулевого символа на конце)
	constexpr const CharT* GetData() const noexcept
	{
		return m_Data;
	}

	// Возвращает размер данных в символах (длину результирующей строки)
	constexpr size_t GetSize() const noexcept
	{
		return m_Size;
	}

	// Возвращает true, если контейнер пуст
	constexpr bool IsEmpty() const noexcept
	{
		return !m_Size;
	}

	// Очищает контейнер
	constexpr void Clear() noexcept
	{
		m_Size = 0;
	}

	// Резервирует буфер для данных размером не менее указанного (в символах)
	void Reserve(size_t capacity)
	{
		if (capacity > m_Capacity)
			Grow(capacity, false);
	}

	// Добавляет символ chr
	void Append(char chr)
	{
		CharT c = chr;
		// Когда тип CharT не совпадает с типом chr, в общем случае требуется преобразование
		// Ansi -> Wide. Если значение chr < 0x80, то оно не изменится при преобразовании.
		// Но если значение выше, то мы заменим исходный символ на символ "�" (U+FFFD)
		if constexpr (!std::is_same_v<CharT, char>)
			c = (chr < 0x80) ? c : 0xfffd;

		if (m_Size < m_Capacity)
			m_Data[m_Size++] = c;
		else
			GrowAndAppend(&c, 1);
	}

	// Добавляет символ chr
	void Append(wchar_t chr)
	{
		CharT c = static_cast<CharT>(chr);
		// Когда тип CharT не совпадает с типом wchar_t, в общем случае требуется преобразование
		// Wide -> Ansi. Если значение chr < 0x80, то оно не изменится при преобразовании. Но
		// если значение выше, то мы заменим исходный символ на символ "?" (U+003F)
		if constexpr (!std::is_same_v<CharT, wchar_t>)
			c = (chr < 0x80) ? c : '?';

		if (m_Size < m_Capacity)
			m_Data[m_Size++] = c;
		else
			GrowAndAppend(&c, 1);
	}

	// Добавляет null-терминированную строку str (используется только для [const] CharT*). В отличие от вывода через вью
	// std::[w]string_view эта функция не вычисляет предварительно длину строки, а делает всю работу в одном цикле. Учитывая
	// неоптимальность реализации std::char_traits::length такой способ даёт существенное преимущество для коротких строк
	template<class T, class = std::enable_if_t<std::is_same_v<T, CharT>>>
	void Append(const T* const& str)
	{
		const CharT* inp = str;
		CharT* out = m_Data + m_Size;
		const auto remains = m_Capacity - m_Size;
		for (size_t i = 0; i < remains; ++i)
		{
			if (auto chr = inp[i])
				out[i] = chr;
			else
			{
				m_Size += i;
				return;
			}
		}
		m_Size += remains;

		inp += remains;
		GrowAndAppend(inp, std::char_traits<CharT>::length(inp));
	}

	// TODO: проверить целесообразность замены memcpy в функциях Append и GrowAndAppend этого класса (а также аналогичных
	// мест в классах MemoryFile и MemoryWriter) своей реализацией для коротких строк. Будет ли своя функция быстрее?

	// Добавляет count первых символов из data
	void Append(const CharT* data, size_t count)
	{
		if (count <= m_Capacity - m_Size)
		{
			memcpy(m_Data + m_Size, data, count * sizeof(CharT));
			m_Size += count;
		} else
		{
			GrowAndAppend(data, count);
		}
	}

	// Добавляет строку str, связанную с вью
	void Append(std::basic_string_view<CharT> str)
	{
		Append(str.data(), str.size());
	}

	// Возвращает строку с копией данных контейнера
	std::basic_string<CharT> ToString() const
	{
		return std::basic_string<CharT>(m_Data, m_Size);
	}

	// Неявно приводит контейнер к типу "std::[w]string_view"
	constexpr operator std::basic_string_view<CharT>() const noexcept
	{
		return std::basic_string_view<CharT>(m_Data, m_Size);
	}

	// Добавляет символ chr (см. Append)
	StringWriter& operator <<(char chr)
	{
		Append(chr);
		return *this;
	}

	// Добавляет символ chr (см. Append)
	StringWriter& operator <<(wchar_t chr)
	{
		Append(chr);
		return *this;
	}

	// Добавляет null-терминированную строку str (см. комментарий к Append)
	template<class T, class = std::enable_if_t<std::is_same_v<T, CharT>>>
	StringWriter& operator <<(const T* const& str)
	{
		Append(str);
		return *this;
	}

	// Добавляет строку str, связанную с вью
	StringWriter& operator <<(std::basic_string_view<CharT> str)
	{
		Append(str.data(), str.size());
		return *this;
	}

protected:
	// Размер внутреннего буфера в символах (нулевое значение
	// является допустимым, но вызовет предупреждение компилятора)
	static constexpr size_t INNER_CAPACITY = innerBuf / sizeof(CharT);

	AML_NOINLINE void GrowAndAppend(const void* data, size_t count)
	{
		Grow(m_Size + count, true);
		memcpy(m_Data + m_Size, data, count * sizeof(CharT));
		m_Size += count;
	}

	void Grow(size_t capacity, bool reserveMore)
	{
		if (capacity > m_Capacity && capacity <= (~size_t(0) / sizeof(CharT)))
		{
			if (reserveMore)
			{
				// До достижения размера в 256 MiB массив становится
				// каждый раз на 1/2 больше запрошенного размера
				if (capacity < 0x10000000 / sizeof(CharT))
					capacity += capacity >> 1;
				else
					capacity = GetHugeSize(capacity);
			}

			auto old = m_Data;
			m_Data = new CharT[capacity];
			memcpy(m_Data, old, m_Size * sizeof(CharT));
			if (m_Capacity > INNER_CAPACITY)
				delete[] old;

			m_Capacity = capacity;
		} else
		{
			// При переполнении (когда capacity = m_Size + count не помещается в переменной size_t), а также
			// если такое значение capacity приведёт к переполнению размера в байтах, выбросим исключение
			throw ERuntime("Too big array size for StringWriter");
		}
	}

	size_t GetHugeSize(size_t capacity) noexcept
	{
		// Свыше 256 MiB массив становится каждый раз больше на ~128 MiB
		if (sizeof(size_t) == 8 || capacity < 0xf4000000 / sizeof(CharT))
		{
			capacity *= sizeof(CharT);
			capacity = (capacity + 0x7ffffff) & (~size_t(0) << 16);
			return capacity / sizeof(CharT);
		}
		return ~size_t(0) / sizeof(CharT);
	}

protected:
	CharT* m_Data = nullptr;	// Указатель на начало массива данных (строки)
	size_t m_Capacity = 0;		// Размер массива, на который указывает m_Data
	size_t m_Size = 0;			// Количество символов в результирующей строке

	// Внутренний буфер для выводимых данных
	CharT m_Buffer[INNER_CAPACITY];
};

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
