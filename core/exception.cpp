//∙AML
// Copyright (C) 2016-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "exception.h"

using namespace util;

//--------------------------------------------------------------------------------------------------------------------------------
EGeneric::EGeneric(std::string_view msg) noexcept
	: m_What(CopyString(msg.data(), msg.size()))
{
}

//--------------------------------------------------------------------------------------------------------------------------------
EGeneric::EGeneric(const EGeneric& that) noexcept
	: m_What(CopyString(that.m_What))
{
}

//--------------------------------------------------------------------------------------------------------------------------------
EGeneric::~EGeneric() noexcept
{
	Tidy();
}

//--------------------------------------------------------------------------------------------------------------------------------
const char* EGeneric::what() const noexcept
{
	return m_What ? m_What : "";
}

//--------------------------------------------------------------------------------------------------------------------------------
EGeneric& EGeneric::operator =(const EGeneric& that) noexcept
{
	if (this != &that)
	{
		Tidy();
		m_What = CopyString(that.m_What);
	}

	return *this;
}

//--------------------------------------------------------------------------------------------------------------------------------
void EGeneric::Tidy() noexcept
{
	if (m_What)
	{
		free(const_cast<char*>(m_What));
		m_What = nullptr;
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE const char* EGeneric::CopyString(const char* str, size_t size)
{
	if (size && size < ~size_t(0))
	{
		if (char* buffer = static_cast<char*>(malloc(size + 1)))
		{
			memcpy(buffer, str, size);
			buffer[size] = 0;

			return buffer;
		}
	}

	return nullptr;
}

//--------------------------------------------------------------------------------------------------------------------------------
const char* EGeneric::CopyString(const char* str)
{
	return str ? CopyString(str, strlen(str)) : nullptr;
}
