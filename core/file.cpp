//∙AML
// Copyright (C) 2016-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "file.h"

#include "array.h"
#include "crc32.h"
#include "filesystem.h"
#include "winapi.h"

using namespace util;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   File
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
std::pair<uint32_t, bool> File::GetCRC32(long long position, size_t size)
{
	std::pair<uint32_t, bool> result(0, false);
	if (IsOpened() && (m_OpenFlags & FILE_OPEN_READ))
	{
		long long bytesToProcess = size;
		if (size == ~size_t(0))
		{
			const long long fileSize = GetSize();
			if (fileSize < 0)
				return result;
			long long filePosition = position;
			if (filePosition < 0)
			{
				filePosition = GetPosition();
				if (filePosition < 0)
					return result;
			}
			bytesToProcess = fileSize - filePosition;
			if (bytesToProcess <= 0)
				size = 0;
		}

		if (bytesToProcess <= 0)
			result.second = !size;
		else if (position < 0 || SetPosition(position))
			result.second = GetCRC32Custom(result.first, bytesToProcess);
	}
	return result;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool File::SaveTo(File& file, bool clearDest)
{
	if (!IsOpened() || !(m_OpenFlags & FILE_OPEN_READ))
		return false;
	if (!file.IsOpened() || !(file.m_OpenFlags & FILE_OPEN_WRITE))
		return false;

	if (clearDest && !file.SetPosition(0))
		return false;

	bool result = SaveToCustom(file);

	if (clearDest && !file.Truncate())
		return false;

	return result;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool File::SaveTo(WZStringView path)
{
	bool result = false;
	if (IsOpened() && (m_OpenFlags & FILE_OPEN_READ))
	{
		BinaryFile dest;
		if (dest.Open(path, FILE_OPEN_WRITE | FILE_CREATE_ALWAYS))
		{
			result = SaveToCustom(dest);
			dest.Close();
		}
	}
	return result;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool File::SaveToCustom(File& file)
{
	long long bytesCopied = 0;
	const auto bytesToCopy = GetSize();
	if (bytesToCopy > 0 && SetPosition(0))
	{
		const size_t BLOCK_SIZE = 64 * 1024;
		DynamicArray<uint8_t> buffer(BLOCK_SIZE);

		while (bytesCopied < bytesToCopy)
		{
			auto [bytesRead, success] = Read(buffer, BLOCK_SIZE);
			if (!success || !bytesRead || !file.Write(buffer, bytesRead))
				break;
			bytesCopied += bytesRead;
		}
	}
	return bytesCopied == bytesToCopy;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool File::GetCRC32Custom(uint32_t& crc, long long size)
{
	const size_t BLOCK_SIZE = 64 * 1024;
	DynamicArray<uint8_t> buffer(BLOCK_SIZE);

	for (unsigned long long bytesLeft = size; bytesLeft;)
	{
		size_t toProcess = (bytesLeft < BLOCK_SIZE) ?
			static_cast<size_t>(bytesLeft) : BLOCK_SIZE;

		auto [bytesRead, success] = Read(buffer, toProcess);
		if (!success || !bytesRead)
			return false;

		crc = hash::GetCRC32(buffer, bytesRead, crc);
		bytesLeft -= bytesRead;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   BinaryFile (Windows)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if AML_OS_WINDOWS

//--------------------------------------------------------------------------------------------------------------------------------
struct BinaryFile::FileSystem : public util::FileSystem
{
	using util::FileSystem::MakeLongPath;
};

//--------------------------------------------------------------------------------------------------------------------------------
BinaryFile::BinaryFile()
	: m_FileHandle(INVALID_HANDLE_VALUE)
{
}

//--------------------------------------------------------------------------------------------------------------------------------
BinaryFile::~BinaryFile()
{
	Close();
}

//--------------------------------------------------------------------------------------------------------------------------------
bool BinaryFile::Open(WZStringView path, unsigned flags)
{
	if (IsOpened() || path.empty())
		return false;

	DWORD desiredAccess = 0;
	if (flags & FILE_OPEN_READ)
		desiredAccess |= GENERIC_READ;
	if (flags & FILE_OPEN_WRITE)
		desiredAccess |= GENERIC_WRITE;

	DWORD shareMode = FILE_SHARE_READ;
	if (flags & FILE_DENY_READ)
		shareMode = 0;

	DWORD createDisposition = OPEN_EXISTING;
	if (flags & FILE_CREATE_ALWAYS)
		createDisposition = CREATE_ALWAYS;
	else if (flags & FILE_OPEN_ALWAYS)
		createDisposition = OPEN_ALWAYS;

	std::wstring tmpPath;
	auto longPath = FileSystem::MakeLongPath(path, tmpPath);
	m_FileHandle = ::CreateFileW(longPath.first, desiredAccess, shareMode,
		nullptr, createDisposition, FILE_ATTRIBUTE_ARCHIVE, nullptr);
	if (m_FileHandle == INVALID_HANDLE_VALUE)
		return false;

	m_OpenFlags = flags & FILE_OPENFLAG_MASK;
	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
void BinaryFile::Close()
{
	if (m_FileHandle != INVALID_HANDLE_VALUE)
	{
		::CloseHandle(m_FileHandle);
		m_FileHandle = INVALID_HANDLE_VALUE;
		m_OpenFlags = 0;
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
bool BinaryFile::IsOpened() const
{
	return m_FileHandle != INVALID_HANDLE_VALUE;
}

//--------------------------------------------------------------------------------------------------------------------------------
std::pair<size_t, bool> BinaryFile::Read(void* buffer, size_t bytesToRead)
{
	std::pair<size_t, bool> result(0, false);
	if (m_FileHandle != INVALID_HANDLE_VALUE && buffer)
	{
		if constexpr (sizeof(bytesToRead) == sizeof(DWORD))
		{
			DWORD bytesActuallyRead = 0;
			const DWORD toRead = static_cast<DWORD>(bytesToRead);
			result.second = ::ReadFile(m_FileHandle, buffer, toRead, &bytesActuallyRead, nullptr) != 0;
			result.first = bytesActuallyRead;
		} else
		{
			const size_t MAX_READ_SIZE = 0x80000000;
			for (uint8_t* data = static_cast<uint8_t*>(buffer);;)
			{
				DWORD bytesActuallyRead = 0;
				const size_t toRead = (bytesToRead < MAX_READ_SIZE) ? bytesToRead : MAX_READ_SIZE;
				result.second = ::ReadFile(m_FileHandle, data, static_cast<DWORD>(toRead), &bytesActuallyRead, nullptr) != 0;
				result.first += bytesActuallyRead;

				bytesToRead -= toRead;
				if (!bytesToRead || bytesActuallyRead != toRead || !result.second)
					break;

				data += toRead;
			}
		}
	}
	return result;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool BinaryFile::Write(const void* buffer, size_t bytesToWrite)
{
	if (m_FileHandle == INVALID_HANDLE_VALUE || !buffer)
		return false;

	DWORD bytesWritten;
	if constexpr (sizeof(bytesToWrite) == sizeof(DWORD))
	{
		const DWORD toWrite = static_cast<DWORD>(bytesToWrite);
		return ::WriteFile(m_FileHandle, buffer, toWrite, &bytesWritten, nullptr) != 0;
	} else
	{
		const size_t MAX_WRITE_SIZE = 0x80000000;
		for (auto data = static_cast<const uint8_t*>(buffer);;)
		{
			const size_t toWrite = (bytesToWrite < MAX_WRITE_SIZE) ? bytesToWrite : MAX_WRITE_SIZE;
			if (!::WriteFile(m_FileHandle, data, static_cast<DWORD>(toWrite), &bytesWritten, nullptr))
				return false;

			bytesToWrite -= toWrite;
			if (!bytesToWrite)
				return true;

			data += toWrite;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
bool BinaryFile::Flush()
{
	if (m_FileHandle == INVALID_HANDLE_VALUE)
		return false;

	return ::FlushFileBuffers(m_FileHandle) != 0;
}

//--------------------------------------------------------------------------------------------------------------------------------
long long BinaryFile::GetSize() const
{
	if (m_FileHandle == INVALID_HANDLE_VALUE)
		return -1;

	ULARGE_INTEGER fileSize;
	fileSize.LowPart = ::GetFileSize(m_FileHandle, &fileSize.HighPart);

	if (fileSize.LowPart == INVALID_FILE_SIZE)
	{
		if (::GetLastError() != NO_ERROR)
			return -1;
	}
	return fileSize.QuadPart;
}

//--------------------------------------------------------------------------------------------------------------------------------
long long BinaryFile::GetPosition() const
{
	if (m_FileHandle == INVALID_HANDLE_VALUE)
		return -1;

	LARGE_INTEGER filePosition;
	filePosition.HighPart = 0;
	filePosition.LowPart = ::SetFilePointer(m_FileHandle, 0, &filePosition.HighPart, FILE_CURRENT);

	if (filePosition.LowPart == INVALID_SET_FILE_POINTER)
	{
		if (::GetLastError() != NO_ERROR)
			return -1;
	}
	return filePosition.QuadPart;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool BinaryFile::SetPosition(long long position)
{
	if (m_FileHandle == INVALID_HANDLE_VALUE || position < 0)
		return false;

	LARGE_INTEGER filePosition;
	filePosition.QuadPart = position;
	filePosition.LowPart = ::SetFilePointer(m_FileHandle, filePosition.LowPart, &filePosition.HighPart, FILE_BEGIN);

	if (filePosition.LowPart == INVALID_SET_FILE_POINTER)
	{
		if (::GetLastError() != NO_ERROR)
			return false;
	}
	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool BinaryFile::Truncate()
{
	if (m_FileHandle == INVALID_HANDLE_VALUE)
		return false;

	return ::SetEndOfFile(m_FileHandle) != 0;
}

#else
	#error Not implemented
#endif // AML_OS_WINDOWS

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   MemoryFile
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
MemoryFile::~MemoryFile()
{
	Close();
}

//--------------------------------------------------------------------------------------------------------------------------------
bool MemoryFile::Open(unsigned flags)
{
	if (m_OpenFlags)
		return false;

	m_Block = m_First = new Block;
	m_First->header.prev = nullptr;
	m_First->header.next = nullptr;

	m_BlockPos = 0;
	m_Size = m_Position = 0;

	m_OpenFlags = (flags | FILE_OPEN_MEMORY) & FILE_OPENFLAG_MASK;

	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
void MemoryFile::Close()
{
	for (Block* block = m_First; block;)
	{
		Block* p = block;
		block = block->header.next;
		delete p;
	}
	m_Block = m_First = nullptr;
	m_OpenFlags = 0;
}

//--------------------------------------------------------------------------------------------------------------------------------
std::pair<size_t, bool> MemoryFile::Read(void* buffer, size_t bytesToRead)
{
	std::pair<size_t, bool> result(0, false);

	if (m_OpenFlags && buffer)
	{
		if (size_t bytesLeft = (m_Position < m_Size) ? m_Size - m_Position : 0; bytesToRead > bytesLeft)
			bytesToRead = bytesLeft;

		if (bytesToRead > BLOCK_SIZE - m_BlockPos)
			DoRead(buffer, bytesToRead);
		else
		{
			auto data = m_Block->data + m_BlockPos;
			memcpy(buffer, data, bytesToRead);

			m_BlockPos += bytesToRead;
			m_Position += bytesToRead;
		}

		result.first = bytesToRead;
		result.second = true;
	}

	return result;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool MemoryFile::Write(const void* buffer, size_t bytesToWrite)
{
	if (!m_OpenFlags || !buffer)
		return false;

	if (bytesToWrite > BLOCK_SIZE - m_BlockPos)
		DoWrite(buffer, bytesToWrite);
	else
	{
		auto out = m_Block->data + m_BlockPos;
		memcpy(out, buffer, bytesToWrite);

		m_BlockPos += bytesToWrite;
		m_Position += bytesToWrite;
		if (m_Position > m_Size)
			m_Size = m_Position;
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool MemoryFile::SetPosition(long long position)
{
	if (!m_OpenFlags || position < 0 || position > ~size_t(0))
		return false;

	const size_t newPos = static_cast<size_t>(position);
	if (newPos <= (m_Size | ((m_Size & (BLOCK_SIZE - 1)) ? BLOCK_SIZE - 1 : 0)))
	{
		size_t p = m_Position - m_BlockPos;
		if (!m_Block || newPos <= p / 2)
		{
			m_Block = m_First;
			p = 0;
		}
		if (newPos > p)
		{
			p += BLOCK_SIZE - 1;
			for (; newPos - 1 > p; p += BLOCK_SIZE)
				m_Block = m_Block->header.next;
			p -= BLOCK_SIZE - 1;
		} else
		{
			for (; newPos < p; p -= BLOCK_SIZE)
				m_Block = m_Block->header.prev;
		}
		m_BlockPos = newPos - p;
	} else
	{
		m_Block = nullptr;
		m_BlockPos = BLOCK_SIZE;
	}

	m_Position = newPos;
	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool MemoryFile::Truncate()
{
	if (!m_OpenFlags)
		return false;

	if (!m_Block)
		ApplyPosition();

	for (Block* block = m_Block->header.next; block;)
	{
		Block* p = block;
		block = block->header.next;
		delete p;
	}
	m_Block->header.next = nullptr;
	m_Size = m_Position;
	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool MemoryFile::LoadFrom(WZStringView path, bool clear)
{
	BinaryFile src;
	bool result = false;
	if (src.Open(path, FILE_OPEN_READ))
	{
		result = LoadFrom(src, clear);
		src.Close();
	}
	return result;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool MemoryFile::LoadFrom(File& file, bool clear)
{
	if (!file.IsOpened() || !(file.GetOpenFlags() & FILE_OPEN_READ))
		return false;

	if (!m_OpenFlags)
		Open();

	const size_t originalPos = m_Position;
	bool result = file.SaveTo(*this, clear);
	SetPosition(clear ? 0 : originalPos);
	return result;
}

//--------------------------------------------------------------------------------------------------------------------------------
MemoryFile& MemoryFile::operator =(MemoryFile&& that)
{
	if (this != &that)
	{
		Close();

		m_First = that.m_First;
		m_Block = that.m_Block;
		m_BlockPos = that.m_BlockPos;

		m_Size = that.m_Size;
		m_Position = that.m_Position;
		m_OpenFlags = that.m_OpenFlags;

		that.m_Block = that.m_First = nullptr;
		that.m_OpenFlags = 0;
	}

	return *this;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool MemoryFile::SaveToCustom(File& file)
{
	size_t bytesLeft = m_Size;
	for (Block* block = m_First; bytesLeft; block = block->header.next)
	{
		const size_t toCopy = (bytesLeft < BLOCK_SIZE) ? bytesLeft : BLOCK_SIZE;
		if (!file.Write(block->data, toCopy))
			return false;

		bytesLeft -= toCopy;
	}
	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool MemoryFile::GetCRC32Custom(uint32_t& crc, long long size)
{
	if (size <= 0)
		return true;
	if (m_Position >= m_Size || static_cast<unsigned long long>(size) > m_Size - m_Position)
		return false;

	if (m_BlockPos == BLOCK_SIZE)
	{
		m_Block = m_Block->header.next;
		m_BlockPos = 0;
	}

	for (size_t bytesToRead = static_cast<size_t>(size);;)
	{
		const size_t bytesLeft = BLOCK_SIZE - m_BlockPos;
		const size_t toRead = (bytesToRead <= bytesLeft) ? bytesToRead : bytesLeft;
		crc = hash::GetCRC32(m_Block->data + m_BlockPos, toRead, crc);

		m_BlockPos += toRead;
		m_Position += toRead;

		bytesToRead -= toRead;
		if (!bytesToRead)
			return true;

		m_Block = m_Block->header.next;
		m_BlockPos = 0;
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
inline void MemoryFile::Grow()
{
	if (m_Block->header.next)
		m_Block = m_Block->header.next;
	else
	{
		Block* newBlock = new Block;
		newBlock->header.prev = m_Block;
		newBlock->header.next = nullptr;
		m_Block->header.next = newBlock;
		m_Block = newBlock;
	}
	m_BlockPos = 0;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void MemoryFile::ApplyPosition()
{
	const size_t newPosition = m_Position;

	m_Block = m_First;
	m_BlockPos = BLOCK_SIZE;
	m_Position = BLOCK_SIZE;

	while (newPosition > m_Position)
	{
		Grow();
		// Изменим значения m_BlockPos, позиции и размера файла, чтобы наш объект файла был консистентным
		// в случае генерации исключения при нехватке памяти при вызове Grow() в следующей итерации цикла
		m_BlockPos = BLOCK_SIZE;
		m_Position += BLOCK_SIZE;
		if (m_Position > m_Size)
			m_Size = m_Position;
	}
	m_BlockPos = newPosition - (m_Position - BLOCK_SIZE);
	// Установим размер файла равным новой текущей позиции. Далее мы либо допишем что-то в наш файл и
	// обновим тем самым размер, либо (если это вызов из функции Truncate) это и будет новый размер
	m_Size = m_Position = newPosition;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void MemoryFile::DoRead(void* buffer, size_t bytesToRead)
{
	if (m_BlockPos == BLOCK_SIZE)
	{
		m_Block = m_Block->header.next;
		m_BlockPos = 0;
	}

	for (auto out = static_cast<uint8_t*>(buffer);;)
	{
		const size_t bytesLeft = BLOCK_SIZE - m_BlockPos;
		const size_t toRead = (bytesToRead <= bytesLeft) ? bytesToRead : bytesLeft;
		memcpy(out, m_Block->data + m_BlockPos, toRead);

		m_BlockPos += toRead;
		m_Position += toRead;

		bytesToRead -= toRead;
		if (!bytesToRead)
			return;

		out += toRead;
		m_Block = m_Block->header.next;
		m_BlockPos = 0;
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void MemoryFile::DoWrite(const void* buffer, size_t bytesToWrite)
{
	if (!m_Block)
		ApplyPosition();
	if (m_BlockPos == BLOCK_SIZE)
		Grow();

	for (auto data = static_cast<const uint8_t*>(buffer);;)
	{
		const size_t bytesLeft = BLOCK_SIZE - m_BlockPos;
		const size_t toWrite = (bytesToWrite <= bytesLeft) ? bytesToWrite : bytesLeft;
		memcpy(m_Block->data + m_BlockPos, data, toWrite);

		m_BlockPos += toWrite;
		m_Position += toWrite;
		if (m_Position > m_Size)
			m_Size = m_Position;

		bytesToWrite -= toWrite;
		if (!bytesToWrite)
			return;

		data += toWrite;
		Grow();
	}
}
