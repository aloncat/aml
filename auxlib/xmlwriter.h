//∙Auxlib
// Copyright (C) 2019-2021 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include <core/array.h>
#include <core/forward.h>
#include <core/strformat.h>
#include <core/util.h>

#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace aux {

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   XmlWriter - SAX (Simple API for XML) сериализатор XML
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class XmlWriter
{
	AML_NONCOPYABLE(XmlWriter)

public:
	XmlWriter();
	~XmlWriter();

	// Начинает новый элемент с именем name
	bool StartTag(std::wstring_view name);
	// Завершает текущий элемент
	bool EndTag();

	// Добавляет атрибут к текущему элементу
	bool Attribute(std::wstring_view name, std::wstring_view value);
	// Добавляет атрибут (значение произвольного типа) к текущему элементу
	template<class T, class = std::enable_if_t<!std::is_convertible_v<const T&, std::wstring_view>>>
	bool Attribute(std::wstring_view name, const T& value)
	{
		if (StartAttribute(name))
		{
			m_Buffer << value;
			return EndAttribute();
		}
		return false;
	}

	// Добавляет данные к текущему элементу. Если параметр newLine равен
	// true, то перед добавлением данных будет начата новая строка
	bool Data(std::wstring_view text, bool newLine = false);

	// Сбрасывает состояние, не очищая связанного файла. Если writeDeclaration
	// равен true, то при открытии первого тега в файл будет записан пролог
	void Reset(bool writeDeclaration);

	// Возвращает ссылку на связанный файл
	util::File& GetOutput() { return *m_Output; }
	// Заменяет текущий связанный файл указанным
	void SetOutput(util::File& output);

protected:
	struct TagInfo {
		std::string name;
		bool hasChildren = false;

		TagInfo(std::string&& _name) : name(std::move(_name)) {}
	};

	using Array = util::FlexibleArray<char>;
	using Buffer = util::Formatter<char, 8>;

	bool WriteDeclaration();
	void WritePadding(bool newLine, int offset = 0);
	bool WriteFile(std::string_view text);

	bool StartAttribute(std::wstring_view name);
	bool EndAttribute();

	// Конвертирует строку str в UTF-8, используя массив m_Array, и возвращает вью на него. Если параметр
	// sanitize == true, то выполняется экранирование (символы ', ", <, >, & и др. заменяется на "&...;"
	std::string_view ToUtf8(std::wstring_view str, bool sanitize);

protected:
	util::File* m_Output = nullptr;		// Связанный файл
	std::vector<TagInfo> m_NestedTags;	// Иерархия элементов

	Array m_Array;						// Буфер для конвертации в UTF-8
	Buffer m_Buffer;					// Буфер вывода (для формирования строк)
	char m_Paddings[32];				// Перевод строки (\n) + символы табуляции (0x09)

	bool m_IsOutputOwned = false;		// true, если мы владеем файлом m_Output
	bool m_NeedDeclaration = false;		// true, если нужно записать в файл пролог
	bool m_IsTagOpened = false;			// true, если тег текущего элемента сейчас открыт
	bool m_NeedEndTag = false;			// true, если текущему элементу требуется закрывающий тег
	bool m_HasData = false;				// true, если текущий элемент оканчивается данными
};

} // namespace aux
