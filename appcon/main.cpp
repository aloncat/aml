//∙AML
// Copyright (C) 2020-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include <auxlib/print.h>
#include <core/strutil.h>
#include <core/util.h>

#include <stdio.h>
#include <string>
#include <string.h>

//--------------------------------------------------------------------------------------------------------------------------------
static std::string GetBuildDateTime(const char* date, const char* time)
{
	int day = 1, month = 0, year = 2000;
	if (date && strlen(date) > 4)
	{
		sscanf_s(date + 4, "%d %d", &day, &year);
		const char* months = "JanFebMarAprMayJunJulAugSepOctNovDec";
		for (; month < 11 && util::StrNInsCmp(date, &months[3 * month], 3); ++month);
	}

	char dateTime[28];
	// NB: 28 байт - мин. размер, достаточный на случай некорректных входных данных
	sprintf_s(dateTime, util::CountOf(dateTime), "%4d-%02d-%02d ", year, month + 1, day);

	if (time && *time)
	{
		strncpy_s(dateTime + 11, util::CountOf(dateTime) - 11, time, 5);
	}

	return std::string(dateTime, (time && *time) ? 16 : 10);
}

//--------------------------------------------------------------------------------------------------------------------------------
int wmain(int argCount, const wchar_t* args[], const wchar_t* envVars[])
{
	util::CheckMinimalRequirements();

	std::string buildVer = GetBuildDateTime(__DATE__, __TIME__);
	aux::Printf("#3AML project. #7Console application sample. #8Built on %s\n", buildVer.c_str());

	return 0;
}
