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
	thread::CriticalSection m_CS;		// Крит. секция для установки флагов
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
		thread::Lock lock(s_This->m_CS);
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
			thread::Lock lock(s_This->m_CS);
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
			thread::Lock lock(s_This->m_CS);
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
thread::CriticalSection* Console::s_MainCS;

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
	static uint8_t data[sizeof(thread::CriticalSection)];
	s_MainCS = new(data) thread::CriticalSection;
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
	thread::Lock lock(s_MainCS);

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
				InitKeyTT();
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
	thread::Lock lock(s_MainCS);

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

		// NB: освобождаем обработчик в последнюю очередь. Это имеет значение, когда счётчик стал равен 0, и вызов
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

	thread::Lock lock(s_IOLocks->output);

	if (m_Info.isRedirected)
	{
		DWORD count = static_cast<DWORD>(strLen);
		::WriteFile(m_Info.outHandle, str.data(), count, &count, nullptr);
	} else
	{
		SetColor(color);

		util::SmartArray<char> buffer(strLen);
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

	thread::Lock lock(s_IOLocks->output);

	if (m_Info.isRedirected)
	{
		int len = static_cast<int>(strLen);
		if (int expectedSize = ::WideCharToMultiByte(CP_ACP, 0, str.data(), len, nullptr, 0, nullptr, nullptr); expectedSize > 0)
		{
			util::SmartArray<char> buffer(expectedSize);
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
	thread::Lock lock(s_IOLocks->input);

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
	thread::Lock lock(s_IOLocks->input);

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
void Console::InitKeyTT()
{
	AML_FILLA(m_KeyTT, 0, util::CountOf(m_KeyTT));

	for (int i = 0; i < 10; ++i)
		m_KeyTT['0' + i] = (KEY_0 + i) & 0xff;
	for (int i = 0; i < 26; ++i)
		m_KeyTT['A' + i] = (KEY_A + i) & 0xff;
	for (int i = 0; i < 12; ++i)
		m_KeyTT[VK_F1 + i] = (KEY_F1 + i) & 0xff;

	m_KeyTT[VK_MENU]		= KEY_ALT;
	m_KeyTT[VK_CONTROL]		= KEY_CTRL;
	m_KeyTT[VK_SHIFT]		= KEY_SHIFT;

	m_KeyTT[VK_NUMLOCK]		= KEY_NUM;
	m_KeyTT[VK_CAPITAL]		= KEY_CAPS;
	m_KeyTT[VK_SCROLL]		= KEY_SCROLL;

	m_KeyTT[VK_ESCAPE]		= KEY_ESC;
	m_KeyTT[VK_TAB]			= KEY_TAB;
	m_KeyTT[VK_SPACE]		= KEY_SPACE;
	m_KeyTT[VK_BACK]		= KEY_BACK;
	m_KeyTT[VK_RETURN]		= KEY_ENTER;

	m_KeyTT[VK_SNAPSHOT]	= KEY_PRINT;
	m_KeyTT[VK_PAUSE]		= KEY_PAUSE;

	m_KeyTT[VK_INSERT]		= KEY_INSERT;
	m_KeyTT[VK_DELETE]		= KEY_DELETE;
	m_KeyTT[VK_HOME]		= KEY_HOME;
	m_KeyTT[VK_END]			= KEY_END;
	m_KeyTT[VK_PRIOR]		= KEY_PAGEUP;
	m_KeyTT[VK_NEXT]		= KEY_PAGEDN;

	m_KeyTT[VK_LEFT]		= KEY_LEFT;
	m_KeyTT[VK_UP]			= KEY_UP;
	m_KeyTT[VK_RIGHT]		= KEY_RIGHT;
	m_KeyTT[VK_DOWN]		= KEY_DOWN;

	m_KeyTT[VK_OEM_3]		= KEY_BSPARK;
	m_KeyTT[VK_OEM_MINUS]	= KEY_MINUS;
	m_KeyTT[VK_OEM_PLUS]	= KEY_EQUAL;
	m_KeyTT[VK_OEM_5]		= KEY_BSLASH;
	m_KeyTT[VK_OEM_4]		= KEY_LBRKT;
	m_KeyTT[VK_OEM_6]		= KEY_RBRKT;
	m_KeyTT[VK_OEM_1]		= KEY_COLON;
	m_KeyTT[VK_OEM_7]		= KEY_QUOTE;
	m_KeyTT[VK_OEM_COMMA]	= KEY_COMMA;
	m_KeyTT[VK_OEM_PERIOD]	= KEY_PERIOD;
	m_KeyTT[VK_OEM_2]		= KEY_SLASH;

	m_KeyTT[VK_DIVIDE]		= KEY_PADDIV;
	m_KeyTT[VK_MULTIPLY]	= KEY_PADMUL;
	m_KeyTT[VK_SUBTRACT]	= KEY_PADSUB;
	m_KeyTT[VK_ADD]			= KEY_PADADD;
	m_KeyTT[VK_NUMPAD0]		= KEY_PAD0;
	m_KeyTT[VK_NUMPAD1]		= KEY_PAD1;
	m_KeyTT[VK_NUMPAD2]		= KEY_PAD2;
	m_KeyTT[VK_NUMPAD3]		= KEY_PAD3;
	m_KeyTT[VK_NUMPAD4]		= KEY_PAD4;
	m_KeyTT[VK_NUMPAD5]		= KEY_PAD5ON;
	m_KeyTT[VK_CLEAR]		= KEY_PAD5OFF;
	m_KeyTT[VK_NUMPAD6]		= KEY_PAD6;
	m_KeyTT[VK_NUMPAD7]		= KEY_PAD7;
	m_KeyTT[VK_NUMPAD8]		= KEY_PAD8;
	m_KeyTT[VK_NUMPAD9]		= KEY_PAD9;
	m_KeyTT[VK_DECIMAL]		= KEY_PADDEC;
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
	// NB: значение, возвращаемое функцией GetTickCount, обычно меняется с интервалом около
	// 15 ms. Поэтому события ввода будут обрабатываться не чаще, чем раз в примерно 15 ms
	bool canPoll = m_LastPollTime != timeStamp;
	m_LastPollTime = timeStamp;
	return canPoll;
}

//--------------------------------------------------------------------------------------------------------------------------------
void Console::PollInput(bool forcePoll)
{
	if (!m_Info.inHandle)
		return;

	thread::Lock lock(s_IOLocks->input);

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

				// NB: иногда мы можем прочитать больше событий, чем ожидали. Так происходит, когда между
				// получением количества событий и их чтением в буфер успевает поступить новое событие
				totalEvents -= (eventCount < totalEvents) ? eventCount : totalEvents;

				for (unsigned i = 0; i < eventCount; ++i)
				{
					INPUT_RECORD& event = eventBuffer[i];
					if (event.EventType == KEY_EVENT)
					{
						const WORD keyCode = event.Event.KeyEvent.wVirtualKeyCode;
						const bool isKeyDown = event.Event.KeyEvent.bKeyDown == TRUE;
						const DWORD ctrlState = event.Event.KeyEvent.dwControlKeyState;
						const bool isCtrlDown = (ctrlState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;

						const auto vkey = m_KeyTT[keyCode & 0xff];
						if (vkey == VirtualKey::KEY_C && isKeyDown && isCtrlDown)
							m_IsCtrlCPressed = true;
						// При заполнении буфера игнорируем новые события
						else if (vkey && m_InputEvents.size() < MAX_KEY_EVENTS)
						{
							m_InputEvents.push_back(KeyEvent { vkey, isKeyDown, isCtrlDown });
						}
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
