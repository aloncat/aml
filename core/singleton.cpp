//∙AML
// Copyright (C) 2016-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "singleton.h"

#include "datetime.h"
#include "debug.h"
#include "log.h"
#include "thread.h"

using namespace util;

thrd::CriticalSection* SingletonHolder::s_Lock;
volatile bool SingletonHolder::s_IsFinalizing;
SingletonHolder::Item* SingletonHolder::s_ItemList;
std::vector<void*>* SingletonHolder::s_ObjStack;

//--------------------------------------------------------------------------------------------------------------------------------
void SingletonHolder::KillAll()
{
	Initialize();
	// Так как KillAll может быть вызван любым потоком (включая ситуацию, когда другой поток в это
	// же время инициализирует какой-либо синглтон), мы должны защитить доступ к списку s_ItemList
	thrd::Lock lock(s_Lock);

	for (Item* p = s_ItemList; p;)
	{
		p->fn();
		Item* old = p;
		p = p->next;
		delete old;
	}
	s_ItemList = nullptr;
}

//--------------------------------------------------------------------------------------------------------------------------------
void SingletonHolder::Initialize()
{
	uint8_t current = 0;
	size_t spinCounter = 0;
	static std::atomic<uint8_t> initLock;
	while (!initLock.compare_exchange_weak(current, 1, std::memory_order_acquire))
	{
		// Значение 2 означает, что другой поток уже закончил инициализацию
		if (current == 2)
			return;

		// Так как мы ещё не можем обращаться к другим синглтонам (в т.ч. к SystemInfo, чтобы узнать
		// количество логических процессоров в системе), то после 1000 попыток захвата спинлока отдаём
		// системе остаток тайм-слайса нашего потока (на случай, если процессор в системе всего один)
		if (++spinCounter == 1000)
		{
			spinCounter = 0;
			thrd::Sleep(0);
		} else
			thrd::CPUPause();

		current = 0;
	}

	static uint8_t data[sizeof(thrd::CriticalSection)];
	// Инициализируем объект критической секции. Благодаря
	// placement new, этот объект никогда не будет разрушен
	s_Lock = new(data) thrd::CriticalSection;

	initLock.store(2, std::memory_order_release);
}

//--------------------------------------------------------------------------------------------------------------------------------
void SingletonHolder::Finalize()
{
	if (!s_IsFinalizing)
	{
		// Значение s_IsFinalizing равно false и деинициализация только начинается. Так как другой поток сейчас может
		// выполнять инициализацию синглтона, мы не должны допустить установку флага s_IsFinalizing после захвата
		// критической секции другим потоком. Перед её захватом убедимся, что она проинициализирована
		Initialize();

		// Захватываем критическую секцию. Пытаясь это сделать, мы дождёмся завершения инициализации
		// синглтона другим потоком (если секция была им захвачена) и гарантируем, что другой поток
		// не начнёт инициализацию снова до того, как мы установим флаг
		thrd::Lock lock(s_Lock);

		s_IsFinalizing = true;
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void SingletonHolder::SetKiller(KillerFn fn)
{
	// Мы не захватываем здесь критическую секцию, так как вызов этой функции делается только
	// из функции Singleton::CreateInstance, в которой к этому моменту секция уже захвачена.
	// Вышенаписанное справедливо и для функций PushObject/PopObject

	if (fn)
	{
		Item* p = new Item { fn, s_ItemList };
		s_ItemList = p;
	}

	if (static bool didOnce; !didOnce)
	{
		// Этот вызов KillAll, добавляемый в очередь atexit, нужен для освобождения памяти, занимаемой списком
		// s_ItemList, при завершении работы. Все destroyable синглтоны к этому моменту уже будут уничтожены
		atexit(KillAll);
		didOnce = true;
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void SingletonHolder::PushObject(void* objPtr)
{
	if (s_ObjStack)
	{
		// Проверим, нет ли в стеке указателя на инициализируемый синглтон. Если он там есть, значит имеется рекурсия. Причём,
		// если этот указатель на вершине стека, значит это внутренняя рекурсия в конструкторе синглтона на самого себя. Если
		// указатель будет найден не на вершине стека, значит у нас рекурсия между двумя различными синглтонами
		for (size_t i = 0, size = s_ObjStack->size(); i < size; ++i)
		{
			if ((*s_ObjStack)[i] == objPtr)
			{
				bool isNotLast = i < size - 1;
				LogErrorAndAbort(isNotLast ? L"Detected cross-singleton recursion" :
					L"Detected recursion during singleton initialization");
			}
		}
	} else
		s_ObjStack = new std::vector<void*>;

	// Поместим новый указатель на вершину стека
	s_ObjStack->push_back(objPtr);
}

//--------------------------------------------------------------------------------------------------------------------------------
void SingletonHolder::PopObject()
{
	if (s_ObjStack && !s_ObjStack->empty())
	{
		s_ObjStack->pop_back();
		if (s_ObjStack->empty())
		{
			delete s_ObjStack;
			s_ObjStack = nullptr;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void SingletonHolder::LogErrorAndAbort(std::wstring_view errorMsg)
{
	// Если синглтон системного журнала существует (уже был создан, но ещё не был уничтожен),
	// то выведем сообщение в журнал. В консоль отладчика оно будет выведено автоматически
	if (SystemLog::InstanceExists() && SystemLog::Instance().IsOpened())
	{
		*LogRecordHolder(SystemLog::Instance(), Log::MsgType::Error) << errorMsg;
		SystemLog::Instance().Flush();
	}
	// Если синглтон системного журнала не существует, то выведем сообщение только
	// в консоль отладчика, но при условии, что синглтон DebugHelper также существует
	else if (DebugHelper::InstanceExists() && DebugHelper::Instance().IsDebugOutputEnabled())
	{
		auto time = DateTime::Now();
		auto header = LogRecord::FormatHeader(Log::MsgType::Error, time);
		DebugHelper::DebugOutput(std::move(header) + errorMsg + L'\n');
	}

	// Аварийно завершаем работу
	DebugHelper::Abort();
}
