//∙AML
// Copyright (C) 2017-2021 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "platform.h"

namespace util {

//--------------------------------------------------------------------------------------------------------------------------------
struct DateTime
{
	uint16_t year;			// Год в 4-значном формате
	uint16_t month;			// Месяц: от 1 (январь) до 12 (декабрь)
	uint16_t day;			// День: от 1 до 31
	uint16_t dayOfWeek;		// День недели: от 0 (воскресенье) до 6 (суббота)

	uint16_t hour;			// Часы: от 0 до 23
	uint16_t minute;		// Минуты: от 0 до 59
	uint16_t second;		// Секунды: от 0 до 59
	uint16_t ms;			// Миллисекунды: от 0 до 999

public:
	// Устанавливает значение даты и времени в полночь понедельника 1 января 1601
	// года. Эти значения соответствует результату вызова функции Decode(0)
	void Clear() noexcept;

	// Декодирует текущее системное (utc == true) или локальное (utc == false) время и обновляет все поля структуры
	void Update(bool utc = false) noexcept;
	// Эти функции аналогичны Update, но обновляют только поля времени (UpdateTime) или только поля даты
	// (UpdateDate). Работают немного быстрее, чем Update, если нужно только время или только дата
	void UpdateTime(bool utc = false) noexcept;
	void UpdateDate(bool utc = false) noexcept;

	// Декодирует указанное значение time (количество мс, прошедших
	// с полуночи 1 января 1601 года) и обновляет все поля структуры
	void Decode(uint64_t time) noexcept;
	// Эти функции аналогичны Decode, но обновляют только поля
	// времени (DecodeTime) или только поля даты (DecodeDate)
	void DecodeTime(uint64_t time) noexcept;
	void DecodeDate(uint64_t time) noexcept;

	// Вычисляет количество мс, прошедших с полуночи 1 января 1601 года до даты и времени, указанных
	// в полях структуры. Если значение поля year меньше 1601, функция вернёт некорректное значение
	uint64_t Encode() noexcept;
	// Вычисляет количество мс, прошедших с полуночи до времени,
	// указанного в полях структуры (поля даты игнорируются)
	uint64_t EncodeTime() noexcept;
	// Вычисляет количество мс, прошедших с полуночи 1 января 1601 года до полуночи
	// даты, указанной в полях структуры (поля времени игнорируются). Если значение
	// поля year меньше 1601, функция вернет некорректное значение
	uint64_t EncodeDate() noexcept;

	// Возвращает количество мс, прошедших с полуночи 1 января 1601 года до текущего момента.
	// Параметр utc задает используемое время: true - системное (UTC), false - локальное
	static uint64_t Now(bool utc = false) noexcept;
	// Эти функции аналогичны функции Now. Функция Time не учитывает дату: вычисляет количество мс,
	// прошедших с полуночи текущих суток до текущего момента. А функция Date не учитывает время:
	// вычисляет количество мс, прошедших с полуночи 1 января 1601 года до полуночи текущих суток
	static uint64_t Time(bool utc = false) noexcept;
	static uint64_t Date(bool utc = false) noexcept;

	// Возвращает true, если year - високосный год
	static bool IsLeapYear(unsigned year) noexcept;

	// Преобразует указанное UTC время (количество мс, прошедших с полуночи
	// 1 января 1601 года) в значение соответствующего локального времени
	static uint64_t ToLocal(uint64_t time) noexcept;

	// Преобразует время, соответствующее указанному количеству мс, прошедших с полуночи 1 января 1601
	// года, в формат времени UNIX (т.е. количество секунд, прошедших с 1 января 1970 года) и обратно
	static int64_t ToUNIX(uint64_t time) noexcept;
	static uint64_t FromUNIX(int64_t time) noexcept;

private:
	void SetTime(unsigned milliseconds) noexcept;
	void SetDate(unsigned totalDays) noexcept;
};

} // namespace util
