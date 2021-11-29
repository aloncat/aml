//∙AML
// Copyright (C) 2017-2021 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "platform.h"
#include "util.h"

#include <type_traits>

namespace thread {

class CriticalSection;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Lock
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Класс Lock делает работу с примитивами синхронизации удобнее. При объявлении локальной переменной этого
// типа примитив будет захвачен (опционально), а при её выходе из области видимости - автоматически освобождён

//--------------------------------------------------------------------------------------------------------------------------------
template<class T>
class Lock final
{
	AML_NONCOPYABLE(Lock)
	static_assert(std::is_base_of_v<CriticalSection, T>, "Unsupported type");

public:
	explicit Lock(T* syncObjPtr, bool acquire = true) noexcept
		: m_ObjPtr(syncObjPtr)
	{
		if (syncObjPtr && acquire)
			syncObjPtr->Enter();
	}

	explicit Lock(T& syncObj, bool acquire = true) noexcept
		: m_ObjPtr(&syncObj)
	{
		if (acquire)
			syncObj.Enter();
	}

	~Lock() noexcept
	{
		if (m_ObjPtr)
			m_ObjPtr->Leave();
	}

	void Leave() noexcept
	{
		if (m_ObjPtr)
		{
			m_ObjPtr->Leave();
			m_ObjPtr = nullptr;
		}
	}

private:
	T* m_ObjPtr;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   CriticalSection
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Класс CriticalSection реализует критическую секцию со спинлоком, поддерживающую возможность
// приостановки потока до момента освобождения секции, используя средства операционной системы

//--------------------------------------------------------------------------------------------------------------------------------
class CriticalSection final
{
	AML_NONCOPYABLE(CriticalSection)

public:
	// Инициализация критической секции. Параметр spinCount задаёт количество циклов ожидания (в случае занятости секции
	// другим потоком) до обращения к функциям ОС ожидания освобождения секции (что обычно обходится достаточно дорого).
	// Если значение == 0, то цикла ожидания не будет (значение игнорируется на системах с одним логическим процессором)
	explicit CriticalSection(unsigned spinCount = 0) noexcept;
	~CriticalSection() noexcept;

	bool TryEnter() noexcept;
	void Enter() noexcept;
	void Leave() noexcept;

private:
	void* m_Data = nullptr;		// Указатель на структуру данных критической секции ОС
	uint8_t m_InnerBuf[40];		// Локальный буфер для структуры данных критической секции
};

} // namespace thread
