﻿//∙AML
// Copyright (C) 2017-2021 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "randgen.h"

#include "thread.h"

#include <time.h>

using namespace math;

// Класс RandGen реализует быстрый и простой генератор псевдослучайных чисел с превосходным распределением и очень длинным
// периодом (близким к 2^96). Он основан на комбинации 3 примитивных генераторов. Буквы в их именах обозначают операции,
// последовательно выполняемые генераторами для получения числа: C - поместить константу в стек, M - умножить 2 числа,
// F - инвертировать все биты числа, R - повернуть число влево (ROL), S - вычесть (A - B, где число A помещается в стек
// первым, B - последним), E - поменять местами 2 операнда на вершине стека. Операции M, F, R и S работают так же, как в
// калькуляторе со стеком: 1 (или 2) операнда извлекаются с вершины стека, над ними выполняется операция и затем результат
// помещается обратно в стек; значения x/y/z всегда помещаются в стек первыми. Все константы, используемые в генераторе,
// были выбраны в результате тестирования полным перебором. Конструктор класса RandGen потокобезопасен: при обращении к
// нему из разных потоков гарантируется, что каждый поток получит своё уникальное зерно

#define RG_ROTL(V, N) (((V) << (N)) | ((V) >> ((8 * sizeof(V)) - (N))))

//--------------------------------------------------------------------------------------------------------------------------------
RandGen::RandGen()
{
	unsigned t = static_cast<unsigned>(time(nullptr));
	static std::atomic<unsigned> seed(t + thread::GetThreadId());

	unsigned newSeed, oldSeed = seed.load(std::memory_order_relaxed);
	do {
		newSeed = oldSeed * 1103515245u + 12345u;
	} while (!seed.compare_exchange_weak(oldSeed, newSeed, std::memory_order_relaxed));
	Seed(t + ((newSeed >> 12) & 0x3ffff));
}

//--------------------------------------------------------------------------------------------------------------------------------
RandGen::RandGen(unsigned seed) noexcept
{
	Seed(seed);
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void RandGen::Seed(unsigned seed) noexcept
{
	m_X = (seed & 0x1fffff) + 4027999010u;			// 21 бит зерна (seed)
	m_Y = ((seed >> 7) & 0x7ffff) + 3993266363u;	// 19 бит зерна (seed)
	m_Z = (seed >> 13) + 3605298456u;				// 19 бит зерна (seed)
}

//--------------------------------------------------------------------------------------------------------------------------------
unsigned RandGen::UInt(unsigned range) noexcept
{
	return range ? Next() % range : 0;
}

//--------------------------------------------------------------------------------------------------------------------------------
unsigned RandGen::UInt(unsigned min, unsigned max) noexcept
{
	if (min >= max)
		return min;

	unsigned num = Next();
	unsigned range = max - min + 1;
	return range ? min + num % range : num;
}

//--------------------------------------------------------------------------------------------------------------------------------
float RandGen::Float() noexcept
{
	return 2.32830644e-10f * Next();
}

//--------------------------------------------------------------------------------------------------------------------------------
float RandGen::Float(float min, float max) noexcept
{
	if (min >= max)
		return min;

	float num = Float();
	return min + num * (max - min);
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE uint32_t RandGen::Next() noexcept
{
	// CMFR, период: 4294951751 (простое число)
	m_X = ~(2911329625u * m_X);
	m_X = RG_ROTL(m_X, 17);

	// CMR, период: 4294881427 (простое число)
	m_Y = 4031235431u * m_Y;
	m_Y = RG_ROTL(m_Y, 15);

	// CERS, период: 4294921861=19*89*2539871
	m_Z = 3286325185u - RG_ROTL(m_Z, 19);

	return (m_X + m_Y) ^ m_Z;
}