﻿//∙Auxlib
// Copyright (C) 2019-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

// В forward.h не добавляются классы синглтонов, так как это не имеет смысла: не должно быть ситуации, когда нам нужно
// хранить указатель или ссылку на синглтон в переменной (члене данных), объявленной в заголовочном файле. Также сюда не
// добавляются никакие шаблонные классы во избежание потенциальных проблем со значениями по умолчанию их параметров

namespace aux {
	class XmlWriter;
}