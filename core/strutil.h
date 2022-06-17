//∙AML
// Copyright (C) 2016-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "exception.h"
#include "platform.h"
#include "strcommon.h"
#include "util.h"

#include <string>
#include <string.h>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

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
//   Сравнение строк (вспомогательные макросы)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define AML_STRCMP_OVERLOADS(FN, T) \
	inline int FN(const std::basic_string<T>& strA, const T* strB) { return FN(strA.c_str(), strB); } \
	inline int FN(const T* strA, const std::basic_string<T>& strB) { return FN(strA, strB.c_str()); }

#define AML_STRNCMP_OVERLOADS(FN, T) \
	inline int FN(const std::basic_string<T>& strA, const T* strB, size_t count) { return FN(strA.c_str(), strB, count); } \
	inline int FN(const T* strA, const std::basic_string<T>& strB, size_t count) { return FN(strA, strB.c_str(), count); }

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Сравнение строк: StrCmp
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Функция StrCmp идентична стандартным функциям strcmp/wcscmp, лексикографически сравнивает строку "A" со
// строкой "B". Если строки одинаковы, функция вернёт 0. Если строка "A" меньше строки "B", функция вернёт
// отрицательное число <=-1. Если строка "A" больше строки "B", то функция вернёт положительное число >=1

int StrCmp(const char* strA, const char* strB);
int StrCmp(const wchar_t* strA, const wchar_t* strB);
int StrCmp(std::string_view strA, std::string_view strB);
int StrCmp(std::wstring_view strA, std::wstring_view strB);
int StrCmp(const std::string& strA, const std::string& strB);
int StrCmp(const std::wstring& strA, const std::wstring& strB);

AML_STRCMP_OVERLOADS(StrCmp, char)
AML_STRCMP_OVERLOADS(StrCmp, wchar_t)

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Сравнение строк: StrInsCmp
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Функция StrInsCmp похожа на POSIX-функции strcasecmp/wcscasecmp, лексикографически сравнивает строку "A"
// со строкой "B" без учёта регистра, но работает так, как если бы была установлена "C" локаль, т.е. регистр
// игнорируется только для латинских букв 'a'..'z' и 'A'..'Z', а для остальных символов поведение этой функции
// идентично StrCmp. Если строки одинаковы, функция вернёт 0. Если строка "A" меньше строки "B", функция вернёт
// отрицательное число <=-1. Если строка "A" больше строки "B", то функция вернёт положительное число >=1

int StrInsCmp(const char* strA, const char* strB);
int StrInsCmp(const wchar_t* strA, const wchar_t* strB);
int StrInsCmp(std::string_view strA, std::string_view strB);
int StrInsCmp(std::wstring_view strA, std::wstring_view strB);
int StrInsCmp(const std::string& strA, const std::string& strB);
int StrInsCmp(const std::wstring& strA, const std::wstring& strB);

AML_STRCMP_OVERLOADS(StrInsCmp, char)
AML_STRCMP_OVERLOADS(StrInsCmp, wchar_t)

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Сравнение строк: StrCaseCmp
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Функция StrCaseCmp идентична POSIX-функциям strcasecmp/wcscasecmp, лексикографически сравнивает строку "A"
// со строкой "B" без учёта регистра, используя текущую локаль. Для этого соответствующие символы обеих строк
// сначала приводятся к нижнему регистру с помощью функции tolower/towlower, после чего символы сравниваются.
// Если строки одинаковы, функция вернёт 0. Если строка "A" меньше строки "B", функция вернёт отрицательное
// число <=-1. Если строка "A" больше строки "B", то функция вернёт положительное число >=1

// TODO: нужна реализация для остальных функций
int StrCaseCmp(const char* strA, const char* strB);
int StrCaseCmp(const wchar_t* strA, const wchar_t* strB);
//int StrCaseCmp(std::string_view strA, std::string_view strB);
//int StrCaseCmp(std::wstring_view strA, std::wstring_view strB);
//int StrCaseCmp(const std::string& strA, const std::string& strB);
//int StrCaseCmp(const std::wstring& strA, const std::wstring& strB);

AML_STRCMP_OVERLOADS(StrCaseCmp, char)
AML_STRCMP_OVERLOADS(StrCaseCmp, wchar_t)

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Сравнение строк: StrNCmp
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Функция StrNCmp идентична стандартным функциям strncmp/wcsncmp, лексикографически сравнивает строку "A" со
// строкой "B", сравнивая не более count их первых символов. Т.е. если длина строки "A" или строки "B" больше
// count, то функция считает, что соответствующая строка содержит только count её первых символов. Если
// строки одинаковы, функция вернёт 0. Если строка "A" меньше строки "B", функция вернёт отрицательное
// число <=-1. Если строка "A" больше строки "B", то функция вернёт положительное число >=1

int StrNCmp(const char* strA, const char* strB, size_t count);
int StrNCmp(const wchar_t* strA, const wchar_t* strB, size_t count);
int StrNCmp(std::string_view strA, std::string_view strB, size_t count);
int StrNCmp(std::wstring_view strA, std::wstring_view strB, size_t count);
int StrNCmp(const std::string& strA, const std::string& strB, size_t count);
int StrNCmp(const std::wstring& strA, const std::wstring& strB, size_t count);

AML_STRNCMP_OVERLOADS(StrNCmp, char)
AML_STRNCMP_OVERLOADS(StrNCmp, wchar_t)

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Сравнение строк: StrNInsCmp
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Функция StrNInsCmp идентична функции StrInsCmp с той лишь разницей, что сравнивает не более count первых символов
// указанных строк. Функция сравнивает строки лексикографически без учёта регистра, работая так, как если бы была
// установлена "C" локаль. Если строки одинаковы (совпадают до регистра первые count символов строк в случае, когда
// одна или обе строки длиннее count символов), то функция вернёт 0. Если строка "A" меньше строки "B", функция
// вернёт отрицательное число <=-1. Если строка "A" больше строки "B", то функция вернёт положительное число >=1

int StrNInsCmp(const char* strA, const char* strB, size_t count);
int StrNInsCmp(const wchar_t* strA, const wchar_t* strB, size_t count);
int StrNInsCmp(std::string_view strA, std::string_view strB, size_t count);
int StrNInsCmp(std::wstring_view strA, std::wstring_view strB, size_t count);
int StrNInsCmp(const std::string& strA, const std::string& strB, size_t count);
int StrNInsCmp(const std::wstring& strA, const std::wstring& strB, size_t count);

AML_STRNCMP_OVERLOADS(StrNInsCmp, char)
AML_STRNCMP_OVERLOADS(StrNInsCmp, wchar_t)

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Сравнение строк: StrNCaseCmp
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Функция StrNCaseCmp идентична функции StrCaseCmp с той лишь разницей, что сравнивает не более count первых символов
// указанных строк. Функция сравнивает строки лексикографически без учёта регистра, используя текущую локаль. Если строки
// одинаковы (совпадают до регистра первые count символов строк в случае, когда одна или обе строки длиннее count символов),
// то функция вернёт 0. Если строка "A" меньше строки "B", функция вернёт отрицательное число <=-1. Если строка "A" больше
// строки "B", то функция вернёт положительное число >=1

// TODO: нужна реализация для остальных функций
int StrNCaseCmp(const char* strA, const char* strB, size_t count);
int StrNCaseCmp(const wchar_t* strA, const wchar_t* strB, size_t count);
//int StrNCaseCmp(std::string_view strA, std::string_view strB, size_t count);
//int StrNCaseCmp(std::wstring_view strA, std::wstring_view strB, size_t count);
//int StrNCaseCmp(const std::string& strA, const std::string& strB, size_t count);
//int StrNCaseCmp(const std::wstring& strA, const std::wstring& strB, size_t count);

AML_STRNCMP_OVERLOADS(StrNCaseCmp, char)
AML_STRNCMP_OVERLOADS(StrNCaseCmp, wchar_t)

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Сравнение строк (реализация)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
inline int StrCmp(const char* strA, const char* strB)
{
	// TODO: возможно, есть смысл попытаться написать свою реализацию этой функции (например, в случае,
	// если строки выровнены, сравнивать по 4/8 байт за раз, что должно быть быстрее - но тут важно,
	// как хорошо функции реализованы на других платформах, чтобы не сделать хуже)
	return strcmp(strA, strB);
}

//--------------------------------------------------------------------------------------------------------------------------------
inline int StrCmp(const wchar_t* strA, const wchar_t* strB)
{
	// TODO: см. комментарий в предыдущей функции
	return wcscmp(strA, strB);
}

//--------------------------------------------------------------------------------------------------------------------------------
inline int StrCmp(std::string_view strA, std::string_view strB)
{
	// Реализация в STL (char_traits) довольно неплоха.
	// Можно сделать лучше, но это будет непросто
	return strA.compare(strB);
}

//--------------------------------------------------------------------------------------------------------------------------------
inline int StrCmp(std::wstring_view strA, std::wstring_view strB)
{
	// См. комментарий в предыдущей функции
	return strA.compare(strB);
}

//--------------------------------------------------------------------------------------------------------------------------------
inline int StrCmp(const std::string& strA, const std::string& strB)
{
	// См. комментарий в предыдущей функции
	return strA.compare(strB);
}

//--------------------------------------------------------------------------------------------------------------------------------
inline int StrCmp(const std::wstring& strA, const std::wstring& strB)
{
	// См. комментарий в предыдущей функции
	return strA.compare(strB);
}

//--------------------------------------------------------------------------------------------------------------------------------
inline int StrNCmp(const char* strA, const char* strB, size_t count)
{
	// TODO: см. комментарий к StrCmp
	return strncmp(strA, strB, count);
}

//--------------------------------------------------------------------------------------------------------------------------------
inline int StrNCmp(const wchar_t* strA, const wchar_t* strB, size_t count)
{
	// TODO: см. комментарий к StrCmp
	return wcsncmp(strA, strB, count);
}

//--------------------------------------------------------------------------------------------------------------------------------
inline int StrNCmp(std::string_view strA, std::string_view strB, size_t count)
{
	// TODO: см. комментарий к соответствующей функции StrCmp
	return strA.compare(0, count, strB, 0, count);
}

//--------------------------------------------------------------------------------------------------------------------------------
inline int StrNCmp(std::wstring_view strA, std::wstring_view strB, size_t count)
{
	// TODO: см. комментарий к соответствующей функции StrCmp
	return strA.compare(0, count, strB, 0, count);
}

//--------------------------------------------------------------------------------------------------------------------------------
inline int StrNCmp(const std::string& strA, const std::string& strB, size_t count)
{
	// TODO: см. комментарий к соответствующей функции StrCmp
	return strA.compare(0, count, strB, 0, count);
}

//--------------------------------------------------------------------------------------------------------------------------------
inline int StrNCmp(const std::wstring& strA, const std::wstring& strB, size_t count)
{
	// TODO: см. комментарий к соответствующей функции StrCmp
	return strA.compare(0, count, strB, 0, count);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Функции преобразования регистра
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Функции LoCase и UpCase переводят все символы строки str в нижний (LoCase) или верхний (UpCase) регистр. Функции используют
// текущую локаль программы, если noLocale == false. Если параметр noLocale == true, то функции работают так, как если бы была
// установлена "C" локаль, т.е. регистр символов изменяется только для латинских букв 'a'..'z' (для UpCase) и 'A'..'Z' (для
// LoCase), а остальные символы не меняются. Значение параметра noLocale влияет на скорость работы функции: при значении
// true скорость всегда значительно выше, особенно в случае, если локаль программы менялась. ПРИМЕЧАНИЕ: эти функции
// всегда выполняют преобразование 1:1, т.е. например, из немецкой ß (нижний регистр) нельзя получить SS (верхний)
std::string LoCase(std::string_view str, bool noLocale = false);
std::wstring LoCase(std::wstring_view str, bool noLocale = false);
std::string UpCase(std::string_view str, bool noLocale = false);
std::wstring UpCase(std::wstring_view str, bool noLocale = false);

// Функции *CaseInplace аналогичны функциям LoCase/UpCase, но изменяют непосредственно строку str, не возвращая результат.
// Эти функции быстрее соответствующих *Case функций и для них справедливо то же замечание о скорости работы в зависимости
// от значения параметра noLocale. ПРИМЕЧАНИЕ: функции всегда выполняют преобразование 1:1 (см. примечание выше)
void LoCaseInplace(std::string& str, bool noLocale = false);
void LoCaseInplace(std::wstring& str, bool noLocale = false);
void UpCaseInplace(std::string& str, bool noLocale = false);
void UpCaseInplace(std::wstring& str, bool noLocale = false);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Функции Trim
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Функция Trim удаляет из строки str начальные и конечные пробелы и символы
// табуляции. TrimLeft удаляет только начальные, TrimRight - только конечные
std::string Trim(std::string_view str);
std::wstring Trim(std::wstring_view str);
std::string TrimLeft(std::string_view str);
std::wstring TrimLeft(std::wstring_view str);
std::string TrimRight(std::string_view str);
std::wstring TrimRight(std::wstring_view str);

// Функции Trim*Inplace отличаются от обычных функций Trim* тем, что изменяют непосредственно строку str
// и не возвращают результат. Параметр fast позволяет выбрать способ модификации строки: если он равен
// true, то будет использован быстрый метод без изменения размера памяти, занимаемой строкой; если
// параметр fast равен false, то по возможности объём пямяти, занимаемый строкой, будет уменьшен
void TrimInplace(std::string& str, bool fast = true);
void TrimInplace(std::wstring& str, bool fast = true);
void TrimLeftInplace(std::string& str, bool fast = true);
void TrimLeftInplace(std::wstring& str, bool fast = true);
void TrimRightInplace(std::string& str, bool fast = true);
void TrimRightInplace(std::wstring& str, bool fast = true);

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
//   Функция Split
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum {
	SPLIT_TRIM					= 0x01,						// Применить к подстрокам функцию Trim
	SPLIT_ALLOW_EMPTY			= 0x02,						// Включать в результат пустые подстроки
	SPLIT_TRAILING_DELIMITER	= 0x04 + SPLIT_ALLOW_EMPTY	// Трактовать разделитель на конце str как пустую подстроку
};

// Разбивает исходную строку str на подстроки, используя delimiters как набор символов-разделителей
std::vector<std::string> Split(std::string_view str, ZStringView delimiters, int flags = SPLIT_TRIM);
std::vector<std::wstring> Split(std::wstring_view str, WZStringView delimiters, int flags = SPLIT_TRIM);

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
