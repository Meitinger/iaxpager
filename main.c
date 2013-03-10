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

#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include "libiax2/iax-client.h"
#include "common.h"
#include "host.h"
#include "settings.h"
#include "service.h"
#include "wave.h"

/* correct the byte order */
static LPVOID ReverseByteOrder(LPVOID buffer, INT length)
{
	INT i;
	u_short *ushbuf;

	for (i = 0, ushbuf = (u_short *)buffer; i < length; i += 2, ushbuf++)
		*ushbuf = ntohs(*ushbuf);
	return buffer;
}

/* free the event data from done wave headers */
static VOID CALLBACK HeaderDone(LPVOID userData)
{
	iax_event_free((struct iax_event *)userData);
}

/* the service main routine, started by the scheduler */
static VOID WINAPI ServiceMain(DWORD argc, LPTSTR argv[])
{
	DWORD exitCode = ERROR_SUCCESS;
	LPSERVICE service = NULL;
	LPSETTINGS settings = NULL;
	INT wsaStartup = ~0;
	INT iaxPort = -1;
	LPWAVE wave = NULL;
	struct iax_session *session = NULL;
	struct iax_session *registeredSession = NULL;
	UINT format;
	DWORD nextRegistration;
	INT waitTimeForEvent;
	INT waitTimeForRegister;
	WSADATA wsaData;
	WSAEVENT netEvent;
	ULONG address;
	struct iax_event *evt;

#define REG_START \
{ \
	if (settings->Register) \
	{ \
		CHECK((registeredSession = iax_session_new()) != NULL, ERROR_OUTOFMEMORY); \
		REG_REFRESH; \
	} \
}
#define REG_REFRESH \
{ \
		iax_register(registeredSession, settings->Host, settings->UserName, settings->Secret, IAX_DEFAULT_REG_EXPIRE); \
		nextRegistration = GetTickCount() + IAX_DEFAULT_REG_EXPIRE*1000; \
}
#define CHECK(condition, error) { if (!(condition)) { exitCode = (error); goto LEAVE; } }

	/* initialize the service */
	if ((service = InitializeService()) == NULL)
		return;
	BeginServiceStatus(service, SERVICE_RUNNING, 1000);

	/* parse the command line arguments */
	CHECK((settings = ParseSettings(argc,argv)) != NULL, ERROR_INVALID_PARAMETER);
	ProgressServiceStatus(service);

	/* start winsock */
	CHECK((wsaStartup = WSAStartup(MAKEWORD(2, 2), &wsaData)) == 0, wsaStartup);
	CHECK(wsaData.wVersion >= MAKEWORD(2, 0), WSAVERNOTSUPPORTED);
	ProgressServiceStatus(service);

	/* initialize iax */
	CHECK((iaxPort = iax_init(settings->Port)) == settings->Port, WSAGetLastError() == ERROR_SUCCESS ? ERROR_OPEN_FAILED : WSAGetLastError());
	REG_START;
	ProgressServiceStatus(service);

	/* associate the socket with an event */
	CHECK(WSAEventSelect(iax_get_fd(), (netEvent = GetServiceEvent(service, SERVICE_EVENT_NETWORK)), FD_READ) == 0, WSAGetLastError());
	ProgressServiceStatus(service);

	/* initialize the wave output */
	CHECK((wave = InitializeWave(settings, GetServiceEvent(service, SERVICE_EVENT_WAVEFORM), &HeaderDone)) != NULL, GetLastWaveError());
	ProgressServiceStatus(service);

	/* report the service start */
	EndServiceStatus(service);

	/* wait for the following things:
	   - shutdown event
	   - network event
	   - wave event
	   - the next scheduled event */
	for (;;)
	{
		waitTimeForEvent = iax_time_to_next_event();
		if (registeredSession != NULL)
		{
			waitTimeForRegister = (INT)(nextRegistration - GetTickCount());
			if (waitTimeForEvent == (INT)INFINITE || waitTimeForRegister < waitTimeForEvent)
				waitTimeForEvent = waitTimeForRegister;
		}
		switch (WaitForServiceEvents(service, (DWORD)waitTimeForEvent))
		{
			/* on a wait fail set the error code and leave */
			case WSA_WAIT_FAILED:
				exitCode = WSAGetLastError();
				goto LEAVE;

			/* on shutdown just leave */
			case WSA_WAIT_EVENT_0 + SERVICE_EVENT_SHUTDOWN:
				goto LEAVE;

			/* on a network event reset the event */
			case WSA_WAIT_EVENT_0 + SERVICE_EVENT_NETWORK:
				CHECK(WSAResetEvent(netEvent), WSAGetLastError());
				break;

			/* on a waveform event cleanup all done headers */
			case WSA_WAIT_EVENT_0 + SERVICE_EVENT_WAVEFORM:
				CHECK(HandleDoneWaveHeaders(wave), GetLastWaveError());
				break;

			/* on timeout just update the hint for the loop and possible the registration */
			case WSA_WAIT_TIMEOUT:
				if (registeredSession != NULL && (INT)(GetTickCount() - nextRegistration) > 0)
					REG_REFRESH;
				break;

			/* all other values should be treated as an error */
			default:
				exitCode = ERROR_INVALID_HANDLE;
				goto LEAVE;
		}

#undef CHECK
#define CHECK(condition, error) { if (!(condition)) { exitCode = (error); iax_event_free(evt); goto LEAVE; } }

		/* handle all pending events */
		while ((evt = iax_get_event(FALSE)) != NULL)
		{
			/* check the type of the events */
			switch (evt->etype)
			{
				/* handle new connections */
				case IAX_EVENT_CONNECT:

					/* reject request if we're already in another session */
					if (session != NULL)
					{
						iax_reject(evt->session, "Already in session.");
						break;
					}

					/* determine host */
					address = iax_get_peer_addr(evt->session).sin_addr.S_un.S_addr;

					/* let's see if it is explicitly restricted */
					if (settings->ForbiddenHosts != NULL && ContainsHost(address, settings->ForbiddenHosts))
					{
						iax_reject(evt->session, "IP address forbidden.");
						break;
					}

					/* test the allowed hosts if at least one matches */
					if (settings->AllowedHosts != NULL && !ContainsHost(address, settings->AllowedHosts))
					{
						iax_reject(evt->session, "IP not allowed.");
						break;
					}

					/* all checks successful, begin the call */
					session = evt->session;
					format = AST_FORMAT_SLINEAR;
					if (settings->RingTone != NULL)
						iax_pref_codec_get(evt->session, &format, 1);
					iax_accept(evt->session, format);
					iax_ring_announce(evt->session);
					if (settings->RingTone == NULL)
						iax_answer(evt->session);
					CHECK(StartWave(wave), GetLastWaveError());
					break;

				/* handle rejects, hangups and timeouts */
				case IAX_EVENT_REJECT:
				case IAX_EVENT_HANGUP:
				case IAX_EVENT_TIMEOUT:

					/* stop any audio playback and leave the session */
					if (evt->session == session)
					{
						CHECK(StopWave(wave), GetLastWaveError());
						session = NULL;
					}

					/* recreate the register session if necessary */
					if (evt->session == registeredSession)
					{
						iax_session_destroy(&registeredSession);
						REG_START;
					}
					break;

				/* handle incoming voice buffers */
				case IAX_EVENT_VOICE:

					/* enque the wave form header */
					if (evt->session == session && settings->RingTone == NULL)
					{
						CHECK(EnqueueWaveHeader(wave, ReverseByteOrder(evt->data, evt->datalen), evt->datalen, evt), GetLastWaveError());
						continue;
					}
					break;
			}

			/* free the memory for the event */
			iax_event_free(evt);
		}
	}

#undef CHECK
#undef REG_REFRESH
#undef REG_START

LEAVE:

	/* report the pending end of the service */
	BeginServiceStatus(service, SERVICE_STOPPED, 1000);

	/* hangup a possibly active call and destroy the registered session */
	if (session != NULL)
		iax_hangup(session, "Gotta go, sorry!");
	if (registeredSession != NULL)
		iax_session_destroy(&registeredSession);
	ProgressServiceStatus(service);

	/* close audio device */
	if (wave != NULL)
		FreeWave(wave);
	ProgressServiceStatus(service);

	/* shutdown iax */
	if (iaxPort > -1)
		iax_shutdown();
	ProgressServiceStatus(service);

	/* close winsock */
	if (wsaStartup == 0)
		WSACleanup();
	ProgressServiceStatus(service);

	/* release the settings */
	if (settings != NULL)
		FreeSettings(settings);
	ProgressServiceStatus(service);

	/* report the end of the service */
	EndServiceStatus(service);

	/* free the service */
	FreeService(service, exitCode);
}

/* the process main routine, started by Windows */
int main(int argc, char *argv[])
{
	SERVICE_TABLE_ENTRY dispatchers[] = {{TEXT(SERVICE_NAME), &ServiceMain}, {NULL, NULL}};

	/* defer control over to the service dispatcher */
	return StartServiceCtrlDispatcher(dispatchers) ? ERROR_SUCCESS : GetLastError();
}
