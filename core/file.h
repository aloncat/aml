//∙AML
// Copyright (C) 2016-2021 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "platform.h"
#include "strcommon.h"
#include "util.h"

#include <utility>

namespace util {

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   File - базовый (абстрактный) класс файла
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

enum {
	FILE_OPEN_READ		= 0x01,		// Открыть файл с доступом на чтение
	FILE_OPEN_WRITE		= 0x02,		// Открыть файл с доступом на запись
	FILE_OPEN_READWRITE	= 0x03,		// То же, что и FILE_OPEN_READ | FILE_OPEN_WRITE
	FILE_OPEN_ALWAYS	= 0x04,		// Создать файл, если не существует; открыть, если существует
	FILE_CREATE_ALWAYS	= 0x08,		// Создать файл, если не существует; перезаписать, если существует
	FILE_DENY_READ		= 0x10,		// Запретить другим процессам доступ к файлу на чтение

	FILE_OPENFLAG_MASK	= 0x1f
};

//--------------------------------------------------------------------------------------------------------------------------------
class File
{
	AML_NONCOPYABLE(File)

public:
	File() = default;
	virtual ~File() = default;

	// Открывает файл в режиме, заданном набором флагов flags. Параметр path задаёт путь к файлу. Если класс
	// наследник (например, такой как MemoryFile) не работает с файловой системой ОС, то он должен определить
	// свою функцию Open, у которой не будет параметра path. Функция вернёт true, если файл будет успешно
	// открыт (создан). Если файл уже открыт, то функция вернёт false, ничего не сделав
	virtual bool Open(WZStringView path, unsigned flags) { return false; }
	// Закрывает файл, если он был открыт
	virtual void Close() { m_OpenFlags = 0; }

	virtual bool IsOpened() const { return m_OpenFlags != 0; }
	unsigned GetOpenFlags() const noexcept { return m_OpenFlags; }

	// Читает не более bytesToRead байт. Возвращает количество реально прочитанных байт в поле first пары.
	// Если файл короче, то функция вернёт true в поле second, а значение first будет меньше bytesToRead.
	// В случае ошибки функция вернёт false в поле second, а позиция файла будет не определена
	virtual std::pair<size_t, bool> Read(void* buffer, size_t bytesToRead) = 0;
	// Записывает bytesToWrite байт из буфера buffer в файл. В случае
	// ошибки функция вернёт false, а позиция файла будет не определена
	virtual bool Write(const void* buffer, size_t bytesToWrite) = 0;
	// Сбрасывает все данные из кэша (буфера записи) в физический файл
	virtual bool Flush() { return IsOpened(); }

	// Функции получения размера файла и его текущей позиции (только для открытого файла).
	// В случае ошибки (а также если файл не открыт) возвращают отрицательное значение
	virtual long long GetSize() const = 0;
	virtual long long GetPosition() const = 0;
	// Устанавливает новую текущую позицию файла. Если позиция находится за реальным концом файла,
	// то при последующей попытке записи данных в файл пустое пространство будет заполнено "мусором"
	virtual bool SetPosition(long long position) = 0;
	// Устанавливает размер файла по текущей позиции. Если текущая позиция файла находится
	// за реальным концом данных, файл будет дополнен "мусором" до необходимого размера
	virtual bool Truncate() = 0;

	// Вычисляет CRC32 файла. Значение возвращается в поле first пары. Параметр position задаёт начало блока данных;
	// нулевое значение соответствует началу файла, значение -1 соответствует текущей позиции. Параметр size задаёт
	// размер блока; если значение не задано (равно ~0), то концом блока считается конец файла. Функция изменяет
	// текущую позицию файла. Если поле second возвращаемой пары равно false, значит произошла ошибка
	std::pair<uint32_t, bool> GetCRC32(long long position = 0, size_t size = ~size_t(0));

	// Сохраняет файл целиком в указанный файл file. Если параметр clearDest равен true, то всё содержимое файла
	// назначения file удаляется; если равен false, то запись начинается с текущей позиции; если file длиннее, чем
	// записываемые данные, то остальная часть файла не изменяется. Функция меняет текущую позицию обоих файлов
	bool SaveTo(File& file, bool clearDest = false);
	// Сохраняет файл целиком в другой файл по указанному пути path. Если такой файл существует,
	// то его содержимое будет полностью заменено. Функция меняет текущую позицию этого файла
	bool SaveTo(WZStringView path);

protected:
	virtual bool SaveToCustom(File& file);
	virtual bool GetCRC32Custom(uint32_t& crc, long long size);

	unsigned m_OpenFlags = 0;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   BinaryFile - файл файловой системы ОС
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class BinaryFile : public File
{
public:
	BinaryFile();
	virtual ~BinaryFile() override;

	virtual bool Open(WZStringView path, unsigned flags = FILE_OPEN_READ) override;
	virtual void Close() override;

	virtual bool IsOpened() const override;

	virtual std::pair<size_t, bool> Read(void* buffer, size_t bytesToRead) override;
	virtual bool Write(const void* buffer, size_t bytesToWrite) override;
	virtual bool Flush() override;

	virtual long long GetSize() const override;
	virtual long long GetPosition() const override;
	virtual bool SetPosition(long long position) override;
	virtual bool Truncate() override;

protected:
	struct FileSystem;
	void* m_FileHandle = nullptr;
};

} // namespace util
