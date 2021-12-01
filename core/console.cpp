//∙AML
// Copyright (C) 2017-2021 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "console.h"

#include "array.h"
#include "debug.h"
#include "winapi.h"

using namespace util;

// TODO: класс Console имеет глобальные недоработки: конструктор всегда подключается к стандартной
// консоли (т.е. ведёт себя корректно только для случая единственного объекта Console в консольном
// приложении). Для GUI приложения окно консоли сейчас вообще не создаётся

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Console::CtrlHandler - Windows
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if AML_OS_WINDOWS

//--------------------------------------------------------------------------------------------------------------------------------
struct Console::CtrlHandler
{
	using HandlerFn = BOOL (WINAPI*)(DWORD);

	// Резервирует обработчик событий консольного окна и возвращает указатель на него. Параметр breakFlag указывает
	// на переменную типа bool, которую обработчик устанавит в true, если пользователь нажмёт Ctrl-C или Ctrl-Break
	static HandlerFn GetHandler(volatile bool* breakFlag);

	// Освобождает зарезервированный ранее обработчик для указанной переменной breakFlag
	static void ReleaseHandler(volatile bool* breakFlag);

protected:
	// Значение MAX_HANDLERS определяет, сколько из одновременно открытых окон будут устанавливать флаг нажатия
	// Ctrl-C/Ctrl-Break. Окна, открытые сверх этого количества, не будут обрабатывать нажатия этих комбинаций
	static constexpr size_t MAX_HANDLERS = 8;

	template<size_t idx> static BOOL WINAPI HandlerN(DWORD ctrlType)
	{
		return Handler(idx, ctrlType);
	}

	static AML_NOINLINE BOOL Handler(size_t index, DWORD ctrlType);

	static void Init();

protected:
	static inline volatile bool* s_Flags[MAX_HANDLERS];
	static inline thread::CriticalSection* s_CS = nullptr;
};

//--------------------------------------------------------------------------------------------------------------------------------
Console::CtrlHandler::HandlerFn Console::CtrlHandler::GetHandler(volatile bool* breakFlag)
{
	static int initOnce = (Init(), 0);

	if (!breakFlag || !s_CS)
		return nullptr;

	thread::Lock lock(s_CS);
	for (size_t idx = 0; idx < MAX_HANDLERS; ++idx)
	{
		if (!s_Flags[idx])
		{
			s_Flags[idx] = breakFlag;

			switch (idx)
			{
				case 0: return HandlerN<0>;
				case 1: return HandlerN<1>;
				case 2: return HandlerN<2>;
				case 3: return HandlerN<3>;
				case 4: return HandlerN<4>;
				case 5: return HandlerN<5>;
				case 6: return HandlerN<6>;
				case 7: return HandlerN<7>;

				default:
					// NB: при изменении значения MAX_HANDLERS нужно
					// также внести изменения в список вариантов выше
					Assert(idx >= MAX_HANDLERS && "Incomplete switch statement");
			}
		}
	}

	// Если свободных обработчиков нет, то вернём "общий"
	// обработчик, который не устанавливает никакой флаг
	return HandlerN<MAX_HANDLERS>;
}

//--------------------------------------------------------------------------------------------------------------------------------
void Console::CtrlHandler::ReleaseHandler(volatile bool* breakFlag)
{
	if (breakFlag && s_CS)
	{
		thread::Lock lock(s_CS);
		for (size_t idx = 0; idx < MAX_HANDLERS; ++idx)
		{
			if (s_Flags[idx] == breakFlag)
				s_Flags[idx] = nullptr;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE BOOL Console::CtrlHandler::Handler(size_t index, DWORD ctrlType)
{
	if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT)
	{
		if (index < MAX_HANDLERS && s_CS)
		{
			thread::Lock lock(s_CS);
			if (volatile bool* breakFlag = s_Flags[index])
				*breakFlag = true;
		}
		return TRUE;
	}
	return FALSE;
}

//--------------------------------------------------------------------------------------------------------------------------------
void Console::CtrlHandler::Init()
{
	static uint8_t data[sizeof(thread::CriticalSection)];
	s_CS = new(data) thread::CriticalSection;

	atexit([]() {
		s_CS->~CriticalSection();
		s_CS = nullptr;
	});
}

#endif // AML_OS_WINDOWS

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Console - общие для всех платформ функции
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void Console::Write(const char* str, int color)
{
	Write(str, str ? strlen(str) : 0, color);
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void Console::Write(const wchar_t* str, int color)
{
	Write(str, str ? wcslen(str) : 0, color);
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void Console::Write(const std::string& str, int color)
{
	Write(str.c_str(), str.size(), color);
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void Console::Write(const std::wstring& str, int color)
{
	Write(str.c_str(), str.size(), color);
}

//--------------------------------------------------------------------------------------------------------------------------------
bool Console::IsCtrlCPressed(bool reset)
{
	PollInput();
	if (m_IsCtrlCPressed)
	{
		m_IsCtrlCPressed = !reset;
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Console - Windows
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if AML_OS_WINDOWS

//--------------------------------------------------------------------------------------------------------------------------------
Console::Console()
{
	HANDLE outHandle = ::GetStdHandle(STD_OUTPUT_HANDLE);
	if (outHandle && outHandle != INVALID_HANDLE_VALUE)
	{
		DWORD mode;
		m_Info.outHandle = outHandle;
		m_Info.isRedirected = ::GetConsoleMode(outHandle, &mode) == 0;

		if (!m_Info.isRedirected)
		{
			CONSOLE_SCREEN_BUFFER_INFO info;
			if (::GetConsoleScreenBufferInfo(m_Info.outHandle, &info))
				m_Info.oldTextColor = info.wAttributes & 0xff;

			HANDLE inHandle = ::GetStdHandle(STD_INPUT_HANDLE);
			if (inHandle && inHandle != INVALID_HANDLE_VALUE)
			{
				m_Info.inHandle = inHandle;
				if (::GetConsoleMode(inHandle, &mode))
				{
					mode &= ~ENABLE_PROCESSED_INPUT;
					::SetConsoleMode(inHandle, mode);
				}
			}

			if (auto handler = CtrlHandler::GetHandler(&m_IsCtrlCPressed))
			{
				m_Info.ctrlHandler = handler;
				::SetConsoleCtrlHandler(handler, TRUE);
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
Console::~Console()
{
	if (m_Info.outHandle && !m_Info.isRedirected)
	{
		if (auto handler = static_cast<CtrlHandler::HandlerFn>(m_Info.ctrlHandler))
		{
			::SetConsoleCtrlHandler(handler, FALSE);
			CtrlHandler::ReleaseHandler(&m_IsCtrlCPressed);
		}

		WORD attr = static_cast<WORD>(m_Info.oldTextColor);
		::SetConsoleTextAttribute(m_Info.outHandle, attr);
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
bool Console::GetInputEvent(KeyEvent& event)
{
	thread::Lock lock(m_InputCS);

	if (!m_InputEvents.empty())
	{
		event = m_InputEvents.front();
		m_InputEvents.pop_front();
		return true;
	}
	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
void Console::SetTitle(const std::string& title)
{
	if (m_Info.outHandle && !m_Info.isRedirected)
		::SetConsoleTitleA(title.c_str());
}

//--------------------------------------------------------------------------------------------------------------------------------
void Console::SetTitle(const std::wstring& title)
{
	if (m_Info.outHandle && !m_Info.isRedirected)
		::SetConsoleTitleW(title.c_str());
}

//--------------------------------------------------------------------------------------------------------------------------------
void Console::SetColor(int color)
{
	color &= 0xff;
	if (m_TextColor != color)
	{
		m_TextColor = color;
		WORD attr = static_cast<WORD>(color);
		::SetConsoleTextAttribute(m_Info.outHandle, attr);
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void Console::Write(const char* str, size_t strLen, int color)
{
	if (!m_Info.outHandle || !str || !strLen || strLen > INT_MAX)
		return;

	thread::Lock lock(m_OutputCS);

	if (m_Info.isRedirected)
	{
		DWORD count = static_cast<DWORD>(strLen);
		::WriteFile(m_Info.outHandle, str, count, &count, nullptr);
	} else
	{
		util::SmartArray<char> buffer(strLen + 1);
		::CharToOemA(str, buffer);

		SetColor(color);
		DWORD count = static_cast<DWORD>(strLen);
		::WriteConsoleA(m_Info.outHandle, buffer, count, &count, nullptr);
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void Console::Write(const wchar_t* str, size_t strLen, int color)
{
	if (!m_Info.outHandle || !str || !strLen || strLen > INT_MAX)
		return;

	thread::Lock lock(m_OutputCS);

	if (m_Info.isRedirected)
	{
		int len = static_cast<int>(strLen);
		if (int expectedSize = ::WideCharToMultiByte(CP_ACP, 0, str, len, nullptr, 0, nullptr, nullptr); expectedSize > 0)
		{
			util::SmartArray<char> buffer(expectedSize);
			if (len = ::WideCharToMultiByte(CP_ACP, 0, str, len, buffer, expectedSize, nullptr, nullptr); len > 0)
			{
				DWORD bytesWritten;
				::WriteFile(m_Info.outHandle, buffer, len, &bytesWritten, nullptr);
			}
		}
	} else
	{
		SetColor(color);
		DWORD count = static_cast<DWORD>(strLen);
		::WriteConsoleW(m_Info.outHandle, str, count, &count, nullptr);
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
bool Console::CheckPollTime()
{
	const DWORD timeStamp = ::GetTickCount();
	// NB: значение, возвращаемое функцией GetTickCount, обычно меняется с интервалом около
	// 15 ms. Поэтому события ввода будут обрабатываться не чаще, чем раз в примерно 15 ms
	bool canPoll = m_LastPollTime != timeStamp;
	m_LastPollTime = timeStamp;
	return canPoll;
}

//--------------------------------------------------------------------------------------------------------------------------------
void Console::PollInput()
{
	if (!m_Info.inHandle)
		return;

	thread::Lock lock(m_InputCS);

	DWORD totalEventC = 0;
	if (CheckPollTime() && ::GetNumberOfConsoleInputEvents(m_Info.inHandle, &totalEventC) && totalEventC)
	{
		const DWORD MAX_EVENT_C = 24;
		INPUT_RECORD eventBuffer[MAX_EVENT_C];

		while (totalEventC)
		{
			DWORD eventC = 0;
			if (::ReadConsoleInputA(m_Info.inHandle, eventBuffer, MAX_EVENT_C, &eventC) == 0 || !eventC)
				break;

			// NB: иногда мы можем прочитать больше событий, чем ожидали. Так происходит, когда между
			// получением количества событий и их чтением в буфер успевает поступить новое событие
			totalEventC -= (eventC < totalEventC) ? eventC : totalEventC;

			for (unsigned i = 0; i < eventC; ++i)
			{
				INPUT_RECORD& event = eventBuffer[i];
				if (event.EventType == KEY_EVENT)
				{
					const WORD vkey = event.Event.KeyEvent.wVirtualKeyCode;
					const bool isKeyDown = event.Event.KeyEvent.bKeyDown == TRUE;
					const DWORD ctrlState = event.Event.KeyEvent.dwControlKeyState;
					const bool isCtrlDown = (ctrlState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;

					if (vkey == 'C' && isKeyDown && isCtrlDown)
						m_IsCtrlCPressed = true;
					// При заполнении буфера игнорируем новые события
					else if (m_InputEvents.size() < MAX_KEY_EVENTS)
					{
						// TODO: такая обработка событий ввода не совсем корректна, так как мы не учитываем значение
						// повторов event.Event.KeyEvent.wRepeatCount. И ещё, было бы неплохо сделать свой enum с
						// кодами, чтобы унифицировать их для разных платформ как в подсистеме ввода Sparky
						m_InputEvents.push_back(KeyEvent { vkey, isKeyDown, isCtrlDown });
					}
				}
			}
		}
	}
}

#endif // AML_OS_WINDOWS

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Console - заглушка для всех остальных платформ
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if !AML_OS_WINDOWS
Console::Console() {}
Console::~Console() {}
bool Console::GetInputEvent(KeyEvent& event) { return false; }
void Console::SetTitle(const std::string& title) {}
void Console::SetTitle(const std::wstring& title) {}
void Console::SetColor(int color) {}
void Console::Write(const char* str, size_t strLen, int color) {}
void Console::Write(const wchar_t* str, size_t strLen, int color) {}
bool Console::CheckPollTime() { return false; }
void Console::PollInput() {}
#endif
