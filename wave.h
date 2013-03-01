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

#ifndef _WAVE_H
#define _WAVE_H

/* number of wave buffers and suggested write block size */
#define WAVE_BUFFERS 100
#define WAVE_SECS_PER_BUFFER 0.015

/* transparent wave structure */
typedef struct tagWAVE WAVE, *LPWAVE;

/* callback for done header data */
typedef VOID (CALLBACK *LPHEADERDONEPROC)(LPVOID);

/* initialize the wave audio device */
extern LPWAVE InitializeWave(LPSETTINGS, WSAEVENT, LPHEADERDONEPROC);

/* stop and release the wave audio device */
extern VOID FreeWave(LPWAVE);

/* return the last error code */
extern DWORD GetLastWaveError();

/* start audio playback */
extern BOOL StartWave(LPWAVE);

/* stop audio playback */
extern BOOL StopWave(LPWAVE);

/* reset the audio event, free the done headers and possible continue the ring tone playback */
extern BOOL HandleDoneWaveHeaders(LPWAVE);

/* enqueue another block of audio for playback */
extern BOOL EnqueueWaveHeader(LPWAVE, LPVOID, DWORD, LPVOID);

#endif
