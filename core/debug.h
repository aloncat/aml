//∙AML
// Copyright (C) 2018-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "platform.h"
#include "singleton.h"
#include "util.h"

#include <string_view>

namespace util {

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Макросы Assert/Verify/Halt
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Макрос AML_DBG_BREAK позволяет остановить выполнение программы в точке его вызова. Работает только
// на платформе Windows, только в отладочных конфигурациях. Срабатывает только при активном отладчике

#if AML_OS_WINDOWS && AML_DEBUG
	#define AML_DBG_BREAK ((void)(util::DebugHelper::IsDebuggerActive() && (__debugbreak(), 0)))
#else
	#define AML_DBG_BREAK ((void) 0)
#endif

// Макрос Assert проверяет переданное ему логическое выражение EXPRESSION, и если оно равно true, то ничего не делает.
// Но если выражение равно false, то показывает пользователю модальное окно с подробным сообщением об ошибке (для GUI
// приложения), или выбрасывает исключение EAssertion (для консольного приложения). Также это сообщение дублируется
// в системный журнал. Это поведение может быть изменено см. функцию DebugHelper::SetAssertHandler. В production
// сборках макрос Assert всегда полностью отключен, а выражение EXPRESSION не вычисляется

// Макрос Verify аналогичен макросу Assert и ведёт себя точно так же. Его главное отличие от Assert в том, что он всегда
// возвращает значение bool, соответствующее значению выражения EXPRESSION. И поэтому обычно используется в конструкциях
// вида "if (Verify(expression && "Error message")) { ... }". В production сборках макрос Verify не выводит никаких
// сообщений и не генерирует исключений, однако в отличие от Assert вычисляет значение выражения EXPRESSION

// Макрос Halt(MESSAGE) работает аналогично Assert(false && MESSAGE), то есть он всегда выводит сообщение об ошибке
// (для GUI проложения) или генерирует исключение EHalt (для консольного приложения). Его важное отличие от Assert
// состоит в том, что передаваемое значение MESSAGE должно быть строкой (любого типа) с сообщением об ошибке. В
// production сборках макрос Halt всегда полностью отключен, а выражение MESSAGE не вычисляется

#if AML_PRODUCTION
	#define Assert(EXPRESSION) ((void) 0)
	#define Verify(EXPRESSION) static_cast<bool>(EXPRESSION)
	#define Halt(MESSAGE) ((void) 0)
#else
	#define Assert(EXPRESSION) \
		((void)(!(EXPRESSION) && ( \
			util::DebugHelper::OnAssert(AML_WSTRING(__FILE__), __LINE__, AML_WSTRING(#EXPRESSION)), \
			AML_DBG_BREAK, 0)))

	#define Verify(EXPRESSION) \
		((EXPRESSION) ? true : ( \
			util::DebugHelper::OnAssert(AML_WSTRING(__FILE__), __LINE__, AML_WSTRING(#EXPRESSION)), \
			AML_DBG_BREAK, false))

	#define Halt(MESSAGE) \
		((void)(util::DebugHelper::OnHalt(AML_WSTRING(__FILE__), __LINE__, MESSAGE), \
			AML_DBG_BREAK, 0))
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   AssertHandler
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Класс AssertHandler предназначен для обработки ситуаций, возникающих при проверке условий в макросах Assert и Verify,
// а также для обработки макроса Halt. В этом классе реализовано поведение по умолчанию: показ окна с сообщением об ошибке
// для GUI-проложения или выброс исключения для консольного приложения. При необходимости это поведение можно изменить,
// унаследовавшись от этого класса (см. функцию DebugHelper::SetAssertHandler)

//--------------------------------------------------------------------------------------------------------------------------------
class AssertHandler
{
	AML_NONCOPYABLE(AssertHandler)

public:
	// TODO
};

//--------------------------------------------------------------------------------------------------------------------------------
class DebugHelper : public Singleton<DebugHelper>
{
public:
	// TODO

	// Возвращает true, если приложение работает под отладчиком. Факт наличия активного отладчика проверяется
	// однократно при инициализации класса. Для релизных конфигураций функция всегда возвращает false
	static bool IsDebuggerActive();

	// Вызывает функцию OnAssert заданного обработчика
	static void OnAssert(const wchar_t* filePath, int line, const wchar_t* expression);

	// Вызывает функцию OnHalt заданного обработчика
	static void OnHalt(const wchar_t* filePath, int line, std::string_view msg);
	static void OnHalt(const wchar_t* filePath, int line, std::wstring_view msg);

	// Аварийно завершает работу приложения. Если пользовательский обработчик был установлен, то он
	// будет вызван с указанным значением кода ошибки exitCode. Если обрабтчик не был установлен,
	// или он вернёт управление, то будет вызвана функция стандартной библиотеки _exit(exitCode)
	[[noreturn]] static void Abort(int exitCode = 3);

protected:
	// TODO
};

} // namespace util
