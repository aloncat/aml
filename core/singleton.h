//∙AML
// Copyright (C) 2016-2021 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "platform.h"
#include "threadsync.h"
#include "util.h"

#include <atomic>
#include <stdlib.h>
#include <string_view>
#include <type_traits>
#include <vector>

namespace util {

//--------------------------------------------------------------------------------------------------------------------------------
class SingletonHolder
{
	AML_NONCOPYABLE(SingletonHolder)

public:
	// Уничтожает все "destroyable" синглтоны в порядке, обратном порядку, в котором они создавались, вызывая по очереди
	// все добавленные в список функторы в порядке обратном порядку их добавления (функцией SetKiller) и очищая список
	static void KillAll();

protected:
	using KillerFn = void (*)();

	SingletonHolder() = default;

	// Инициализирует при первом вызове критическую секцию s_Lock
	static void Initialize();

	// Эта функция должна вызываться при завершении работы (только во время обработки списка atexit)
	static void Finalize();

	// Добавляет функтор fn в последовательность "уничтожения". Первый вызов
	// этой функции добавляет вызов KillAll в глобальный список atexit
	static void SetKiller(KillerFn fn = nullptr);

	// Проверяет, что указатель objPtr отсутствует в стеке, создаёт объект стека s_ObjStack
	// (при необходимости) и помещает указатель на его вершину. Если такой указатель уже
	// есть в стеке, то будет вызвана функция LogErrorAndAbort с сообщением об ошибке
	static void PushObject(void* objPtr);

	// Извлекает указатель с вершины стека. Если при этом стек
	// становится пустым, то уничтожает и сам объект стека
	static void PopObject();

	// По возможности выводит сообщение об ошибке errorMsg в системный журнал и консоль отладчика (если их
	// синглтоны были проинициализированы и ещё не уничтожены) и затем вызывает функцию DebugHelper::Abort
	[[noreturn]] static void LogErrorAndAbort(std::wstring_view errorMsg);

private:
	struct Item {
		KillerFn fn;
		Item* next;
	};

protected:
	static thread::CriticalSection* s_Lock;
	static volatile bool s_IsFinalizing;

private:
	static Item* s_ItemList;
	static std::vector<void*>* s_ObjStack;
};

//--------------------------------------------------------------------------------------------------------------------------------
template<class T, bool destroyable = false>
class Singleton : private SingletonHolder
{
public:
	static T& Instance()
	{
		if (T* p = s_This.load(std::memory_order_acquire))
			return *p;
		return *CreateInstance();
	}

	static bool InstanceExists() noexcept
	{
		return s_This.load(std::memory_order_relaxed) != nullptr;
	}

protected:
	Singleton() = default;
	virtual ~Singleton() = default;

	// Эта функция будет вызвана непосредственно перед уничтожением синглтона, т.е. перед вызовом его
	// деструктора. Произойдёт это либо при штатном завершении работы программы, либо при вызове функции
	// KillAll (только для синглтонов с destroyable == true). Функция не должна выбрасывать С++ исключений
	virtual void OnDestroy() noexcept
	{
	}

private:
	// Эта функция должна создавать объект синглтона и возвращать указатель на него. Указатель
	// затем будет использован для уничтожения синглтона вызовом delete. Класс-наследник может
	// переопределить эту функцию, если синглтон создаётся конструктором с параметрами
	template<bool doCall = false> static T* Create()
	{
		if constexpr (std::is_final_v<T>)
		{
			if constexpr (doCall)
				return T::Create();
			else
				return new T;
		} else
		{
			class Derived : public T {
			public:
				using T::Create;
			};

			if constexpr (doCall)
				return Derived::Create();
			else
				return new Derived;
		}
	}

	static AML_NOINLINE T* CreateInstance()
	{
		Initialize();
		thread::Lock lock(s_Lock);

		if (T* p = s_This.load(std::memory_order_relaxed))
			return p;

		// Если мы находимся в процессе завершения работы приложения (флаг s_IsFinalizing установлен, и сейчас вызываются
		// обработчики списка atexit), то инициализация синглтона невозможна, так же как невозможно и корректное продолжение
		// обработки списка atexit. Данная ситуация возникает тогда, когда деструктор какого-либо класса пытается обратиться
		// к функции Instance синглтона, который уже был уничтожен (или не был проинициализирован). Для решения проблемы
		// нужно в конструктор этого класса добавить тот же вызов функции Instance, что и в деструкторе. Это приведёт
		// к тому, что синглтон будет уничтожаться позже класса (т.е. будет всё еще доступен из его деструктора)
		if (s_IsFinalizing)
		{
			LogErrorAndAbort(L"Attempt to initialize singleton during finalization");
		}

		// Проверим, что синглтон, который мы собираемся проинициализировать, отсутствует в стеке инициализируемых
		// синглтонов. Если он там всё же есть, значит имеет место рекурсия (т.е. либо 2 синглтона обращаются друг
		// к другу из своих конструкторов, либо наш синглтон обращается сам к себе, например при инициализации
		// своего члена данных, который в своём конструкторе обращается к Instance нашего синглтона)
		PushObject(&s_This);

		// Если мы не завершились с ошибкой при добавлении объекта в стек, значит
		// всё хорошо. Удалим объект из стека, когда закончим его инициализацию
		struct Invoke {
			~Invoke()
			{
				PopObject();
			}
		} whenDone;

		T* instance = Create<true>();

		if (destroyable)
			SetKiller(Destroy);
		atexit(OnExit);

		s_This.store(instance, std::memory_order_release);
		return instance;
	}

	static void Destroy()
	{
		thread::Lock lock(s_Lock);
		if (Singleton* instance = s_This.load(std::memory_order_relaxed))
		{
			instance->OnDestroy();
			delete instance;

			s_This.store(nullptr, std::memory_order_release);
		}
	}

	static void OnExit()
	{
		Finalize();
		Destroy();
	}

protected:
	static inline std::atomic<T*> s_This;
};

} // namespace util
