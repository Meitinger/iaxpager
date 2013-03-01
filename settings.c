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

#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include "libiax2/iax-client.h"
#include "common.h"
#include "host.h"
#include "settings.h"

/* function to parse all command arguments */
LPSETTINGS ParseSettings(DWORD argc, LPTSTR argv[])
{
#define CHECK(condition) { if (!(condition)) goto ON_ERROR; }
#define CHECKREGSTR(str) { CHECK(argv[i][0]!=_T('\0')); tcstombs((str),argv[i],MAXSTRLEN); (str)[MAXSTRLEN-1]='\0'; settings->Register = TRUE; }

	TCHAR lastFlag = _T('\0');
	LPSETTINGS settings;
	DWORD i;

	/* create and initialize the structure */
	ALLOC(settings);
	settings->WaveOutDevID = WAVE_MAPPER;
	settings->Volume = -1;
	settings->AllowedHosts = NULL;
	settings->ForbiddenHosts = NULL;
	settings->Port = IAX_DEFAULT_PORTNO;
	settings->RingTone = NULL;
	settings->PlayLoop = FALSE;

	/* parse the given arguments */
	for (i = 1; i < argc; i++)
	{
		/* check if we're expecting a flag */
		if (lastFlag == _T('\0'))
		{
			/* check whether the format is appropriate */
			if (_tcslen(argv[i]) != 2 || (argv[i][0] != _T('-') && argv[i][0] != _T('/')))
				goto ON_ERROR;

			/* check flag arguments */
			switch (argv[i][1])
			{
				/* loop flag (only used when a ringtone is specified) */
				case _T('l'):
					settings->PlayLoop = TRUE;
					break;

				/* store the flag for the next iteration */
				default:
					lastFlag = argv[i][1];
					break;
			}
		}

		/* no flag, we expect a parameter */
		else
		{
			/* check which parameter */
			switch (lastFlag)
			{
				/* soundcard to use */
				case _T('c'):
					CHECK(_stscanf(argv[i], _T("%u"), &settings->WaveOutDevID) == 1);
					break;

				/* volume */
				case _T('v'):
					CHECK(_stscanf(argv[i], _T("%d"), &settings->Volume) == 1 && 0 <= settings->Volume && settings->Volume <= 0xFFFF);
					break;

				/* allowed address */
				case _T('a'):
					CHECK(AppendHost(argv[i], &settings->AllowedHosts));
					break;

				/* forbidden address */
				case _T('f'):
					CHECK(AppendHost(argv[i], &settings->ForbiddenHosts));
					break;

				/* iax port number */
				case _T('p'):
					CHECK(_stscanf(argv[i], _T("%hu"), &settings->Port) == 1);
					break;

				/* ring tone file name */
				case _T('r'):
					CHECK((settings->RingTone = argv[i])[0] != _T('\0'));
					break;

				/* account host */
				case _T('h'):
					CHECKREGSTR(settings->Host);
					break;

				/* account user name */
				case _T('u'):
					CHECKREGSTR(settings->UserName);
					break;

				/* account secret */
				case _T('s'):
					CHECKREGSTR(settings->Secret);
					break;

				/* illegal flag */
				default:
					goto ON_ERROR;
			}

			/* reset to flag mode */
			lastFlag = _T('\0');
		}
	}

	/* return the settings if no argument is missing */
	if
	(
		lastFlag == _T('\0') &&
		(settings->RingTone != NULL || !settings->PlayLoop) &&
		settings->Register == (settings->Host[0] != '\0') &&
		settings->Register == (settings->UserName[0] != '\0') &&
		settings->Register == (settings->Secret[0] != '\0')
	)
		return settings;

ON_ERROR:
	/* free the memory and return NULL */
	FreeSettings(settings);
	return NULL;

#undef CHECKREGSTR
#undef CHECK
}

/* release all host memory */
VOID FreeSettings(LPSETTINGS settings)
{
	RemoveAllHosts(&settings->AllowedHosts);
	RemoveAllHosts(&settings->ForbiddenHosts);
	FREE(settings);
}
