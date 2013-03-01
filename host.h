/*
 * IAX-Pager -- Turns your Windows Machine into a Phone-Speaker
 *
 * Copyright (C) 2008-2013, Manuel Meitinger
 *
 * Manuel Meitinger <m.meitinger@aufbauwerk.com>
 *
 * This program is free software, distributed under the terms of
 * the GNU General Public License Version 2. See the LICENSE file
 * at the top of the source tree.
 */

#ifndef _HOST_H
#define _HOST_H

/* transparent host structure */
typedef struct tagHOST HOST, *LPHOST;

/* checks if a given address is within a host list */
extern BOOL ContainsHost(ULONG, LPHOST);

/* function to parse an allowed/forbidden host */
extern BOOL AppendHost(LPTSTR, LPHOST *);

/* release all host memory */
extern VOID RemoveAllHosts(LPHOST *);

#endif
