//∙AML
// Copyright (C) 2020-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#pragma once

#include "platform.h"

namespace util {

//--------------------------------------------------------------------------------------------------------------------------------
class VirtualKey
{
public:
	// Коды виртуальных клавиш
	enum {
		KEY_LMB		= 0x01,		// Левая кнопка мыши
		KEY_RMB		= 0x02,		// Правая кнопка мыши
		KEY_MMB		= 0x03,		// Средняя кнопка мыши

		KEY_ALT		= 0x08,		// Любая из клавиш Alt
		KEY_CTRL	= 0x09,		// Любая из клавиш Ctrl
		KEY_SHIFT	= 0x0a,		// Любая из клавиш Shift

		KEY_LALT	= 0x0c,		// Левый Alt (только для состояний)
		KEY_RALT	= 0x0d,		// Правый Alt (только для состояний)
		KEY_LCTRL	= 0x0e,		// Левый Ctrl (только для состояний)
		KEY_RCTRL	= 0x0f,		// Правый Ctrl (только для состояний)
		KEY_LSHIFT	= 0x10,		// Левый Shift (только для состояний)
		KEY_RSHIFT	= 0x11,		// Правый Shift (только для состояний)

		KEY_NUM		= 0x14,		// Num Lock
		KEY_CAPS	= 0x15,		// Caps Lock
		KEY_SCROLL	= 0x16,		// Scroll Lock

		KEY_PRINT	= 0x18,		// Print Screen (SysRq)
		KEY_PAUSE	= 0x19,		// Pause (Break)

		KEY_ESC		= 0x1c,		// Esc
		KEY_TAB		= 0x1d,		// Tab
		KEY_BACK	= 0x1e,		// Backspace
		KEY_ENTER	= 0x1f,		// Enter (основная и цифровая клавиатуры)
		KEY_SPACE	= 0x20,		// Пробел

		KEY_INSERT	= 0x24,		// Insert (ср. секция и цифр. клавиатура при выкл. NumLock)
		KEY_DELETE	= 0x25,		// Delete (ср. секция и цифр. клавиатура при выкл. NumLock)
		KEY_HOME	= 0x26,		// Home (ср. секция и цифр. клавиатура при выкл. NumLock)
		KEY_END		= 0x27,		// End (ср. секция и цифр. клавиатура при выкл. NumLock)
		KEY_PAGEUP	= 0x28,		// Page Up (ср. секция и цифр. клавиатура при выкл. NumLock)
		KEY_PAGEDN	= 0x29,		// Page Down (ср. секция и цифр. клавиатура при выкл. NumLock)

		KEY_LEFT	= 0x2a,		// Влево (ср. секция и цифр. клавиатура при выкл. NumLock)
		KEY_UP		= 0x2b,		// Вверх (ср. секция и цифр. клавиатура при выкл. NumLock)
		KEY_RIGHT	= 0x2c,		// Вправо (ср. секция и цифр. клавиатура при выкл. NumLock)
		KEY_DOWN	= 0x2d,		// Вниз (ср. секция и цифр. клавиатура при выкл. NumLock)

		KEY_0		= 0x30,		// Цифровые клавиши 0-9
		KEY_1		= 0x31,
		KEY_2		= 0x32,
		KEY_3		= 0x33,
		KEY_4		= 0x34,
		KEY_5		= 0x35,
		KEY_6		= 0x36,
		KEY_7		= 0x37,
		KEY_8		= 0x38,
		KEY_9		= 0x39,

		KEY_A		= 0x41,		// Буквенные клавиши A-Z
		KEY_B		= 0x42,
		KEY_C		= 0x43,
		KEY_D		= 0x44,
		KEY_E		= 0x45,
		KEY_F		= 0x46,
		KEY_G		= 0x47,
		KEY_H		= 0x48,
		KEY_I		= 0x49,
		KEY_J		= 0x4a,
		KEY_K		= 0x4b,
		KEY_L		= 0x4c,
		KEY_M		= 0x4d,
		KEY_N		= 0x4e,
		KEY_O		= 0x4f,
		KEY_P		= 0x50,
		KEY_Q		= 0x51,
		KEY_R		= 0x52,
		KEY_S		= 0x53,
		KEY_T		= 0x54,
		KEY_U		= 0x55,
		KEY_V		= 0x56,
		KEY_W		= 0x57,
		KEY_X		= 0x58,
		KEY_Y		= 0x59,
		KEY_Z		= 0x5a,

		KEY_BSPARK	= 0x60,		// Клавиша `~
		KEY_MINUS	= 0x61,		// Клавиша -_
		KEY_EQUAL	= 0x62,		// Клавиша =+
		KEY_BSLASH	= 0x63,		// Клавиша \|
		KEY_LBRKT	= 0x64,		// Клавиша [{
		KEY_RBRKT	= 0x65,		// Клавиша ]}
		KEY_COLON	= 0x66,		// Клавиша ;:
		KEY_QUOTE	= 0x67,		// Клавиша '"
		KEY_COMMA	= 0x68,		// Клавиша ,<
		KEY_PERIOD	= 0x69,		// Клавиша .>
		KEY_SLASH	= 0x6a,		// Клавиша /?

		KEY_F1		= 0x70,		// Клавиши F1-F12
		KEY_F2		= 0x71,
		KEY_F3		= 0x72,
		KEY_F4		= 0x73,
		KEY_F5		= 0x74,
		KEY_F6		= 0x75,
		KEY_F7		= 0x76,
		KEY_F8		= 0x77,
		KEY_F9		= 0x78,
		KEY_F10		= 0x79,
		KEY_F11		= 0x7a,
		KEY_F12		= 0x7b,

		KEY_PADDIV	= 0x80,		// NumPad: клавиша /
		KEY_PADMUL	= 0x81,		// NumPad: клавиша *
		KEY_PADSUB	= 0x82,		// NumPad: клавиша -
		KEY_PADADD	= 0x83,		// NumPad: клавиша +
		KEY_PAD0	= 0x84,		// NumPad: клавиша 0 (при NumLock On, иначе -> KEY_INSERT)
		KEY_PAD1	= 0x85,		// NumPad: клавиша 1 (при NumLock On, иначе -> KEY_END)
		KEY_PAD2	= 0x86,		// NumPad: клавиша 2 (при NumLock On, иначе -> KEY_DOWN)
		KEY_PAD3	= 0x87,		// NumPad: клавиша 3 (при NumLock On, иначе -> KEY_PAGEDN)
		KEY_PAD4	= 0x88,		// NumPad: клавиша 4 (при NumLock On, иначе -> KEY_LEFT)
		KEY_PAD5ON	= 0x89,		// NumPad: клавиша 5 (при NumLock On)
		KEY_PAD5OFF	= 0x8a,		// NumPad: клавиша 5 (при NumLock Off)
		KEY_PAD6	= 0x8b,		// NumPad: клавиша 6 (при NumLock On, иначе -> KEY_RIGHT)
		KEY_PAD7	= 0x8c,		// NumPad: клавиша 7 (при NumLock On, иначе -> KEY_HOME)
		KEY_PAD8	= 0x8d,		// NumPad: клавиша 8 (при NumLock On, иначе -> KEY_UP)
		KEY_PAD9	= 0x8e,		// NumPad: клавиша 9 (при NumLock On, иначе -> KEY_PAGEUP)
		KEY_PADDEC	= 0x8f		// NumPad: клавиша . (при NumLock On, иначе -> KEY_DELETE)
	};

	// Флаги состояния клавиш ACS
	enum {
		ALT_DOWN	= 0x1000,	// Этот бит установлен, если любая из клавиш Alt нажата
		CTRL_DOWN	= 0x2000,	// Этот бит установлен, если любая из клавиш Ctrl нажата
		SHIFT_DOWN	= 0x4000	// Этот бит установлен, если любая из клавиш Shift нажата
	};

protected:
	// Инициализирует таблицу трансляции кодов виртуальных клавиш (ОС Windows)
	static void InitKeyTTWin(uint8_t* table, size_t size);
};

} // namespace util
