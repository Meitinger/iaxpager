/*
 * IAX-Pager -- Turns your Windows Machine into a Phone-Speaker
 *
 * Copyright (C) 2008-2011, Manuel Meitinger
 *
 * Manuel Meitinger <m.meitinger@aufbauwerk.com>
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

#ifndef _COMMON_H
#define _COMMON_H

/* basic macros */
#define ALLOC(pointer) (pointer)=HeapAlloc(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS|HEAP_ZERO_MEMORY,sizeof(*(pointer)))
#define FREE(pointer) HeapFree(GetProcessHeap(),HEAP_GENERATE_EXCEPTIONS,(pointer))
#define ZERO(pointer) ZeroMemory((pointer),sizeof(*(pointer)))

/* some additions */
#ifdef _UNICODE
#define tcstombs wcstombs
#else
#define tcstombs strncpy
#endif

#endif
