//∙AML
// Copyright (C) 2020-2022 Dmitry Maslov
// For conditions of distribution and use, see readme.txt

#include "pch.h"
#include "vkey.h"

#include "debug.h"
#include "util.h"
#include "winapi.h"

using namespace util;

//--------------------------------------------------------------------------------------------------------------------------------
void VirtualKey::InitKeyTTWin(uint8_t* table, size_t size)
{
	Assert(table && size >= 256 && "Insufficient table size");
	AML_FILLA(table, 0, size);

	#if AML_OS_WINDOWS
		for (int i = 0; i < 10; ++i)
			table['0' + i] = (KEY_0 + i) & 0xff;
		for (int i = 0; i < 26; ++i)
			table['A' + i] = (KEY_A + i) & 0xff;
		for (int i = 0; i < 12; ++i)
			table[VK_F1 + i] = (KEY_F1 + i) & 0xff;

		table[VK_MENU] = KEY_ALT;
		table[VK_CONTROL] = KEY_CTRL;
		table[VK_SHIFT] = KEY_SHIFT;

		table[VK_NUMLOCK] = KEY_NUM;
		table[VK_CAPITAL] = KEY_CAPS;
		table[VK_SCROLL] = KEY_SCROLL;

		table[VK_SNAPSHOT] = KEY_PRINT;
		table[VK_PAUSE] = KEY_PAUSE;

		table[VK_ESCAPE] = KEY_ESC;
		table[VK_TAB] = KEY_TAB;
		table[VK_BACK] = KEY_BACK;
		table[VK_RETURN] = KEY_ENTER;
		table[VK_SPACE] = KEY_SPACE;

		table[VK_INSERT] = KEY_INSERT;
		table[VK_DELETE] = KEY_DELETE;
		table[VK_HOME] = KEY_HOME;
		table[VK_END] = KEY_END;
		table[VK_PRIOR] = KEY_PAGEUP;
		table[VK_NEXT] = KEY_PAGEDN;

		table[VK_LEFT] = KEY_LEFT;
		table[VK_UP] = KEY_UP;
		table[VK_RIGHT] = KEY_RIGHT;
		table[VK_DOWN] = KEY_DOWN;

		table[VK_OEM_3] = KEY_BSPARK;
		table[VK_OEM_MINUS] = KEY_MINUS;
		table[VK_OEM_PLUS] = KEY_EQUAL;
		table[VK_OEM_5] = KEY_BSLASH;
		table[VK_OEM_4] = KEY_LBRKT;
		table[VK_OEM_6] = KEY_RBRKT;
		table[VK_OEM_1] = KEY_COLON;
		table[VK_OEM_7] = KEY_QUOTE;
		table[VK_OEM_COMMA] = KEY_COMMA;
		table[VK_OEM_PERIOD] = KEY_PERIOD;
		table[VK_OEM_2] = KEY_SLASH;

		table[VK_DIVIDE] = KEY_PADDIV;
		table[VK_MULTIPLY] = KEY_PADMUL;
		table[VK_SUBTRACT] = KEY_PADSUB;
		table[VK_ADD] = KEY_PADADD;
		table[VK_NUMPAD0] = KEY_PAD0;
		table[VK_NUMPAD1] = KEY_PAD1;
		table[VK_NUMPAD2] = KEY_PAD2;
		table[VK_NUMPAD3] = KEY_PAD3;
		table[VK_NUMPAD4] = KEY_PAD4;
		table[VK_NUMPAD5] = KEY_PAD5ON;
		table[VK_CLEAR] = KEY_PAD5OFF;
		table[VK_NUMPAD6] = KEY_PAD6;
		table[VK_NUMPAD7] = KEY_PAD7;
		table[VK_NUMPAD8] = KEY_PAD8;
		table[VK_NUMPAD9] = KEY_PAD9;
		table[VK_DECIMAL] = KEY_PADDEC;
	#endif
}
