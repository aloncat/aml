//∙AML
// Copyright (C) 2016-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "strformat.h"

#include "array.h"

namespace util {

//--------------------------------------------------------------------------------------------------------------------------------
template<class CharT>
class BasicZSView : public BasicZStringView<CharT>
{
public:
	// Параметр str указывает на начало null-terminated строки
	constexpr BasicZSView(const CharT* str, size_t size) noexcept
		: BasicZStringView<CharT>(str, size)
	{
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Форматирование строк
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(_MSC_VER) && _MSC_VER < 1900
	// Начиная с версии MSVC 2015, функция vsnprintf полностью соответствует стандарту C99. Но в более
	// ранних версиях её поведение отличается. Функция vsnprintf_c99 исправляет поведение этой функции
	// на соответствующее стандарту ANSI C99. Также в версиях MSVC до 2015 отсутствует макрос va_copy
	#define AML_VSNPRINTF vsnprintf_c99
	#define va_copy(DST, SRC) DST = SRC

//--------------------------------------------------------------------------------------------------------------------------------
static int vsnprintf_c99(char* buffer, size_t bufferSize, const char* format, va_list args)
{
	// Сохраняем указатель на буфер, чтобы потом записать в него терминирующий
	// "0". Указатель будет ненулевым, если буфер задан и его размер больше 0
	char* tzBuffer = bufferSize ? buffer : nullptr;

	if (buffer)
	{
		// Буфер задан. Если размер буфера больше 0, то уменьшаем его
		// на 1, чтобы зарезервировать место для терминирующего "0"
		if (bufferSize)
			--bufferSize;
		// Если после этого размер стал равен 0 (то есть если он был 0 или 1), то обнуляем
		// и указатель на буфер, так как нам теперь нужно только подсчитать размер строки
		if (!bufferSize)
			buffer = nullptr;
	}

	va_list argsCopy;
	va_copy(argsCopy, args);

	#pragma warning(push)
	#pragma warning(disable: 4996)
	// В версиях MSVC до 2015 функции vsnprintf и _vsnprintf идентичны, и не соответствуют C99.
	// Так, в случае недостаточного размера буфера, функция вернёт -1 (а не длину результирующей
	// строки, какой бы она была, если бы буфер был достаточен). Также, терминирующий ноль будет
	// записан, только если в буфере будет достаточно для него места
	int len = _vsnprintf(buffer, bufferSize, format, args);
	// Если len отрицательный, то это может означать, что произошла ошибка форматирования, или
	// что размер буфера недостаточен. Чтобы исключить второй вариант, вызываем функцию снова
	if (len < 0 && buffer)
		len = _vsnprintf(nullptr, 0, format, argsCopy);
	// Пишем терминирующий ноль, если в буфере для него не хватило места
	if (tzBuffer && len >= 0 && static_cast<size_t>(len) >= bufferSize)
		tzBuffer[bufferSize] = 0;
	#pragma warning(pop)

	va_end(argsCopy);
	return len;
}

#else
	#define AML_VSNPRINTF vsnprintf
#endif // _MSC_VER

//--------------------------------------------------------------------------------------------------------------------------------
static AML_NOINLINE bool FormatStringA(size_t expectedSize, const char* format, va_list args,
	const std::function<void(ZStringView)>& cb)
{
	if (expectedSize < INT_MAX)
	{
		DynamicArray<char> buffer(expectedSize + 1);
		int len = vsnprintf(buffer, expectedSize + 1, format, args);
		if (len >= 0 && static_cast<size_t>(len) <= expectedSize)
		{
			cb(BasicZSView<char>(buffer, len));
			return true;
		}
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
static AML_NOINLINE bool FormatStringW(const wchar_t* format, va_list args, const std::function<void(WZStringView)>& cb)
{
	// Обычно функции форматирования строк работают довольно медленно. Даже вычисление длины результирующей строки без
	// сохранения самой строки в буфер займёт значительно больше времени, чем выделение/освобождение достаточно большого
	// буфера в куче (даже если он сильно больше, чем необходимо). Эта функция вызывается, когда форматирование в небольшой
	// буфер на стеке завершилось с ошибкой. Не будем пытаться найти размер результирующей строки, просто выделим достаточно
	// большой буфер в куче. Но если форматируемая строка и в самом деле огромна, то эта функция также завершится с ошибкой

	const size_t MAX_LENGTH = 8160;
	DynamicArray<wchar_t> buffer(MAX_LENGTH + 1);
	if (int len = vswprintf(buffer, MAX_LENGTH + 1, format, args); len >= 0)
	{
		cb(BasicZSView<wchar_t>(buffer, len));
		return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE std::string Format(const char* format, ...)
{
	std::string out;
	if (format && *format)
	{
		const size_t LOCAL_SIZE = 3840;
		char localBuffer[LOCAL_SIZE];

		va_list args;
		va_start(args, format);
		// Сначала попытаемся сразу сформатировать строку в небольшой буфер localBuffer,
		// выделенный на стеке. В случае успеха, т.е. если результирующая строка поместится
		// в этом буфере, мы ограничимся всего одним вызовом функции *printf
		int len = AML_VSNPRINTF(localBuffer, LOCAL_SIZE, format, args);
		va_end(args);

		if (len >= LOCAL_SIZE)
		{
			va_start(args, format);
			// Нам не удалось сформатировать строку из-за того, что буфер оказался слишком
			// мал. Пробуем другой способ с выделением буфера достаточного размера в куче
			FormatStringA(len, format, args, [&](ZStringView s) { out = s; });
			va_end(args);
		}
		else if (len > 0)
		{
			out.assign(localBuffer, len);
		}
	}

	return out;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE std::wstring Format(const wchar_t* format, ...)
{
	std::wstring out;
	if (format && *format)
	{
		const size_t LOCAL_SIZE = 3840 / sizeof(wchar_t);
		wchar_t localBuffer[LOCAL_SIZE];

		va_list args;
		va_start(args, format);
		// Сначала попытаемся сразу сформатировать строку в небольшой буфер localBuffer,
		// выделенный на стеке. В случае успеха, т.е. если результирующая строка поместится
		// в этом буфере, мы ограничимся всего одним вызовом функции *printf
		int len = vswprintf(localBuffer, LOCAL_SIZE, format, args);
		va_end(args);

		if (len < 0)
		{
			va_start(args, format);
			// Нам не удалось сформатировать строку. Возможно из-за того, что буфер
			// оказался слишком мал. Пробуем другой способ с выделением буфера в куче
			FormatStringW(format, args, [&](WZStringView s) { out = s; });
			va_end(args);
		} else
		{
			out.assign(localBuffer, len);
		}
	}

	return out;
}

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring FormatW(const char* format, ...)
{
	std::wstring out;
	if (format && *format)
	{
		va_list args;
		va_start(args, format);
		FormatEx(format, args, [&](ZStringView s) { out = FromAnsi(s); });
		va_end(args);
	}

	return out;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE int FormatEx(char* buffer, size_t bufferSize, const char* format, ...)
{
	if (!buffer || !bufferSize)
		return -1;

	int len = 0;
	buffer[0] = 0;

	if (format && *format)
	{
		va_list args;
		va_start(args, format);
		len = AML_VSNPRINTF(buffer, bufferSize, format, args);
		va_end(args);

		if (len > 0 && static_cast<unsigned>(len) >= bufferSize)
			len = -1;
	}

	return len;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE int FormatEx(wchar_t* buffer, size_t bufferSize, const wchar_t* format, ...)
{
	if (!buffer || !bufferSize)
		return -1;

	int len = 0;
	buffer[0] = 0;

	if (format && *format)
	{
		va_list args;
		va_start(args, format);
		len = vswprintf(buffer, bufferSize, format, args);
		va_end(args);
	}

	return len;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE bool FormatEx(const char* format, va_list args, const std::function<void(ZStringView)>& cb)
{
	if (!cb)
		return false;

	const size_t LOCAL_SIZE = 3840;
	char localBuffer[LOCAL_SIZE];

	int len = 0;
	localBuffer[0] = 0;

	if (format && *format)
	{
		va_list argsCopy;
		va_copy(argsCopy, args);
		len = AML_VSNPRINTF(localBuffer, LOCAL_SIZE, format, argsCopy);
		va_end(argsCopy);

		if (len >= LOCAL_SIZE)
			return FormatStringA(len, format, args, cb);
		else if (len < 0)
			return false;
	}

	cb(BasicZSView<char>(localBuffer, len));
	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE bool FormatEx(const wchar_t* format, va_list args, const std::function<void(WZStringView)>& cb)
{
	if (!cb)
		return false;

	const size_t LOCAL_SIZE = 3840 / sizeof(wchar_t);
	wchar_t localBuffer[LOCAL_SIZE];

	int len = 0;
	localBuffer[0] = 0;

	if (format && *format)
	{
		va_list argsCopy;
		va_copy(argsCopy, args);
		len = vswprintf(localBuffer, LOCAL_SIZE, format, argsCopy);
		va_end(argsCopy);

		if (len < 0)
			return FormatStringW(format, args, cb);
	}

	cb(BasicZSView<wchar_t>(localBuffer, len));
	return true;
}

} // namespace util
