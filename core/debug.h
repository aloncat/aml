//∙AML
// Copyright (C) 2018-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "platform.h"
#include "singleton.h"
#include "threadsync.h"
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
	enum class Reason {
		AssertFailed,	// Источник ошибки: выражение в Assert
		HaltInvoked		// Источник ошибки: макрос Halt
	};

	enum class Result {
		Skip,			// Пропустить этот Assert и продолжить работу
		SkipAll,		// Пропустить этот Assert, продолжить работу и далее пропускать его автоматически
		Terminate		// Аварийно завершить работу программы
	};

	AssertHandler() = default;
	virtual ~AssertHandler() = default;

	// Эта функция вызывается в результате проверки условия в макросах Assert и Verify
	// (Reason::AssertFailed). Также она вызывается макросом Halt (Reason::HaltInvoked)
	virtual Result OnError(Reason reason, std::wstring_view filePath, int line, std::wstring_view text);

protected:
	static std::wstring FormatMsg(Reason reason, bool forMsgBox, std::wstring_view filePath, int line, std::wstring_view text);
	static void LogError(std::wstring_view msg);
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   DebugHelper
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class DebugHelper : public Singleton<DebugHelper>
{
public:
	// Прототип пользовательской функции аварийного завершения работы. Параметр exitCode
	// содержит значение кода ошибки, который необходимо передать операционной системе
	using AbortHandler = void (*)(int exitCode);

	// Устанавливает новый обработчик аварийного завершения
	void SetAbortHandler(AbortHandler handler);

	// Устанавливает новый обработчик handler вместо текущего. После вызова этой функции владельцем обработчика станет
	// синглтон DebugHelper. Если значение handler равно nullptr, то текущий обработчик будет уничтожен, а макросы
	// Assert/Verify/Halt перестанут обрабатываться. По умолчанию установлен стандартный обработчик AssertHandler
	void SetAssertHandler(AssertHandler* handler);

	// Возвращает true, если приложение работает под отладчиком. Факт наличия активного отладчика проверяется
	// однократно при инициализации класса. Для релизных конфигураций функция всегда возвращает false
	static bool IsDebuggerActive();

	// Выводит сообщение в консоль отладчика
	static void DebugOutput(std::string_view msg);
	static void DebugOutput(std::wstring_view msg);
	// Разрешает или запрещает вывод в консоль отладчика. По
	// умолчанию вывод запрещён только для production сборок
	void EnableDebugOutput(bool enabled) { m_IsDebugOutputEnabled = enabled; }
	// Возвращает true, если вывод в консоль отладчика разрешён
	bool IsDebugOutputEnabled() const { return m_IsDebugOutputEnabled; }

	// Вызывает функцию OnAssert заданного обработчика
	static void OnAssert(const wchar_t* filePath, int line, const wchar_t* expression);

	// Вызывает функцию OnHalt заданного обработчика
	static void OnHalt(const wchar_t* filePath, int line, std::string_view msg);
	static void OnHalt(const wchar_t* filePath, int line, std::wstring_view msg);

	// Аварийно завершает работу приложения. Если пользовательский обработчик был установлен, то он
	// будет вызван с указанным значением кода ошибки exitCode. Если обрабтчик не был установлен,
	// или он вернёт управление, то будет вызвана функция стандартной библиотеки _exit(exitCode)
	[[noreturn]] static void Abort(int exitCode = 3);

	// Показывает модальное окно с сообщением об ошибке msgText и заголовком title
	static void ShowErrorMsgBox(std::wstring_view msgText, std::wstring_view title);

protected:
	DebugHelper();
	virtual ~DebugHelper() override;

	thread::CriticalSection m_CS;
	AssertHandler* m_AssertHandler = nullptr;
	AbortHandler m_AbortHandler = nullptr;
	bool m_IsDebugOutputEnabled = false;
	bool m_IsDebuggerActive = false;
};

} // namespace util
