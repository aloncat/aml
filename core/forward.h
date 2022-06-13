﻿//∙AML
// Copyright (C) 2016-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

// В forward.h не добавляются классы синглтонов, так как это не имеет смысла: не должно быть ситуации, когда нам нужно
// хранить указатель или ссылку на синглтон в переменной (члене данных), объявленной в заголовочном файле. Также сюда
// не добавляются никакие шаблонные классы во избежание потенциальных проблем со значениями по умолчанию параметров
// этих шаблонов. Не следует добавлять сюда и те классы, которые предназначены только для внутреннего пользования

namespace math {
	class RandGen;
}

namespace thrd {
	class CriticalSection;
}

namespace util {
	// Классы исключений
	class EAssertion;
	class EGeneric;
	class EHalt;
	class ELogic;
	class ERuntime;
	// Классы файлов
	class BinaryFile;
	class File;
	class MemoryFile;
	// Журналы
	class FileLog;
	class Log;
	class Logable;
	class LogRecord;
	// Разное
	class AssertHandler;
	class Console;
	class FuncToggle;
	class VirtualKey;
	struct DateTime;
}
