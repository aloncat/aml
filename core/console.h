﻿//∙AML
// Copyright (C) 2017-2021 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "platform.h"
#include "threadsync.h"
#include "util.h"

#include <deque>
#include <string>

namespace util {

//--------------------------------------------------------------------------------------------------------------------------------
class Console
{
	AML_NONCOPYABLE(Console)

public:
	struct KeyEvent {
		uint16_t vkey;		// Код виртуальной клавиши
		bool isKeyDown;		// true, если клавиша нажата; false, если отпущена
		bool isCtrlDown;	// true, если левая или праавая клавиша CTRL нажата
	};

	// Создаёт объект. Для первого созданного объекта Console в консольном приложении ОС Windows
	// происходит подключение к основному окну приложения (родительскому консольному окну). Если
	// это GUI приложение или создаётся второй объект, то будет создано новое консольное окно
	Console();

	// Отключает объект от основного окна консоли (уничтожает созданное окно)
	~Console();

	// Выводит в консоль текст строки заданным цветом
	void Write(const char* str, int color = 7);
	void Write(const wchar_t* str, int color = 7);
	void Write(const std::string& str, int color = 7);
	void Write(const std::wstring& str, int color = 7);

	// Извлекает из очереди ввода следующее событие и помещает его в event. Если событие
	// извлечено, то функция вернёт true. Если очередь событий пуста, то вернёт false
	bool GetInputEvent(KeyEvent& event);

	// Возвращает true, если была нажата комбинация клавиш Ctrl-C или Ctrl-Break.
	// Если параметр reset равен true, то это состояние будет сброшено
	bool IsCtrlCPressed(bool reset = false);

	// Задаёт текст для заголовка окна консоли
	void SetTitle(const std::string& title);
	void SetTitle(const std::wstring& title);

protected:
	static constexpr size_t MAX_KEY_EVENTS = 48;

	struct CtrlHandler;
	struct ConsoleInfo {
		void* inHandle = nullptr;		// Хэндл устройства ввода
		void* outHandle = nullptr;		// Хэндл устройства вывода
		void* ctrlHandler = nullptr;	// Указатель на обработчик событий
		int oldTextColor = 7;			// Цвет текста консоли при инициализации
		bool isRedirected = false;		// true, если вывод перенаправлен
	};

	void SetColor(int color);
	void Write(const char* str, size_t strLen, int color);
	void Write(const wchar_t* str, size_t strLen, int color);

	bool CheckPollTime();
	void PollInput();

protected:
	thread::CriticalSection m_InputCS;
	thread::CriticalSection m_OutputCS;
	ConsoleInfo m_Info;

	std::deque<KeyEvent> m_InputEvents;

	int m_TextColor = -1;						// Текущий цвет текста
	unsigned m_LastPollTime = 0;				// Время последней обработки событий ввода
	volatile bool m_IsCtrlCPressed = false;		// true, если были нажаты Ctrl-C или Ctrl-Break
};

} // namespace util
