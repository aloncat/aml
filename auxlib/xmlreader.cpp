//∙Auxlib
// Copyright (C) 2021-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "xmlreader.h"

#include <core/debug.h>
#include <core/file.h>
#include <core/strutil.h>

namespace aux {

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   NumDecoder
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// TODO: сейчас в классе NumDecoder всего 2 функции: для декодирования десятичного числа в тип int и шестнадцатеричного
// числа в тип unsigned. В будущем мы захотим читать значения атрибутов XML других типов (например, [u]int64_t). Поэтому
// функцию Decode лучше сделать шаблонной, чтобы она умела работать с любым целочисленным типом. Также в C++17 появилась
// функция from_chars: она работает только с типом char* (что неудобно), зато её можно использовать для типа float

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
bool NumDecoder::DecodeHex(const wchar_t* from, const wchar_t* to, unsigned& value)
{
	if (from >= to)
		return false;

	for (size_t remains = to - from; remains > 8; --remains)
	{
		if (*from++ != '0')
			return false;
	}

	unsigned result = 0;
	for (unsigned v; from < to; ++from)
	{
		if (v = static_cast<unsigned>(*from) - '0'; v <= 9)
			result = (result << 4) + v;
		else if (v -= (v < 49) ? 17 : 49; v <= 5)
			result = (result << 4) + v + 10;
		else
			return false;
	}

	value = result;
	return true;
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
//   XmlData
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
const uint8_t utf8Tab[256] = {
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	// 0: 1 байт -> [0xxxxxxx] -> U+0000..U+007F
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	// 1
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	// 2
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	// 3
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	// 4
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	// 5
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	// 6
	1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,	// 7
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 8: недопустимые значения для первого байта
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// 9
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// A
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	// B
	0, 0, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,	// C: 2 байта -> [110xxxxx] + 1x [10xxxxxx] -> U+0080..U+07FF
	2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,	// D:
	3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,	// E: 3 байта -> [1110xxxx] + 2x [10xxxxxx] -> U+0800..U+FFFF
	4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0	// F: 4 байта -> [11110xxx] + 3x [10xxxxxx] -> U+10000..U+10FFFF
};

//--------------------------------------------------------------------------------------------------------------------------------
XmlData::XmlData(util::File& file)
	: m_File(file)
{
	m_DataBuffer = new wchar_t[DATA_BUFFER_SIZE];
	m_SwapBuffer = new wchar_t[DATA_BUFFER_SIZE];
	m_FileBuffer = new uint8_t[FILE_BUFFER_SIZE];

	data = m_DataBuffer;
	data[0] = 0;

	ReadFileData();
	CheckBom();

	GetMoreData();
}

//--------------------------------------------------------------------------------------------------------------------------------
XmlData::~XmlData()
{
	AML_SAFE_DELETEA(m_FileBuffer);
	AML_SAFE_DELETEA(m_SwapBuffer);
	AML_SAFE_DELETEA(m_DataBuffer);
}

//--------------------------------------------------------------------------------------------------------------------------------
bool XmlData::GetMoreData(bool append)
{
	if (m_Remains < 2 * DATA_BUFFER_SIZE)
		ReadFileData();

	// Меняем буферы местами, это необходимо парсеру для корректной обработки данных вне элементов:
	// благодаря обмену данные предыдущего буфера остаются без изменений на том же месте в памяти
	wchar_t* out = m_SwapBuffer;
	m_SwapBuffer = m_DataBuffer;
	m_DataBuffer = out;

	if (append && *data)
	{
		const size_t dataRemains = wcslen(data);
		memcpy(out, data, dataRemains * sizeof(wchar_t));
		out += dataRemains;
	}
	data = m_DataBuffer;

	if (!m_ErrorFlag)
	{
		if (m_Encoding == Encoding::Utf8)
			out = DecodeUtf8(out);
		else
			out = DecodeUtf16(out);
	}

	*out = 0;
	return out > data;
}

//--------------------------------------------------------------------------------------------------------------------------------
void XmlData::CheckBom()
{
	m_Encoding = Encoding();

	if (m_Remains >= 2)
	{
		// UTF-16 (LE)
		const uint8_t* fileData = m_FileBuffer + m_Pos;
		if (fileData[0] == 0xff && fileData[1] == 0xfe)
		{
			m_Encoding = Encoding::Utf16Le;
			m_Remains -= 2;
			m_Pos += 2;
			return;
		}
		// UTF-16 (BE)
		if (fileData[0] == 0xfe && fileData[1] == 0xff)
		{
			m_Encoding = Encoding::Utf16Be;
			m_Remains -= 2;
			m_Pos += 2;
			return;
		}
		// UTF-8
		if (m_Remains >= 3 && fileData[0] == 0xef && fileData[1] == 0xbb && fileData[2] == 0xbf)
		{
			m_Encoding = Encoding::Utf8;
			m_Remains -= 3;
			m_Pos += 3;
			return;
		}
	}

	uint8_t first = m_Remains ? m_FileBuffer[m_Pos] : 32;
	// В случае, если BOM не распознан, то проверим, что либо файл пуст, либо его первый байт - это пробел, табуляция,
	// перевод строки или открывающая угловая скобка. Во всех этих случаях будем считать, что файл имеет кодировку UTF-8
	if (first == '<' || first == 9 || first == 10 || first == 13 || first == 32)
	{
		// Убедимся, что это не UTF-16-LE (для UTF-16 BOM обязателен)
		if (m_Remains < 2 || m_FileBuffer[1])
			m_Encoding = Encoding::Utf8;
	}

	if (m_Encoding == Encoding())
	{
		SetError(L"File encoding not recognized");
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
bool XmlData::ReadFileData()
{
	if (!m_NoMoreData)
	{
		// Если в буфере остались данные, сдвинем их к началу
		if (m_Remains && m_Pos)
		{
			memmove(m_FileBuffer, m_FileBuffer + m_Pos, m_Remains);
		}
		m_Pos = 0;

		// Пытаемся полностью заполнить буфер
		const size_t toRead = FILE_BUFFER_SIZE - m_Remains;
		if (auto [bytesRead, success] = m_File.Read(m_FileBuffer + m_Remains, toRead); success)
		{
			m_Remains += bytesRead;
			m_NoMoreData = bytesRead < toRead;
		} else
		{
			SetError(L"Failed to read data from file");
			// При возникновении ошибки чтения файла будем
			// считать, что данных в файле больше нет
			m_NoMoreData = true;
		}
	}

	return m_Remains > 0;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE wchar_t* XmlData::DecodeUtf8(wchar_t* out)
{
	size_t remains = (DATA_BUFFER_SIZE - 1) - (out - m_DataBuffer);
	const uint8_t* inp = m_FileBuffer + m_Pos;

	for (;; ++out, --remains)
	{
		for (;; ++out, --remains)
		{
			remains = (remains < m_Remains) ? remains : m_Remains;
			if (!remains)
				return out;

			if (uint8_t c = *inp; static_cast<int8_t>(c) > 0)
			{
				size_t count = 0;
				do {
					out[count++] = c;
					if (count >= remains)
					{
						m_Pos += count;
						m_Remains -= count;
						return out + count;
					}
					c = inp[count];
				} while (static_cast<int8_t>(c) > 0);

				out += count;
				remains -= count;
				m_Remains -= count;
				m_Pos += count;
				inp += count;
			}

			const size_t size = utf8Tab[*inp];
			if (size > m_Remains)
				break;

			if (size == 2)
			{
				if ((inp[1] & 0xc0) == 0x80)
				{
					*out = ((inp[0] & 0x1f) << 6) | (inp[1] & 0x3f);
					// Значение code point при длине последовательности в 2 байта не должно быть меньше 0x80. Но
					// мы не проверяем это здесь (в целях производительности), а делаем проверку с помощью самой
					// таблицы utf8Tab - недопустимые значения приведут к тому, что size будет равен 0
					m_Remains -= 2;
					m_Pos += 2;
					inp += 2;
				} else
					break;
			}
			else if (size == 3)
			{
				if ((inp[1] & 0xc0) == 0x80)
				{
					unsigned codePoint = ((inp[0] & 0x0f) << 12) | ((inp[1] & 0x3f) << 6);
					// Значение code point при длине последовательности в 3 байта не должно
					// быть меньше 0x800, а также не должно лежать в диапазоне 0xd800-0xdfff
					if (auto h5b = codePoint & 0xf800; h5b && h5b != 0xd800 && (inp[2] & 0xc0) == 0x80)
					{
						*out = static_cast<wchar_t>(codePoint | (inp[2] & 0x3f));
						m_Remains -= 3;
						m_Pos += 3;
						inp += 3;
					} else
						break;
				} else
					break;
			}
			else if (size >= 4)
			{
				// TODO: мы пока не умеем обрабатывать 4-байтовые последовательности. Если мы встретим
				// такую последовательность, то завершим декодирование и установим флаг ошибки. Для
				// size == 4 значение code point не должно быть меньше 0x10000 или больше 0x10ffff
				SetError(L"UTF-8 decoding error");
				return out;
			}
			// Это code point U+0000, который согласно спецификации XML не должен встречаться в тексте документа. Мы
			// используем символ с кодом 0 как признак конца данных в буфере, поэтому заменим его на "�" (U+FFFD)
			else if (size == 1)
			{
				*out = 0xfffd;
				--m_Remains;
				++m_Pos;
				++inp;
			} else
				break;
		}

		// Возникла ситуация, в которой мы не смогли корректно декодировать
		// последовательность. Поэтому заменим её на символ "�" (U+FFFD)
		*out = 0xfffd;

		// Теперь пропустим первый и все дополнительные байты этой последовательности
		size_t count = 1;
		for (;; ++count)
		{
			if (count >= m_Remains)
			{
				if (m_NoMoreData)
					break;
				// В буфере недостаточно данных, чтобы гарантированно пропустить текущую
				// последовательность, но есть данные в файле. Сделаем это в другой раз
				return out;
			}
			if ((inp[count] & 0xc0) != 0x80)
				break;
		}
		m_Remains -= count;
		m_Pos += count;
		inp += count;
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE wchar_t* XmlData::DecodeUtf16(wchar_t* out)
{
	// Здесь, как и в случае UTF-8, мы используем символ с кодом 0 как признак конца данных в буфере. Согласно
	// спецификации XML символ U+0000 не должен встречаться в тексте документа. Поэтому мы будем заменять такие
	// символы на символ "�" (U+FFFD). Помимо этого, мы не будем обрабатывать суррогаты UTF-16 для ОС Windows
	// по причине того, что Windows допускает использование непарных суррогатов (например, в именах файлов)

	const size_t remainsInFile = m_Remains >> 1;
	const size_t remainsInData = (DATA_BUFFER_SIZE - 1) - (out - m_DataBuffer);
	size_t remains = (remainsInData < remainsInFile) ? remainsInData : remainsInFile;
	const uint8_t* inp = m_FileBuffer + m_Pos;

	size_t count = 0;
	for (; count < remains;)
	{
		unsigned codePoint = 0;
		if (m_Encoding == Encoding::Utf16Le)
		{
			for (; count < remains; ++out)
			{
				#if AML_LITTLE_ENDIAN
					auto p = reinterpret_cast<const uint16_t*>(inp);
					codePoint = p[count++];
				#else
					auto p = inp + 2 * count++;
					codePoint = p[0] | (p[1] << 8);
				#endif
				*out = static_cast<wchar_t>(codePoint);
				if (codePoint - 1 >= 0xd7ff)
					break;
			}
		} else
		{
			for (; count < remains; ++out)
			{
				#if AML_BIG_ENDIAN
					auto p = reinterpret_cast<const uint16_t*>(inp);
					codePoint = p[count++];
				#elif !AML_DEBUG
					auto p = reinterpret_cast<const uint16_t*>(inp);
					codePoint = util::ByteSwap16(p[count++]);
				#else
					auto p = inp + 2 * count++;
					codePoint = (p[0] << 8) | p[1];
				#endif
				*out = static_cast<wchar_t>(codePoint);
				if (codePoint - 1 >= 0xd7ff)
					break;
			}
		}

		// Суррогатные пары, только для UTF-32
		if (sizeof(wchar_t) == 4 && (codePoint & 0xf800) == 0xd800)
		{
			if (!(codePoint & 0x0400))
			{
				if (count < remains)
				{
					auto p = inp + 2 * count;
					unsigned low = (m_Encoding == Encoding::Utf16Le) ?
						p[0] | (p[1] << 8) : (p[0] << 8) | p[1];
					if ((low & 0xfc00) == 0xdc00)
					{
						++count;
						low &= 0x3ff;
						unsigned high = codePoint & 0x3ff;
						*out = static_cast<wchar_t>(0x10000 + (high << 10) + low);
						continue;
					}
				}
				else if (!m_NoMoreData)
				{
					// В буфере данных нет, но есть в файле. Поэтому откатимся на 1 символ
					// назад (чтобы обработать суррогатную пару целиком в другой раз)
					--count;
					break;
				}
			}
			// Если суррогатная пара неполная или некорректная, то заменим символ на
			// "�" (U+FFFD) и продолжим декодирование со следующего 16-битного слова
			*out = 0xfffd;
		}
		else if (!codePoint)
			*out = 0xfffd;
	}

	m_Remains -= 2 * count;
	m_Pos += 2 * count;

	// Если в буфере файла остался 1 байт и данных в файле больше нет, значит последний символ неполный.
	// Тогда если в буфере данных есть место для 1 символа, "декодируем" наш байт в символ "�" (U+FFFD)
	if (m_Remains == 1 && m_NoMoreData && out < m_DataBuffer + DATA_BUFFER_SIZE - 1)
	{
		*out++ = 0xfffd;
		m_Remains = 0;
		++m_Pos;
	}

	return out;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void XmlData::SetError(std::wstring_view text)
{
	m_ErrorFlag = true;
	if (m_LastError.empty())
		m_LastError = text;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   XmlBufferedView
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
void XmlBufferedView::Append(const wchar_t* first, size_t count)
{
	if (!m_IsBuffered)
	{
		m_Buffer.assign(data, size);
		m_IsBuffered = true;
	}

	m_Buffer.append(first, count);
	data = m_Buffer.c_str();
	size += count;
}

//--------------------------------------------------------------------------------------------------------------------------------
void XmlBufferedView::Buffer()
{
	if (!m_IsBuffered)
	{
		m_Buffer.assign(data, size);
		data = m_Buffer.c_str();
		m_IsBuffered = true;
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void XmlBufferedView::KeepTail(size_t count)
{
	count = (size < count) ? size : count;

	if (!m_IsBuffered)
	{
		m_Buffer.assign(data + size - count, count);
		m_IsBuffered = true;
	}
	else if (count < size)
	{
		m_Buffer.erase(0, size - count);
	}

	data = m_Buffer.c_str();
	size = count;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   XmlReader
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
enum {
	ST_TAG = 0,
	ST_ATTR_NAME,
	ST_ATTR_VALUE,

	STOP_TAB_COUNT
};

//--------------------------------------------------------------------------------------------------------------------------------
XmlReader::XmlReader()
{
	m_StopTabs = new StopTab[STOP_TAB_COUNT];

	InitStopTab(ST_TAG, "> \x09\x0a\x0d");
	InitStopTab(ST_ATTR_NAME, "/>= \x09\x0a\x0d");
	InitStopTab(ST_ATTR_VALUE, "/> \x09\x0a\x0d");

	m_EscapeBuffer.Grow(1024);
}

//--------------------------------------------------------------------------------------------------------------------------------
XmlReader::~XmlReader()
{
	AML_SAFE_DELETEA(m_StopTabs);
}

//--------------------------------------------------------------------------------------------------------------------------------
bool XmlReader::Parse(util::WZStringView filePath)
{
	m_LastError.clear();

	util::BinaryFile f;
	if (!f.Open(filePath, util::FILE_OPEN_READ))
	{
		SetError(L"Couldn't open file");
		return false;
	}

	bool result = Parse(f);
	f.Close();

	return result;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool XmlReader::Parse(util::File& file)
{
	m_LastError.clear();
	m_IsParsingProlog = false;
	m_HasParsedProlog = false;
	m_ShouldStop = false;

	XmlData data(file);
	if (CheckData(data))
	{
		Invoke(docOpenedCb);
		if (ParseDocument(data))
		{
			Invoke(docClosedCb);
			return true;
		}
	}

	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool XmlReader::ParseDocument(XmlData& data)
{
	DataInfo info;
	for (wchar_t c;;)
	{
		const wchar_t* p = data.data;
		info.text.data = p;

		for (;; ++p)
		{
			c = *p;
			// Символ с кодом 0 используется исключительно
			// в качестве признака окончания данных в буфере
			if (!c || c == '<')
				break;
		}

		info.text.size = p - info.text.data;
		data.data += info.text.size;

		if (c == '<')
		{
			OnDataReady(info);
			if (!ParseElement(data))
			{
				CheckData(data);
				return false;
			}
			if (m_ShouldStop)
			{
				SetError(L"Parsing has been cancelled");
				return false;
			}
		} else
		{
			OnMoreData(info);
			// Буфер пуст, пробуем загрузить больше данных. Если данные будут загружены, то
			// функция вернёт true. Если же она вернёт false, значит данных больше нет
			if (!data.GetMoreData())
			{
				OnDataReady(info);
				return CheckData(data);
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
bool XmlReader::ParseElement(XmlData& data)
{
	++data.data;
	auto& s = m_TextString;
	GetNextToken(s, data, ST_TAG);

	if (*data.data == 0)
	{
		SetError(L"Unexpected end of data");
		return false;
	}

	if (s.size && s.data[0] == '!')
	{
		// Комментарий
		if (s.size >= 2 && s.data[1] == '-' && s.data[2] == '-')
		{
			if (*data.data != '>' || s.size < 5 || s.data[s.size - 2] != '-' || s.data[s.size - 1] != '-')
			{
				if (!SkipComment(data))
				{
					SetError(L"Unexpected end of data");
					return false;
				}
			}
		} else
		{
			// TODO: мы пока не поддерживаем другие элементы "<!", такие как "<!DOCTYPE" или "<![CDATA["
			SetError(L"Unsupported \"<!\" element");
			return false;
		}
	} else
	{
		XmlStringView empty;
		SkipWhitespaces(data, &s);
		if (*data.data == '>')
		{
			// Пустой элемент без атрибутов
			if (s.size && s.data[s.size - 1] == '/')
			{
				--s.size;
				Invoke(tagOpenedCb, s);
				OnTagClosed(empty);
			}
			// Закрывающий тег. Не проверяем длину s, так как здесь она нулевой быть не может
			else if (s.data[0] == '/')
			{
				++s.data;
				--s.size;
				OnTagClosed(s);
			} else
			{
				// Открывающий тег. Мы допускаем элементы вида "<>". У таких элементов не
				// будет имени. Предполагается, что защита от таких конструкций будет снаружи
				Invoke(tagOpenedCb, s);
			}
		} else
		{
			// Пролог XML
			const bool isControl = s.size && s.data[0] == '?';
			if (isControl && !util::StrInsCmp(s, L"?xml"))
			{
				if (m_HasParsedProlog)
				{
					SetError(L"Unexpected XML declaration encountered");
					return false;
				}
				m_IsParsingProlog = true;
			}
			// Аналогично элементам без атрибутов здесь мы допускаем элемент (открывающий
			// тег) без имени, полагая что валидация XML будет делаться снаружи парсера
			Invoke(tagOpenedCb, s);
			bool tagClosed = false;

			while (*data.data != '>')
			{
				// Пустой элемент (без данных, без вложенных элементов)
				if (*data.data == '/')
				{
					++data.data;
					SkipWhitespaces(data);
					if (*data.data == '>')
					{
						OnTagClosed(empty);
						tagClosed = true;
						break;
					}
					if (!data.data[0])
					{
						SetError(L"Unexpected end of data");
						return false;
					}
					// Символ "/", встреченный внутри элемента, и не являющийся
					// частью пустого/закрывающего тега. Завершимся с ошибкой
					SetError(L"Unexpected \"/\" encountered");
					return false;
				}

				// Если в буфере менее 2 байт, пробуем догрузить
				if ((!data.data[0] || !data.data[1]) && !data.GetMoreData(true))
				{
					SetError(L"Unexpected end of data");
					return false;
				}

				// Завершение пролога (управляющего элемента)
				if (data.data[0] == '?' && data.data[1] == '>')
				{
					if (!isControl)
					{
						SetError(L"Unexpected \"?>\" encountered");
						return false;
					}
					OnTagClosed(empty);
					tagClosed = true;
					++data.data;
					break;
				}

				// Атрибут
				if (!ParseAttr(data))
					return false;

				SkipWhitespaces(data);
			}

			// Вызовем колл-бэк закрытия тега после обработки управляющего элемента,
			// если вместо "?>" или "/>" была использована только угловая скобка ">"
			if (isControl && !tagClosed)
				OnTagClosed(empty);
		}
	}

	++data.data;
	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool XmlReader::ParseAttr(XmlData& data)
{
	GetNextToken(m_AttrName, data, ST_ATTR_NAME);
	SkipWhitespaces(data, &m_AttrName);

	while (const auto next = *data.data)
	{
		m_TextString.Reset();

		if (next == '=')
		{
			if (!m_AttrName.size)
			{
				SetError(L"Unexpected \"=\" encountered");
				return false;
			}

			++data.data;
			SkipWhitespaces(data, &m_AttrName);

			const auto quote = *data.data;
			if (quote != '\'' && quote != '\"')
			{
				m_AttrName.Buffer();
				GetNextToken(m_TextString, data, ST_ATTR_VALUE);
			}
			else if (!GetQuotedAttrValue(data))
				break;
		}

		Assert(m_AttrName.size);
		return ProcessAttr(data);
	}

	SetError(L"Unexpected end of data");
	return false;
}

//--------------------------------------------------------------------------------------------------------------------------------
void XmlReader::OnMoreData(DataInfo& info)
{
	if (dataCb && info.text.size)
	{
		if (info.prev.size)
		{
			wchar_t last = info.text.data[info.text.size - 1];
			if (last != 32 && last != 9 && last != 10 && last != 13)
			{
				if (size_t remains = InvokeDataCb(info.prev, info.firstPart, false))
				{
					info.prev.KeepTail(remains);
					info.prev.Append(info.text);
				} else
				{
					info.prev.Set(info.text);
				}
				info.firstPart = false;
			} else
			{
				info.prev.Append(info.text);
			}
		} else
		{
			info.prev.Set(info.text);
			Trim(info.prev, TRIM_LEFT);
		}

		// Здесь важно обнулить вью info.text, потому что далее может быть вызвана функция
		// OnDataReady, которая будет рассматривать info.text как продолжение данных info.prev
		info.text.size = 0;
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void XmlReader::OnDataReady(DataInfo& info)
{
	if (dataCb)
	{
		if (info.text.size)
		{
			Trim(info.text, info.prev.size ? TRIM_RIGHT : TRIM_ALL);
		}

		size_t remains = 0;
		if (info.prev.size)
		{
			if (!info.text.size)
			{
				Trim(info.prev, TRIM_RIGHT);
			}

			remains = InvokeDataCb(info.prev, info.firstPart, !info.text.size);
			info.firstPart = false;
		}

		if (info.text.size)
		{
			if (remains)
			{
				info.prev.KeepTail(remains);
				info.prev.Append(info.text);
				info.text = info.prev;
			}

			InvokeDataCb(info.text, info.firstPart, true);
		}

		info.prev.Reset();
		// Приводим структуру info в исходное состояние. Вью info.text не обнуляем, так как это не
		// имеет значения: далее мы либо начнём формировать в нём новую строку, либо закончим работу
		info.firstPart = true;
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void XmlReader::OnTagClosed(XmlStringView name)
{
	// Мы позволяем закрывать управляющий "<? ... ?>" элемент как обычный. Это неправильно,
	// но приемлемо, учитывая, что документ должен быть валиден, а мы валидацию не делаем
	if (m_IsParsingProlog)
	{
		m_IsParsingProlog = false;
		m_HasParsedProlog = true;
	}

	Invoke(tagClosedCb, name);
}

//--------------------------------------------------------------------------------------------------------------------------------
void XmlReader::Invoke(CommonCb cb)
{
	if (cb)
	{
		cb(userData);
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
void XmlReader::Invoke(TagCb cb, XmlStringView name)
{
	if (cb)
	{
		cb(userData, name);
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
size_t XmlReader::InvokeDataCb(XmlStringView text, bool firstPart, bool lastPart)
{
	size_t lastPos = text.size;
	const wchar_t* p = text.data;
	for (size_t i = 0; i < lastPos; ++i)
	{
		if (p[i] == '&')
		{
			lastPos = i;
			break;
		}
	}

	size_t remains = 0;
	if (lastPos < text.size)
	{
		const size_t size = text.size;
		text = UnescapeString(text, lastPos, lastPart);
		remains = size - lastPos;
	}

	// Мы не проверяем значение dataCb перед вызовом, так как
	// эта функция вызывается, только если колл-бэк установлен
	dataCb(userData, text, firstPart);
	return remains;
}

//--------------------------------------------------------------------------------------------------------------------------------
void XmlReader::Trim(XmlStringView& text, int options)
{
	const wchar_t* p = text.data;
	const wchar_t* pEnd = p + text.size;

	if (options & TRIM_LEFT)
	{
		for (; p < pEnd; ++p)
		{
			wchar_t c = *p;
			if (c != 32 && c != 9 && c != 10 && c != 13)
				break;
		}

		text.data = p;
		text.size = pEnd - p;
	}

	if (options & TRIM_RIGHT)
	{
		for (; p < pEnd; --pEnd)
		{
			wchar_t c = *(pEnd - 1);
			if (c != 32 && c != 9 && c != 10 && c != 13)
				break;
		}

		text.size = pEnd - p;
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
bool XmlReader::ProcessAttr(XmlData& data)
{
	if (m_IsParsingProlog)
	{
		if (!util::StrInsCmp(m_AttrName, L"encoding"))
		{
			const wchar_t* encoding = nullptr;
			switch (data.GetEncoding())
			{
				case XmlData::Encoding::Utf8:
					encoding = L"utf-8";
					break;
				case XmlData::Encoding::Utf16Le:
				case XmlData::Encoding::Utf16Be:
					encoding = L"utf-16";
					break;
			}
			if (!encoding || util::StrInsCmp(m_TextString, encoding))
			{
				SetError(L"Incorrect XML encoding value");
				return false;
			}
		}
		else if (!util::StrInsCmp(m_AttrName, L"version") && m_TextString != L"1.0")
		{
			SetError(L"Unsupported XML version");
			return false;
		}
	}

	if (attrCb)
	{
		size_t lastPos = m_TextString.size;
		const wchar_t* p = m_TextString.data;
		for (size_t i = 0; i < lastPos; ++i)
		{
			if (p[i] == '&')
			{
				lastPos = i;
				break;
			}
		}

		if (lastPos < m_TextString.size)
		{
			auto s = UnescapeString(m_TextString, lastPos, true);
			m_TextString.Set(s);
		}

		attrCb(userData, m_AttrName, m_TextString);
	}

	return true;
}

//--------------------------------------------------------------------------------------------------------------------------------
void XmlReader::InitStopTab(int idx, const char* stops)
{
	uint8_t* tab = m_StopTabs[idx];
	AML_FILLA(tab, 0, STOP_TAB_SIZE);

	tab[0] = 1;
	for (auto p = stops; *p; ++p)
	{
		const unsigned char v = *p;
		Assert(v < STOP_TAB_SIZE);
		tab[v] = 1;
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
inline size_t XmlReader::GetNextTokenLen(const wchar_t* first, const StopTab stops)
{
	const wchar_t* p;
	for (p = first;; ++p)
	{
		const unsigned v = *p;
		if (v < STOP_TAB_SIZE && stops[v])
			break;
	}

	return p - first;
}

//--------------------------------------------------------------------------------------------------------------------------------
void XmlReader::GetNextToken(XmlBufferedView& out, XmlData& data, int stopTabIdx)
{
	const uint8_t* stops = m_StopTabs[stopTabIdx];
	size_t len = GetNextTokenLen(data.data, stops);
	out.Set(data.data, len);

	if (data.data[len])
	{
		// Данных в буфере было достаточно (остановились на разделителе)
		data.data += len;
	} else
	{
		// Данных недостаточно (остановились на 0), пробуем загрузить
		// больше и продолжаем, накапливая текст во временном буфере
		while (data.GetMoreData())
		{
			len = GetNextTokenLen(data.data, stops);
			out.Append(data.data, len);
			if (data.data[len])
			{
				data.data += len;
				break;
			}
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
bool XmlReader::GetQuotedAttrValue(XmlData& data)
{
	m_TextString.Reset();
	for (const wchar_t quote = *data.data++;;)
	{
		wchar_t c, *p;
		for (p = data.data;; ++p)
		{
			c = *p;
			if (c == quote || !c)
				break;
		}

		if (const size_t size = p - data.data)
		{
			if (m_TextString.size)
				m_TextString.Append(data.data, size);
			else
				m_TextString.Set(data.data, size);
		}

		if (c != 0)
		{
			data.data = p + 1;
			return true;
		}

		m_AttrName.Buffer();
		if (!data.GetMoreData())
			return false;
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
inline void XmlReader::SkipWhitespaces(XmlData& data, XmlBufferedView* safeBuffered)
{
	// Довольно часто, например слева и справа от "=" при парсинге атрибутов, "пустые" символы
	// отсутствуют. Чтобы не вызывать каждый раз функцию (для полной проверки), убедимся перед
	// её вызовом, что мы стоим на пробеле или другом символе с меньшим кодом (включая 0)
	if (*data.data <= 32)
	{
		SkipWhitespacesImpl(data, safeBuffered);
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void XmlReader::SkipWhitespacesImpl(XmlData& data, XmlBufferedView* safeBuffered)
{
	for (wchar_t* p = data.data;;)
	{
		const wchar_t c = *p++;
		if (c != 32 && c != 9 && c != 10 && c != 13)
		{
			if (c)
			{
				data.data = p - 1;
				break;
			}

			if (safeBuffered)
				safeBuffered->Buffer();
			if (!data.GetMoreData())
				break;
			p = data.data;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
bool XmlReader::SkipComment(XmlData& data)
{
	for (wchar_t c, *p = data.data;;)
	{
		for (;;)
		{
			c = *p++;
			if (c == '-')
				break;

			if (!c)
			{
				if (!data.GetMoreData())
					return false;
				p = data.data;
			}
		}

		for (size_t len = 1;;)
		{
			c = *p;
			while (c == '-')
			{
				++len;
				c = *(++p);
			}

			if (c == '>')
			{
				if (len >= 2)
				{
					data.data = p;
					return true;
				}
			}
			else if (!c)
			{
				if (!data.GetMoreData())
					return false;
				p = data.data;
				continue;
			}

			++p;
			break;
		}
	}
}

//--------------------------------------------------------------------------------------------------------------------------------
XmlStringView XmlReader::UnescapeString(XmlStringView source, size_t& from, bool isComplete)
{
	// Чтобы не вычислять точный размер результирующей строки, увеличим
	// размер массива до длины исходной строки, если его размер меньше
	m_EscapeBuffer.Grow(source.size);
	wchar_t* out = m_EscapeBuffer;
	size_t outSize = 0;

	const wchar_t* p = source.data;
	const wchar_t* next = p + from;
	for (auto end = p + source.size;;)
	{
		while (next < end && *next != '&')
			++next;

		memcpy(out + outSize, p, (next - p) * sizeof(wchar_t));
		outSize += next - p;
		p = next;

		if (next >= end)
			break;

		auto dp = next + 1;
		while (dp < end && *dp != ';')
			++dp;

		if (dp >= end)
			break;

		if (auto count = UnescapeChar(next + 1, dp - next, out + outSize))
		{
			outSize += count;
			p = dp + 1;
		}
		next = dp + 1;
	}

	from = p - source.data;
	// Если последняя escape-последовательность была неполной, то from будет меньше размера исходной
	// строки. Если флаг isComplete установлен, то добавим оставшиеся в source символы к результату
	if (const size_t remains = source.size - from; remains && isComplete)
	{
		memcpy(out + outSize, source.data + from, remains * sizeof(wchar_t));
		outSize += remains;
		from += remains;
	}

	return { out, outSize };
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE size_t XmlReader::UnescapeChar(const wchar_t* from, size_t size, wchar_t* out)
{
	if (size < 3 || size > 9)
		return false;

	if (from[0] == '#')
	{
		if (from[1] == 'x' || from[1] == 'X')
		{
			unsigned num;
			if (NumDecoder::DecodeHex(from + 2, from + size - 1, num))
				return EncodeCP(out, num);
		} else
		{
			int num;
			if (NumDecoder::Decode(from + 1, from + size - 1, num) && num >= 0)
				return EncodeCP(out, num);
		}
	}
	else if (size == 3)
	{
		if (!util::StrNInsCmp(from, L"gt", 2))
		{
			*out = '>';
			return 1;
		}
		if (!util::StrNInsCmp(from, L"lt", 2))
		{
			*out = '<';
			return 1;
		}
	}
	else if (size == 4)
	{
		if (!util::StrNInsCmp(from, L"amp", 3))
		{
			*out = '&';
			return 1;
		}
	}
	else if (size == 5)
	{
		if (!util::StrNInsCmp(from, L"apos", 4))
		{
			*out = '\'';
			return 1;
		}
		if (!util::StrNInsCmp(from, L"quot", 4))
		{
			*out = '\"';
			return 1;
		}
	}

	return 0;
}

//--------------------------------------------------------------------------------------------------------------------------------
size_t XmlReader::EncodeCP(wchar_t* out, unsigned codePoint)
{
	if (codePoint > 0xd7ff)
	{
		// Невалидные значения заменяем на "�" (U+FFFD)
		if (codePoint < 0xe000 || codePoint > 0x10ffff)
		{
			*out = 0xfffd;
			return 1;
		}
		// Суррогатная пара UTF-16 (ОС Windows)
		else if (sizeof(wchar_t) == 2 && codePoint > 0xffff)
		{
			codePoint -= 0x10000;
			out[0] = static_cast<wchar_t>(0xd800 + (codePoint >> 10));
			out[1] = 0xdc00 + (codePoint & 0x3ff);
			return 2;
		}
	}

	*out = static_cast<wchar_t>(codePoint);
	return 1;
}

//--------------------------------------------------------------------------------------------------------------------------------
AML_NOINLINE void XmlReader::SetError(std::wstring_view text)
{
	if (m_LastError.empty())
		m_LastError = text;
}

//--------------------------------------------------------------------------------------------------------------------------------
bool XmlReader::CheckData(XmlData& data)
{
	if (data.Check())
		return true;

	// Если была ошибка в данных, то при парсинге тоже случится ошибка.
	// Поэтому перезапишем сообщение, если оно уже было задано парсером
	m_LastError = data.GetLastError();
	return false;
}

} // namespace aux
