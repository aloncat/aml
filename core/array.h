//∙AML
// Copyright (C) 2017-2021 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "platform.h"
#include "util.h"

#include <string.h>
#include <type_traits>

namespace util {

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   DynamicArray
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Класс DynamicArray - простейший контейнер, представляет собой массив элементов одного
// типа. Массив заданного размера выделяется в куче при создании объекта. Контейнер может
// быть использован только для простых типов без инициализатора (POD-types)

//--------------------------------------------------------------------------------------------------------------------------------
template<class T>
class DynamicArray final
{
	AML_NONCOPYABLE(DynamicArray)
	static_assert(std::is_pod_v<T>, "Only POD-types are allowed");

public:
	// Параметр size задаёт желаемый размер контейнера (количество элементов). Если параметр
	// data задан, то элементы контейнера будут проинициализированы значениями из массива data
	explicit DynamicArray(size_t size, const T* data = nullptr)
		: m_Items(size ? new Item[size] : nullptr)
	{
		if (data)
			memcpy(m_Items, data, size * sizeof(T));
	}

	~DynamicArray() noexcept
	{
		if (m_Items)
			delete[] m_Items;
	}

	operator T*() noexcept
	{
		return reinterpret_cast<T*>(m_Items);
	}

	operator const T*() const noexcept
	{
		return reinterpret_cast<T*>(m_Items);
	}

private:
	using Item = uint8_t[sizeof(T)];

	Item* m_Items;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SmartArray
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Класс SmartArray - простой контейнер, представляет собой массив элементов одного типа. Его главное отличие от DynamicArray
// в том, что этот контейнер имеет внутренний буфер. Если запрошенный размер (параметр size конструктора) меньше или равен
// значению capacity (по умолчанию 80), то массив для элементов будет размещён во внутреннем буфере (на стеке). Иначе он
// будет выделен в куче. Контейнер может быть использован только для простых типов без инициализатора (POD-types)

//--------------------------------------------------------------------------------------------------------------------------------
template<class T, size_t capacity = 80>
class SmartArray final
{
	AML_NONCOPYABLE(SmartArray)
	static_assert(capacity > 0, "Capacity must be greater than 0");
	static_assert(std::is_pod_v<T>, "Only POD-types are allowed");

public:
	// Параметр size задаёт желаемый размер контейнера (количество элементов). Если параметр
	// data задан, то элементы контейнера будут проинициализированы значениями из массива data
	explicit SmartArray(size_t size, const T* data = nullptr)
		: m_Items((size <= capacity) ? m_Buffer : new Item[size])
	{
		if (data)
			memcpy(m_Items, data, size * sizeof(T));
	}

	~SmartArray() noexcept
	{
		if (m_Items != m_Buffer)
			delete[] m_Items;
	}

	operator T*() noexcept
	{
		return reinterpret_cast<T*>(m_Items);
	}

	operator const T*() const noexcept
	{
		return reinterpret_cast<T*>(m_Items);
	}

private:
	using Item = uint8_t[sizeof(T)];

	Item* m_Items;
	Item m_Buffer[capacity];
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   FlexibleArray
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Класс FlexibleArray - это контейнер, представляющий собой массив элементов одного типа. От DynamicArray и
// SmartArray этот контейнер отличает то, что он может быть проинициализирован пользовательским массивом заданного
// размера. Также, он может менять свой размер в сторону увеличения: в этом случае новый массив большего размера
// всегда выделяется в куче. Контейнер может быть использован только для простых типов без инициализатора

//--------------------------------------------------------------------------------------------------------------------------------
template<class T>
class FlexibleArray final
{
	AML_NONCOPYABLE(FlexibleArray)
	static_assert(std::is_pod_v<T>, "Only POD-types are allowed");

public:
	// Инициализирует контейнер размером size элементов. Если значение параметра
	// size больше 0, то массив соответствующего размера сразу выделяется в куче
	explicit FlexibleArray(size_t size = 0)
		: m_Buffer(size ? new Item[size] : nullptr)
		, m_Size(size)
	{
		m_Items = m_Buffer;
	}

	// Инициализирует контейнер пользовательским массивом userBuffer размером userSize
	// элементов. Этот массив будет использован для хранения элементов контейнера
	FlexibleArray(T* userBuffer, size_t userSize) noexcept
		: m_Items(reinterpret_cast<Item*>(userBuffer))
		, m_Buffer(nullptr)
		, m_Size(userBuffer ? userSize : 0)
	{
	}

	template<size_t userSize>
	explicit FlexibleArray(T (&userBuffer)[userSize]) noexcept
		: m_Items(reinterpret_cast<Item*>(userBuffer))
		, m_Buffer(nullptr)
		, m_Size(userBuffer ? userSize : 0)
	{
	}

	~FlexibleArray() noexcept
	{
		if (m_Buffer)
			delete[] m_Buffer;
	}

	// Реинициализирует контейнер новым пользовательским
	// массивом userBuffer размером userSize элементов
	void Set(T* userBuffer, size_t userSize) noexcept
	{
		if (m_Buffer)
			delete[] m_Buffer;
		m_Items = reinterpret_cast<Item*>(userBuffer);
		m_Buffer = nullptr;
		m_Size = userBuffer ? userSize : 0;
	}

	template<size_t userSize>
	void Set(T (&userBuffer)[userSize]) noexcept
	{
		Set(userBuffer, userSize);
	}

	size_t GetSize() const noexcept
	{
		return m_Size;
	}

	// Увеличивает (при необходимости) размер контейнера до newSize элементов. Если значение newSize
	// меньше или равно текущему размеру контейнера, то ничего не меняется. Если retainContents
	// равен true, то при увеличении размера контейнера его содержимое будет сохранено
	void Grow(size_t newSize, bool retainContents = false)
	{
		if (newSize > m_Size)
		{
			if (retainContents)
				Resize(newSize);
			else
				Reallocate(newSize);
		}
	}

	operator T*() noexcept
	{
		return reinterpret_cast<T*>(m_Items);
	}

	operator const T*() const noexcept
	{
		return reinterpret_cast<T*>(m_Items);
	}

private:
	using Item = uint8_t[sizeof(T)];

	AML_NOINLINE void Reallocate(size_t newSize)
	{
		if (m_Buffer)
			delete[] m_Buffer;
		m_Items = m_Buffer = nullptr;
		m_Size = 0;

		m_Buffer = new Item[newSize];
		m_Items = m_Buffer;
		m_Size = newSize;
	}

	AML_NOINLINE void Resize(size_t newSize)
	{
		Item* newBuffer = new Item[newSize];
		if (m_Size)
		{
			memcpy(newBuffer, m_Items, m_Size * sizeof(T));
			if (m_Buffer)
				delete[] m_Buffer;
		}
		m_Buffer = newBuffer;
		m_Items = m_Buffer;
		m_Size = newSize;
	}

private:
	Item* m_Items;
	Item* m_Buffer;
	size_t m_Size;
};

} // namespace util
