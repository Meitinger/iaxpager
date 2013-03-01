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

#ifndef _SETTINGS_H
#define _SETTINGS_H

/* all service parameters */
typedef struct tagSETTINGS
{
	/* the audio ouput device and volume to use */
	UINT WaveOutDevID;
	INT Volume;

	/* first allowed and forbidden host */
	LPHOST AllowedHosts;
	LPHOST ForbiddenHosts;

	/* host, user name and password */
	BOOL Register;
	CHAR Host[MAXSTRLEN];
	CHAR UserName[MAXSTRLEN];
	CHAR Secret[MAXSTRLEN];

	/* preferred iax-client port */
	USHORT Port;

	/* file name of the ring tone and loop flag */
	LPTSTR RingTone;
	BOOL PlayLoop;
} SETTINGS, *LPSETTINGS;

/* function to parse all command arguments */
extern LPSETTINGS ParseSettings(DWORD, LPTSTR []);

/* release all host memory */
extern VOID FreeSettings(LPSETTINGS);

#endif
