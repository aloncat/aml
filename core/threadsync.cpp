//∙AML
// Copyright (C) 2017-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "threadsync.h"

#include "winapi.h"

using namespace thrd;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   CriticalSection (Windows)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if AML_OS_WINDOWS

//--------------------------------------------------------------------------------------------------------------------------------
CriticalSection::CriticalSection(unsigned spinCount) noexcept
{
	static_assert(sizeof(m_InnerBuf) >= sizeof(CRITICAL_SECTION),
		"Insufficient size of m_InnerBuf array");

	auto cs = reinterpret_cast<CRITICAL_SECTION*>(m_InnerBuf);
	::InitializeCriticalSectionAndSpinCount(cs, spinCount);
	m_Data = cs;
}

//--------------------------------------------------------------------------------------------------------------------------------
CriticalSection::~CriticalSection() noexcept
{
	auto cs = static_cast<CRITICAL_SECTION*>(m_Data);
	::DeleteCriticalSection(cs);
}

//--------------------------------------------------------------------------------------------------------------------------------
bool CriticalSection::TryEnter() noexcept
{
	auto cs = static_cast<CRITICAL_SECTION*>(m_Data);
	return ::TryEnterCriticalSection(cs) != 0;
}

//--------------------------------------------------------------------------------------------------------------------------------
void CriticalSection::Enter() noexcept
{
	auto cs = static_cast<CRITICAL_SECTION*>(m_Data);
	::EnterCriticalSection(cs);
}

//--------------------------------------------------------------------------------------------------------------------------------
void CriticalSection::Leave() noexcept
{
	auto cs = static_cast<CRITICAL_SECTION*>(m_Data);
	::LeaveCriticalSection(cs);
}

#else
	#error Not implemented
#endif // AML_OS_WINDOWS
