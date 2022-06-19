//∙Auxlib
// Copyright (C) 2021-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "xmldoc.h"

#include <core/debug.h>
#include <core/strutil.h>

using namespace aux;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   NumDecoder
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
bool NumDecoder::Decode(const wchar_t* from, const wchar_t* to, int& value)
{
	if (from >= to)
		return false;

	bool negative = false;
	if (*from == '-')
	{
		negative = true;
		if (++from == to)
			return false;
	}

	for (size_t remains = to - from; remains >= 10; --remains)
	{
		if (*from == '0')
			++from;
		else if (remains > 10 || IsGreater(from, negative ? "2147483648" : "2147483647"))
			return false;
		else
			break;
	}

	for (unsigned v, result = 0;;)
	{
		if (v = static_cast<unsigned>(*from) - '0'; v > 9)
			return false;

		result = 10 * result + v;

		if (++from == to)
		{
			value = negative ? 0 - result : result;
			return true;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
inline bool NumDecoder::IsGreater(const wchar_t* lhs, const char* rhs)
{
	for (; *rhs; ++lhs, ++rhs)
	{
		auto l = static_cast<unsigned>(*lhs);
		auto r = static_cast<unsigned char>(*rhs);

		if (l != r)
		{
			return l > r;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   XmlObjectPool
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
XmlObjectPool::~XmlObjectPool()
{
	Release();
}

//--------------------------------------------------------------------------------------------------------------------------------
void XmlObjectPool::Release()
{
	m_NodeBlock = nullptr;
	m_Remains = 0;

	for (Block* block = m_Blocks; block;)
	{
		auto nodes = reinterpret_cast<XmlNode*>(block->data);
		for (size_t i = 0; i < block->nodeCount; ++i)
			nodes[i].~XmlNode();

		auto p = reinterpret_cast<uint8_t*>(block);
		m_Blocks = block = block->next;
		delete[] p;
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
XmlStringView XmlObjectPool::MakeString(std::wstring_view str)
{
	const size_t size = GetSize<wchar_t>(str.size());
	auto out = static_cast<wchar_t*>(Allocate(size));
	memcpy(out, str.data(), size);

	return XmlStringView(out, str.size());
}

//--------------------------------------------------------------------------------------------------------------------------------
XmlNode* XmlObjectPool::MakeNode(XmlNode* parent)
{
	const size_t maxCount = BLOCK_DATA_SIZE / sizeof(XmlNode);
	if (!m_NodeBlock || m_NodeBlock->nodeCount >= maxCount)
		m_NodeBlock = Allocate(BlockType::Nodes, maxCount);

	XmlNode* nodes = reinterpret_cast<XmlNode*>(m_NodeBlock->data);
	return new(nodes + m_NodeBlock->nodeCount++) XmlNode(parent);
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class Iter>
auto XmlObjectPool::MakeArray(Iter first, size_t count)
{
	using T = typename Iter::value_type;
	static_assert(std::is_trivially_copyable_v<T>, "Only trivially copyable types are allowed");

	auto items = static_cast<T*>(Allocate(GetSize<T>(count)));

	auto it = first;
	for (size_t i = 0; i < count; ++i, ++it)
		items[i] = *it;

	return XmlArrayView<T>(items, count);
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class T>
constexpr size_t XmlObjectPool::GetSize(size_t itemCount)
{
	return (itemCount <= size_t(-1) / sizeof(T)) ?
		itemCount * sizeof(T) : size_t(-1);
}

//--------------------------------------------------------------------------------------------------------------------------------
XmlObjectPool::Block* XmlObjectPool::Allocate(BlockType itemType, size_t itemCount)
{
	constexpr size_t mask = sizeof(size_t) - 1;
	constexpr size_t headerSize = offsetof(Block, data);
	constexpr size_t maxSize = ~mask - headerSize;

	switch (itemType)
	{
		case BlockType::Bytes:
			if (itemCount > maxSize)
				throw std::bad_alloc();
			itemCount = (itemCount + mask) & ~mask;
			break;
		case BlockType::Nodes:
			if (itemCount > maxSize / sizeof(XmlNode))
				throw std::bad_alloc();
			itemCount *= sizeof(XmlNode);
			break;
	};
	auto p = new uint8_t[headerSize + itemCount];
	auto block = reinterpret_cast<Block*>(p);

	block->next = m_Blocks;
	block->nodeCount = 0;
	m_Blocks = block;

	return block;
}

//--------------------------------------------------------------------------------------------------------------------------------
void* XmlObjectPool::Allocate(size_t sizeInBytes)
{
	void* out = nullptr;
	if (sizeInBytes)
	{
		constexpr size_t mask = sizeof(size_t) - 1;
		constexpr size_t headerSize = offsetof(Block, data);
		static_assert(!(BLOCK_DATA_SIZE & mask), "BLOCK_DATA_SIZE must be a multiple of 8");
		static_assert(!(headerSize & mask), "Block::data must be aligned at 8 bytes");

		const size_t alignedSize = (sizeInBytes + mask) & ~mask;

		if (sizeInBytes <= m_Remains)
		{
			out = m_Data;
			m_Data += alignedSize;
			m_Remains -= alignedSize;
		}
		else if (sizeInBytes < BLOCK_DATA_SIZE / 16)
		{
			auto block = Allocate(BlockType::Bytes, BLOCK_DATA_SIZE);
			out = block->data;
			m_Data = block->data + alignedSize;
			m_Remains = BLOCK_DATA_SIZE - alignedSize;
		} else
		{
			auto block = Allocate(BlockType::Bytes, sizeInBytes);
			out = block->data;
		}
	}

	return out;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   XmlNode
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
XmlNode::XmlNode(XmlNode* parent)
	: m_Parent(parent)
{
}

//--------------------------------------------------------------------------------------------------------------------------------
const XmlNode* XmlNode::Node(std::wstring_view name) const
{
	auto p = name.data();
	const auto pEnd = p + name.size();
	while (p < pEnd && *p != '.') ++p;

	if (p == pEnd)
	{
		if (auto i = FindNode(name); i != NPOS)
			return m_Nodes[i];
	} else
	{
		std::wstring_view left(name.data(), p - name.data());
		if (auto i = FindNode(left); i != NPOS)
		{
			std::wstring_view right(p + 1, pEnd - p - 1);
			return m_Nodes[i]->Node(right);
		}
	}

	return nullptr;
}

//--------------------------------------------------------------------------------------------------------------------------------
std::wstring_view XmlNode::Attr(std::wstring_view name) const
{
	if (auto i = FindAttr(name); i != NPOS)
		return m_Attributes[i].value;

	return std::wstring_view();
}

//--------------------------------------------------------------------------------------------------------------------------------
int XmlNode::Attr(std::wstring_view name, int def) const
{
	if (auto i = FindAttr(name); i != NPOS)
	{
		int result;
		const XmlStringView& v = m_Attributes[i].value;
		if (NumDecoder::Decode(v.data, v.data + v.size, result))
			return result;
	}

	return def;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool XmlNode::Attr(std::wstring_view name, bool def) const
{
	if (auto i = FindAttr(name); i != NPOS)
	{
		const XmlStringView& v = m_Attributes[i].value;

		if (v.size == 4)
		{
			if (!util::StrNInsCmp(v.data, L"true", 4))
				return true;
		}
		else if (v.size == 5)
		{
			if (!util::StrNInsCmp(v.data, L"false", 5))
				return false;
		}
	}

	return def;
}

//--------------------------------------------------------------------------------------------------------------------------------
void XmlNode::ForEachAttr(const std::function<void(std::wstring_view, std::wstring_view)>& cb) const
{
	if (cb)
	{
		for (size_t i = 0, count = m_Attributes.size(); i < count; ++i)
		{
			const auto& attr = m_Attributes[i];
			cb(attr.name, attr.value);
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
size_t XmlNode::FindAttr(std::wstring_view name) const
{
	if (!name.empty())
	{
		// Сейчас используется линейный поиск со сравнением строк. Это не плохо. Среднее количество атрибутов среди
		// всех узлов с атрибутами обычно небольшое (в среднем не превышает 4). Поэтому бинарный поиск не даст эффекта.
		// Также, тест показал, что добавление хеша имени снижает и скорость загрузки, и скорость работы с атрибутами
		for (size_t i = 0, count = m_Attributes.size(); i < count; ++i)
		{
			if (m_Attributes[i].name == name)
				return i;
		}
	}

	return NPOS;
}

//--------------------------------------------------------------------------------------------------------------------------------
size_t XmlNode::FindNode(std::wstring_view name) const
{
	if (!name.empty())
	{
		// TODO: сейчас используется линейный поиск со сравнением строк. Возможно, стоит завести массив отсортированных
		// индексов (так как сам массив нод сортировать нельзя) и использовать бинарный поиск (и, возможно, стоит также
		// использовать хеш имени в качестве ключа, чтобы было меньше сравнений строк). Но обычно количество вложенных
		// узлов небольшое. А для тех узлов, которые имеют много вложенных, мы обычно используем доступ через индекс,
		// а не поиск по имени. Поэтому бинарный поиск с хешем имени пока не реализованы
		for (size_t i = 0, count = m_Nodes.size(); i < count; ++i)
		{
			if (m_Nodes[i]->m_Name == name)
				return i;
		}
	}

	return NPOS;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   XmlDocument
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
XmlDocument::XmlDocument()
{
	m_Root = m_Pool.MakeNode();
}

//--------------------------------------------------------------------------------------------------------------------------------
void XmlDocument::Clear()
{
	m_Root = nullptr;
	m_Pool.Release();
	m_LastError.clear();

	m_Root = m_Pool.MakeNode();
}

//--------------------------------------------------------------------------------------------------------------------------------
bool XmlDocument::Load(util::WZStringView filePath)
{
	return LoadFrom(filePath);
}

//--------------------------------------------------------------------------------------------------------------------------------
bool XmlDocument::Load(util::File& file)
{
	return LoadFrom(file);
}

//--------------------------------------------------------------------------------------------------------------------------------
template<class T> bool XmlDocument::LoadFrom(T& source)
{
	Clear();

	Info info;
	info.self = this;
	aux::XmlReader parser;
	info.parser = &parser;

	info.attributes.reserve(10);
	info.bufferedNodes.reserve(1000);
	info.nodeStack.reserve(10);

	info.node = m_Root;
	info.nodeStack.push_back(0);

	SetCallbacks(info);
	if (!parser.Parse(source))
	{
		OnError(info, parser.GetLastError());
	}
	else if (m_LastError.empty())
	{
		if (info.node == m_Root)
		{
			SetNodeChildren(info);
			SetNodeData(info);
			return true;
		}

		// Некоторые элементы остались не закрыты
		OnError(info, L"Unexpected end of data");
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
void XmlDocument::SetCallbacks(Info& info)
{
	info.parser->userData = &info;
	info.parser->tagOpenedCb = OnTagOpened;
	info.parser->tagClosedCb = OnTagClosed;
	info.parser->attrCb = OnAttr;
	info.parser->dataCb = OnData;
}

//--------------------------------------------------------------------------------------------------------------------------------
void XmlDocument::OnTagOpened(void* infoPtr, std::wstring_view name)
{
	if (Info* info = static_cast<Info*>(infoPtr))
	{
		if (name.empty())
		{
			// Элемент без имени ("<>" или "</>") недопустим
			OnError(*info, L"Element without a name encountered");
			return;
		}

		if (name.front() != '?')
		{
			info->hasStarted = true;
			if (!SetNodeAttrAndData(*info))
				return;

			auto& pool = info->self->m_Pool;
			auto node = pool.MakeNode(info->node);
			node->m_Name = pool.MakeString(name);

			info->bufferedNodes.push_back(node);
			info->nodeStack.push_back(info->bufferedNodes.size());

			info->node = node;
		}
		else if (info->hasStarted)
		{
			// Управляющий элемент не в начале документа
			OnError(*info, L"Unexpected control element");
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void XmlDocument::OnTagClosed(void* infoPtr, std::wstring_view name)
{
	if (Info* info = static_cast<Info*>(infoPtr))
	{
		if (name.empty())
		{
			// Пропускаем обработку пролога XML
			if (info->node == info->self->m_Root)
				return;
		}
		else if (info->node->m_Name != name)
		{
			OnError(*info, L"Unpaired closing tag encountered");
			return;
		}

		SetNodeChildren(*info);
		if (!SetNodeAttrAndData(*info))
			return;

		info->node = info->node->m_Parent;
		Assert(info->node);
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void XmlDocument::OnAttr(void* infoPtr, std::wstring_view name, std::wstring_view value)
{
	if (Info* info = static_cast<Info*>(infoPtr))
	{
		// Атрибуты пролога (узел == m_Root) пропускаем
		if (info->node != info->self->m_Root && Verify(!name.empty()))
		{
			auto& pool = info->self->m_Pool;
			info->attributes.emplace_back(pool.MakeString(name), pool.MakeString(value));
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void XmlDocument::OnData(void* infoPtr, std::wstring_view text, bool)
{
	if (Info* info = static_cast<Info*>(infoPtr))
	{
		info->hasStarted = true;
		if (info->node->m_Data.size)
		{
			Assert(info->data.empty());
			info->data.assign(info->node->m_Data);
			info->node->m_Data.size = 0;
			info->data += L'\n';
		}
		info->data.append(text);
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void XmlDocument::SetNodeData(Info& info)
{
	// Копируем данные узла
	if (!info.data.empty())
	{
		auto& pool = info.self->m_Pool;
		info.node->m_Data = pool.MakeString(info.data);
		info.data.clear();
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
bool XmlDocument::SetNodeAttrAndData(Info& info)
{
	// Копируем полученные атрибуты элемента
	if (const size_t count = info.attributes.size())
	{
		// Проверим на дубликаты. Возможно, способ не оптимальный. Но с учётом небольшого среднего количества атрибутов
		// у отдельно взятого элемента, это быстрее, чем сначала сортировать атрибуты и затем попарно сравнивать их имена
		const auto& v = info.attributes;
		for (size_t i = 1; i < count; ++i)
		{
			const auto& name = v[i].name;
			for (size_t j = 0; j < i; ++j)
			{
				if (name == v[j].name)
				{
					OnError(info, L"Duplicated attribute encountered");
					return false;
				}
			}
		}

		info.node->m_Attributes = info.self->m_Pool.MakeArray(v.begin(), count);
		info.attributes.clear();
	}

	SetNodeData(info);
	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
void XmlDocument::SetNodeChildren(Info& info)
{
	// Копируем вложенные узлы
	const size_t prevIndex = info.nodeStack.back();
	const size_t nodeCount = info.bufferedNodes.size();
	if (const size_t childCount = nodeCount - prevIndex)
	{
		auto first = info.bufferedNodes.begin() + prevIndex;
		info.node->m_Nodes = info.self->m_Pool.MakeArray(first, childCount);
		info.bufferedNodes.resize(prevIndex);
	}

	info.nodeStack.pop_back();
}

//--------------------------------------------------------------------------------------------------------------------------------
void XmlDocument::OnError(Info& info, std::wstring_view text)
{
	// При обнаружении ошибки прерываем парсинг. Точнее, сам парсер ещё некоторое
	// время будет продолжать работу, но мы уже не будем обрабатывать документ
	info.parser->tagOpenedCb = nullptr;
	info.parser->tagClosedCb = nullptr;
	info.parser->attrCb = nullptr;
	info.parser->dataCb = nullptr;

	if (info.self->m_LastError.empty())
		info.self->m_LastError = text;

	info.parser->StopParsing();
}
