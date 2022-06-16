//∙AML
// Copyright (C) 2019-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "file.h"
#include "platform.h"
#include "singleton.h"
#include "strcommon.h"
#include "strformat.h"
#include "strutil.h"
#include "threadsync.h"
#include "util.h"

#include <string>
#include <string_view>
#include <type_traits>

namespace util {

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Logable
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Класс Logable позволяет его наследнику переопределить функцию LogToString и выводить в любой журнал объекты своего
// типа удобным способом через оператор "<<". Вообще, любой класс, имеющий функцию ToString или оператор приведения к
// std::[w]string_view, может быть выведен в журнал с помощью оператора <<. Разница заключается в приоритете: функция
// LogToString имеет самый высокий приоритет, затем проверяется возможность приведения к std::[w]string_view и только
// потом проверяется наличие у класса функции ToString

//--------------------------------------------------------------------------------------------------------------------------------
class Logable
{
	friend class LogRecord;

protected:
	virtual std::wstring LogToString() const;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   LogRecord/LogRecordHolder
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class LogRecord final
{
	friend class Log;
	AML_NONCOPYABLE(LogRecord)

public:
	// Типы сообщений
	enum class MsgType
	{
		Info,		// Информация
		Debug,		// Отладочная информация (обычно не выводится в production сборках)
		Warning,	// Предупреждение (что-то более важное, чем просто информация)
		Error		// Ошибка
	};

	// Возвращает true, если формирование сообщения разрешено
	bool IsOutputEnabled() const noexcept { return m_IsOutputEnabled; }
	// Разрешает или запрещает формирование сообщения. По умолчанию вывод разрешён. Иногда для файловых журналов вывод
	// не имеет смысла, так как сессия журнала не открыта. В таких случаях нет смысла форматировать и сохранять вывод
	void SetOutputEnabled(bool enabled) noexcept { m_IsOutputEnabled = enabled; }

	// Возвращает ссылку на string-view-ish объект StringWriter с накопленным сообщением
	const StringWriter<wchar_t>& GetData() const noexcept { return m_Data; }

	// Информирует класс журнала о завершении формирования сообщения. После вызова этой
	// функции использование объекта (т.е. вызовы операторов << и т.п.) недопустимы
	void End();

	LogRecord& operator <<(bool value);
	LogRecord& operator <<(double value);
	LogRecord& operator <<(int32_t value);
	LogRecord& operator <<(uint32_t value);
	LogRecord& operator <<(int64_t value);
	LogRecord& operator <<(uint64_t value);

	LogRecord& operator <<(char value);
	LogRecord& operator <<(wchar_t value);
	LogRecord& operator <<(const char* str);
	LogRecord& operator <<(const wchar_t* str);
	LogRecord& operator <<(std::string_view str);
	LogRecord& operator <<(std::wstring_view str);

	LogRecord& operator <<(const Logable& obj);

	// Выводит объект obj любого типа при условии, что у него есть функция ToString. Эта
	// функция должна возвращать значение любого типа, поддерживаемого классом LogRecord
	template<class T, class = std::enable_if_t<
		!std::is_convertible_v<const T&, std::string_view> &&
		!std::is_convertible_v<const T&, std::wstring_view>,
		decltype(std::declval<T>().ToString())>>
	LogRecord& operator <<(const T& obj)
	{
		if (m_IsOutputEnabled)
		{
			auto&& data = obj.ToString();
			return *this << data;
		}

		return *this;
	}

	// Выводит в указанный массив buffer отформатированный заголовок сообщения с датой и временем time и маркером типа
	// сообщения msgType. Параметр bufferSize задаёт размер буфера в символах (размер буфера должен быть достаточен для
	// вывода заголовка и терминирующего ноля). В случае успеха функция возвращает количество выведенных символов (без
	// учёта ноля). Если размер буфера окажется недостаточным, функция вернёт отрицательное число
	static int FormatHeader(wchar_t* buffer, size_t bufferSize, MsgType msgType, uint64_t time);
	// Функция, эквивалентная предыдущей, но возвращающая строку с заголовком
	static std::wstring FormatHeader(MsgType msgType, uint64_t time);

private:
	explicit LogRecord(Log& log);
	~LogRecord() = default;

	void Clear() { m_Data.Clear(); }

	// Начинает новое сообщение с даты, времени и маркера типа
	void Start(MsgType msgType, uint64_t time);
	// Добавляет строку str к сообщению
	void Append(std::string_view str);

private:
	Log& m_Log;
	Formatter<wchar_t> m_Data;
	bool m_IsOutputEnabled = true;
};

//--------------------------------------------------------------------------------------------------------------------------------
class LogRecordHolder final
{
	AML_NONCOPYABLE(LogRecordHolder)

public:
	explicit LogRecordHolder(LogRecord::MsgType msgType);
	LogRecordHolder(Log& log, LogRecord::MsgType msgType);

	~LogRecordHolder() { m_Record->End(); }

	LogRecord& operator *() { return *m_Record; }
	LogRecord* operator ->() { return m_Record; }

private:
	LogRecord* m_Record;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Log
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class Log
{
	AML_NONCOPYABLE(Log)

public:
	using MsgType = LogRecord::MsgType;

	enum class TimeFormat {
		Utc,	// Использовать для штампа даты/времени UTC время
		Local	// Использовать для штампа даты/времени локальное время
	};

	Log();
	virtual ~Log();

	TimeFormat GetTimeFormat() const { return m_TimeFormat; }
	// Устанавливает формат штампа даты/времени (локальное
	// либо UTC время). По умолчанию используется локальное время
	void SetTimeFormat(TimeFormat format) { m_TimeFormat = format; }

	// Разрешает или запрещает вывод в журнал сообщений типа MsgType::Debug (по
	// умолчанию вывод сообщений этого типа запрещён только в production сборках)
	void SetDebugMsgAllowed(bool allowed) { m_IsDebugMsgAllowed = allowed; }

	// Сбрасывает все накопленные записи журнала в файл (для файловых журналов, если они используют
	// отложенную запись). Функция вернёт управление только после полного завершения операции
	virtual void Flush() {}

protected:
	// Возвращает указатель на объект LogRecord для формирования записи указанного типа. После того,
	// как новая запись будет сформирована, нужно вызвать функцию LogRecord::End для добавления
	// сформированной записи в журнал и освобождения объекта LogRecord
	virtual LogRecord* StartRecord(MsgType msgType);

	// Эта функция должна вызываться из функции LogRecord::End, чтобы инициировать копирование
	// готового сообщения из объекта record во внутренний буфер (файл) журнала, после которого
	// объект record будет освобождён и помещён в стек (для повторного использования)
	virtual void OnRecordEnd(LogRecord* record);

protected:
	TimeFormat m_TimeFormat = TimeFormat::Local;

	bool m_IsOutputEnabled = false;
	bool m_IsDebugMsgAllowed = false;

private:
	class LogRecordStack;
	LogRecordStack* m_Records = nullptr;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   FileLog
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class FileLog : public Log
{
public:
	virtual ~FileLog() override;

	// Возвращает true, если сессия журнала активна
	bool IsOpened() const;

	// Начинает новую сессию журнала в указанном файле. Если указанный файл не существует, то он создается. Если
	// параметр append равен false, то файл очищается, и в него выводится сообщение о начале новой сессии. Если
	// append равен true, то сообщение дописывается к концу файла. До первого вызова этой функции весь вывод в
	// лог игнорируется. Если на момент вызова функции сессия журнала открыта, то она будет завершена. Если
	// файл будет успешно создан/открыт, то функция вернёт true. В случае ошибки функция вернёт false
	bool Open(WZStringView filePath, bool append = false);

	// Закрывает текущую сессию, если она была открыта. После вызова этой функции весь вывод в лог
	// будет игнорироваться до тех пор, пока не будет открыта новая сессия вызовом функции Open
	void Close();

	virtual void Flush() override { m_File.Flush(); }

protected:
	virtual void OnRecordEnd(LogRecord* record) override;
	void WriteToFile(std::wstring_view text);

protected:
	thrd::CriticalSection m_CS;
	BinaryFile m_File;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SystemLog
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class SystemLog : public FileLog, public Singleton<SystemLog>
{
protected:
	SystemLog();
	virtual ~SystemLog() override = default;

	virtual LogRecord* StartRecord(MsgType msgType) override;
	virtual void OnRecordEnd(LogRecord* record) override;
};

//--------------------------------------------------------------------------------------------------------------------------------
#define AML_SYSLOG_IMPL(MSG_TYPE, MSG) \
	((void)(*util::LogRecordHolder(MSG_TYPE) << MSG, 0))

#define LOG_INFO(MSG) AML_SYSLOG_IMPL(util::Log::MsgType::Info, MSG)
#define LOG_DEBUG(MSG) AML_SYSLOG_IMPL(util::Log::MsgType::Debug, MSG)
#define LOG_WARNING(MSG) AML_SYSLOG_IMPL(util::Log::MsgType::Warning, MSG)
#define LOG_ERROR(MSG) AML_SYSLOG_IMPL(util::Log::MsgType::Error, MSG)

} // namespace util
