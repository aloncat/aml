﻿//∙AML
// Copyright (C) 2018-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "debug.h"

#include "array.h"
#include "exception.h"
#include "filesystem.h"
#include "strcommon.h"
#include "strformat.h"
#include "strutil.h"
#include "sysinfo.h"
#include "winapi.h"

using namespace util;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   AssertHandler
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
AssertHandler::Result AssertHandler::OnError(Reason reason, std::wstring_view filePath, int line, std::wstring_view text)
{
	if (filePath.empty())
		filePath = L"[no file]";

	if (text.empty())
	{
		text = (reason == Reason::AssertFailed) ?
			L"[no expression]" : L"[no message]";
	}

	auto errorMsg = FormatMsg(reason, false, filePath, line, text);
	LogError(errorMsg);

	if (IsProductionBuild())
		return Result::Skip;

	// TODO: тут хотелось бы кастомное окно, в котором будут три кнопки ("Skip", "Skip all" и "Terminate") и текст
	// сообщения об ошибке. Возможно также стоит добавить кнопку "Copy to clipboard" для копирования текста сообщения
	// в буфер обмена. Не забыть о проблеме обработки сообщений главного окна (главного потока): если мы будем ожидать
	// в этой функции нажатия пользователем кнопки в окне, то нужно как-то продолжать обработку очереди сообщений.
	// Пока же мы будем использовать простой MessageBox с одной кнопкой OK, выполняющей роль кнопки "Skip"

	#if AML_OS_WINDOWS
		if (SystemInfo::Instance().IsConsoleApp())
		{
			auto msg = ToAnsi(errorMsg);
			if (reason == Reason::AssertFailed)
				throw EAssertion(msg);
			else
				throw EHalt(msg);
		}
		else if (!DebugHelper::IsDebuggerActive())
		{
			errorMsg = FormatMsg(reason, true, filePath, line, text);
			DebugHelper::ShowErrorMsgBox(errorMsg, (reason == Reason::AssertFailed) ?
				L"Assertion failed" : L"Halt occured");
		}

		return Result::Skip;
	#else
		#error Not implemented
	#endif
}

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring AssertHandler::FormatMsg(Reason reason, bool forMsgBox, std::wstring_view filePath, int line, std::wstring_view text)
{
	// TODO
	return std::wstring();
}

//--------------------------------------------------------------------------------------------------------------------------------
void AssertHandler::LogError(std::wstring_view msg)
{
	// TODO
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   DebugHelper
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
DebugHelper::DebugHelper()
{
	// NB: так как функция Abort, а также функции AssertHelper обращаются к синглтону SystemInfo, то мы бы
	// хотели, чтобы к моменту вызова Abort или обработки макросов Assert/Verify/Halt он уже был готов, а не
	// инициализировался по требованию. Поэтому проинициализируем его сейчас, если это ещё не было сделано
	SystemInfo::Instance();

	#if !AML_PRODUCTION
		// Создаём обработчик по умолчанию, реализующий дефолтную обработку Assert/Verify/Halt. Позже по
		// необходимости этот обработчик может быть заменён другим при помощи функции SetAssertHandler
		m_AssertHandler = new AssertHandler;
		// Вывод в консоль отладчика разрешён во всех сборках, кроме production
		m_IsDebugOutputEnabled = true;
	#endif

	#if AML_OS_WINDOWS && AML_DEBUG
		if (::IsDebuggerPresent())
		{
			m_IsDebuggerActive = true;
			// Для отладочных конфигураций при активном отладчике мы всегда хотим видеть сообщение
			// об ошибке в окне, не зависимо от того, консольное это приложение или GUI-приложение
			_set_error_mode(_OUT_TO_MSGBOX);
		}
	#endif
}

//--------------------------------------------------------------------------------------------------------------------------------
DebugHelper::~DebugHelper()
{
	AML_SAFE_DELETE(m_AssertHandler);
}

//--------------------------------------------------------------------------------------------------------------------------------
void DebugHelper::SetAbortHandler(AbortHandler handler)
{
	m_AbortHandler = handler;
}

//--------------------------------------------------------------------------------------------------------------------------------
void DebugHelper::SetAssertHandler(AssertHandler* handler)
{
	AML_SAFE_DELETE(m_AssertHandler);
	m_AssertHandler = handler;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE bool DebugHelper::IsDebuggerActive()
{
	return Instance().m_IsDebuggerActive;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void DebugHelper::DebugOutput(std::string_view msg)
{
	#if AML_OS_WINDOWS
		DebugHelper& instance = Instance();
		if (instance.m_IsDebugOutputEnabled && !msg.empty())
		{
			ZString<char, 1024> text(msg);
			thread::Lock lock(instance.m_CS);
			::OutputDebugStringA(text.c_str());
		}
	#else
		#error Not implemented
	#endif
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void DebugHelper::DebugOutput(std::wstring_view msg)
{
	#if AML_OS_WINDOWS
		const size_t count = msg.size();
		DebugHelper& instance = Instance();
		if (instance.m_IsDebugOutputEnabled && count)
		{
			char localBuffer[1024];
			FlexibleArray buffer(localBuffer);
			int outputLen = -1;

			// Из соображений производительности для коротких строк мы будем пытаться сделать преобразование за
			// один вызов. Но если буфер окажется недостаточен, мы получим ошибку и тогда перейдём к стандартному
			// способу. Чтобы снизить вероятность недостаточности буфера (и "лишней" попытки) мы выбрали здесь
			// максимальную длину строки в 1/2 размера буфера
			if (count <= CountOf(localBuffer) / 2)
			{
				outputLen = ToAnsi(buffer, CountOf(localBuffer) - 1, msg);
			}

			// Стандратный (более медленный способ) за 2 вызова: сначала получаем размер необходимого буфера,
			// а затем выполняем преобразование. Если необходимый размер окажется меньше размера локального
			// буфера buffer, то нам даже не придётся выделять память в куче под буфер большего размера
			if (outputLen < 0 && count <= INT_MAX)
			{
				const int bufLen = ToAnsi(nullptr, 0, msg);
				if (bufLen > 0)
				{
					buffer.Grow(static_cast<size_t>(bufLen) + 1);
					outputLen = ToAnsi(buffer, bufLen, msg);
				}
			}

			if (outputLen > 0)
			{
				buffer[outputLen] = 0;
				thread::Lock lock(instance.m_CS);
				::OutputDebugStringA(buffer);
			}
		}
	#else
		#error Not implemented
	#endif
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void DebugHelper::OnAssert(const wchar_t* filePath, int line, const wchar_t* expression)
{
	if (!filePath || !filePath[0])
		filePath = L"[no file]";
	if (!expression || !expression[0])
		expression = L"[no expression]";

	const DebugHelper& instance = Instance();
	auto result = AssertHandler::Result::Skip;
	if (auto handler = instance.m_AssertHandler)
	{
		result = handler->OnError(AssertHandler::Reason::AssertFailed,
			filePath, line, expression);
	}

	// TODO: сейчас мы ничего не делаем с возвращаемым значением result (выбором действия пользователем). Позже нужно
	// будет добавить вычисление хеша от трёх параметров, добавление его в std::set и проверку наличия этого хеша в
	// наборе перед вызовом обработчика, чтобы в случае выбора пользователем SkipAll не выводить сообщение повторно
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void DebugHelper::OnHalt(const wchar_t* filePath, int line, std::string_view msg)
{
	auto text = FromAnsi(msg);
	OnHalt(filePath, line, text);
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void DebugHelper::OnHalt(const wchar_t* filePath, int line, std::wstring_view msg)
{
	if (!filePath || !filePath[0])
		filePath = L"[no file]";
	if (msg.empty())
		msg = L"[no message]";

	const DebugHelper& instance = Instance();
	auto result = AssertHandler::Result::Skip;
	if (auto handler = instance.m_AssertHandler)
	{
		result = handler->OnError(AssertHandler::Reason::HaltInvoked,
			filePath, line, msg);
	}

	// TODO: см. комментарий к result в функции OnAssert
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void DebugHelper::Abort(int exitCode)
{
	static int firstExitCode = exitCode;
	if (DebugHelper::InstanceExists())
	{
		AML_DBG_BREAK;
		auto& instance = Instance();
		instance.m_CS.Enter();

		static int enteredC = 0;
		if (enteredC++ == 0)
		{
			auto errorText = L"[DebugHelper] Abort has been called";

			// TODO: вывод в системный журнал и консоль отладчика

			if (instance.m_AbortHandler)
			{
				instance.m_AbortHandler(firstExitCode);
			}

			// Если пользовательский обработчик не установлен или вернул управление,
			// то перед аварийным завершением работы выведем сообщение об ошибке
			const auto& appPath = SystemInfo::Instance().GetAppExePath();
			std::wstring exeFileName = FileSystem::ExtractFullName(appPath);
			std::wstring errorMessage = Format(L"Application \"%s\" has been terminated due to a"
				L" fatal error. Please contact the developer for support", exeFileName.c_str());
			ShowErrorMsgBox(errorMessage, L"Fatal error");
		}
	}

	_exit(firstExitCode);
}

//--------------------------------------------------------------------------------------------------------------------------------
void DebugHelper::ShowErrorMsgBox(std::wstring_view msgText, std::wstring_view title)
{
	if (msgText.empty())
		msgText = L"Unknown error has occured";

	if (title.empty())
		title = L"Error";

	#if AML_OS_WINDOWS
		ZString<wchar_t, 120> zTitle(title);
		ZString<wchar_t, 840> zMsgText(msgText);
		::MessageBoxW(nullptr, zMsgText.c_str(), zTitle.c_str(), MB_ICONERROR | MB_OK);
	#else
		#error Not implemented
	#endif
}
