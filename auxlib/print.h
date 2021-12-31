//∙Auxlib
// Copyright (C) 2019-2021 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include <string_view>

namespace aux {

// Выводит в окно системной консоли строку str цветом color
void Print(std::string_view str, int color = 7);
void Print(std::wstring_view str, int color = 7);

// Выводит в окно системной консоли строку с тегами цвета. Чтобы задать цвет текста, нужно добавить символ # и за ним 1-3
// цифры, обозначающие код цвета. Если нужно напечатать сам символ #, то его нужно повторить (т.е. использовать ##). Если
// после 1 или 2 цифр кода цвета должна быть выведена цифра, то нужно перед ней вставить # (например, "#2#123" выведет
// "123" зелёным цветом; символ # после кода цвета игнорируется и служит необязательным признаком его окончания)
void Printc(std::string_view coloredStr);
void Printc(std::wstring_view coloredStr);

// Выводит в окно консоли форматированную строку. Спецификаторы формата для format полностью идентичны спецификаторам
// формата для printf. Помимо этого в строке форматирования можно задать цвет текста (также, как и для Printc). Теги
// цвета обрабатываются после форматирования параметров, поэтому цвет можно задавать с помощью параметров функции.
// Так, вызов Printf("Hello, #%dworld!", i) при int i = 10, будет эквивалентен вызову Printc("Hello, #10world!")
void Printf(const char* format, ...);
void Printf(const wchar_t* format, ...);

} // namespace aux
