//∙AML
// Copyright (C) 2019-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "log.h"

#include "array.h"
#include "datetime.h"
#include "debug.h"

#include <vector>

using namespace util;

// TODO: сейчас вывод в консоль отладчика, запись в файл, а также преобразование Wide->UTF-8 происходят в одном
// и том же потоке. Чтобы ускорить работу журнала, можно вынести часть этой работы во вспомогательный поток. Но
// сделать это нужно так, чтобы при отладке перед тем, как отладчик приостановит работу всех потоков в макросах
// Assert/Verify/Halt, сообщение уже вывелось в файл и консоль (для этого должно быть достаточно вызова Flush)

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Logable
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring Logable::LogToString() const
{
	Halt("Function LogToString must be overriden in derived class");
	return std::wstring();
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   LogAccessor
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class LogAccessor final : public Log
{
public:
	using Log::StartRecord;
	using Log::OnRecordEnd;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   LogRecord
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
LogRecord::LogRecord(Log& log)
	: m_Log(log)
{
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void LogRecord::End()
{
	if (m_IsOutputEnabled)
		m_Data.Append(L'\n');

	auto& log = static_cast<LogAccessor&>(m_Log);
	log.OnRecordEnd(this);
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE LogRecord& LogRecord::operator <<(bool value)
{
	if (m_IsOutputEnabled)
		m_Data << value;

	return *this;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE LogRecord& LogRecord::operator <<(double value)
{
	if (m_IsOutputEnabled)
		m_Data << value;

	return *this;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE LogRecord& LogRecord::operator <<(int32_t value)
{
	if (m_IsOutputEnabled)
		m_Data << value;

	return *this;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE LogRecord& LogRecord::operator <<(uint32_t value)
{
	if (m_IsOutputEnabled)
		m_Data << value;

	return *this;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE LogRecord& LogRecord::operator <<(int64_t value)
{
	if (m_IsOutputEnabled)
		m_Data << value;

	return *this;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE LogRecord& LogRecord::operator <<(uint64_t value)
{
	if (m_IsOutputEnabled)
		m_Data << value;

	return *this;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE LogRecord& LogRecord::operator <<(char value)
{
	if (m_IsOutputEnabled)
	{
		if (value < 0x80)
		{
			wchar_t c = value;
			m_Data.Append(c);
		} else
		{
			Append({ &value, 1 });
		}
	}

	return *this;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE LogRecord& LogRecord::operator <<(wchar_t value)
{
	if (m_IsOutputEnabled)
		m_Data.Append(value);

	return *this;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE LogRecord& LogRecord::operator <<(const char* str)
{
	if (m_IsOutputEnabled)
	{
		size_t count = str ? strlen(str) : 0;
		Append({ str, count });
	}

	return *this;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE LogRecord& LogRecord::operator <<(const wchar_t* str)
{
	if (m_IsOutputEnabled && str)
		m_Data.Append(str);

	return *this;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE LogRecord& LogRecord::operator <<(std::string_view str)
{
	if (m_IsOutputEnabled)
		Append(str);

	return *this;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE LogRecord& LogRecord::operator <<(std::wstring_view str)
{
	if (m_IsOutputEnabled)
		m_Data.Append(str);

	return *this;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE LogRecord& LogRecord::operator <<(const Logable& obj)
{
	if (m_IsOutputEnabled)
	{
		auto&& text = obj.LogToString();
		m_Data.Append(text);
	}

	return *this;
}

//--------------------------------------------------------------------------------------------------------------------------------
int LogRecord::FormatHeader(wchar_t* buffer, size_t bufferSize, MsgType msgType, uint64_t time)
{
	if (!buffer || !bufferSize)
		return -1;

	wchar_t typeLetter = '*';
	switch (msgType)
	{
		case MsgType::Info:
			typeLetter = 'I';
			break;
		case MsgType::Debug:
			typeLetter = 'D';
			break;
		case MsgType::Warning:
			typeLetter = 'W';
			break;
		case MsgType::Error:
			typeLetter = 'E';
			break;
	}

	DateTime dt;
	dt.Decode(time);

	return FormatEx(buffer, bufferSize, L"[%04u-%02u-%02u %02u:%02u:%02u.%03u][%c] ",
		dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second, dt.ms, typeLetter);
}

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring LogRecord::FormatHeader(MsgType msgType, uint64_t time)
{
	wchar_t buffer[32];
	int len = FormatHeader(buffer, CountOf(buffer), msgType, time);
	return std::wstring(buffer, (len > 0) ? len : 0);
}

//--------------------------------------------------------------------------------------------------------------------------------
void LogRecord::Start(MsgType msgType, uint64_t time)
{
	m_Data.Clear();
	if (m_IsOutputEnabled)
	{
		wchar_t buffer[32];
		int len = FormatHeader(buffer, CountOf(buffer), msgType, time);
		m_Data.Append(buffer, (len > 0) ? len : 0);
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void LogRecord::Append(std::string_view str)
{
	const size_t count = str.size();
	wchar_t buffer[3840 / sizeof(wchar_t)];

	// При преобразовании Ansi -> Unicode вполне реальна ситуация, когда каждый символ исходной строки будет
	// представлен несколькими (до 3) символами в результирующей. Из соображений производительности, мы будем
	// для коротких строк пытаться сделать преобразование за один вызов. Но если буфер окажется недостаточен,
	// мы получим ошибку и тогда перейдём к стандартному способу. Чтобы снизить вероятность недостаточности
	// буфера (и "лишней" попытки) мы выбрали здесь максимальную длину строки в 1/2 размера буфера
	if (count <= CountOf(buffer) / 2)
	{
		if (int len = FromAnsi(buffer, CountOf(buffer), str); len >= 0)
		{
			m_Data.Append(buffer, len);
			return;
		}
	}

	// Стандратный (более медленный) способ за 2 вызова: сначала получаем размер необходимого буфера,
	// а затем выполняем преобразование. Если необходимый размер окажется меньше размера локального
	// буфера buffer, то нам даже не придётся выделять память в куче под буфер большего размера
	if (int bufLen = FromAnsi(nullptr, 0, str); bufLen > 0)
	{
		FlexibleArray bigBuffer(buffer);
		bigBuffer.Grow(bufLen);

		if (int len = FromAnsi(bigBuffer, bufLen, str); len > 0)
			m_Data.Append(bigBuffer, len);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   LogRecordHolder
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE LogRecordHolder::LogRecordHolder(LogRecord::MsgType msgType)
	: LogRecordHolder(SystemLog::Instance(), msgType)
{
}

//--------------------------------------------------------------------------------------------------------------------------------
LogRecordHolder::LogRecordHolder(Log& log, LogRecord::MsgType msgType)
	: m_Record(static_cast<LogAccessor&>(log).StartRecord(msgType))
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Log::LogRecordStack
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class Log::LogRecordStack final
{
public:
	LogRecordStack(Log& log);
	~LogRecordStack();

	LogRecord* Get();
	void Release(LogRecord* record);

private:
	Log& m_Log;
	thread::CriticalSection m_CS;
	std::vector<LogRecord*> m_Records;
};

//--------------------------------------------------------------------------------------------------------------------------------
Log::LogRecordStack::LogRecordStack(Log& log)
	: m_Log(log)
	, m_CS(500)
{
}

//--------------------------------------------------------------------------------------------------------------------------------
Log::LogRecordStack::~LogRecordStack()
{
	for (auto record : m_Records)
		AML_SAFE_DELETE(record);
}

//--------------------------------------------------------------------------------------------------------------------------------
LogRecord* Log::LogRecordStack::Get()
{
	thread::Lock lock(m_CS);

	if (!m_Records.empty())
	{
		auto p = m_Records.back();
		m_Records.pop_back();
		return p;
	}

	lock.Leave();
	return new LogRecord(m_Log);
}

//--------------------------------------------------------------------------------------------------------------------------------
void Log::LogRecordStack::Release(LogRecord* record)
{
	if (record)
	{
		record->Clear();

		thread::Lock lock(m_CS);
		m_Records.push_back(record);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Log
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
Log::Log()
	: m_IsDebugMsgAllowed(!IsProductionBuild())
{
	m_Records = new LogRecordStack(*this);
}

//--------------------------------------------------------------------------------------------------------------------------------
Log::~Log()
{
	AML_SAFE_DELETE(m_Records);
}

//--------------------------------------------------------------------------------------------------------------------------------
LogRecord* Log::StartRecord(MsgType msgType)
{
	const bool utc = m_TimeFormat == TimeFormat::Utc;
	const bool outputEnabled = m_IsOutputEnabled && (msgType != MsgType::Debug || m_IsDebugMsgAllowed);

	auto record = m_Records->Get();
	record->SetOutputEnabled(outputEnabled);
	record->Start(msgType, outputEnabled ? DateTime::Now(utc) : 0);

	return record;
}

//--------------------------------------------------------------------------------------------------------------------------------
void Log::OnRecordEnd(LogRecord* record)
{
	m_Records->Release(record);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   FileLog
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
FileLog::~FileLog()
{
	Close();
}

//--------------------------------------------------------------------------------------------------------------------------------
bool FileLog::IsOpened() const
{
	return m_File.IsOpened();
}

//--------------------------------------------------------------------------------------------------------------------------------
bool FileLog::Open(WZStringView filePath, bool append)
{
	Close();

	// TODO
	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
void FileLog::Close()
{
	m_IsOutputEnabled = false;
	if (m_File.IsOpened())
		m_File.Close();
}

//--------------------------------------------------------------------------------------------------------------------------------
void FileLog::OnRecordEnd(LogRecord* record)
{
	if (record && record->IsOutputEnabled() && IsOpened())
		WriteToFile(record->GetData());

	Log::OnRecordEnd(record);
}

//--------------------------------------------------------------------------------------------------------------------------------
void FileLog::WriteToFile(std::wstring_view text)
{
	// TODO
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   SystemLog
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
SystemLog::SystemLog()
{
	// NB: так как синглтон DebugHelper используется при выводе сообщений, в том числе о критических ошибках,
	// то мы не хотим, чтобы он инициализировался по требованию. Мы хотим, чтобы к моменту вывода в системный
	// журнал DebugHelper уже был проиницилизирован. Таким образом, факт наличия инстанса системного журнала
	// будет гарантировать и наличие инстанса DebugHelper
	DebugHelper::Instance();
}

//--------------------------------------------------------------------------------------------------------------------------------
LogRecord* SystemLog::StartRecord(MsgType msgType)
{
	m_IsOutputEnabled = IsOpened() || DebugHelper::Instance().IsDebugOutputEnabled();
	return FileLog::StartRecord(msgType);
}

//--------------------------------------------------------------------------------------------------------------------------------
void SystemLog::OnRecordEnd(LogRecord* record)
{
	if (record && record->IsOutputEnabled())
	{
		auto&& s = record->GetData();
		DebugHelper::DebugOutput(s);
	}

	FileLog::OnRecordEnd(record);
}
