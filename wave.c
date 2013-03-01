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
#include "libiax2/iax-client.h"
#include "common.h"
#include "host.h"
#include "settings.h"
#include "wave.h"

__declspec(thread) DWORD lastError = ERROR_SUCCESS;

struct tagWAVE
{
	LPSETTINGS Settings;
	WSAEVENT Event;
	LPHEADERDONEPROC Callback;
	HWAVEOUT Device;
	HANDLE File;
	HANDLE Mapping;
	LPVOID Data;
	DWORD StartOffset;
	DWORD EndOffset;
	DWORD NextBlockOffset;
	DWORD BlockSize;
	BOOL NoAvailableHeaders;
	USHORT FirstPreparedHeader;
	USHORT NextAvailableHeader;
	BOOL HasLastVolume;
	DWORD LastVolume;
	WAVEHDR Headers[WAVE_BUFFERS];
};

/* free all done headers starting from the first */
static BOOL InternalHandleDoneWaveHeaders(LPWAVE wave)
{
	while ((wave->NoAvailableHeaders || wave->FirstPreparedHeader != wave->NextAvailableHeader) && (wave->Headers[wave->FirstPreparedHeader].dwFlags & WHDR_DONE) == WHDR_DONE)
	{
		lastError = waveOutUnprepareHeader(wave->Device, &wave->Headers[wave->FirstPreparedHeader], sizeof(WAVEHDR));
		if (lastError != MMSYSERR_NOERROR)
			return !wave->NoAvailableHeaders;
		wave->NoAvailableHeaders = FALSE;
		wave->FirstPreparedHeader = (wave->FirstPreparedHeader + 1) % WAVE_BUFFERS;
		if (wave->Headers[wave->FirstPreparedHeader].dwUser != 0)
			wave->Callback((LPVOID)wave->Headers[wave->FirstPreparedHeader].dwUser);
	}
	return TRUE;
}

/* play the next ring tone data */
static BOOL PlayData(LPWAVE wave)
{
	LPVOID buffer;
	DWORD size;

	/* only play something if possible */
	while (!wave->NoAvailableHeaders)
	{
		/* rewind the playback if the settings demand a loop */
		if (wave->NextBlockOffset >= wave->EndOffset)
		{
			if (!wave->Settings->PlayLoop)
				break;
			wave->NextBlockOffset = wave->StartOffset;
		}

		/* determine buffer and size */
		buffer = (LPBYTE)wave->Data + wave->NextBlockOffset;
		size = wave->BlockSize;
		wave->NextBlockOffset += size;
		if (wave->NextBlockOffset > wave->EndOffset)
			size -= wave->NextBlockOffset - wave->EndOffset;

		/* play the buffer */
		if (!EnqueueWaveHeader(wave, buffer, size, NULL))
			return FALSE;
	}
	return TRUE;
}

/* initialize the wave audio device */
LPWAVE InitializeWave(LPSETTINGS settings, WSAEVENT event, LPHEADERDONEPROC callback)
{
	CONST FOURCC riffID = mmioFOURCC('R','I','F','F');
	CONST FOURCC waveID = mmioFOURCC('W','A','V','E');
	WAVEFORMATEX slinFormat = {WAVE_FORMAT_PCM, 1, 8000, 16000, 2, 16, 0};
	LPWAVE wave;
	DWORD fileSize;
	FOURCC chunk;
	DWORD chunkSize;
	LPWAVEFORMATEX format;
	DWORD offset;

	/* create and initialize the wave structure */
	ALLOC(wave);
	wave->Settings = settings;
	wave->Event = event;
	wave->Callback = callback;
	wave->File = INVALID_HANDLE_VALUE;

	/* either lookup the wave format in the ringtone file or use slin */
	if (wave->Settings->RingTone != NULL)
	{
		/* open and map the file to memory */
		if
		(
			(wave->File = CreateFile(wave->Settings->RingTone, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE ||
			(fileSize = GetFileSize(wave->File, &offset)) == INVALID_FILE_SIZE ||
			(wave->Mapping = CreateFileMapping(wave->File, NULL, PAGE_READONLY, 0, 0, NULL)) == NULL ||
			(wave->Data = MapViewOfFile(wave->Mapping, FILE_MAP_READ, 0, 0, 0)) == NULL
		)
		{
			lastError = GetLastError();
			goto ON_ERROR;
		}

		/* check the file format and find the format and data offset */
#define OFFSET ((LPBYTE)wave->Data+offset)
#define CHECK(what) if ((offset+sizeof(what))>fileSize || memcmp(OFFSET,&what,sizeof(what))!=0) goto ON_ERROR; offset += sizeof(what)
#define READ(what) if ((offset+sizeof(what))>fileSize) goto ON_ERROR; memcpy(&what,OFFSET,sizeof(what)); offset += sizeof(what)
		lastError = ERROR_INVALID_DATA;
		if (offset > 0)
			/* large files are not supported */
			goto ON_ERROR;
		CHECK(riffID);
		READ(chunkSize);
		if ((chunkSize + 8) > fileSize)
			goto ON_ERROR;
		fileSize = chunkSize + 8;
		CHECK(waveID);
		while (offset < fileSize)
		{
			READ(chunk);
			READ(chunkSize);
			switch (chunk)
			{
				case mmioFOURCC('f','m','t',' '):
					if (chunkSize < sizeof(WAVEFORMAT))
						goto ON_ERROR;
					format = (LPWAVEFORMATEX)OFFSET;
					wave->BlockSize = (DWORD)(WAVE_SECS_PER_BUFFER * format->nAvgBytesPerSec);
					wave->BlockSize -= wave->BlockSize % format->nBlockAlign;
					if (wave->BlockSize == 0)
						wave->BlockSize = format->nBlockAlign;
					break;
				case mmioFOURCC('d','a','t','a'):
					wave->StartOffset = offset;
					wave->EndOffset = offset + chunkSize;
					break;
			}
			offset += chunkSize;
		}
		if (offset > fileSize)
			goto ON_ERROR;
		lastError = ERROR_SUCCESS;
#undef READ
#undef CHECK
#undef OFFSET
	}
	else
		format = &slinFormat;

	/* open the wave device and return the handle */
	if ((lastError = waveOutOpen(&wave->Device, wave->Settings->WaveOutDevID, format, (DWORD_PTR)wave->Event, 0, CALLBACK_EVENT)) != MMSYSERR_NOERROR)
		goto ON_ERROR;
	return wave;

ON_ERROR:
	/* free the wave structure and return NULL on error */
	FREE(wave);
	return NULL;
}

/* stop and release the wave audio device */
VOID FreeWave(LPWAVE wave)
{
	if (wave->Device != NULL)
	{
		StopWave(wave);
		waveOutClose(wave->Device);
	}
	if (wave->Data != NULL)
		UnmapViewOfFile(wave->Data);
	if (wave->Mapping != NULL)
		CloseHandle(wave->Mapping);
	if (wave->File != INVALID_HANDLE_VALUE)
		CloseHandle(wave->File);
	FREE(wave);
}

/* return the last error code */
DWORD GetLastWaveError()
{
	return lastError;
}

/* start audio playback */
BOOL StartWave(LPWAVE wave)
{
	/* set the volume */
	if (wave->Settings->Volume > -1 && !wave->HasLastVolume)
	{
		if (waveOutGetVolume(wave->Device, &wave->LastVolume) == MMSYSERR_NOERROR)
			wave->HasLastVolume = waveOutSetVolume(wave->Device, MAKELONG((WORD)wave->Settings->Volume,(WORD)wave->Settings->Volume)) == MMSYSERR_NOERROR;
	}

	/* just exit if no ringtone is loaded */
	if (wave->Data == NULL)
		return TRUE;

	/* start the ringtone playback */
	wave->NextBlockOffset = wave->StartOffset;
	return PlayData(wave);
}

/* stop audio playback */
BOOL StopWave(LPWAVE wave)
{
	/* reset the volume */
	if (wave->Settings->Volume > -1 && wave->HasLastVolume)
		wave->HasLastVolume = waveOutSetVolume(wave->Device, wave->LastVolume) != MMSYSERR_NOERROR;

	/* stop any pending playback */
	lastError = waveOutReset(wave->Device);
	if (lastError != MMSYSERR_NOERROR)
		return FALSE;

	/* reset the ringtone (if present) and free the headers */
	if (wave->Data != NULL)
		wave->NextBlockOffset = 0;
	return InternalHandleDoneWaveHeaders(wave);
}

/* reset the audio event, free the done headers and possible continue the ringtone playback */
BOOL HandleDoneWaveHeaders(LPWAVE wave)
{
	/* reset the event */
	if (!WSAResetEvent(wave->Event))
	{
		lastError = WSAGetLastError();
		return FALSE;
	}

	/* free done headers */
	if (!InternalHandleDoneWaveHeaders(wave))
		return FALSE;

	/* either return or continue ringtone playback */
	return (wave->Data == NULL || wave->NextBlockOffset == 0) ? TRUE : PlayData(wave);
}

/* enqueue another block of audio for playback */
BOOL EnqueueWaveHeader(LPWAVE wave, LPVOID buffer, DWORD size, LPVOID userData)
{
	/* if we don't have any free buffers we have to skip this header */
	if (wave->NoAvailableHeaders)
		return TRUE;

	/* set the header structure */
	memset(&wave->Headers[wave->NextAvailableHeader], 0, sizeof(WAVEHDR));
	wave->Headers[wave->NextAvailableHeader].lpData = (LPSTR)buffer;
	wave->Headers[wave->NextAvailableHeader].dwBufferLength = size;
	wave->Headers[wave->NextAvailableHeader].dwUser = (DWORD_PTR)userData;

	/* prepare the header */
	lastError = waveOutPrepareHeader(wave->Device, &wave->Headers[wave->NextAvailableHeader], sizeof(WAVEHDR));
	if (lastError != MMSYSERR_NOERROR)
		return FALSE;

	/* send the header to the device */
	lastError = waveOutWrite(wave->Device, &wave->Headers[wave->NextAvailableHeader], sizeof(WAVEHDR));
	if (lastError != MMSYSERR_NOERROR)
	{
		waveOutUnprepareHeader(wave->Device, &wave->Headers[wave->NextAvailableHeader], sizeof(WAVEHDR));
		return FALSE;
	}

	/* increment the prepared counter */
	wave->NextAvailableHeader = (wave->NextAvailableHeader + 1) % WAVE_BUFFERS;
	wave->NoAvailableHeaders = wave->NextAvailableHeader == wave->FirstPreparedHeader;
	return TRUE;
}
