//∙AML
// Copyright (C) 2017-2021 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "datetime.h"

#include "winapi.h"

namespace util {

// Суммарное количество дней в невисокосном году до последнего дня предыдущего месяца
const int monthA[16] = { -1, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333 };

//--------------------------------------------------------------------------------------------------------------------------------
void DateTime::Clear() noexcept
{
	year = 1601;
	month = 1;
	day = 1;
	dayOfWeek = 1;
	hour = 0;
	minute = 0;
	second = 0;
	ms = 0;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void DateTime::Update(bool utc) noexcept
{
	uint64_t time = Now(utc);
	SetDate(static_cast<unsigned>(time / 86400000));
	SetTime(time % 86400000);
}

//--------------------------------------------------------------------------------------------------------------------------------
void DateTime::UpdateTime(bool utc) noexcept
{
	uint64_t time = Now(utc);
	SetTime(time % 86400000);
}

//--------------------------------------------------------------------------------------------------------------------------------
void DateTime::UpdateDate(bool utc) noexcept
{
	uint64_t time = Now(utc);
	SetDate(static_cast<unsigned>(time / 86400000));
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void DateTime::Decode(uint64_t time) noexcept
{
	SetDate(static_cast<unsigned>(time / 86400000));
	SetTime(time % 86400000);
}

//--------------------------------------------------------------------------------------------------------------------------------
void DateTime::DecodeTime(uint64_t time) noexcept
{
	SetTime(time % 86400000);
}

//--------------------------------------------------------------------------------------------------------------------------------
void DateTime::DecodeDate(uint64_t time) noexcept
{
	SetDate(static_cast<unsigned>(time / 86400000));
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE uint64_t DateTime::Encode() noexcept
{
	return EncodeDate() + EncodeTime();
}

//--------------------------------------------------------------------------------------------------------------------------------
uint64_t DateTime::EncodeTime() noexcept
{
	return ((60 * hour + minute) * 60 + second) * 1000 + ms;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE uint64_t DateTime::EncodeDate() noexcept
{
	unsigned y = year - 1601;
	unsigned leapYearC = y >> 2;
	leapYearC = leapYearC - leapYearC / 25 + ((leapYearC / 25) >> 2);
	unsigned days = y * 365 + leapYearC + monthA[month & 15] + day;

	if (month > 2 && !(year & 3) && (year % 100 || !(year % 400)))
		++days;

	return days * 86400000ll;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE uint64_t DateTime::Now(bool utc) noexcept
{
	#if AML_OS_WINDOWS
		FILETIME system, t;
		::GetSystemTimeAsFileTime(&system);
		if (utc || (::FileTimeToLocalFileTime(&system, &t) == 0))
			t = system;
		return ((static_cast<uint64_t>(t.dwHighDateTime) << 32) | t.dwLowDateTime) / 10000;
	#else
		#error Not implemented
	#endif
}

//--------------------------------------------------------------------------------------------------------------------------------
uint64_t DateTime::Time(bool utc) noexcept
{
	uint64_t time = Now(utc);
	return time % 86400000;
}

//--------------------------------------------------------------------------------------------------------------------------------
uint64_t DateTime::Date(bool utc) noexcept
{
	uint64_t time = Now(utc);
	return time - time % 86400000;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool DateTime::IsLeapYear(unsigned year) noexcept
{
	if (year & 3)
		return false;

	unsigned k = year / 100;
	return year - 100 * k || !(k & 3);
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE uint64_t DateTime::ToLocal(uint64_t time) noexcept
{
	#if AML_OS_WINDOWS
		time *= 10000;
		FILETIME system, t;
		system.dwHighDateTime = time >> 32;
		system.dwLowDateTime = static_cast<uint32_t>(time);
		if (::FileTimeToLocalFileTime(&system, &t) == 0)
			return 0;
		return ((static_cast<uint64_t>(t.dwHighDateTime) << 32) | t.dwLowDateTime) / 10000;
	#else
		#error Not implemented
	#endif
}

//--------------------------------------------------------------------------------------------------------------------------------
int64_t DateTime::ToUNIX(uint64_t time) noexcept
{
	return time / 1000 - 11644473600ll;
}

//--------------------------------------------------------------------------------------------------------------------------------
uint64_t DateTime::FromUNIX(int64_t time) noexcept
{
	return (time + 11644473600ll) * 1000;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void DateTime::SetTime(unsigned milliseconds) noexcept
{
	unsigned h = milliseconds / 3600000;
	hour = static_cast<uint16_t>(h);
	milliseconds -= h * 3600000;

	unsigned m = milliseconds / 60000;
	minute = static_cast<uint16_t>(m);
	milliseconds -= m * 60000;

	second = static_cast<uint16_t>(milliseconds / 1000);
	ms = static_cast<uint16_t>(milliseconds % 1000);
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void DateTime::SetDate(unsigned totalDays) noexcept
{
	dayOfWeek = (totalDays + 1) % 7;

	unsigned y400 = totalDays / 146097;
	totalDays -= y400 * 146097;
	year = static_cast<uint16_t>(1601 + y400 * 400);
	for (int i = 3; i && totalDays >= 36524; --i)
	{
		totalDays -= 36524;
		year += 100;
	}
	unsigned y4 = totalDays / 1461;
	totalDays -= y4 * 1461;
	year += static_cast<uint16_t>(y4 << 2);
	for (int i = 3; i && totalDays >= 365; --i)
	{
		totalDays -= 365;
		++year;
	}

	unsigned janFeb = 59;
	if (!(year & 3) && (year % 100 || !(year % 400)))
	{
		if (totalDays >= 60)
			--totalDays;
		else
			++janFeb;
	}
	if (totalDays < 181)
	{
		if (totalDays < 90)
			month = (totalDays < 31) ? 1 : (totalDays < janFeb) ? 2 : 3;
		else
			month = (totalDays < 120) ? 4 : (totalDays < 151) ? 5 : 6;
	}
	else if (totalDays < 273)
		month = (totalDays < 212) ? 7 : (totalDays < 243) ? 8 : 9;
	else
		month = (totalDays < 304) ? 10 : (totalDays < 334) ? 11 : 12;

	day = static_cast<uint16_t>(totalDays - monthA[month]);
}

} // namespace util
