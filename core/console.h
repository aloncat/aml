//∙AML
// Copyright (C) 2017-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "platform.h"
#include "singleton.h"
#include "strcommon.h"
#include "threadsync.h"
#include "util.h"
#include "vkey.h"

#include <deque>
#include <string_view>

namespace util {

//--------------------------------------------------------------------------------------------------------------------------------
class Console : public VirtualKey
{
	AML_NONCOPYABLE(Console)

public:
	struct KeyEvent {
		uint16_t vkey;		// Код виртуальной клавиши
		bool isKeyDown;		// true, если клавиша нажата; false, если отпущена
		bool isCtrlDown;	// true, если левая или праавая клавиша CTRL нажата
	};

	// Создаёт объект консоли. Если у приложения уже есть окно консоли, то объект подколючается
	// к нему. Если окна консоли нет (например, для GUI приложения), то создаётся новое окно
	Console();

	// Отключает объект от окна консоли приложения. Если первым созданным объектом было создано
	// новое окно консоли, то оно будет уничтожено деструктором последнего существующего объекта
	~Console();

	// Выводит в консоль текст строки заданным цветом
	void Write(std::string_view str, int color = 7);
	void Write(std::wstring_view str, int color = 7);

	// Включает или выключает отображение курсора
	void ShowCursor(bool visible);

	// Извлекает из очереди ввода следующее событие и помещает его в event. Если событие
	// извлечено, то функция вернёт true. Если очередь событий пуста, то вернёт false
	bool GetInputEvent(KeyEvent& event);
	// Очищает буфер событий ввода
	void ClearEvents();

	// Возвращает true, если была нажата комбинация клавиш Ctrl-C или Ctrl-Break.
	// Если параметр reset равен true, то это состояние будет сброшено
	bool IsCtrlCPressed(bool reset = false);

	// Задаёт текст для заголовка окна консоли
	void SetTitle(ZStringView title);
	void SetTitle(WZStringView title);

protected:
	static constexpr size_t MAX_KEY_EVENTS = 64;

	struct CtrlHandler;

	struct ConsoleInfo {
		void* inHandle = nullptr;			// Хэндл устройства ввода
		void* outHandle = nullptr;			// Хэндл устройства вывода
		void* ctrlHandler = nullptr;		// Указатель на обработчик событий
		int oldTextColor = 7;				// Цвет текста консоли при инициализации
		bool isRedirected = false;			// true, если вывод перенаправлен
		bool isCursorVisible = true;		// true, если курсор был видим при инициализации
	};

	struct IOLocks {
		thrd::CriticalSection input;		// Критическая секция для обработки ввода
		thrd::CriticalSection output;		// Критическая секция для обработки вывода
	};

	void InitMainCS();
	void InitKeyTT();

	void SetColor(int color);

	bool CheckPollTime();
	void PollInput(bool forcePoll);

protected:
	ConsoleInfo m_Info;
	uint8_t m_KeyTT[256];						// Таблица трансляции кодов виртуальных клавиш
	std::deque<KeyEvent> m_InputEvents;			// Буфер событий ввода

	int m_TextColor = -1;						// Текущий цвет текста
	unsigned m_LastPollTime = 0;				// Время последней обработки событий ввода
	volatile bool m_IsCtrlCPressed = false;		// true, если были нажаты Ctrl-C или Ctrl-Break

	static IOLocks* s_IOLocks;					// Критические секции (ввод и вывод)
	static thrd::CriticalSection* s_MainCS;		// Основная критическая секция

	static unsigned s_RefCounter;				// Счётчик существующих объектов
	static bool s_HasAllocatedConsole;			// true, если было создано окно консоли
	static bool s_HasSetCtrlHandler;			// true, если обработчик Ctrl-C установлен
};

//--------------------------------------------------------------------------------------------------------------------------------
class SystemConsole : public Console, public Singleton<SystemConsole>
{
protected:
	SystemConsole() = default;
	virtual ~SystemConsole() override = default;
};

} // namespace util
