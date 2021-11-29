//∙AML
// Copyright (C) 2016-2021 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "strcommon.h"

#include <string>
#include <string_view>
#include <utility>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Отсутствующие в C++17 бинарные операторы + для классов std::[w]string и std::[w]string_view (-ish)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace std {

//--------------------------------------------------------------------------------------------------------------------------------
template<class CharT, class Traits, class Alloc, class StringViewIsh,
	class = util::IsStringViewIsh<StringViewIsh, CharT, Traits>>
inline basic_string<CharT, Traits, Alloc> operator +(
	const basic_string<CharT, Traits, Alloc>& lhs,
	const StringViewIsh& rhs)
{
	const basic_string_view<CharT, Traits> sv = rhs;
	basic_string<CharT, Traits, Alloc> result;
	result.reserve(lhs.size() + sv.size());
	result.assign(lhs);
	result.append(sv.data(), sv.size());
	return result;
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class CharT, class Traits, class Alloc, class StringViewIsh,
	class = util::IsStringViewIsh<StringViewIsh, CharT, Traits>>
inline basic_string<CharT, Traits, Alloc> operator +(
	const StringViewIsh& lhs,
	const basic_string<CharT, Traits, Alloc>& rhs)
{
	const basic_string_view<CharT, Traits> sv = lhs;
	basic_string<CharT, Traits, Alloc> result;
	result.reserve(sv.size() + rhs.size());
	result.assign(sv.data(), sv.size());
	result.append(rhs);
	return result;
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class CharT, class Traits, class Alloc, class StringViewIsh,
	class = util::IsStringViewIsh<StringViewIsh, CharT, Traits>>
inline basic_string<CharT, Traits, Alloc> operator +(
	basic_string<CharT, Traits, Alloc>&& lhs,
	const StringViewIsh& rhs)
{
	const basic_string_view<CharT, Traits> sv = rhs;
	return move(lhs.append(sv.data(), sv.size()));
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class CharT, class Traits, class Alloc, class StringViewIsh,
	class = util::IsStringViewIsh<StringViewIsh, CharT, Traits>>
inline basic_string<CharT, Traits, Alloc> operator +(
	const StringViewIsh& lhs,
	basic_string<CharT, Traits, Alloc>&& rhs)
{
	const basic_string_view<CharT, Traits> sv = lhs;
	return move(rhs.insert(0, sv.data(), sv.size()));
}

} // namespace std
