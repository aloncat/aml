//∙AML
// Copyright (C) 2017-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "sysinfo.h"

#include "console.h"
#include "datetime.h"
#include "filesystem.h"
#include "winapi.h"

#if AML_OS_WINDOWS
	#include <shlobj.h>
#endif

using namespace util;

//--------------------------------------------------------------------------------------------------------------------------------
SystemInfo::SystemInfo()
{
	m_FirstTick = GetOSUptime();
	m_LaunchDateTime = DateTime::Now(true);

	InitCoreCount();

	#if AML_OS_WINDOWS
		const wchar_t* cmdLine = ::GetCommandLineW();
		if (GetNextCmdLineParam(m_AppExePath, cmdLine))
		{
			if (!m_AppExePath.empty())
			{
				if (auto fullPath = FileSystem::GetFullPath(m_AppExePath); !fullPath.empty())
					m_AppExePath = std::move(fullPath);
			}
			std::wstring param;
			while (GetNextCmdLineParam(param, cmdLine))
				m_CmdLineParameters.push_back(param);
		}

		wchar_t path[MAX_PATH];
		if (::SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, path) == S_OK)
		{
			std::wstring fullPath(path);
			m_AppDataPath = FileSystem::GetFullPath(fullPath.append(L"\\"));
		}
	#else
		#error Not implemented
	#endif
}

//--------------------------------------------------------------------------------------------------------------------------------
unsigned SystemInfo::GetAppUptime() const
{
	return GetOSUptime() - m_FirstTick;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE unsigned SystemInfo::GetOSUptime()
{
	#if AML_OS_WINDOWS
		uint64_t osUptime;
		if (WinAPI::CanGetTickCount64())
			osUptime = WinAPI::GetTickCount64();
		else
		{
			size_t last = ::GetTickCount();
			static const auto first = last;
			osUptime = last;
			if (last < first)
				osUptime += 1ll << 32;
		}

		return static_cast<unsigned>(osUptime / 1000);
	#else
		#error Not implemented
	#endif
}

//--------------------------------------------------------------------------------------------------------------------------------
char SystemInfo::GetDecimalPoint(bool localeChanged) const
{
	static char cachedValue;
	if (!cachedValue || localeChanged)
	{
		char buffer[8];
		// Если локаль программы была установлена, то функции форматирования будут использовать в качестве
		// десятичной точки символ, заданный в настройках ОС (по умолчанию в "C" локали используется точка)
		auto i = sprintf_s(buffer, CountOf(buffer), "%.1f", .1f);
		cachedValue = (i > 0) ? buffer[1] : '.';
	}
	return cachedValue;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool SystemInfo::IsConsoleApp()
{
	class Helper : public Console
	{
	public:
		static bool HasAllocatedConsole() { return s_HasAllocatedConsole; }
	};

	#if AML_OS_WINDOWS
		// Приложение будет считаться консольным (функция вернёт true), если классом Console не создавалось окно
		// консоли и в данный момент к нашему процессу не подключено никакое другое окно. Недостаток этого способа
		// в том, что если наше приложение консольное, но запущено в DETACHED состоянии, то функция вёрнет true
		return !Helper::HasAllocatedConsole() && ::GetConsoleWindow() != nullptr;
	#else
		#error Not implemented
	#endif
}

//--------------------------------------------------------------------------------------------------------------------------------
void SystemInfo::InitCoreCount()
{
	#if AML_OS_WINDOWS
		SYSTEM_INFO sysInfo;
		::GetSystemInfo(&sysInfo);
		m_CoreCount.logical = sysInfo.dwNumberOfProcessors ? sysInfo.dwNumberOfProcessors : 1;
		m_CoreCount.physical = m_CoreCount.logical;

		DWORD bufferSize = 0;
		::GetLogicalProcessorInformation(nullptr, &bufferSize);
		if (::GetLastError() == ERROR_INSUFFICIENT_BUFFER && bufferSize)
		{
			void* p = new uint8_t[bufferSize];
			auto info = static_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION>(p);
			if (::GetLogicalProcessorInformation(info, &bufferSize))
			{
				unsigned cores = 0, threads = 0;
				for (size_t size = 0; size < bufferSize; size += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION))
				{
					if (info->Relationship == RelationProcessorCore)
					{
						++cores;
						for (auto mask = info->ProcessorMask; mask; mask >>= 1)
							threads += (mask & 1) ? 1 : 0;
					}
					++info;
				}
				m_CoreCount.logical = threads ? threads : 1;
				m_CoreCount.physical = cores ? cores : 1;
				if (m_CoreCount.logical < m_CoreCount.physical)
					m_CoreCount.logical = m_CoreCount.physical;
			}
			delete[] p;
		}
	#else
		#error Not implemented
	#endif
}

//--------------------------------------------------------------------------------------------------------------------------------
bool SystemInfo::GetNextCmdLineParam(std::wstring& out, const wchar_t*& cmdLine)
{
	out.clear();
	if (!cmdLine || !cmdLine[0])
		return false;

	const wchar_t* p = cmdLine;
	while (*p == 32) ++p;

	bool allowEmpty = false;
	const wchar_t* from = p;
	for (bool quote = false; *p; ++p)
	{
		if (*p == '"')
		{
			quote = !quote;
			allowEmpty = true;
			if (p > from)
				out.append(from, p - from);
			from = p + 1;
		}
		else if (*p == 32 && !quote)
			break;
	}

	cmdLine = p;
	if (p > from)
		out.append(from, p - from);

	return !out.empty() || allowEmpty;
}
