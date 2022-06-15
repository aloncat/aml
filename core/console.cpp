//∙AML
// Copyright (C) 2017-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "console.h"

#include "array.h"
#include "winapi.h"

using namespace util;

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

	// Регистрирует флаг breakFlag в обработчике событий консольного окна и возвращает указатель на
	// обработчик. Параметр breakFlag (если задан) должен указывать на переменную типа bool, которую
	// обработчик установит в true, если пользователь нажмёт Ctrl-C или Ctrl-Break в окне консоли
	static HandlerFn GetHandler(volatile bool* breakFlag = nullptr);

	// Отменяет регистрацию указанного флага
	static void ReleaseHandler(volatile bool* breakFlag);

protected:
	static BOOL WINAPI Handler(DWORD ctrlType);

	unsigned m_RefCounter = 0;			// Счетчик использования обработчика
	thrd::CriticalSection m_CS;			// Крит. секция для установки флагов
	std::set<volatile bool*> m_Flags;	// Зарегистрированные флаги

	static inline CtrlHandler* s_This;
};

//--------------------------------------------------------------------------------------------------------------------------------
Console::CtrlHandler::HandlerFn Console::CtrlHandler::GetHandler(volatile bool* breakFlag)
{
	if (!s_This)
	{
		// При первом вызове создаём объект. Многопоточная синхронизация не нужна,
		// так как функция вызывается из критической секции конструктора Console
		s_This = new CtrlHandler;
	}

	if (breakFlag)
	{
		thrd::Lock lock(s_This->m_CS);
		s_This->m_Flags.insert(breakFlag);
	}

	++s_This->m_RefCounter;
	return Handler;
}

//--------------------------------------------------------------------------------------------------------------------------------
void Console::CtrlHandler::ReleaseHandler(volatile bool* breakFlag)
{
	if (s_This)
	{
		if (breakFlag)
		{
			auto& flags = s_This->m_Flags;
			thrd::Lock lock(s_This->m_CS);
			if (auto it = flags.find(breakFlag); it != flags.end())
				flags.erase(it);
		}

		// Если счётчик достиг 0, то уничтожаем объект. Многопоточная синхронизация не
		// нужна, так как функция вызывается из критической секции деструктора Console
		if (!(--s_This->m_RefCounter))
			AML_SAFE_DELETE(s_This);
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE BOOL Console::CtrlHandler::Handler(DWORD ctrlType)
{
	if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT)
	{
		if (s_This)
		{
			thrd::Lock lock(s_This->m_CS);
			for (auto breakFlag : s_This->m_Flags)
				*breakFlag = true;
		}
		return TRUE;
	}
	return FALSE;
}

#endif // AML_OS_WINDOWS

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Console - общие для всех платформ функции
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Console::IOLocks* Console::s_IOLocks;
thrd::CriticalSection* Console::s_MainCS;

unsigned Console::s_RefCounter;
bool Console::s_HasAllocatedConsole;
bool Console::s_HasSetCtrlHandler;

//--------------------------------------------------------------------------------------------------------------------------------
bool Console::IsCtrlCPressed(bool reset)
{
	PollInput(false);
	if (m_IsCtrlCPressed)
	{
		m_IsCtrlCPressed = !reset;
		return true;
	}
	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
void Console::InitMainCS()
{
	static uint8_t data[sizeof(thrd::CriticalSection)];
	s_MainCS = new(data) thrd::CriticalSection;
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
	// В самый первый раз инициализируем основную критическую
	// секцию; эта крит. секция никогда не будет уничтожена
	static int initOnce = (InitMainCS(), 0);
	thrd::Lock lock(s_MainCS);

	if (::GetConsoleWindow() == nullptr && ::AllocConsole())
		s_HasAllocatedConsole = true;

	HANDLE outHandle = ::GetStdHandle(STD_OUTPUT_HANDLE);
	if (outHandle && outHandle != INVALID_HANDLE_VALUE)
	{
		++s_RefCounter;

		if (!s_IOLocks)
		{
			s_IOLocks = new IOLocks;
		}

		DWORD mode;
		m_Info.outHandle = outHandle;
		m_Info.isRedirected = ::GetConsoleMode(outHandle, &mode) == 0;

		if (!m_Info.isRedirected)
		{
			CONSOLE_SCREEN_BUFFER_INFO bufferInfo;
			if (::GetConsoleScreenBufferInfo(m_Info.outHandle, &bufferInfo))
				m_Info.oldTextColor = bufferInfo.wAttributes & 0xff;

			CONSOLE_CURSOR_INFO cursorInfo;
			if (::GetConsoleCursorInfo(m_Info.outHandle, &cursorInfo))
				m_Info.isCursorVisible = cursorInfo.bVisible != 0;

			HANDLE inHandle = ::GetStdHandle(STD_INPUT_HANDLE);
			if (inHandle && inHandle != INVALID_HANDLE_VALUE)
			{
				m_Info.inHandle = inHandle;
				if (::GetConsoleMode(inHandle, &mode))
				{
					mode &= ~ENABLE_PROCESSED_INPUT;
					::SetConsoleMode(inHandle, mode);
				}

				InitKeyTTWin(m_KeyTT, util::CountOf(m_KeyTT));
			}
		}

		volatile bool* breakFlag = m_Info.isRedirected ? nullptr : &m_IsCtrlCPressed;
		auto handler = CtrlHandler::GetHandler(breakFlag);
		m_Info.ctrlHandler = handler;

		if (!m_Info.isRedirected && !s_HasSetCtrlHandler)
		{
			::SetConsoleCtrlHandler(handler, TRUE);
			s_HasSetCtrlHandler = true;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
Console::~Console()
{
	thrd::Lock lock(s_MainCS);

	if (m_Info.outHandle)
	{
		if (!m_Info.isRedirected)
		{
			WORD attr = static_cast<WORD>(m_Info.oldTextColor);
			::SetConsoleTextAttribute(m_Info.outHandle, attr);
			ShowCursor(m_Info.isCursorVisible);
		}

		if (!(--s_RefCounter))
		{
			if (s_HasSetCtrlHandler)
			{
				auto handler = static_cast<CtrlHandler::HandlerFn>(m_Info.ctrlHandler);
				::SetConsoleCtrlHandler(handler, FALSE);
				s_HasSetCtrlHandler = false;
			}

			AML_SAFE_DELETE(s_IOLocks);
		}

		// Освобождаем обработчик в последнюю очередь. Это имеет значение, когда счётчик стал равен 0, и вызов
		// должен разрушить его объект. Мы должны делать это только после отключения обработчика от окна консоли
		CtrlHandler::ReleaseHandler(&m_IsCtrlCPressed);
	}

	if (!s_RefCounter && s_HasAllocatedConsole)
	{
		::FreeConsole();
		s_HasAllocatedConsole = false;
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void Console::Write(std::string_view str, int color)
{
	const size_t strLen = str.size();
	if (!m_Info.outHandle || !strLen || strLen > INT_MAX)
		return;

	thrd::Lock lock(s_IOLocks->output);

	if (m_Info.isRedirected)
	{
		DWORD count = static_cast<DWORD>(strLen);
		::WriteFile(m_Info.outHandle, str.data(), count, &count, nullptr);
	} else
	{
		SetColor(color);

		util::SmartArray<char, 640> buffer(strLen);
		DWORD count = static_cast<DWORD>(strLen);
		::CharToOemBuffA(str.data(), buffer, count);

		::WriteConsoleA(m_Info.outHandle, buffer, count, &count, nullptr);
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void Console::Write(std::wstring_view str, int color)
{
	const size_t strLen = str.size();
	if (!m_Info.outHandle || !strLen || strLen > INT_MAX)
		return;

	thrd::Lock lock(s_IOLocks->output);

	if (m_Info.isRedirected)
	{
		int len = static_cast<int>(strLen);
		if (int expectedSize = ::WideCharToMultiByte(CP_ACP, 0, str.data(), len, nullptr, 0, nullptr, nullptr); expectedSize > 0)
		{
			util::SmartArray<char, 640> buffer(expectedSize);
			if (len = ::WideCharToMultiByte(CP_ACP, 0, str.data(), len, buffer, expectedSize, nullptr, nullptr); len > 0)
			{
				DWORD bytesWritten;
				::WriteFile(m_Info.outHandle, buffer, len, &bytesWritten, nullptr);
			}
		}
	} else
	{
		SetColor(color);

		DWORD count = static_cast<DWORD>(strLen);
		::WriteConsoleW(m_Info.outHandle, str.data(), count, &count, nullptr);
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void Console::ShowCursor(bool visible)
{
	if (m_Info.outHandle)
	{
		CONSOLE_CURSOR_INFO cursorInfo;
		if (::GetConsoleCursorInfo(m_Info.outHandle, &cursorInfo) && visible != (cursorInfo.bVisible != 0))
		{
			cursorInfo.bVisible = visible ? TRUE : FALSE;
			::SetConsoleCursorInfo(m_Info.outHandle, &cursorInfo);
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
bool Console::GetInputEvent(KeyEvent& event)
{
	thrd::Lock lock(s_IOLocks->input);

	PollInput(false);
	if (!m_InputEvents.empty())
	{
		event = m_InputEvents.front();
		m_InputEvents.pop_front();
		return true;
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
void Console::ClearEvents()
{
	thrd::Lock lock(s_IOLocks->input);

	PollInput(true);
	m_InputEvents.clear();
}

//--------------------------------------------------------------------------------------------------------------------------------
void Console::SetTitle(ZStringView title)
{
	if (m_Info.outHandle && !m_Info.isRedirected)
		::SetConsoleTitleA(title.c_str());
}

//--------------------------------------------------------------------------------------------------------------------------------
void Console::SetTitle(WZStringView title)
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
bool Console::CheckPollTime()
{
	const DWORD timeStamp = ::GetTickCount();
	// Значение, возвращаемое функцией GetTickCount, обычно меняется с интервалом около 15
	// миллисекунд. Поэтому события ввода будут обрабатываться не чаще, чем раз в 15 ms
	bool canPoll = m_LastPollTime != timeStamp;
	m_LastPollTime = timeStamp;
	return canPoll;
}

//--------------------------------------------------------------------------------------------------------------------------------
void Console::PollInput(bool forcePoll)
{
	if (!m_Info.inHandle)
		return;

	thrd::Lock lock(s_IOLocks->input);

	if (CheckPollTime() || forcePoll)
	{
		DWORD totalEvents = 0;
		if (::GetNumberOfConsoleInputEvents(m_Info.inHandle, &totalEvents) && totalEvents)
		{
			const DWORD MAX_EVENTS = 24;
			INPUT_RECORD eventBuffer[MAX_EVENTS];

			while (totalEvents)
			{
				DWORD eventCount = 0;
				if (::ReadConsoleInputA(m_Info.inHandle, eventBuffer, MAX_EVENTS, &eventCount) == 0 || !eventCount)
					break;

				// Иногда мы можем прочитать больше событий, чем ожидали. Так происходит, когда между
				// получением количества событий и их чтением в буфер успевает поступить новое событие
				totalEvents -= (eventCount < totalEvents) ? eventCount : totalEvents;

				for (unsigned i = 0; i < eventCount; ++i)
				{
					INPUT_RECORD& event = eventBuffer[i];
					if (event.EventType == KEY_EVENT)
					{
						KeyEvent e;
						const WORD keyCode = event.Event.KeyEvent.wVirtualKeyCode;
						e.vkey = m_KeyTT[keyCode & 0xff];

						e.flags = event.Event.KeyEvent.bKeyDown ? 1 : 0;
						const DWORD ctrlState = event.Event.KeyEvent.dwControlKeyState;
						e.flags |= (ctrlState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) ? ALT_DOWN : 0;
						e.flags |= (ctrlState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) ? CTRL_DOWN : 0;
						e.flags |= (ctrlState & SHIFT_PRESSED) ? SHIFT_DOWN : 0;

						if (e.vkey == KEY_C && e.IsKeyDown() && e.IsCtrlDown())
							m_IsCtrlCPressed = true;

						// При заполнении буфера игнорируем новые события
						if (e.vkey && m_InputEvents.size() < MAX_KEY_EVENTS)
							m_InputEvents.push_back(e);
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
void Console::Write(std::string_view str, int color) {}
void Console::Write(std::wstring_view str, int color) {}
void Console::ShowCursor(bool visible) {}
bool Console::GetInputEvent(KeyEvent& event) { return false; }
void Console::ClearEvents() {}
void Console::SetTitle(ZStringView title) {}
void Console::SetTitle(WZStringView title) {}
void Console::PollInput() {}
#endif
