//∙AML
// Copyright (C) 2017-2021 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "platform.h"

namespace math {

//--------------------------------------------------------------------------------------------------------------------------------
class RandGen
{
public:
	RandGen();
	explicit RandGen(unsigned seed) noexcept;

	// Задаёт новое зерно генератора
	void Seed(unsigned seed) noexcept;

	// Генерирует псевдо-случайное 32-битное целое число
	unsigned UInt() noexcept { return Next(); }
	// Генерирует псевдо-случайное целое число X, такое что 0 <= X < range
	unsigned UInt(unsigned range) noexcept;
	// Генерирует псевдо-случайное целое число X, такое что min <= X <= max
	unsigned UInt(unsigned min, unsigned max) noexcept;

	// Генерирует псевдо-случайное дробное число X, такое что 0.0 <= X <= 1.0
	float Float() noexcept;
	// Генерирует псевдо-случайное дробное число X, такое что min <= X <= max
	float Float(float min, float max) noexcept;

protected:
	uint32_t Next() noexcept;

	uint32_t m_X, m_Y, m_Z;
};

} // namespace math
