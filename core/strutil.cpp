//∙AML
// Copyright (C) 2016-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "strutil.h"

#include "array.h"
#include "winapi.h"

#include <ctype.h>

namespace util {

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   EMPTY - члены данных
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Так как у strcommon.h нет соответствующего .cpp, то мы разместили статические
// члены данных класса EmptyStringContainer и саму константу EMPTY здесь

const std::string EmptyStringContainer::s_String;
const std::wstring EmptyStringContainer::s_WString;

const EmptyStringContainer EMPTY;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Вспомогательные lookup таблицы для функций Str*InsCmp, LoCase* и UpCase*
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
struct caseTT final
{
	// Lookup таблица для преобразования латинских букв нижнего регистра к верхнему (вычитание). Первые 32
	// байта нулевые, далее значения совпадают с началом массива down: значение 0x20 в индексах 0x61-0x7a
	static inline const uint8_t up[32] = {};

	// Lookup таблица для преобразования латинских букв верхнего регистра к нижнему (сложение).
	// Все байты массива равны 0, кроме индексов 0x41-0x5a, в которых значение равно 0x20
	static inline const uint8_t down[256] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
		32, 32, 32
	};
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Сравнение строк
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
int StrInsCmp(const char* strA, const char* strB)
{
	int a, b;
	do {
		a = static_cast<unsigned char>(*strA++);
		a += caseTT::down[a];
		b = static_cast<unsigned char>(*strB++);
		b += caseTT::down[b];
	} while (b && a == b);
	return a - b;
}

//--------------------------------------------------------------------------------------------------------------------------------
int StrInsCmp(const wchar_t* strA, const wchar_t* strB)
{
	// В целях оптимизации мы полагаем, что тип wchar_t беззнаковый. Если это не так и при
	// этом размер типа wchar_t меньше размера переменных a и b, то функция может работать
	// некорректно, если строки содержат символы, у которых значения старших битов равны 1
	static_assert(std::is_unsigned_v<wchar_t> || sizeof(wchar_t) == sizeof(unsigned),
		"Type wchar_t must be unsigned or same-sized as unsigned (int) type");

	unsigned a, b;
	do {
		a = *strA++;
		if (a <= 0x7f)
			a += caseTT::down[a];
		b = *strB++;
		if (b <= 0x7f)
			b += caseTT::down[b];
	} while (a && a == b);

	return (sizeof(wchar_t) < sizeof(a)) ? static_cast<int>(a - b) :
		(a == b) ? 0 : (a < b) ? -1 : 1;
}

//--------------------------------------------------------------------------------------------------------------------------------
static int StrInsCmpImpl(const char* strA, const char* strB, size_t count)
{
	if (!count)
		return 0;

	int a, b;
	do {
		a = static_cast<unsigned char>(*strA++);
		a += caseTT::down[a];
		b = static_cast<unsigned char>(*strB++);
		b += caseTT::down[b];
	} while (a == b && --count);
	return a - b;
}

//--------------------------------------------------------------------------------------------------------------------------------
static int StrInsCmpImpl(const wchar_t* strA, const wchar_t* strB, size_t count)
{
	// См. комментарий к функции StrInsCmp(wchar_t, wchar_t)
	static_assert(std::is_unsigned_v<wchar_t> || sizeof(wchar_t) == sizeof(unsigned),
		"Type wchar_t must be unsigned or same-sized as unsigned (int) type");

	if (!count)
		return 0;

	unsigned a, b;
	do {
		a = *strA++;
		if (a <= 0x7f)
			a += caseTT::down[a];
		b = *strB++;
		if (b <= 0x7f)
			b += caseTT::down[b];
	} while (--count && a == b);

	return (sizeof(wchar_t) < sizeof(a)) ? static_cast<int>(a - b) :
		(a == b) ? 0 : (a < b) ? -1 : 1;
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class StringViewIsh>
static int StrInsCmpImpl(const StringViewIsh& strA, const StringViewIsh& strB)
{
	const size_t sizeA = strA.size();
	const size_t sizeB = strB.size();

	const size_t count = (sizeA < sizeB) ? sizeA : sizeB;
	int result = StrInsCmpImpl(strA.data(), strB.data(), count);
	return (result || sizeA == sizeB) ? result : (sizeA < sizeB) ? -1 : 1;
}

//--------------------------------------------------------------------------------------------------------------------------------
int StrInsCmp(std::string_view strA, std::string_view strB)
{
	return StrInsCmpImpl(strA, strB);
}

//--------------------------------------------------------------------------------------------------------------------------------
int StrInsCmp(std::wstring_view strA, std::wstring_view strB)
{
	return StrInsCmpImpl(strA, strB);
}

//--------------------------------------------------------------------------------------------------------------------------------
int StrInsCmp(const std::string& strA, const std::string& strB)
{
	return StrInsCmpImpl(strA, strB);
}

//--------------------------------------------------------------------------------------------------------------------------------
int StrInsCmp(const std::wstring& strA, const std::wstring& strB)
{
	return StrInsCmpImpl(strA, strB);
}

//--------------------------------------------------------------------------------------------------------------------------------
int StrCaseCmp(const char* strA, const char* strB)
{
	#if AML_OS_WINDOWS
		// TODO: это MS-specific функция. Возможно, её стоит заменить своей реализацией: цикл сравнения
		// с приведением каждого символа обеих строк к нижнему регистру с помощью функции tolower
		return _stricmp(strA, strB);
	#else
		return strcasecmp(strA, strB);
	#endif
}

//--------------------------------------------------------------------------------------------------------------------------------
int StrCaseCmp(const wchar_t* strA, const wchar_t* strB)
{
	#if AML_OS_WINDOWS
		// TODO: это MS-specific функция. Возможно, её стоит заменить своей реализацией: цикл сравнения
		// с приведением каждого символа обеих строк к нижнему регистру с помощью функции towlower
		return _wcsicmp(strA, strB);
	#else
		return wcscasecmp(strA, strB);
	#endif
}

//--------------------------------------------------------------------------------------------------------------------------------
int StrNInsCmp(const char* strA, const char* strB, size_t count)
{
	if (!count)
		return 0;

	int a, b;
	do {
		a = static_cast<unsigned char>(*strA++);
		a += caseTT::down[a];
		b = static_cast<unsigned char>(*strB++);
		b += caseTT::down[b];
	} while (b && a == b && --count);
	return a - b;
}

//--------------------------------------------------------------------------------------------------------------------------------
int StrNInsCmp(const wchar_t* strA, const wchar_t* strB, size_t count)
{
	// См. комментарий к функции StrInsCmp(wchar_t, wchar_t)
	static_assert(std::is_unsigned_v<wchar_t> || sizeof(wchar_t) == sizeof(unsigned),
		"Type wchar_t must be unsigned or same-sized as unsigned (int) type");

	if (!count)
		return 0;

	unsigned a, b;
	do {
		a = *strA++;
		if (a <= 0x7f)
			a += caseTT::down[a];
		b = *strB++;
		if (b <= 0x7f)
			b += caseTT::down[b];
	} while (--count && a && a == b);

	return (sizeof(wchar_t) < sizeof(a)) ? static_cast<int>(a - b) :
		(a == b) ? 0 : (a < b) ? -1 : 1;
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class StringViewIsh>
static int StrNInsCmpImpl(const StringViewIsh& strA, const StringViewIsh& strB, size_t count)
{
	const size_t sizeA = strA.size();
	const size_t sizeB = strB.size();

	if (count <= sizeA && count <= sizeB)
	{
		return StrInsCmpImpl(strA.data(), strB.data(), count);
	}

	count = (sizeA < sizeB) ? sizeA : sizeB;
	int result = StrInsCmpImpl(strA.data(), strB.data(), count);
	return (result || sizeA == sizeB) ? result : (sizeA < sizeB) ? -1 : 1;
}

//--------------------------------------------------------------------------------------------------------------------------------
int StrNInsCmp(std::string_view strA, std::string_view strB, size_t count)
{
	return StrNInsCmpImpl(strA, strB, count);
}

//--------------------------------------------------------------------------------------------------------------------------------
int StrNInsCmp(std::wstring_view strA, std::wstring_view strB, size_t count)
{
	return StrNInsCmpImpl(strA, strB, count);
}

//--------------------------------------------------------------------------------------------------------------------------------
int StrNInsCmp(const std::string& strA, const std::string& strB, size_t count)
{
	return StrNInsCmpImpl(strA, strB, count);
}

//--------------------------------------------------------------------------------------------------------------------------------
int StrNInsCmp(const std::wstring& strA, const std::wstring& strB, size_t count)
{
	return StrNInsCmpImpl(strA, strB, count);
}

//--------------------------------------------------------------------------------------------------------------------------------
int StrNCaseCmp(const char* strA, const char* strB, size_t count)
{
	#if AML_OS_WINDOWS
		// TODO: это MS-specific функция. Возможно, её стоит заменить своей реализацией: цикл сравнения
		// с приведением каждого символа обеих строк к нижнему регистру с помощью функции tolower
		return _strnicmp(strA, strB, count);
	#else
		return strncasecmp(strA, strB, count);
	#endif
}

//--------------------------------------------------------------------------------------------------------------------------------
int StrNCaseCmp(const wchar_t* strA, const wchar_t* strB, size_t count)
{
	#if AML_OS_WINDOWS
		// TODO: это MS-specific функция. Возможно, её стоит заменить своей реализацией: цикл сравнения
		// с приведением каждого символа обеих строк к нижнему регистру с помощью функции towlower
		return _wcsnicmp(strA, strB, count);
	#else
		return wcsncasecmp(strA, strB, count);
	#endif
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Функции преобразования регистра
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if AML_OS_WINDOWS
	#pragma warning(push)
	#pragma warning(disable: 4996)
#endif

//--------------------------------------------------------------------------------------------------------------------------------
static inline char* Strlwr(char* str, bool useLocale)
{
	if (useLocale)
	{
		#if AML_OS_WINDOWS
			// Эта функция работает почти вдвое быстрее цикла с вызовом tolower в
			// каждой итерации (при условии, что локаль программы не изменялась)
			return _strlwr(str);
		#else
			for (char* p = str; *p; ++p)
			{
				unsigned char c = *p;
				*p = static_cast<char>(tolower(c));
			}
			return str;
		#endif
	}

	unsigned c;
	for (char* p = str;; ++p)
	{
		c = static_cast<unsigned char>(*p);
		if (c == 0)
			return str;

		c += caseTT::down[c];
		*p = static_cast<char>(c);
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
static inline char* Strupr(char* str, bool useLocale)
{
	if (useLocale)
	{
		#if AML_OS_WINDOWS
			// Эта функция работает почти вдвое быстрее цикла с вызовом toupper в
			// каждой итерации (при условии, что локаль программы не изменялась)
			return _strupr(str);
		#else
			for (char* p = str; *p; ++p)
			{
				unsigned char c = *p;
				*p = static_cast<char>(toupper(c));
			}
			return str;
		#endif
	}

	unsigned c;
	for (char* p = str;; ++p)
	{
		c = static_cast<unsigned char>(*p);
		if (c == 0)
			return str;

		c -= caseTT::up[c];
		*p = static_cast<char>(c);
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
static inline wchar_t* Wcslwr(wchar_t* str, bool useLocale)
{
	if (useLocale)
	{
		#if AML_OS_WINDOWS
			// Эта функция работает в ~4 раза быстрее цикла с вызовом towlower в
			// каждой итерации (при условии, что локаль программы не изменялась)
			return _wcslwr(str);
		#else
			for (wchar_t* p = str; *p; ++p)
				*p = static_cast<wchar_t>(towlower(*p));
			return str;
		#endif
	}

	for (wchar_t* p = str;; ++p)
	{
		unsigned c = *p;
		if (c == 0)
			return str;

		if (c <= 0x7f)
			c += caseTT::down[c];
		*p = static_cast<wchar_t>(c);
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
static inline wchar_t* Wcsupr(wchar_t* str, bool useLocale)
{
	if (useLocale)
	{
		#if AML_OS_WINDOWS
			// Эта функция работает в ~4 раза быстрее цикла с вызовом towupper в
			// каждой итерации (при условии, что локаль программы не изменялась)
			return _wcsupr(str);
		#else
			for (wchar_t* p = str; *p; ++p)
				*p = static_cast<wchar_t>(towupper(*p));
			return str;
		#endif
	}

	for (wchar_t* p = str;; ++p)
	{
		unsigned c = *p;
		if (c == 0)
			return str;

		if (c <= 0x7f)
			c -= caseTT::up[c];
		*p = static_cast<wchar_t>(c);
	}
}

#if AML_OS_WINDOWS
	#pragma warning(pop)
#endif

//--------------------------------------------------------------------------------------------------------------------------------
std::string LoCase(std::string_view str, bool noLocale)
{
	const size_t len = str.size();
	if (!len)
		return std::string();

	SmartArray<char, 512> buffer(len + 1);
	memcpy(buffer, str.data(), len);
	buffer[len] = 0;

	return std::string(Strlwr(buffer, !noLocale), len);
}

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring LoCase(std::wstring_view str, bool noLocale)
{
	const size_t len = str.size();
	if (!len)
		return std::wstring();

	SmartArray<wchar_t, 512> buffer(len + 1);
	memcpy(buffer, str.data(), len * sizeof(wchar_t));
	buffer[len] = 0;

	return std::wstring(Wcslwr(buffer, !noLocale), len);
}

//--------------------------------------------------------------------------------------------------------------------------------
std::string UpCase(std::string_view str, bool noLocale)
{
	const size_t len = str.size();
	if (!len)
		return std::string();

	SmartArray<char, 512> buffer(len + 1);
	memcpy(buffer, str.data(), len);
	buffer[len] = 0;

	return std::string(Strupr(buffer, !noLocale), len);
}

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring UpCase(std::wstring_view str, bool noLocale)
{
	const size_t len = str.size();
	if (!len)
		return std::wstring();

	SmartArray<wchar_t, 512> buffer(len + 1);
	memcpy(buffer, str.data(), len * sizeof(wchar_t));
	buffer[len] = 0;

	return std::wstring(Wcsupr(buffer, !noLocale), len);
}

//--------------------------------------------------------------------------------------------------------------------------------
void LoCaseInplace(std::string& str, bool noLocale)
{
	if (const size_t len = str.size())
	{
		SmartArray<char, 512> buffer(len + 1, str.c_str());
		str.assign(Strlwr(buffer, !noLocale), len);
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void LoCaseInplace(std::wstring& str, bool noLocale)
{
	if (const size_t len = str.size())
	{
		SmartArray<wchar_t, 512> buffer(len + 1, str.c_str());
		str.assign(Wcslwr(buffer, !noLocale), len);
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void UpCaseInplace(std::string& str, bool noLocale)
{
	if (const size_t len = str.size())
	{
		SmartArray<char, 512> buffer(len + 1, str.c_str());
		str.assign(Strupr(buffer, !noLocale), len);
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void UpCaseInplace(std::wstring& str, bool noLocale)
{
	if (const size_t len = str.size())
	{
		SmartArray<wchar_t, 512> buffer(len + 1, str.c_str());
		str.assign(Wcsupr(buffer, !noLocale), len);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Функции Trim
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
template<class T>
static size_t FindFirstNotSpace(const T& str)
{
	auto p = str.data();
	for (size_t i = 0, len = str.size(); i < len; ++i, ++p)
	{
		if (*p != 9 && *p != 32)
			return i;
	}
	return T::npos;
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class T>
static size_t FindLastNotSpace(const T& str)
{
	const size_t len = str.size();
	auto p = str.data() + len - 1;
	for (size_t i = len; i > 0; --i, --p)
	{
		if (*p != 9 && *p != 32)
			return i - 1;
	}
	return T::npos;
}

//--------------------------------------------------------------------------------------------------------------------------------
std::string Trim(std::string_view str)
{
	size_t startPos = FindFirstNotSpace(str);
	if (startPos == str.npos)
		return std::string();

	size_t endPos = FindLastNotSpace(str);
	return std::string(str.data() + startPos, endPos - startPos + 1);
}

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring Trim(std::wstring_view str)
{
	size_t startPos = FindFirstNotSpace(str);
	if (startPos == str.npos)
		return std::wstring();

	size_t endPos = FindLastNotSpace(str);
	return std::wstring(str.data() + startPos, endPos - startPos + 1);
}

//--------------------------------------------------------------------------------------------------------------------------------
std::string TrimLeft(std::string_view str)
{
	if (size_t startPos = FindFirstNotSpace(str); startPos != str.npos)
		return std::string(str.data() + startPos, str.size() - startPos);

	return std::string();
}

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring TrimLeft(std::wstring_view str)
{
	if (size_t startPos = FindFirstNotSpace(str); startPos != str.npos)
		return std::wstring(str.data() + startPos, str.size() - startPos);

	return std::wstring();
}

//--------------------------------------------------------------------------------------------------------------------------------
std::string TrimRight(std::string_view str)
{
	if (size_t endPos = FindLastNotSpace(str); endPos != str.npos)
		return std::string(str.data(), endPos + 1);

	return std::string();
}

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring TrimRight(std::wstring_view str)
{
	if (size_t endPos = FindLastNotSpace(str); endPos != str.npos)
		return std::wstring(str.data(), endPos + 1);

	return std::wstring();
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class StringT>
static bool TrimInplaceImpl(StringT& str)
{
	size_t startPos = FindFirstNotSpace(str);
	if (startPos == StringT::npos)
	{
		str.clear();
		return true;
	}

	bool result = false;
	size_t endPos = FindLastNotSpace(str);
	if (endPos + 1 < str.size())
	{
		str.erase(endPos + 1);
		result = true;
	}
	if (startPos > 0)
	{
		str.erase(0, startPos);
		result = true;
	}
	return result;
}

//--------------------------------------------------------------------------------------------------------------------------------
void TrimInplace(std::string& str, bool fast)
{
	if (TrimInplaceImpl(str) && !fast)
		str.shrink_to_fit();
}

//--------------------------------------------------------------------------------------------------------------------------------
void TrimInplace(std::wstring& str, bool fast)
{
	if (TrimInplaceImpl(str) && !fast)
		str.shrink_to_fit();
}

//--------------------------------------------------------------------------------------------------------------------------------
void TrimLeftInplace(std::string& str, bool fast)
{
	if (size_t startPos = FindFirstNotSpace(str))
	{
		if (startPos != str.npos)
			str.erase(0, startPos);
		else
			str.clear();

		if (!fast)
			str.shrink_to_fit();
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void TrimLeftInplace(std::wstring& str, bool fast)
{
	if (size_t startPos = FindFirstNotSpace(str))
	{
		if (startPos != str.npos)
			str.erase(0, startPos);
		else
			str.clear();

		if (!fast)
			str.shrink_to_fit();
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void TrimRightInplace(std::string& str, bool fast)
{
	size_t endPos = FindLastNotSpace(str);

	if (endPos == str.npos)
		str.clear();
	else if (endPos + 1 < str.size())
		str.erase(endPos + 1);
	else
		return;

	if (!fast)
		str.shrink_to_fit();
}

//--------------------------------------------------------------------------------------------------------------------------------
void TrimRightInplace(std::wstring& str, bool fast)
{
	size_t endPos = FindLastNotSpace(str);

	if (endPos == str.npos)
		str.clear();
	else if (endPos + 1 < str.size())
		str.erase(endPos + 1);
	else
		return;

	if (!fast)
		str.shrink_to_fit();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Конвертация строк Ansi/UTF-8/Wide
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
enum class MBCodePage
{
	Ansi,
	Utf8
};

//--------------------------------------------------------------------------------------------------------------------------------
static size_t FastMBToWide(const char* str, size_t size)
{
	for (size_t i = 0; i < size; ++i)
	{
		if (str[i] & 0x80)
			return i;
	}

	return size;
}

//--------------------------------------------------------------------------------------------------------------------------------
static size_t FastMBToWide(wchar_t* out, const char* str, size_t size)
{
	// TODO: эта и предыдущая функции - попытка оптимизировать алгоритм конвертации строк Ansi/UTF-8 в Wide. Идея
	// заключается в том, чтобы быстро скопировать в выходной буфер символы, значения которых меньше 0x80, а затем
	// обработать оставшуюся часть строки обычным способом, т.е. с помощью функций ОС. Но обрабатывать по 1 символу
	// за раз - неоптимально. А чтобы делать это оптимально, нужны разные версии функций для x86/x64, а также разного
	// размера типа wchar_t. Тем не менее, для коротких строк даже такая оптимизация даёт существенный выигрыш

	for (size_t i = 0; i < size; ++i)
	{
		const char c = str[i];
		if (c & 0x80)
			return i;

		out[i] = c;
	}

	return size;
}

//--------------------------------------------------------------------------------------------------------------------------------
static void MBToWide(std::wstring& out, MBCodePage codePage, const char* str, size_t size)
{
	#if AML_OS_WINDOWS
		const size_t LOCAL_SIZE = 3840 / sizeof(wchar_t);
		int bufferSize = LOCAL_SIZE;

		// Мне не известно, в какое максимальное количество codepoint UTF-16 может быть закодирован отдельный символ Ansi
		// или какая-либо комбинация символов при конвертировании из Ansi (ACP). И чтобы наверняка не ошибиться, мы будем
		// искать необходимый размер буфера, только если длина исходной строки будет больше 1/4 размера локального буфера.
		// Для UTF-8 в худшем случае на каждый байт исходной строки будет приходится по одному codepoint UTF-16, в других
		// случаях длина результирующей строки всегда будет меньше количества байт в исходной
		if (auto limit = (codePage == MBCodePage::Ansi) ? LOCAL_SIZE / 4 : LOCAL_SIZE; size > limit)
		{
			if (size > INT_MAX)
				return;

			const unsigned cp = (codePage == MBCodePage::Ansi) ? CP_ACP : CP_UTF8;
			bufferSize = ::MultiByteToWideChar(cp, 0, str, static_cast<int>(size), nullptr, 0);
			if (bufferSize <= 0)
				return;
		}

		SmartArray<wchar_t, LOCAL_SIZE> buffer(bufferSize);
		// Сначала пытаемся быстро сконвертировать те символы в начале строки, значение которых
		// меньше 0x80. В случае коротких строк функция WinAPI будет работать намного медленнее
		size_t count = (size < 120) ? FastMBToWide(buffer, str, size) : 0;

		if (count < size)
		{
			const unsigned cp = (codePage == MBCodePage::Ansi) ? CP_ACP : CP_UTF8;
			int len = ::MultiByteToWideChar(cp, 0, str + count, static_cast<int>(size - count),
				buffer + count, bufferSize - static_cast<int>(count));
			if (len <= 0)
				return;
			count += len;
		}

		out.assign(buffer, count);
	#else
		#error Not implemented
	#endif
}

//--------------------------------------------------------------------------------------------------------------------------------
static int MBToWide(wchar_t* buffer, size_t bufferSize, MBCodePage codePage, const char* str, size_t size)
{
	#if AML_OS_WINDOWS
		size_t count;
		if (!bufferSize || !buffer)
		{
			// Сначала пытаемся быстро сконвертировать те символы в начале строки, значение которых
			// меньше 0x80. В случае коротких строк функция WinAPI будет работать намного медленнее
			count = (size < 120) ? FastMBToWide(str, size) : 0;

			if (count < size)
			{
				if (size > INT_MAX)
					return -1;

				const unsigned cp = (codePage == MBCodePage::Ansi) ? CP_ACP : CP_UTF8;
				int len = ::MultiByteToWideChar(cp, 0, str + count, static_cast<int>(size - count), nullptr, 0);
				if (len <= 0 || len > INT_MAX - static_cast<int>(count))
					return -1;
				count += len;
			}
		} else
		{
			bufferSize = (bufferSize < INT_MAX) ? bufferSize : INT_MAX;
			count = (size < 120) ? FastMBToWide(buffer, str, (size < bufferSize) ? size : bufferSize) : 0;

			if (count < size)
			{
				if (count >= bufferSize)
					return -1;

				const unsigned cp = (codePage == MBCodePage::Ansi) ? CP_ACP : CP_UTF8;
				int len = ::MultiByteToWideChar(cp, 0, str + count, static_cast<int>(size - count),
					buffer + count, static_cast<int>(bufferSize - count));
				if (len <= 0 || len > INT_MAX - static_cast<int>(count))
					return -1;
				count += len;
			}
		}

		return static_cast<int>(count);
	#else
		#error Not implemented
	#endif
}

//--------------------------------------------------------------------------------------------------------------------------------
static size_t FastWideToMB(const wchar_t* str, size_t size)
{
	for (size_t i = 0; i < size; ++i)
	{
		if (str[i] & 0xffffff80)
			return i;
	}

	return size;
}

//--------------------------------------------------------------------------------------------------------------------------------
static size_t FastWideToMB(char* out, const wchar_t* str, size_t size)
{
	// TODO: эта и предыдущая функции - попытка оптимизировать конвертацию
	// Wide-строк в Ansi/UTF-8. См. комментарий в функции FastMBToWide

	for (size_t i = 0; i < size; ++i)
	{
		const wchar_t c = str[i];
		if (c & 0xffffff80)
			return i;

		out[i] = static_cast<char>(c);
	}

	return size;
}

//--------------------------------------------------------------------------------------------------------------------------------
static void WideToMB(std::string& out, MBCodePage codePage, const wchar_t* str, size_t size)
{
	#if AML_OS_WINDOWS
		const size_t LOCAL_SIZE = 3840;
		int bufferSize = LOCAL_SIZE;

		// Мне не известно, в какое максимальное количество байт может быть закодирован отдельный символ UCS-2 (UTF-16) или
		// какая-либо комбинация символов при конвертировании в Ansi (ACP). И чтобы наверняка не ошибиться, мы будем искать
		// необходимый размер буфера, только если длина исходной строки будет больше 1/4 размера локального буфера. Каждый
		// символ UCS-2 (эта кодировка использовалась до Windows 2000) может стать максимум 3 байтами в UTF-8. Это
		// же справедливо и для UTF-16, но её суррогатные пары (2 16-битных слова) будут закодированы 4 байтами
		if (auto limit = (codePage == MBCodePage::Ansi) ? LOCAL_SIZE / 4 : LOCAL_SIZE / 3; size > limit)
		{
			if (size > INT_MAX)
				return;

			const unsigned cp = (codePage == MBCodePage::Ansi) ? CP_ACP : CP_UTF8;
			bufferSize = ::WideCharToMultiByte(cp, 0, str, static_cast<int>(size), nullptr, 0, nullptr, nullptr);
			if (bufferSize <= 0)
				return;
		}

		SmartArray<char, LOCAL_SIZE> buffer(bufferSize);
		// Сначала пытаемся быстро сконвертировать те символы в начале строки, значение которых
		// меньше 0x80. В случае коротких строк функция WinAPI будет работать намного медленнее
		size_t count = (size < 120) ? FastWideToMB(buffer, str, size) : 0;

		if (count < size)
		{
			const unsigned cp = (codePage == MBCodePage::Ansi) ? CP_ACP : CP_UTF8;
			int len = ::WideCharToMultiByte(cp, 0, str + count, static_cast<int>(size - count),
				buffer + count, bufferSize - static_cast<int>(count), nullptr, nullptr);
			if (len <= 0)
				return;
			count += len;
		}

		out.assign(buffer, count);
	#else
		#error Not implemented
	#endif
}

//--------------------------------------------------------------------------------------------------------------------------------
static int WideToMB(char* buffer, size_t bufferSize, MBCodePage codePage, const wchar_t* str, size_t size)
{
	#if AML_OS_WINDOWS
		size_t count;
		if (!bufferSize || !buffer)
		{
			// Сначала пытаемся быстро сконвертировать те символы в начале строки, значение которых
			// меньше 0x80. В случае коротких строк функция WinAPI будет работать намного медленнее
			count = (size < 120) ? FastWideToMB(str, size) : 0;

			if (count < size)
			{
				if (size > INT_MAX)
					return -1;

				const unsigned cp = (codePage == MBCodePage::Ansi) ? CP_ACP : CP_UTF8;
				int len = ::WideCharToMultiByte(cp, 0, str + count, static_cast<int>(size - count),
					nullptr, 0, nullptr, nullptr);
				if (len <= 0 || len > INT_MAX - static_cast<int>(count))
					return -1;
				count += len;
			}
		} else
		{
			bufferSize = (bufferSize < INT_MAX) ? bufferSize : INT_MAX;
			count = (size < 120) ? FastWideToMB(buffer, str, (size < bufferSize) ? size : bufferSize) : 0;

			if (count < size)
			{
				if (count >= bufferSize)
					return -1;

				const unsigned cp = (codePage == MBCodePage::Ansi) ? CP_ACP : CP_UTF8;
				int len = ::WideCharToMultiByte(cp, 0, str + count, static_cast<int>(size - count),
					buffer + count, static_cast<int>(bufferSize - count), nullptr, nullptr);
				if (len <= 0 || len > INT_MAX - static_cast<int>(count))
					return -1;
				count += len;
			}
		}

		return static_cast<int>(count);
	#else
		#error Not implemented
	#endif
}

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring FromAnsi(std::string_view str)
{
	std::wstring out;
	MBToWide(out, MBCodePage::Ansi, str.data(), str.size());
	return out;
}

//--------------------------------------------------------------------------------------------------------------------------------
int FromAnsi(wchar_t* buffer, size_t bufferSize, std::string_view str)
{
	return MBToWide(buffer, bufferSize, MBCodePage::Ansi, str.data(), str.size());
}

//--------------------------------------------------------------------------------------------------------------------------------
std::string ToAnsi(std::wstring_view str)
{
	std::string out;
	WideToMB(out, MBCodePage::Ansi, str.data(), str.size());
	return out;
}

//--------------------------------------------------------------------------------------------------------------------------------
int ToAnsi(char* buffer, size_t bufferSize, std::wstring_view str)
{
	return WideToMB(buffer, bufferSize, MBCodePage::Ansi, str.data(), str.size());
}

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring FromUtf8(std::string_view str)
{
	std::wstring out;
	MBToWide(out, MBCodePage::Utf8, str.data(), str.size());
	return out;
}

//--------------------------------------------------------------------------------------------------------------------------------
int FromUtf8(wchar_t* buffer, size_t bufferSize, std::string_view str)
{
	return MBToWide(buffer, bufferSize, MBCodePage::Utf8, str.data(), str.size());
}

//--------------------------------------------------------------------------------------------------------------------------------
std::string ToUtf8(std::wstring_view str)
{
	std::string out;
	WideToMB(out, MBCodePage::Utf8, str.data(), str.size());
	return out;
}

//--------------------------------------------------------------------------------------------------------------------------------
int ToUtf8(char* buffer, size_t bufferSize, std::wstring_view str)
{
	return WideToMB(buffer, bufferSize, MBCodePage::Utf8, str.data(), str.size());
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Функция Split
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
template<class CharT, class ViewT = std::basic_string_view<CharT>>
static size_t FindFirstOf(const ViewT str, const CharT* what, size_t from)
{
	if (what && *what)
	{
		const size_t len = str.size();
		const CharT* p = str.data();

		if (!what[1])
		{
			for (size_t i = from; i < len; ++i)
			{
				if (p[i] == *what)
					return i;
			}
		} else
		{
			for (size_t i = from; i < len; ++i)
			{
				for (size_t j = 0; what[j]; ++j)
				{
					if (p[i] == what[j])
						return i;
				}
			}
		}
	}

	return ViewT::npos;
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class CharT, class StringT = std::basic_string<CharT>>
static void Split(std::vector<StringT>& tokens, const std::basic_string_view<CharT> str,
	const CharT* const delimiters, int flags)
{
	const size_t MAX_DELIMS = 32;
	size_t delimPos[MAX_DELIMS];

	const size_t len = str.size();
	size_t tokenCount = 0, lastDelim = StringT::npos;
	for (size_t fromPos = 0, delimCount = 0; fromPos < len;)
	{
		lastDelim = FindFirstOf(str, delimiters, fromPos);
		if (delimCount < MAX_DELIMS)
			delimPos[delimCount++] = lastDelim;
		if (lastDelim == fromPos)
		{
			++fromPos;
			if (flags & SPLIT_ALLOW_EMPTY)
				++tokenCount;
		} else
		{
			fromPos = (lastDelim == StringT::npos) ? len : lastDelim + 1;
			++tokenCount;
		}
	}

	const int TRAILING_FLAG = SPLIT_ALLOW_EMPTY | SPLIT_TRAILING_DELIMITER;
	if ((flags & TRAILING_FLAG) == TRAILING_FLAG && lastDelim != StringT::npos)
		++tokenCount;
	else if (!tokenCount)
		return;

	tokens.reserve(tokenCount);
	for (size_t fromPos = 0, delimIdx = 0; fromPos < len;)
	{
		lastDelim = (delimIdx < MAX_DELIMS) ? delimPos[delimIdx++] :
			FindFirstOf(str, delimiters, fromPos);
		if (lastDelim == fromPos)
		{
			++fromPos;
			if (flags & SPLIT_ALLOW_EMPTY)
				tokens.emplace_back();
		} else
		{
			size_t tokenLen = ((lastDelim == StringT::npos) ? len : lastDelim) - fromPos;
			auto left = str.data() + fromPos;
			fromPos += tokenLen + 1;

			if (flags & SPLIT_TRIM)
			{
				auto right = left + tokenLen - 1;
				while (left <= right && (*left == 32 || *left == 9)) ++left;
				if (left < right) while (*right == 32 || *right == 9) --right;
				tokenLen = right - left + 1;
			}

			if (tokenLen || (flags & SPLIT_ALLOW_EMPTY))
				tokens.emplace_back(left, tokenLen);
		}
	}

	if ((flags & TRAILING_FLAG) == TRAILING_FLAG && lastDelim != StringT::npos)
		tokens.emplace_back();
}

//--------------------------------------------------------------------------------------------------------------------------------
std::vector<std::string> Split(std::string_view str, ZStringView delimiters, int flags)
{
	std::vector<std::string> tokens;
	Split(tokens, str, delimiters.c_str(), flags);
	return tokens;
}

//--------------------------------------------------------------------------------------------------------------------------------
std::vector<std::wstring> Split(std::wstring_view str, WZStringView delimiters, int flags)
{
	std::vector<std::wstring> tokens;
	Split(tokens, str, delimiters.c_str(), flags);
	return tokens;
}

} // namespace util
