//∙Auxlib
// Copyright (C) 2021-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include <core/forward.h>
#include <core/platform.h>
#include <core/strcommon.h>
#include <core/util.h>

#include <string>
#include <string_view>

namespace aux {

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   XmlData - контейнер данных документа XML (декодирование UTF-8 и UTF-16)
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class XmlData
{
	AML_NONCOPYABLE(XmlData)

public:
	// Декодированные данные. Всегда оканчиваются нолём. Максимально возможное количество символов ограничено
	// константой DATA_BUFFER_SIZE (размером внутреннего буфера). Этот указатель одновременно служит "текущей
	// позицией" в массиве данных, поэтому при чтении символов его нужно двигать вперёд. Не допускается выход
	// указателя за пределы внутреннего буфера (т.е. дальше терминирующего ноля), если при вызове GetMoreData
	// параметр append функции будет равен true
	wchar_t* data = nullptr;

public:
	enum class Encoding {
		Utf8 = 1,
		Utf16Le,
		Utf16Be
	};

	XmlData(util::File& file);
	~XmlData();

	// Возвращает кодировку файла. Кодировка определяется по маркеру BOM.
	// Если маркера нет, то считается, что файл имеет кодировку UTF-8
	Encoding GetEncoding() const { return m_Encoding; }

	// Возвращает true, если ошибок нет. Если произойдёт ошибка
	// при чтении файла, то функция станет возвращать false
	bool Check() const { return !m_ErrorFlag; }
	// Возвращает ссылку на строку с сообщением об ошибке (актуально после вызова функции
	// GetMoreData, если она завершилась ошибкой и функция Check теперь возвращает false)
	const std::wstring& GetLastError() const { return m_LastError; }

	// Заполняет массив data следующей порцией данных из файла. Если данных больше нет, то функция вернёт false, а первый
	// символ массива data будет равен 0. Если данные были прочитаны, функция вернёт true. Если данные не были прочитаны
	// из файла, но параметр append == true и в массиве ещё остались данные, то функция вернёт true. Если параметр append
	// равен false, то функция удалит прежнее содержимое массива data (если он был не пуст), а если true, то дополнит.
	// Если при чтении файла произойдёт ошибка, то функция не будет добавлять данные в массив при следующих вызовах
	bool GetMoreData(bool append = false);

protected:
	static constexpr size_t DATA_BUFFER_SIZE = 1024;		// Размер внутреннего буфера (в символах)
	static constexpr size_t FILE_BUFFER_SIZE = 32 * 1024;	// Размер буфера файла (в байтах)

	// Проверяет BOM файла и выставляет значение m_Encoding
	// в случае, если маркер распознан или отсутствует
	void CheckBom();

	// Читает из файла данные до полного заполнения буфера или конца файла. Функция вернёт true, если
	// в буфере есть данные. Вернёт false, если произошла ошибка чтения файла либо данных больше нет
	bool ReadFileData();

	// Эти функции декодируют данные из буфера файла во внутренний буфер до полного заполнения внутреннего буфера или
	// конца данных в буфере файла. Функции возвращают указатель на символ, следующий за последним декодированным
	wchar_t* DecodeUtf8(wchar_t* out);
	wchar_t* DecodeUtf16(wchar_t* out);

	void SetError(std::wstring_view text);

protected:
	util::File& m_File;					// Связанный файл
	Encoding m_Encoding = Encoding();	// Кодировка файла
	std::wstring m_LastError;			// Сообщение об ошибке

	wchar_t* m_DataBuffer = nullptr;	// Буфер декодированных данных (размером DATA_BUFFER_SIZE символов)
	wchar_t* m_SwapBuffer = nullptr;	// Второй буфер декодированных данных для свапа с основным буфером
	uint8_t* m_FileBuffer = nullptr;	// Буфер данных файла (размером FILE_BUFFER_SIZE байт)
	size_t m_Remains = 0;				// Количество оставшихся байт в буфере файла
	size_t m_Pos = 0;					// Текущая позиция в буфере файла

	bool m_NoMoreData = false;			// true, если в файле больше нет данных
	bool m_ErrorFlag = false;			// true, если произошла ошибка
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   Классы строк для парсера XML
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
struct XmlStringView
{
	const wchar_t* data;	// Указатель на первый символ строки
	size_t size = 0;		// Количество символов в строке

public:
	XmlStringView() noexcept = default;
	constexpr XmlStringView(const wchar_t* first, size_t count) noexcept
		: data(first), size(count) {}

	constexpr operator std::wstring_view() const noexcept
	{
		return std::wstring_view(data, size);
	}

	// Сравнивает строки с учётом регистра
	constexpr bool operator ==(std::wstring_view rhs) const noexcept
	{
		return size == rhs.size() && !Traits::compare(data, rhs.data(), size);
	}

	// Сравнивает строки с учётом регистра
	constexpr bool operator !=(std::wstring_view rhs) const noexcept
	{
		return size != rhs.size() || Traits::compare(data, rhs.data(), size);
	}

protected:
	using Traits = std::char_traits<wchar_t>;
};

//--------------------------------------------------------------------------------------------------------------------------------
struct XmlBufferedView : public XmlStringView
{
	AML_NONCOPYABLE(XmlBufferedView)

public:
	XmlBufferedView() = default;

	constexpr void Reset() noexcept
	{
		size = 0;
		m_IsBuffered = false;
	}

	constexpr void Set(XmlStringView str) noexcept
	{
		data = str.data;
		size = str.size;
		m_IsBuffered = false;
	}

	constexpr void Set(const wchar_t* first, size_t count) noexcept
	{
		data = first;
		size = count;
		m_IsBuffered = false;
	}

	// Буферизирует строку (если она ещё не в буфере) и дополняет её строкой str (первыми count символами
	// строки first). Функция не проверяет корректность передаваемых ей параметров! После вызова этой функции
	// вью гарантированно будет null-терминированным (если значения data и size не модифицировались напрямую)
	void Append(XmlStringView str) { Append(str.data, str.size); }
	void Append(const wchar_t* first, size_t count);

	// Копирует строку data длины size во внутренний буфер (если строка ещё не в буфере) и обновляет
	// значение поля data. После вызова этой функции вью гарантированно будет null-терминированным
	// (если значения data и size не модифицировались напрямую)
	void Buffer();

protected:
	std::wstring m_Buffer;		// Внутренний буфер
	bool m_IsBuffered = false;	// true, если строка в буфере
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//   XmlReader - SAX (Simple API for XML) парсер XML
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//--------------------------------------------------------------------------------------------------------------------------------
class XmlReader
{
	AML_NONCOPYABLE(XmlReader)

public:
	// Пользовательское значение, которое будет передаваться
	// первым параметром при вызове любой колл-бэк функции
	void* userData = nullptr;

	// Прототип общей колл-бэк функции
	using CommonCb = void (*)(void* userData);
	// Прототип колл-бэк функции для начала и завершения элемента
	using TagCb = void (*)(void* userData, std::wstring_view name);
	// Прототип колл-бэк функции для атрибута элемента
	using AttrCb = void (*)(void* userData, std::wstring_view attr, std::wstring_view value);
	// Прототип колл-бэк функции для данных (текста вне элемента)
	using DataCb = void (*)(void* userData, const std::wstring_view text, bool firstPart);

	// Эти функции будут вызваны по 1 разу в начале и в конце парсинга документа
	CommonCb docOpenedCb = nullptr;
	CommonCb docClosedCb = nullptr;

	// Функция tagOpenedCb будет вызвана при открытии тега с его именем в параметре. Аналогично, функция
	// tagClosedCb будет вызвана при закрытии тега (если это пустой элемент, то его имя будет пустым)
	TagCb tagOpenedCb = nullptr;
	TagCb tagClosedCb = nullptr;

	// Эта функция будет вызвана при обнаружении атрибута
	AttrCb attrCb = nullptr;

	// Эта функция будет вызвана при обнаружении данных (текста вне элемента). Если данных много, то она будет
	// вызвана несколько раз: для первого фрагмента параметр firstPart будет равен true, для остальных - false
	DataCb dataCb = nullptr;

public:
	XmlReader();
	~XmlReader();

	// Парсит документ из указанного файла
	bool Parse(util::WZStringView filePath);
	// Парсит документ из файла file
	bool Parse(util::File& file);

	// Вызов этой функции из любого колл-бэка прервёт парсинг (функция Parse вернёт false)
	void StopParsing() { m_ShouldStop = true; }

	// Возвращает ссылку на строку с сообщением об ошибке (актуально после
	// вызова функции Parse, если она завершилась ошибкой и вернула false)
	const std::wstring& GetLastError() const { return m_LastError; }

protected:
	static constexpr size_t STOP_TAB_SIZE = 64;
	using StopTab = uint8_t[STOP_TAB_SIZE];

	enum {
		TRIM_LEFT	= 0x01,
		TRIM_RIGHT	= 0x02,
		TRIM_ALL	= TRIM_LEFT | TRIM_RIGHT
	};

	struct DataInfo {
		XmlStringView text;		// Текущий (последний) выделенный фрагмент данных
		XmlBufferedView prev;	// Предыдущий (накопленный) фрагмент данных
		bool firstPart = true;	// true, если это самый первый фрагмент
	};

	bool ParseDocument(XmlData& data);
	bool ParseElement(XmlData& data);
	bool ParseAttr(XmlData& data);

	void OnMoreData(DataInfo& info);
	void OnDataReady(DataInfo& info);
	void OnTagClosed(XmlStringView name);

	void Invoke(CommonCb cb);
	void Invoke(TagCb cb, XmlStringView name);
	void InvokeDataCb(XmlStringView text, bool firstPart);

	static void Trim(XmlStringView& text, int options);
	bool ProcessAttr(XmlData& data);

	void InitStopTab(int idx, const char* stops);
	static size_t GetNextTokenLen(const wchar_t* first, const StopTab stops);
	void GetNextToken(XmlBufferedView& out, XmlData& data, int stopTabIdx);
	bool GetQuotedAttrValue(XmlData& data);

	static void SkipWhitespaces(XmlData& data, XmlBufferedView* safeBuffered = nullptr);
	static void SkipWhitespacesImpl(XmlData& data, XmlBufferedView* safeBuffered);
	static bool SkipComment(XmlData& data);

	void SetError(std::wstring_view text);
	bool CheckData(XmlData& data);

protected:
	std::wstring m_LastError;		// Сообщение об ошибке (если Parse вернула false)
	XmlBufferedView m_AttrName;		// Временный буфер для хранения имён атрибутов
	XmlBufferedView m_TextString;	// Временный буфер для функции GetNextToken
	StopTab* m_StopTabs = nullptr;	// Таблицы разделителей токенов для парсинга
	bool m_IsParsingProlog = false;	// true, если обрабатывается элемент пролога
	bool m_HasParsedProlog = false;	// true, если пролог обработан (тег закрыт)
	bool m_ShouldStop = false;		// true, если парсинг должен быть прерван
};

} // namespace aux
