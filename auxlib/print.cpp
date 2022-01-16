//∙Auxlib
// Copyright (C) 2019-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "print.h"

#include <core/array.h>
#include <core/console.h>
#include <core/strcommon.h>
#include <core/strformat.h>

#include <stdarg.h>

namespace aux {

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Print
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
void Print(std::string_view str, int color)
{
	util::SystemConsole::Instance().Write(str, color);
}

//--------------------------------------------------------------------------------------------------------------------------------
void Print(std::wstring_view str, int color)
{
	util::SystemConsole::Instance().Write(str, color);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Printc
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
template<class CharT>
static void PrintColored(const CharT* str, size_t count)
{
	CharT localBuffer[640];
	util::FlexibleArray<CharT> buffer(localBuffer);
	CharT* out = buffer;

	int color, nextColor = 7;
	const CharT* const end = str + count;
	for (const CharT* p = str; p < end;)
	{
		color = nextColor;
		CharT* const outEnd = std::min(buffer + buffer.GetSize(), out + (end - p));

		while (out < outEnd && *p != '#')
			*out++ = *p++;

		if (out < outEnd)
		{
			size_t i = 0;
			nextColor = 0;
			for (++p; i < 3 && p < end && *p >= '0' && *p <= '9'; ++i, ++p)
				nextColor = 10 * nextColor + *p - '0';
			if (p < end && *p == '#')
				++p;
			if (i == 0)
			{
				nextColor = color;
				*out++ = '#';
				if (p < end)
					continue;
			}
			else if (nextColor == color && p < end)
				continue;
		}
		else if (p < end && buffer.GetSize() < 10240)
		{
			const size_t writtenCount = out - buffer;
			buffer.Grow(2 * buffer.GetSize(), true);
			out = buffer + writtenCount;
			continue;
		}

		if (out > buffer)
		{
			std::basic_string_view<CharT> s(buffer, out - buffer);
			util::SystemConsole::Instance().Write(s, color);
			out = buffer;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void Printc(std::string_view coloredStr)
{
	PrintColored(coloredStr.data(), coloredStr.size());
}

//--------------------------------------------------------------------------------------------------------------------------------
void Printc(std::wstring_view coloredStr)
{
	PrintColored(coloredStr.data(), coloredStr.size());
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Printf
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
template<class CharT>
static void PrintfHelper(const CharT* format, va_list args)
{
	if (format && *format)
	{
		// Сканируем строку, чтобы проверить, нуждается ли она в форматировании. Если в строке содержатся только
		// коды цвета, то прямой вызов PrintColored даст значительное ускорение. В противном случае проверка не
		// повлияет (если '%' расположен в начале строки) или увеличит время выполнения не более, чем на 1%
		const CharT* p = format;
		while (*p && *p != '%') ++p;

		if (*p == 0)
		{
			PrintColored(format, p - format);
		} else
		{
			util::FormatEx(format, args, [](util::BasicZStringView<CharT> str) {
				PrintColored(str.data(), str.size());
			});
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void Printf(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	PrintfHelper(format, args);
	va_end(args);
}

//--------------------------------------------------------------------------------------------------------------------------------
void Printf(const wchar_t* format, ...)
{
	va_list args;
	va_start(args, format);
	PrintfHelper(format, args);
	va_end(args);
}

} // namespace aux
