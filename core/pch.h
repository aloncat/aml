//∙AML
// Copyright (C) 2016-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

// NB: platform.h обязательно должен присутствовать в pch.h, так как этот заголовок необходим во многих .cpp файлах, но он
// не подключается в них явно. Это связано с исповедуемым принципом не подключать в .cpp того, что уже подключается в pch.h и
// заголовке той же единицы трансляции. В заголовочных файлах мы, напротив, всегда подключаем всё, что необходимо (на случай,
// если этот заголовок будет использован, например, из другого проекта, где вообще может не быть precompiled headers)
#include "platform.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <algorithm>
#include <atomic>
#include <deque>
#include <exception>
#include <functional>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
