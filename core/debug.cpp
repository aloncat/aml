//∙AML
// Copyright (C) 2018-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "debug.h"

#include "array.h"
#include "datetime.h"
#include "exception.h"
#include "fasthash.h"
#include "filesystem.h"
#include "log.h"
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

	// Перед тем, как выбросить исключение, показать окно с сообщением или приостановить
	// выполнение, убедимся, что возникшая ошибка выведена в файл системного журнала
	if (SystemLog::InstanceExists())
		SystemLog::Instance().Flush();

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
			return ShowPopup(reason, errorMsg);
		}

		return s_DefaultAction;
	#else
		#error Not implemented
	#endif
}

//--------------------------------------------------------------------------------------------------------------------------------
void AssertHandler::OnTerminate()
{
	LogError(L"User has initiated program termination");

	// По умолчанию завершаем работу с помощью вызова функции _exit(). По
	// желанию в наследнике класса может быть реализовано иное поведение
	_exit(3);
}

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring AssertHandler::FormatMsg(Reason reason, bool forMsgBox, std::wstring_view filePath, int line, std::wstring_view text)
{
	Formatter<wchar_t> fmt;

	if (reason == Reason::AssertFailed)
		fmt << L"Assertion failed";
	else if (reason == Reason::HaltInvoked)
		fmt << L"Halt occured";

	if (forMsgBox)
	{
		fmt << L" in file:\n    \"" << filePath << L"\"\n    at line " << line << L":\n\n";
		if (reason == Reason::AssertFailed)
			fmt << L"Expression:\n";
		else if (reason == Reason::HaltInvoked)
			fmt << L"Message:\n";
	} else
	{
		fmt << L" in file \"" << filePath << L"\", line " << line << L": ";
	}
	fmt << text;

	return fmt.ToString();
}

//--------------------------------------------------------------------------------------------------------------------------------
void AssertHandler::LogError(std::wstring_view msg)
{
	if (msg.empty())
		return;

	// Если синглтон системного журнала существует (уже был создан, но ещё не был уничтожен),
	// то выведем сообщение в журнал. В консоль отладчика оно будет выведено автоматически
	if (SystemLog::InstanceExists() && SystemLog::Instance().IsOpened())
	{
		*LogRecordHolder(SystemLog::Instance(), Log::MsgType::Error) << msg;
	}
	// Если синглтон системного журнала не существует, то выведем сообщение только в консоль отладчика
	else if (DebugHelper::Instance().IsDebugOutputEnabled())
	{
		auto time = DateTime::Now();
		auto header = LogRecord::FormatHeader(Log::MsgType::Error, time);
		DebugHelper::DebugOutput(std::move(header) + msg + L'\n');
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
AssertHandler::Result AssertHandler::ShowPopup(Reason reason, std::wstring_view errorMsg)
{
	DebugHelper::ShowErrorMsgBox(errorMsg, (reason == Reason::AssertFailed) ?
		L"Assertion failed" : L"Halt occured");

	return s_DefaultAction;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   DebugHelper
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
DebugHelper::DebugHelper()
{
	// Так как функция Abort, а также функции AssertHelper обращаются к синглтону SystemInfo, то мы бы
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
			// об ошибке в окне, независимо от того, консольное это приложение или GUI-приложение
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
			ZExStringView<char, 1024> text(msg);
			thrd::Lock lock(instance.m_CS);
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
				thrd::Lock lock(instance.m_CS);
				::OutputDebugStringA(buffer);
			}
		}
	#else
		#error Not implemented
	#endif
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE bool DebugHelper::OnAssert(const wchar_t* filePath, int line, const wchar_t* expression)
{
	if (!filePath || !filePath[0])
		filePath = L"[no file]";
	if (!expression || !expression[0])
		expression = L"[no expression]";

	DebugHelper& instance = Instance();
	if (auto handler = instance.m_AssertHandler)
	{
		const auto hash = instance.GetErrorHash(filePath, line, expression);
		if (instance.m_IgnoredErrors.find(hash) != instance.m_IgnoredErrors.end())
			return false;

		auto result = handler->OnError(AssertHandler::Reason::AssertFailed,
			filePath, line, expression);

		if (result == AssertHandler::Result::SkipAll)
			instance.m_IgnoredErrors.insert(hash);
		else if (result == AssertHandler::Result::Terminate)
			instance.Terminate();
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE bool DebugHelper::OnHalt(const wchar_t* filePath, int line, std::string_view msg)
{
	auto text = FromAnsi(msg);
	return OnHalt(filePath, line, text);
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE bool DebugHelper::OnHalt(const wchar_t* filePath, int line, std::wstring_view msg)
{
	if (!filePath || !filePath[0])
		filePath = L"[no file]";
	if (msg.empty())
		msg = L"[no message]";

	DebugHelper& instance = Instance();
	if (auto handler = instance.m_AssertHandler)
	{
		const auto hash = instance.GetErrorHash(filePath, line, msg);
		if (instance.m_IgnoredErrors.find(hash) != instance.m_IgnoredErrors.end())
			return false;

		auto result = handler->OnError(AssertHandler::Reason::HaltInvoked,
			filePath, line, msg);

		if (result == AssertHandler::Result::SkipAll)
			instance.m_IgnoredErrors.insert(hash);
		else if (result == AssertHandler::Result::Terminate)
			instance.Terminate();
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void DebugHelper::Abort(int exitCode)
{
	static const int firstExitCode = exitCode;

	if (DebugHelper::InstanceExists())
	{
		AML_DBG_BREAK;

		Instance().m_CS.Enter();

		if (static int entranceCount = 0; !entranceCount++)
		{
			auto errorText = L"[DebugHelper] Abort has been called";

			if (SystemLog::InstanceExists() && SystemLog::Instance().IsOpened())
			{
				*LogRecordHolder(SystemLog::Instance(), Log::MsgType::Error) << errorText;
				SystemLog::Instance().Flush();
			}
			else if (Instance().m_IsDebugOutputEnabled)
			{
				auto time = DateTime::Now();
				auto header = LogRecord::FormatHeader(Log::MsgType::Error, time);
				DebugOutput(std::move(header) + errorText + L'\n');
			}

			if (auto handler = Instance().m_AbortHandler)
				handler(firstExitCode);

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
		ZExStringView<wchar_t, 120> zTitle(title);
		ZExStringView<wchar_t, 840> zMsgText(msgText);
		::MessageBoxW(nullptr, zMsgText.c_str(), zTitle.c_str(), MB_ICONERROR | MB_OK);
	#else
		#error Not implemented
	#endif
}

//--------------------------------------------------------------------------------------------------------------------------------
unsigned DebugHelper::GetErrorHash(const wchar_t* filePath, int line, std::wstring_view msg)
{
	Formatter<wchar_t> fmt;

	fmt << filePath << '\n';
	fmt << line << '\n';
	fmt << msg;

	return hash::GetFastHash(fmt, false);
}

//--------------------------------------------------------------------------------------------------------------------------------
void DebugHelper::Terminate()
{
	thrd::Lock lock(m_CS);

	if (m_AssertHandler && !m_IsTerminating)
	{
		m_IsTerminating = true;
		m_AssertHandler->OnTerminate();
	}
}
