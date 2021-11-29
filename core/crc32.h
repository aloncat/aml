//∙AML
// Copyright (C) 2016-2021 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "platform.h"

namespace hash {

// Вычисляет контрольную сумму CRC32 блока данных data размером size
// байт. Параметр prevHash используется для инкрементного вычисления хеша
uint32_t GetCRC32(const void* data, size_t size, uint32_t prevHash = 0) noexcept;

} // namespace hash
