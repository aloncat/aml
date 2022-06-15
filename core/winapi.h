﻿//∙AML
// Copyright (C) 2016-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "platform.h"

// Подключение этого заголовочного файла значительно увеличивает время компиляции единицы трансляции.
// Не стоит подключать его в других заголовочных файлах во избежание различных неожиданных эффектов
// и увеличения времени компиляции всего проекта. Также не стоит добавлять его в precompiled header

#if AML_OS_WINDOWS

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Подключение windows.h
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Мы поддерживаем версии ОС семейства Windows, начиная с Windows Server 2003 (необходим
// SP2 для x86 и x64 версий ОС) и Windows XP (необходим SP3 для x86 и SP2 для x64)
#define _WIN32_WINNT 0x0501
#define WINVER 0x0501

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <windows.h>

// Если в настройках проекта Visual Studio в качестве Platform Tools будет выбрана поддержка ОС
// Windows XP "Visual Studio 20xx - Windows XP...", то вместо SDK 8.1 (Windows Kit 8.1) будет
// использован Windows SDK v7.1A, в котором недоступны некоторые новые заголовочные файлы
#include <ntverp.h>
#if VER_PRODUCTBUILD > 7600
	#include <VersionHelpers.h>
#endif

// Мы вынуждены отменить макросы для некоторых функций WinAPI, для которых имеются Ansi (A) и Unicode (W)
// версии, и имена которых совпадают с именами функций в наших собственных классах. Если этого не сделать,
// то неизбежно будут возникать проблемы компиляции кода из-за того, что препроцессор будет изменять имена
// наших функций, добавляя к ним A или W, когда в компилируемом файле подключен заголовок windows.h. В
// качестве решения этой проблемы в #include нужно использовать <core/winapi.h> вместо <windows.h>
#undef GetCurrentDirectory
#undef RemoveDirectory

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Объявления функторов для функций WinAPI
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace util {

// Мы объявляем функторы внутри пространства имен util::winapi. Во-первых, так объявляемые идентификаторы не
// будут засорять пространство имён util (ведь они всё равно не используются нигде кроме структуры WinAPI).
// Во-вторых, мы могли бы расположить их внутри самой структуры WinAPI, но это было бы неудобно, потому что
// в таком случае аналогичным образом засорялось бы пространство имён, но уже внутри структуры WinAPI

namespace winapi {

// Объявления указателей на функции не содержат спецификатора noexcept. Причина в том, что добавление
// noexcept(true) никак не поможет компилятору в оптимизации их вызовов (пролог и эпилог всё равно
// будут добавлены, так как стандарт требует обеспечения гарантии невыброса исключений)

// Windows Server 2008 / Windows Vista
using GetTickCount64Fn = ULONGLONG (WINAPI*)();

} // namespace winapi

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Структура WinAPI
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Каждая функция Windows API внутри класса представлена публичным статическим членом данных (тип функтор для соответствующей
// функции) и публичной статической функцией bool CanNAME(void), где NAME - это имя WinAPI-функции. Функтор используется
// для вызова WinAPI-функции, а функция CanNAME - для проверки доступности этой WinAPI-функции в системе

//--------------------------------------------------------------------------------------------------------------------------------
#define AML_DECLARE_WINAPI_FN(NAME) \
	static winapi::NAME##Fn NAME; \
	static bool Can##NAME() noexcept;

//--------------------------------------------------------------------------------------------------------------------------------
struct WinAPI final
{
	AML_DECLARE_WINAPI_FN(GetTickCount64)

private:
	WinAPI() = delete;
	static void Load() noexcept;

	static bool s_IsLoaded;
};

} // namespace util

#endif // AML_OS_WINDOWS
