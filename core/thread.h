//∙AML
// Copyright (C) 2017-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

namespace thrd {

// Возвращает идентификатор потока, в контексте которого вызвана эта функция. Гарантируется, что
// пока этот поток не завершился, возвращённое значение уникально в пределах операционной системы
unsigned GetThreadId();

// Выполняет инструкцию pause на CPU Intel или аналогичную на других процессорах. Используется внутри
// циклов ожидания для увеличения общей производительности системы и уменьшения её энергопотребления
void CPUPause();

// Прерывает выполнение текущего потока на milliseconds мс и передаёт управление системе. Реальное время,
// на которое будет приостановлен поток, может отличаться от запрошенного в меньшую или большую сторону.
// Если milliseconds равен 0, то остаток кванта времени будет передан другому потоку, ожидающему своей
// очереди на выполнение. Если таких потоков в данный момент нет, функция немедленно вернёт управление
void Sleep(unsigned milliseconds);

} // namespace thrd
