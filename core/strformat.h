//∙AML
// Copyright (C) 2016-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "platform.h"
#include "strcommon.h"
#include "strutil.h"
#include "sysinfo.h"
#include "util.h"

#include <functional>
#include <math.h>
#include <stdarg.h>
#include <string>
#include <string_view>
#include <type_traits>

namespace util {

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Форматирование строк
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Функция Format возвращает форматированную строку. Спецификаторы формата для format полностью идентичны
// спецификаторам функции printf. ПРИМЕЧАНИЕ: Wide-версия функции имеет ограничение длины результирующей
// строки: если она окажется длиннее 8160 символов, то функция вернёт пустую строку
std::string Format(const char* format, ...);
std::wstring Format(const wchar_t* format, ...);

// Функция FormatW идентична Ansi-версии функции Format, возвращает ту же
// форматированную строку, но преобразованную в Wide-строку std::wstring
std::wstring FormatW(const char* format, ...);

// Эти функции FormatEx форматируют строку и помещают её в указанный буфер buffer вместе с терминирующим нолём.
// Параметр bufferSize задаёт размер буфера в символах, параметр format задаёт формат. В случае успеха функция
// вернёт количество записанных в буфер символов (без учёта терминирующего ноля). Если размер буфера окажется
// недостаточным или произойдёт ошибка форматирования, функция вернёт отрицательное значение
int FormatEx(char* buffer, size_t bufferSize, const char* format, ...);
int FormatEx(wchar_t* buffer, size_t bufferSize, const wchar_t* format, ...);

// Эти функции FormatEx форматируют строку. Параметр format задаёт формат, а args - аргументы. В случае успешного
// форматирования будет вызвана пользовательская функция cb, которой будет передан вью на сформированную строку.
// Функция вернёт true, если форматирование было успешно выполнено (и функция cb была вызвана). И вернёт false,
// если форматирование не было выполнено (в этом случае функция cb не вызывается). ПРИМЕЧАНИЕ: Wide-версия функции
// имеет ограничение длины результирующей строки: если она окажется длиннее 8160 символов, то функция вернёт false
bool FormatEx(const char* format, va_list args, const std::function<void(ZStringView)>& cb);
bool FormatEx(const wchar_t* format, va_list args, const std::function<void(WZStringView)>& cb);

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Formatter - форматирование в строку
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
template<class CharT, size_t innerBuf = 640>
class Formatter : public StringWriter<CharT, innerBuf>
{
public:
	Formatter()
	{
		auto& sysInfo = SystemInfo::Instance();
		m_DecimalPoint = sysInfo.GetDecimalPoint();
	}

	// Выводит "true" или "false" в соответствии со значением value. Этот оператор объявлен как шаблонная
	// функция, так как не должен иметь приоритет над другими операторами <<, принимающими строковые типы
	template<class T, class = std::enable_if_t<std::is_same_v<T, bool>>>
	Formatter& operator <<(T value)
	{
		if constexpr (std::is_same_v<CharT, wchar_t>)
			this->Append(value ? L"true" : L"false");
		else
			this->Append(value ? "true" : "false");

		return *this;
	}

	Formatter& operator <<(int32_t value)
	{
		uint32_t v;
		if (value >= 0)
			v = value;
		else
		{
			this->Append('-');
			v = -value;
		}

		return *this << v;
	}

	Formatter& operator <<(uint32_t value)
	{
		CharT buffer[10];
		CharT* p = buffer + CountOf(buffer);

		size_t n, count = 0;
		do {
			n = value;
			value /= 10;
			*(--p) = '0' + static_cast<char>(n - 10 * value);
			++count;
		} while (value);

		this->Append(p, count);
		return *this;
	}

	Formatter& operator <<(int64_t value)
	{
		uint64_t v;
		if (value >= 0)
			v = value;
		else
		{
			this->Append('-');
			v = -value;
		}

		return *this << v;
	}

	Formatter& operator <<(uint64_t value)
	{
		CharT buffer[20];
		CharT* p = buffer + CountOf(buffer);

		#if AML_64BIT
			size_t n;
			do {
				n = value;
				value /= 10;
				*(--p) = '0' + static_cast<char>(n - 10 * value);
			} while (value);
		#else
			for (auto required = p;;)
			{
				size_t n, low;
				if (value >> 32)
				{
					required -= 9;
					uint64_t hi = value / 1000000000;
					low = static_cast<uint32_t>(value - hi * 1000000000);
					value = hi;
				} else
				{
					low = static_cast<uint32_t>(value);
					value = 0;
				}

				do {
					n = low;
					low /= 10;
					*(--p) = '0' + static_cast<char>(n - 10 * low);
				} while (low);

				if (!value)
					break;

				while (p > required)
					*(--p) = '0';
			}
		#endif

		auto count = buffer + CountOf(buffer) - p;
		this->Append(p, count);

		return *this;
	}

	// Выводит значение value как число с плавающей запятой, округляя его до 6-го знака после запятой. Если абсолютное
	// значение value меньше 1e+6, то число будет выведено в формате "%.6f" (1-6 знаков до запятой, 1-6 знаков после,
	// без экспоненты), иначе в формате "%.6e" (1 знак до запятой, 1-6 знаков после, экспонента). Разделителем целой
	// и дробной частей числа независимо от установленной локали всегда является символ точка
	Formatter& operator <<(double value)
	{
		char buffer[16];
		// TODO: в С++17 появилась функция std::to_chars, работающая в разы быстрее функции FormatEx для чисел с плавающей
		// запятой. Но мы пока не можем её использовать. Проблема в том, что в VS 2017 эта функция реализована частично: без
		// возможности округления до заданного количества знаков (полная поддержка есть в VS 2019, начиная с версии 16.2)
		/*if (auto result = std::to_chars(buffer, buffer + CountOf(buffer), value, (fabs(value) < 999999.9999995) ?
			std::chars_format::fixed : std::chars_format::scientific, 6); result.ec == std::errc())
		{
			auto count = result.ptr - buffer;
			buffer[count] = 0;*/

		const char* fmt = (fabs(value) < 999999.9999995) ? "%.6f" : "%.6e";
		if (int count = FormatEx(buffer, CountOf(buffer), fmt, value); count > 0)
		{
			// Если в качестве разделителя целой и дробной частей числа используется не точка (возможно
			// только при установленной локали программы), то найдём и заменим этот символ на точку '.'
			if (const char dp = m_DecimalPoint; dp != '.')
			{
				for (auto p = buffer; *p; ++p)
				{
					if (*p == dp)
					{
						*p = '.';
						break;
					}
				}
			}

			// Удалим ноли справа в дробной части (кроме первого ноля после точки)
			for (auto p = buffer; *p; ++p)
			{
				if (*p == '.')
				{
					int z = 0;
					for (p += 2; *p >= '0' && *p <= '9'; ++p)
						z = (*p == '0') ? z + 1 : 0;
					if (z)
					{
						count -= z;
						for (p -= z; p[z]; ++p)
							*p = p[z];
					}
					break;
				}
			}

			if constexpr (std::is_same_v<CharT, char>)
				this->Append(buffer, count);
			else
			{
				CharT wbuf[CountOf(buffer)];
				for (int i = 0; i < count; ++i)
					wbuf[i] = buffer[i];
				this->Append(wbuf, count);
			}
		}

		return *this;
	}

	// Выводит символ chr. Если тип chr не совпадает с CharT, то функция выполнит преобразование
	// автоматически, только если код символа меньше 0x80, иначе символ будет заменён на U+FFFD
	Formatter& operator <<(char chr)
	{
		this->Append(chr);
		return *this;
	}

	// Выводит символ chr. Если тип chr не совпадает с CharT, то функция выполнит преобразование
	// автоматически, только если код символа меньше 0x80, иначе символ будет заменён на '?'
	Formatter& operator <<(wchar_t chr)
	{
		this->Append(chr);
		return *this;
	}

	// Выводит null-терминированную строку str (только для [const] CharT*)
	template<class T, class = std::enable_if_t<std::is_same_v<T, CharT>>>
	Formatter& operator <<(const T* const& str)
	{
		const CharT* text = str;
		this->Append(text);

		return *this;
	}

	// Выводит строку str (любого другого типа)
	Formatter& operator <<(std::basic_string_view<CharT> str)
	{
		this->Append(str.data(), str.size());
		return *this;
	}

	// Выводит объект obj любого типа при условии, что у него есть функция ToString и нет оператора приведения к
	// std::[w]string_view. Эта функция должна возвращать значение любого типа, поддерживаемого классом Formatter
	template<class T, class = std::enable_if_t<
		!std::is_convertible_v<const T&, std::basic_string_view<CharT>>,
		decltype(std::declval<T>().ToString())>>
	Formatter& operator <<(const T& obj)
	{
		auto&& data = obj.ToString();
		return *this << data;
	}

protected:
	char m_DecimalPoint;
};

} // namespace util
