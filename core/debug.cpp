//∙AML
// Copyright (C) 2018-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "debug.h"

using namespace util;

//--------------------------------------------------------------------------------------------------------------------------------
bool DebugHelper::IsDebuggerActive()
{
	// TODO
	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
void DebugHelper::OnAssert(const wchar_t* filePath, int line, const wchar_t* expression)
{
	// TODO
}

//--------------------------------------------------------------------------------------------------------------------------------
void DebugHelper::OnHalt(const wchar_t* filePath, int line, std::string_view msg)
{
	// TODO
}

//--------------------------------------------------------------------------------------------------------------------------------
void DebugHelper::OnHalt(const wchar_t* filePath, int line, std::wstring_view msg)
{
	// TODO
}

//--------------------------------------------------------------------------------------------------------------------------------
void DebugHelper::Abort(int exitCode)
{
	// TODO
	_exit(exitCode);
}
