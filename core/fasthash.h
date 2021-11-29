//∙AML
// Copyright (C) 2017-2021 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "platform.h"

#include <string_view>

namespace hash {

// Функции GetFastHash вычисляют 32-битный хеш (FNV-1a) от заданной строки. Функции с парметром toLower,
// если его значение равно true, ведут себя так, как если бы каждый символ исходной строки был переведён
// в нижний регистр перед вычислением хеша (применимо только к латинским буквам от 'A' до 'Z')

// Возвращает значение хеша для пустой строки (зерно FNV-1a)
constexpr unsigned GetFastHash() noexcept { return /*FNV_SEED*/ 0x811c9dc5; }

// Вычисляет хеш null-terminated строки str, состоящей из 8-битных
// символов. Может использоваться как для Ansi, так и для UTF-8 строк
unsigned GetFastHash(const char* str, bool toLower = false) noexcept;

// Вычисляет хеш строки str, состоящей из 8-битных символов.
// Может использоваться как для Ansi, так и для UTF-8 строк
unsigned GetFastHash(std::string_view str, bool toLower = false) noexcept;

// Вычисляет хеш null-terminated Wide строки str
unsigned GetFastHash(const wchar_t* str, bool toLower = false) noexcept;

// Вычисляет хеш Wide строки str
unsigned GetFastHash(std::wstring_view str, bool toLower = false) noexcept;

// Вычисляет хеш null-terminated строки str, состоящей из 16-битных символов. Функция подходит
// для UTF-16 строк. Эта функция даёт одинаковый хеш на little-endian и big-endian платформах
unsigned GetFastHash(const uint16_t* str) noexcept;

// Вычисляет хеш для count первых символов строки str, которая состоит из 16-битных
// символов. Функция даёт одинаковый хеш на little-endian и big-endian платформах
unsigned GetFastHash(const uint16_t* str, size_t count) noexcept;

// Вычисляет хеш массива data длиной size байт. Параметр prevHash может
// использоваться для инкрементного вычисления хеша или задания значения зерна
unsigned GetFastHash(const void* data, size_t size, unsigned prevHash = GetFastHash()) noexcept;

} // namespace hash
