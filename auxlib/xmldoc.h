//∙Auxlib
// Copyright (C) 2021-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "xmlreader.h"

#include <core/forward.h>
#include <core/platform.h>
#include <core/strcommon.h>
#include <core/util.h>

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace aux {

class XmlNode;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   NumDecoder - декодирование чисел из строк
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
struct NumDecoder
{
	// Декодирует указанную строку в целое число и сохраняет его в value. Функция вернёт
	// true, если строка содержит корректное число и оно умещается в переменной типа int
	static bool Decode(const wchar_t* from, const wchar_t* to, int& value);

protected:
	// Возвращает true, если число во from больше числа в num. Строка num
	// должна быть null-терминированной, а длины строк from и num - равны
	static bool IsGreater(const wchar_t* from, const char* num);
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   XmlArrayView - вспомогательный вью на массив для узлов и атрибутов XmlNode
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
template<class T>
class XmlArrayView
{
public:
	XmlArrayView() = default;
	constexpr XmlArrayView(const T* first, size_t count) noexcept
		: m_Items(first)
		, m_Count(count)
	{
	}

	constexpr size_t size() const noexcept
	{
		return m_Count;
	}

	constexpr const T& operator[](size_t i) const noexcept
	{
		return m_Items[i];
	}

protected:
	const T* m_Items = nullptr;
	size_t m_Count = 0;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   XmlObjectPool - пул объектов для документа XML
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class XmlObjectPool
{
	AML_NONCOPYABLE(XmlObjectPool)

public:
	XmlObjectPool() = default;
	~XmlObjectPool();

	// Вызывает деструкторы всех созданных объектов
	// XmlNode и освобождает всю выделенную память
	void Release();

	// Выделяет в пуле пространство для указанной строки,
	// копирует её в пул и возвращает вью на неё в пуле
	XmlStringView MakeString(std::wstring_view str);

	// Выделяет в пуле память под объект XmlNode, вызывает его конструктор
	// и возвращает указатель на объект (владельцем объекта является пул)
	XmlNode* MakeNode(XmlNode* parent = nullptr);

	// Выделяет в пуле память под массив из count элементов того же типа, что и тип
	// значения итератора first, затем копирует в массив count элементов, начиная с
	// first. Функция возвращает объект XmlArrayView на выделенный массив
	template<class Iter> auto MakeArray(Iter first, size_t count);

protected:
	// Стандартный размер массива Block::data (в байтах)
	static constexpr size_t BLOCK_DATA_SIZE = 32 * 1024;

	enum class BlockType {
		Bytes,	// Универсальный массив (uint8_t)
		Nodes	// Массив узлов XML (XmlNode)
	};

	struct Block {
		Block* next;			// Указатель на следующий блок в списке
		size_t nodeCount;		// Количество созданных объектов XmlNode в data
		uint8_t data[1024];		// Данные (реальный размер массива будет другим)
	};

	// Возвращает размер в байтах массива из itemCount элементов типа T. Если результат
	// выходит за пределы диапазона типа size_t, то функция вернёт значение size_t(-1)
	template<class T> static constexpr size_t GetSize(size_t itemCount);

	// Выделяет в куче новый блок, добавляет его в список m_Blocks и возвращает указатель на
	// блок. Параметр itemType задаёт тип, а itemCount - количество элементов в массиве data
	Block* Allocate(BlockType itemType, size_t itemCount);
	// Выделяет в текущем общем блоке массив размера sizeInBytes байт и возвращает указатель на начало
	// выделенной области. Если в текущем блоке недостаточно места, то будет выделен новый общий блок
	void* Allocate(size_t sizeInBytes);

protected:
	Block* m_Blocks = nullptr;		// Односвязный список блоков пула
	Block* m_NodeBlock = nullptr;	// Текущий блок для выделения памяти под узлы
	uint8_t* m_Data = nullptr;		// Указатель на начало свободной области текущего общего блока
	size_t m_Remains = 0;			// Размер свободной области текущего общего блока в байтах
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   XmlNode - узел документа XML (элемент и его содержимое)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class XmlNode
{
	friend class XmlDocument;
	AML_NONCOPYABLE(XmlNode)

public:
	explicit XmlNode(XmlNode* parent);

	std::wstring_view Name() const { return m_Name; }
	std::wstring_view Data() const { return m_Data; }

	// Возвращает указатель на родительский узел
	// (функция вернёт nullptr для корневого узла)
	const XmlNode* Parent() const { return m_Parent; }

	// Возвращает количество дочерних узлов (элементов)
	size_t NodeCount() const { return m_Nodes.size(); }

	// Возвращает дочерний узел по его индексу. Если индекс некорректный, функция вернёт nullptr
	const XmlNode* Node(size_t index) const { return (index < m_Nodes.size()) ? m_Nodes[index] : nullptr; }
	// Возвращает дочерний узел c именем name. Если узлов с этим именем несколько, то будет
	// выбран самый первый. Если узла с таким именем нет, то функция вернёт nullptr. Имя
	// может содержать вложенные узлы, разделённые точкой, например: "Level.Field.Cells"
	const XmlNode* Node(std::wstring_view name) const;

	// Возвращает количество атрибутов элемента
	size_t AttrCount() const { return m_Attributes.size(); }
	// Возвращает true, если у элемента есть атрибут с именем name
	bool HasAttr(std::wstring_view name) const { return FindAttr(name) != NPOS; }

	// Возвращает строковое значение атрибута с именем name. Если
	// атрибута с этим именем нет, то функция вернёт пустую строку
	std::wstring_view Attr(std::wstring_view name) const;
	// Возвращает значение атрибута name типа int. Если атрибута с этим именем нет или его
	// значение не может быть корректно преобразовано в int, функция вернёт значение def
	int Attr(std::wstring_view name, int def) const;
	// Возвращает значение атрибута name как bool. Строковое значение атрибута "true" (без учёта
	// регистра) соответствует значению true, а "false" - false. Если атрибута с этим именем нет
	// или его строковое значение не равно "true" или "false", то функция вернёт значение def
	bool Attr(std::wstring_view name, bool def) const;

	// Вызывает функцию cb для каждого атрибута элемента, передавая ей его имя и строковое значение
	void ForEachAttr(const std::function<void(std::wstring_view, std::wstring_view)>& cb) const;

protected:
	static constexpr size_t NPOS = size_t(-1);

	struct Attribute
	{
		XmlStringView name;		// Имя атрибута
		XmlStringView value;	// Значение атрибута

		Attribute(XmlStringView n, XmlStringView v)
			: name(n), value(v) {}
	};

	// Находит индекс в массиве m_Attributes для атрибута с именем name.
	// Если атрибута с этим именем нет, то функция вернёт значение NPOS
	size_t FindAttr(std::wstring_view name) const;
	// Находит индекс в массиве m_Nodes для узла с именем name. Если
	// узла с этим именем нет, то функция вернёт значение NPOS
	size_t FindNode(std::wstring_view name) const;

protected:
	XmlNode* m_Parent = nullptr;

	XmlStringView m_Name;
	XmlStringView m_Data;
	XmlArrayView<XmlNode*> m_Nodes;
	XmlArrayView<Attribute> m_Attributes;
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   XmlDocument - документ XML (DOM - Document Object Model)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class XmlDocument
{
	AML_NONCOPYABLE(XmlDocument)

public:
	XmlDocument();

	void Clear();

	// Загружает документ из указанного файла
	bool Load(util::WZStringView filePath);
	// Загружает документ из файла file
	bool Load(util::File& file);

	// Возвращает указатель на корневой узел
	const XmlNode* GetRoot() const { return m_Root; }
	// Возвращает узел c именем name. Если узлов с этим именем несколько, то будет выбран
	// самый первый. Если узла с таким именем нет, то функция вернёт nullptr. Имя может
	// содержать вложенные узлы, разделённые точкой, например: "Level.Field.Cells"
	const XmlNode* GetNode(std::wstring_view name) const { return m_Root->Node(name); }

	// Возвращает сообщение об ошибке (актуально после вызова
	// функции Load, если она завершилась ошибкой и вернула false)
	const std::wstring& GetLastError() const { return m_LastError; }

protected:
	using Nodes = std::vector<XmlNode*>;
	using Attributes = std::vector<XmlNode::Attribute>;

	struct Info {
		XmlReader* parser = nullptr;	// Указатель на парсер
		XmlDocument* self = nullptr;	// Указатель на наш документ

		XmlNode* node = nullptr;		// Указатель на текущий узел
		Attributes attributes;			// Атрибуты текущего узла
		std::wstring data;				// Данные текущего узла

		Nodes bufferedNodes;			// Буфер (стек) загруженных узлов
		std::vector<size_t> nodeStack;	// Стек индексов узлов для перемещения по иерархии

		bool hasStarted = false;		// true, если было открытие тега или данные
	};

	template<class T> bool LoadFrom(T& source);
	void SetCallbacks(Info& info);

	static void OnTagOpened(void* infoPtr, std::wstring_view name);
	static void OnTagClosed(void* infoPtr, std::wstring_view name);
	static void OnAttr(void* infoPtr, std::wstring_view name, std::wstring_view value);
	static void OnData(void* infoPtr, std::wstring_view text, bool);

	static void SetNodeData(Info& info);
	static bool SetNodeAttrAndData(Info& info);
	static void SetNodeChildren(Info& info);

	static void OnError(Info& info, std::wstring_view text);

protected:
	XmlObjectPool m_Pool;		// Пул объектов (строки, узлы, массивы узлов и атрибутов)
	XmlNode* m_Root = nullptr;	// Указатель на корневой узел
	std::wstring m_LastError;	// Сообщение об ошибке
};

} // namespace aux
