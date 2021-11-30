//∙AML
// Copyright (C) 2016-2021 Dmitry Maslov
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
