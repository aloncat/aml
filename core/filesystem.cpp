//∙AML
// Copyright (C) 2016-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "filesystem.h"

#include "array.h"
#include "winapi.h"

using namespace util;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   FileSystem::Helper - вспомогательные функции (Windows)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if AML_OS_WINDOWS

//--------------------------------------------------------------------------------------------------------------------------------
struct FileSystem::Helper final
{
	static bool IsUNCPath(std::wstring_view path);
	static std::wstring GetFullUNCPath(const wchar_t* path);
	static void MakeFullPath(std::wstring& fullPath, WZStringView path);

private:
	static size_t CompactPath(const wchar_t* src, wchar_t* out, size_t outPos);
};

//--------------------------------------------------------------------------------------------------------------------------------
bool FileSystem::Helper::IsUNCPath(std::wstring_view path)
{
	if (path.size() < 2)
		return false;

	auto p = path.data();
	// Несмотря на то, что правильный UNC путь должен начинаться с 2 обратных слэшей, ОС Windows корректно
	// работает с путями вида "//?/C:\file", в которых вместо некоторых обратных слэшей используются прямые
	return (p[0] == '\\' || p[0] == '/') && (p[1] == '\\' || p[1] == '/');
}

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring FileSystem::Helper::GetFullUNCPath(const wchar_t* path)
{
	const DWORD LOCAL_SIZE = 640;
	wchar_t localBuffer[LOCAL_SIZE];
	FlexibleArray buffer(localBuffer);

	DWORD len = ::GetFullPathNameW(path, LOCAL_SIZE, buffer, nullptr);
	if (len > LOCAL_SIZE)
	{
		buffer.Grow(len);
		DWORD count = ::GetFullPathNameW(path, len, buffer, nullptr);
		len = (count && count < len) ? count : 0;
	}

	return std::wstring(buffer, len);
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void FileSystem::Helper::MakeFullPath(std::wstring& fullPath, WZStringView path)
{
	size_t i = 0, op = 0, drive = 0;
	const wchar_t* src = path.c_str();
	for (size_t j = 0; src[j] && src[j] != '\\' && src[j] != '/'; ++j)
	{
		if (src[j] == ':')
		{
			drive = i = j + 1;
			while (src[i] == ':') ++i;
			break;
		}
	}
	bool root = src[i] == '\\' || src[i] == '/';
	if (root) do ++i; while (src[i] == '\\' || src[i] == '/');

	const size_t LOCAL_SIZE = 640;
	wchar_t localBuffer[LOCAL_SIZE];
	wchar_t* out = localBuffer;

	if (drive && (root || drive != 2))
	{
		const size_t maxLen = drive + 1 + path.size();
		if (maxLen > LOCAL_SIZE)
			out = new wchar_t[maxLen];
		for (size_t j = 0; j < drive; ++j)
			out[j] = src[j];
		if (root)
			out[drive++] = '\\';
		op = drive;
	} else
	{
		const wchar_t driveBuf[3] = { src[0], ':' };
		const wchar_t* basePath = (drive > 0) ? driveBuf : root ? L"\\" : L".";
		op = ::GetFullPathNameW(basePath, LOCAL_SIZE, out, nullptr);
		if (auto maxLen = op + 1 + path.size(); maxLen > LOCAL_SIZE)
		{
			out = new wchar_t[maxLen];
			if (op && op < LOCAL_SIZE)
				memcpy(out, localBuffer, op * sizeof(wchar_t));
			else
				op = ::GetFullPathNameW(basePath, static_cast<DWORD>(maxLen), out, nullptr);
		}
		drive = (op >= 3 && out[1] == ':') ? 3 : (op >= 2) ? 2 : op;
	}

	op = CompactPath(&src[i], &out[drive], op - drive);
	fullPath.append(out, drive + op);

	if (out != localBuffer)
		delete[] out;
}

//--------------------------------------------------------------------------------------------------------------------------------
size_t FileSystem::Helper::CompactPath(const wchar_t* src, wchar_t* out, size_t outPos)
{
	size_t op = outPos;
	for (size_t i = 0, j = 0; src[i]; i = j)
	{
		bool slashed = false;
		while (!slashed && src[++j])
			slashed = src[j] == '\\' || src[j] == '/';

		if (i + 2 == j && src[i] == '.' && src[i + 1] == '.')
		{
			if (op && out[op - 1] == '\\')
				--op;
			while (op && out[op - 1] != '\\')
				--op;
		}
		else if (i + 1 != j || src[i] != '.')
		{
			if (op && out[op - 1] != '\\')
				out[op++] = '\\';
			for (size_t k = i; k < j; ++k)
				out[op++] = src[k];
		}

		if (slashed)
		{
			if (op && out[op - 1] != '\\')
				out[op++] = '\\';
			do ++j; while (src[j] == '\\' || src[j] == '/');
		}
		else if (op && out[op - 1] == '\\')
			--op;
	}
	return op;
}

#endif // AML_OS_WINDOWS

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   FileSystem (Windows)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if AML_OS_WINDOWS

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring FileSystem::GetFullPath(WZStringView path)
{
	if (path.empty() || path.size() > 32767)
		return std::wstring();

	if (Helper::IsUNCPath(path))
		return Helper::GetFullUNCPath(path.c_str());

	std::wstring fullPath;
	Helper::MakeFullPath(fullPath, path);
	return fullPath;
}

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring FileSystem::CombinePath(std::wstring_view parent, std::wstring_view path)
{
	std::wstring result;
	if (parent.empty())
		result.assign(path);
	else
	{
		result.reserve(parent.size() + path.size() + 1);
		result.assign(parent);

		if (!path.empty())
		{
			// Дополняем путь parent слэшем
			const wchar_t last = parent.back();
			if (last != '\\' && last != '/' && last != ':')
				result.push_back('\\');

			// Удаляем из пути path все слэши в начале
			if (auto pos = path.find_first_not_of(L"\\/"); pos != path.npos)
			{
				path.remove_prefix(pos);
				result.append(path);
			}
		}
	}

	return result;
}

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring FileSystem::ExtractPath(std::wstring_view path)
{
	size_t i = path.find_last_of(L"\\/:");
	i = (i != path.npos) ? i + 1 : 0;

	auto p = path.data();
	if (Helper::IsUNCPath(path))
	{
		while (i > 2 && (p[i - 1] == '\\' || p[i - 1] == '/'))
			--i;
	} else
	{
		while (i > 1 && (p[i - 1] == '\\' || p[i - 1] == '/') && p[i - 2] != ':')
			--i;
	}

	return std::wstring(p, i);
}

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring FileSystem::ExtractFullName(std::wstring_view path)
{
	if (auto pos = path.find_last_of(L"\\/:"); pos != path.npos)
		path.remove_prefix(pos + 1);

	return std::wstring(path);
}

//--------------------------------------------------------------------------------------------------------------------------------
bool FileSystem::FileExists(WZStringView path)
{
	int attr = GetAttributes(path);
	return attr >= 0 && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

//--------------------------------------------------------------------------------------------------------------------------------
bool FileSystem::DirectoryExists(WZStringView path)
{
	int attr = GetAttributes(path);
	return attr > 0 && (attr & FILE_ATTRIBUTE_DIRECTORY);
}

//--------------------------------------------------------------------------------------------------------------------------------
bool FileSystem::MakeDirectory(WZStringView path, bool createAll)
{
	if (path.empty())
		return false;

	std::wstring tmpPath;
	auto longPath = MakeLongPath(path, tmpPath);

	if (::CreateDirectoryW(longPath.first, nullptr))
		return true;

	DWORD error = ::GetLastError();
	if (error == ERROR_ALREADY_EXISTS)
		return true;

	if (createAll && error == ERROR_PATH_NOT_FOUND && longPath.second)
	{
		const auto p = longPath.first;
		size_t pos = longPath.second - 1;
		// Убираем концевые слэши, если они есть
		while (pos && (p[pos] == '\\' || p[pos] == '/'))
			--pos;
		// Ищем предыдущий слэш в пути. Если он не будет найден или вместо
		// слэша мы обнаружим двоеточие, то такой путь будет некорректным
		while (pos && p[pos] != '\\' && p[pos] != '/')
		{
			if (p[pos] == ':')
				return false;
			--pos;
		}
		// Пропускаем лишние (идущие подряд) слэши
		while (pos && (p[pos - 1] == '\\' || p[pos - 1] == '/'))
			--pos;
		// Если в оставшемся пути есть имя директории, пытаемся создать её
		if (pos && p[pos - 1] != '?' && p[pos - 1] != ':')
		{
			ZExStringView<wchar_t, 80> prePath(p, pos);
			return MakeDirectory(prePath, true) && ::CreateDirectoryW(longPath.first, nullptr);
		}
	}

	return false;
}

//----------------------------------------------------------------------------------------------------------------------
bool FileSystem::RemoveFile(WZStringView path)
{
	if (path.empty())
		return false;

	std::wstring tmpPath;
	auto longPath = MakeLongPath(path, tmpPath);

	return ::DeleteFileW(longPath.first) != 0;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool FileSystem::Rename(WZStringView path, WZStringView newName)
{
	if (path.empty() || newName.empty())
		return false;

	std::wstring tmpPath1, tmpPath2;
	auto oldPath = MakeLongPath(path, tmpPath1);
	auto newPath = MakeLongPath(newName, tmpPath2);

	return ::MoveFileW(oldPath.first, newPath.first) != 0;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE int FileSystem::GetAttributes(WZStringView path)
{
	std::wstring tmpPath;
	auto longPath = MakeLongPath(path, tmpPath);

	DWORD attr = ::GetFileAttributesW(longPath.first);
	if (attr != INVALID_FILE_ATTRIBUTES)
		return static_cast<int>(attr);

	DWORD error = ::GetLastError();
	if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND || error == ERROR_INVALID_NAME)
		return -1;

	WIN32_FIND_DATAW findData;
	HANDLE handle = ::FindFirstFileW(longPath.first, &findData);
	if (handle == INVALID_HANDLE_VALUE)
		return -1;

	::FindClose(handle);
	return static_cast<int>(findData.dwFileAttributes);
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE FileSystem::LongPath FileSystem::MakeLongPath(WZStringView path, std::wstring& tmp)
{
	const size_t len = path.size();
	if (len < MAX_PATH || len > 32767 || Helper::IsUNCPath(path))
		return { path.c_str(), len };

	tmp.assign(L"\\\\?\\");
	Helper::MakeFullPath(tmp, path);
	return { tmp.c_str(), tmp.size() };
}

#else
	#error Not implemented
#endif // AML_OS_WINDOWS
