//∙AML
// Copyright (C) 2020-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include <auxlib/print.h>
#include <core/util.h>

//--------------------------------------------------------------------------------------------------------------------------------
int wmain(int argC, const wchar_t* argA[], const wchar_t* envA[])
{
	util::CheckMinimalRequirements();

	aux::Printc("#3AML project. #7Console application sample.\n");

	return 0;
}
