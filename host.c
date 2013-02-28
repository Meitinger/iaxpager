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

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include "common.h"
#include "host.h"

/* structure for storing allowed/forbidden hosts */
struct tagHOST
{
	ULONG Address;
	ULONG Network;
	LPHOST Next;
};

/* checks if a given address is within a host list */
BOOL ContainsHost(ULONG address, LPHOST hosts)
{
	while (hosts != NULL)
	{
		if ((address & hosts->Network) == hosts->Address)
			return TRUE;
		hosts = hosts->Next;
	}
	return FALSE;
}

/* function to parse an allowed/forbidden host */
BOOL AppendHost(LPTSTR subnet, LPHOST *hosts)
{
	INT b1, b2, b3, b4, net;
	LPHOST host;

	if (_stscanf(subnet, _T("%d.%d.%d.%d/%d"), &b1, &b2, &b3, &b4, &net) != 5 || b1 < 0 || b1 > 0xFF || b2 < 0 || b2 > 0xFF || b3 < 0 || b3 > 0xFF || b4 < 0 || b4 > 0xFF || net < 0 || net > 32)
		return FALSE;
	ALLOC(host);
	host->Address = htonl((b1 << 24) | (b2 << 16) | (b3 << 8) | b4);
	host->Network = htonl(0xFFFFFFFF << (32 - net));
	host->Next = *hosts;
	*hosts = host;
	return TRUE;
}

/* release all host memory */
VOID RemoveAllHosts(LPHOST *hosts)
{
	LPHOST host;

	while (*hosts != NULL)
	{
		host = (*hosts)->Next;
		FREE(*hosts);
		*hosts = host;
	}
}
