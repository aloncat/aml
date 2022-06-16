//∙AML
// Copyright (C) 2016-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "platform.h"
#include "strcommon.h"

#include <string>
#include <string_view>
#include <utility>

namespace util {

//--------------------------------------------------------------------------------------------------------------------------------
struct FileSystem
{
	// Возвращает полный (абсолютный) путь к указанному файлу (директории),
	// исправляет слэши в пути на правильные, убирая лишние повторения
	static std::wstring GetFullPath(WZStringView path);
	// Возвращает путь path, из которого удалены все концевые слеши
	// кроме слеша в начале строки и слеша, следующего за двоеточием
	static std::wstring RemoveTrailingSlashes(std::wstring_view path);
	// Возвращает путь, результатом которого является объединение родительского пути parent с
	// путём path. Функция автоматически добавляет слэш между путями при необходимости. Путь
	// parent может быть абсолютным или относительным, path должен быть относительным путём
	static std::wstring CombinePath(std::wstring_view parent, std::wstring_view path);

	// Извлекает из указанного пути path путь к файлу (последней директории в указанном пути). Возвращаемый путь не
	// содержит слэш на конце, кроме случаев, когда path имеет вид "C:\path" (в этих случаях функция вернёт "C:\").
	// Если указанный путь содержит только имя файла (директории) без слэшей, то функция вернёт пустую строку
	static std::wstring ExtractPath(std::wstring_view path);
	// Извлекает из указанного пути path имя файла (директории) с расширением. Если
	// путь path оканчивается слэшем или двоеточием, то функция вернёт пустую строку
	static std::wstring ExtractFullName(std::wstring_view path);
	// Извлекает из указанного пути path расширение файла (директории). Если путь
	// path оканчивается слэшем или двоеточием, то функция вернёт пустую строку
	static std::wstring ExtractExtension(std::wstring_view path);

	// Возвращает путь path, в котором для имени файла (последней директории) расширение заменено на newExtension.
	// Если newExtension это пустая строка, то функция удаляет расширение файла (директории), оставляя только имя.
	// Значение path может быть относительным или абсолютным путем и не должно оканчиваться слешем или двоеточием
	static std::wstring ChangeExtension(std::wstring_view path, std::wstring_view newExtension);

	// Проверяет существование указанного файла или
	// директории. Параметр path не поддерживает маску
	static bool FileExists(WZStringView path);
	static bool DirectoryExists(WZStringView path);

	// Создаёт указанную директорию path. Если path является составным путём (т.е. состоит из нескольких
	// директорий, разделённых слэшами), то при createAll равном false функция попытается создать только
	// последнюю директорию пути (полагая, что остальные директории уже существуют). Если createAll
	// будет равен true, то функция попытается создать все несуществующие директории составного
	// пути. Функция вернёт true, если директория была успешно создана или уже существует
	static bool MakeDirectory(WZStringView path, bool createAll = false);

	// Удаляет указанный файл path
	static bool RemoveFile(WZStringView path);

	// Переименовывает (перемещает) файл (или директорию со всеми файлами и поддиректориями).
	// Параметр path указывает на исходный файл или директорию, newName задает новое имя или
	// новый путь (при перемещении). Перемещение работает только в пределах одного тома
	static bool Rename(WZStringView path, WZStringView newName);

protected:
	struct Helper;

	// Возвращает атрибуты указанного файла или директории. Если файл или директория
	// не существует или произошла ошибка, то функция вернёт отрицательное значение
	static int GetAttributes(WZStringView path);

	#if AML_OS_WINDOWS
	using LongPath = std::pair<const wchar_t*, size_t>;
	// Если размер строки path менее MAX_PATH символов, то функция возвращает path.c_str(). Если длина пути
	// path равна или превышает указанное значение и путь не является UNC путём, то строке tmp присваивается
	// значение GetFullPath(path), дополненное префиксом "\\?\", и функция возвращает tmp.c_str()
	static LongPath MakeLongPath(WZStringView path, std::wstring& tmp);
	#endif

private:
	FileSystem() = delete;
};

} // namespace util
