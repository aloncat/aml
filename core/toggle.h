//∙AML
// Copyright (C) 2017-2021 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "util.h"

#include <functional>
#include <type_traits>
#include <utility>

namespace util {

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Toggle
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Класс Toggle - это "переключатель". В момент своей инициализации он запоминает значение указанной
// переменной, затем (опционально) присваивает ей новое значение. При разрушении объекта Toggle (при
// выходе переменной из области видимости) исходное значение указанной переменной восстанавливается

//--------------------------------------------------------------------------------------------------------------------------------
template<class T>
class Toggle
{
	AML_NONCOPYABLE(Toggle)
	static_assert(std::is_nothrow_destructible_v<T> && (std::is_nothrow_move_assignable_v<T> ||
		(!std::is_move_assignable_v<T> && std::is_nothrow_copy_assignable_v<T>)),
		"Class T must have nothrow destructor & move/copy assignment operator");

public:
	// Инициализирует объект, не меняя значение переменной var
	explicit Toggle(T& var)
		: m_VarPtr(&var)
		, m_Value(var)
	{
	}

	// Инициализирует объект и меняет значение переменной var на newValue
	Toggle(T& var, const T& newValue)
		: m_VarPtr(&var)
		, m_Value(std::move(var))
	{
		var = newValue;
	}

	// Инициализирует объект и меняет значение переменной var на newValue
	Toggle(T& var, T&& newValue)
		: m_VarPtr(&var)
		, m_Value(std::move(var))
	{
		var = std::forward<T>(newValue);
	}

	// Восстанавливает исходное значение переменной
	~Toggle() noexcept
	{
		// NB: для корректной работы класс T должен иметь nothrow оператор перемещающего
		// присваивания или nothrow оператор копирования (только если перемещающего нет)
		*m_VarPtr = std::move(m_Value);
	}

	// Возвращает исходное значение переменной
	const T& GetOriginal() const noexcept
	{
		return m_Value;
	}

	// Задаёт новое "исходное" значение для переменной. Теперь при разрушении объекта
	// переменной будет присвоенно значение newRestoreValue. Если параметр setNow равен
	// true, то значение самой переменной также будет изменено на newRestoreValue
	void SetRestore(const T& newRestoreValue, bool setNow = false)
	{
		m_Value = newRestoreValue;
		if (setNow)
			*m_VarPtr = m_Value;
	}

	// Функция аналогична предыдущей (но использует R-value)
	void SetRestore(T&& newRestoreValue, bool setNow = false)
	{
		m_Value = std::forward<T>(newRestoreValue);
		if (setNow)
			*m_VarPtr = m_Value;
	}

	// Восстанавливает исходное значение переменной
	void Restore()
	{
		*m_VarPtr = m_Value;
	}

	// Возвращает ссылку на переменную
	T& operator *() noexcept { return *m_VarPtr; }
	const T& operator *() const noexcept { return *m_VarPtr; }

	// Позволяет обратиться к члену переменной (если переменная - это класс или структура)
	T* operator ->() noexcept { return m_VarPtr; }
	const T* operator ->() const noexcept { return m_VarPtr; }

protected:
	T* m_VarPtr;
	T m_Value;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   FuncToggle
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Класс FuncToggle - это "переключатель" с пользовательской функцией. В момент своей
// инициализации и разрушения он вызывает заданный (при вызове конструктора) функтор

//--------------------------------------------------------------------------------------------------------------------------------
class FuncToggle
{
	AML_NONCOPYABLE(FuncToggle)

public:
	// Прототип функции, которая будет вызываться конструктором и деструктором. При вызове функции из конструктора
	// bool параметр будет равен true, а при вызове из деструктора - false. Если функция вернёт true в первый раз
	// (при вызове из конструктора), она будет вызвана из деструктора, если вернёт false, то не будет
	using ToggleFn = std::function<bool(bool)>;

	FuncToggle(const ToggleFn& fn)
		: m_Fn(fn)
	{
		if (m_Fn && !m_Fn(true))
			m_Fn = nullptr;
	}

	FuncToggle(ToggleFn&& fn)
		: m_Fn(std::move(fn))
	{
		if (m_Fn && !m_Fn(true))
			m_Fn = nullptr;
	}

	~FuncToggle() noexcept(false)
	{
		if (m_Fn)
			m_Fn(false);
	}

	// Вызывает пользовательскую функцию с параметром bool, равным false (так же, как это делает деструктор
	// класса). Если функция вернула false или параметр noMoreRestore был равен true, то пользовательская
	// функция не будет вызвана из деструктора при разрушении объекта. Если пользовательская функция
	// вернула false в первый раз (при вызове из конструктора), то Restore её не вызовет
	void Restore(bool noMoreRestore = false)
	{
		if (m_Fn && (!m_Fn(false) || noMoreRestore))
			m_Fn = nullptr;
	}

protected:
	ToggleFn m_Fn;
};

} // namespace util
