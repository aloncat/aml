//∙AML
// Copyright (C) 2017-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "platform.h"
#include "singleton.h"

#include <string>
#include <vector>

namespace util {

//--------------------------------------------------------------------------------------------------------------------------------
class SystemInfo : public Singleton<SystemInfo>
{
public:
	struct CoreCount {
		unsigned logical;	// Кол-во логических процессоров (макс. число одновременно исполняемых потоков)
		unsigned physical;	// Кол-во физических процессоров (равно logical, если CPU без технологии HT)
	};

	// Возвращает ссылку на структуру с информацией о количестве
	// логических и физических процессоров (ядер CPU) в системе
	const CoreCount& GetCoreCount() const { return m_CoreCount; }
	// Возвращает дату и время запуска (UTC) в формате DateTime. Инициализируется
	// значением текущего системного времени ОС в момент инициализации SystemInfo
	uint64_t GetLaunchDateTime() const { return m_LaunchDateTime; }

	// Возвращает список параметров командной строки (параметры запуска приложения)
	const std::vector<std::wstring>& GetCommandLineParameters() const { return m_CmdLineParameters; }

	// Возвращает полный путь к исполнимому файлу приложения
	const std::wstring& GetAppExePath() const { return m_AppExePath; }
	// Возвращает полный путь (оканчивающийся слешем) к директории локальных пользовательских данных приложений
	const std::wstring& GetAppDataPath() const { return m_AppDataPath; }

	// Возвращает количество секунд, прошедших с момента запуска приложения, а точнее с момента инициализации SystemInfo.
	// На Windows Server 2003 и Windows XP значение ограничено 4,294,967 секундами (примерно 49.71 суток). При превышении
	// этого порога отсчёт снова начинается с 0. На более новых ОС семейства Windows такого ограничения нет
	unsigned GetAppUptime() const;

	// Возвращает количество секунд, прошедших с момента старта операционной системы. На Windows Server 2003
	// и Windows XP эта функция возвращает корректное значение, если на момент инициализации SystemInfo время,
	// прошедшее с момента старта ОС, не превышало 49.71 суток, и только до момента, пока время работы самого
	// приложения не превысит это значение. На более новых ОС семейства Windows такого ограничения нет
	static unsigned GetOSUptime();

	// Возвращает символ, используемый функциями форматирования как разделитель целой и дробной части чисел с плавающей
	// запятой. Если параметр localeChanged равен false, то функция вернёт значение из кеша, а если он равен true, то
	// обновит кеш и вернёт актуальное значение. Если локаль программы не устанавливалась, то это всегда будет '.'
	char GetDecimalPoint(bool localeChanged = false) const;

	// Возвращает true, если приложение консольное (точнее, если к нему прикреплено внешнее окно консоли),
	// и возвращает false, если это обычное приложение (без окна консоли или с окном, но созданным нами)
	static bool IsConsoleApp();

protected:
	SystemInfo();
	virtual ~SystemInfo() override = default;

	void InitCoreCount();
	static bool GetNextCmdLineParam(std::wstring& out, const wchar_t*& cmdLine);

private:
	CoreCount m_CoreCount = { 1, 1 };
	std::vector<std::wstring> m_CmdLineParameters;

	std::wstring m_AppExePath;
	std::wstring m_AppDataPath;

	uint64_t m_LaunchDateTime = 0;
	unsigned m_FirstTick = 0;
};

} // namespace util
