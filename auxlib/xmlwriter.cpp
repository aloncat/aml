﻿//∙Auxlib
// Copyright (C) 2019-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "xmlwriter.h"

#include <core/debug.h>
#include <core/file.h>
#include <core/strutil.h>

using namespace aux;

//--------------------------------------------------------------------------------------------------------------------------------
XmlWriter::XmlWriter()
	: m_Output(new util::MemoryFile)
	, m_IsOutputOwned(true)
	, m_NeedDeclaration(true)
{
	static_cast<util::MemoryFile*>(m_Output)->Open();

	m_Array.Grow(2048);
	m_Buffer.Reserve(2048);

	memset(m_Paddings, 9, sizeof(m_Paddings));
	m_Paddings[0] = '\n';
}

//--------------------------------------------------------------------------------------------------------------------------------
XmlWriter::~XmlWriter()
{
	if (m_IsOutputOwned && m_Output)
	{
		m_Output->Close();
		delete m_Output;
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
bool XmlWriter::StartTag(std::wstring_view name)
{
	if (!Verify(!name.empty() && "XML element must have a name"))
		return false;

	if (m_NeedDeclaration)
	{
		m_NeedDeclaration = false;
		if (!WriteDeclaration())
			return false;
	}

	m_Buffer.Clear();
	if (m_IsTagOpened)
	{
		m_Buffer.Append(">\n", 2);
	}

	WritePadding(m_HasData);
	std::string utf8Name = util::ToUtf8(name);
	m_Buffer << '<' << utf8Name;
	if (!WriteFile(m_Buffer))
		return false;

	if (!m_NestedTags.empty())
	{
		auto& info = m_NestedTags.back();
		info.hasChildren = true;
	}
	m_NestedTags.emplace_back(std::move(utf8Name));

	m_IsTagOpened = true;
	m_NeedEndTag = false;
	m_HasData = false;

	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool XmlWriter::EndTag()
{
	if (!Verify(!m_NestedTags.empty() && "No XML tag to close"))
		return false;

	m_Buffer.Clear();
	if (m_NeedEndTag)
	{
		const auto& info = m_NestedTags.back();
		if (info.hasChildren)
		{
			WritePadding(m_HasData, -1);
		}
		m_Buffer << "</" << info.name << ">\n";
	} else
	{
		m_Buffer.Append(" />\n", 4);
	}
	if (!WriteFile(m_Buffer))
		return false;

	m_NestedTags.pop_back();

	m_IsTagOpened = false;
	m_NeedEndTag = true;
	m_HasData = false;

	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool XmlWriter::Attribute(std::wstring_view name, std::wstring_view value)
{
	if (StartAttribute(name))
	{
		m_Buffer << ToUtf8(value, true);
		return EndAttribute();
	}
	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool XmlWriter::Data(std::wstring_view text, bool newLine)
{
	if (text.empty())
		return true;

	m_Buffer.Clear();
	if (m_IsTagOpened)
	{
		m_Buffer.Append('>');
	}

	if (newLine)
	{
		WritePadding(m_HasData || m_IsTagOpened);
	}

	m_IsTagOpened = false;
	m_NeedEndTag = true;
	m_HasData = true;

	return (m_Buffer.IsEmpty() || WriteFile(m_Buffer)) &&
		WriteFile(ToUtf8(text, true));
}

//--------------------------------------------------------------------------------------------------------------------------------
void XmlWriter::Reset(bool writeDeclaration)
{
	m_NestedTags.clear();

	m_NeedDeclaration = writeDeclaration;
	m_IsTagOpened = false;
	m_NeedEndTag = false;
	m_HasData = false;
}

//--------------------------------------------------------------------------------------------------------------------------------
void XmlWriter::SetOutput(util::File& output)
{
	if (m_IsOutputOwned && m_Output)
	{
		m_Output->Close();
		delete m_Output;
	}

	m_Output = &output;
	m_IsOutputOwned = false;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool XmlWriter::WriteDeclaration()
{
	Assert(m_Output && m_NestedTags.empty() && !m_HasData);
	return WriteFile("\xef\xbb\xbf<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
}

//--------------------------------------------------------------------------------------------------------------------------------
void XmlWriter::WritePadding(bool newLine, int offset)
{
	// Первый символ в массиве m_Paddings - перевод строки, остальные - символы
	// табуляции. Если newLine == true, выведем перед отступом перевод строки
	int padding = static_cast<int>(m_NestedTags.size()) + offset;
	for (size_t lb = newLine ? 1 : 0; padding > 0; lb = 0)
	{
		const int maxPadding = static_cast<int>(sizeof(m_Paddings)) - 1;
		int toWrite = (padding < maxPadding) ? padding : maxPadding;
		m_Buffer.Append(m_Paddings + 1 - lb, toWrite + lb);
		padding -= toWrite;
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
inline bool XmlWriter::WriteFile(std::string_view text)
{
	return m_Output->Write(text.data(), text.size());
}

//--------------------------------------------------------------------------------------------------------------------------------
bool XmlWriter::StartAttribute(std::wstring_view name)
{
	if (Verify(!name.empty() && "XML attribute must have a name") &&
		Verify(m_IsTagOpened && "No XML tag to add attribute to"))
	{
		m_Buffer.Clear();
		m_Buffer << ' ' << ToUtf8(name, false) << "=\"";
		return true;
	}
	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool XmlWriter::EndAttribute()
{
	m_Buffer << '\"';
	return WriteFile(m_Buffer);
}

//--------------------------------------------------------------------------------------------------------------------------------
std::string_view XmlWriter::ToUtf8(std::wstring_view str, bool sanitize)
{
	// TODO: реализовать работу флага sanitize. Нужно экранировать символы < (&lt;),
	// > (&gt;), & (&amp;), ' (&apos;), " (&quot;) и все символы с кодами ниже 0x20,
	// кроме символа табуляции 0x09 и перевода строки (0x0a и 0x0d)

	if (str.size() > m_Array.GetSize() / 3)
	{
		// Вычисляем точный необходимый размер буфера, только если размер строки больше 1/3 размера массива.
		// Мы можем это делать, потому что 1 символ исходной строки становится 1-3 байтами в результирующей
		if (int requiredSize = util::ToUtf8(nullptr, 0, str); requiredSize > 0)
			m_Array.Grow(requiredSize);
	}

	int size = util::ToUtf8(m_Array, m_Array.GetSize(), str);
	return { m_Array, (size > 0) ? size : 0u };
}
