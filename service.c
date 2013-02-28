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
#include "service.h"

/* variables associated with a service */
struct tagSERVICE
{
	/* the handle and status of the service */
	SERVICE_STATUS_HANDLE Handle;
	SERVICE_STATUS Status;
	DWORD TargetState;

	/* events indicating shutdown/stop, network data and waveform completion */
	WSAEVENT ShutdownEvent;
	WSAEVENT NetworkEvent;
	WSAEVENT WaveformEvent;

	/* array of all events */
	WSAEVENT Events[SERVICE_EVENT_MAX];
};

/* serive handler routine */
static DWORD WINAPI Handler(DWORD control, DWORD eventType, LPVOID eventData, LPVOID context)
{
	LPSERVICE service = (LPSERVICE)context;

	/* perform the action as requested by the control */
	switch (control)
	{
		/* signal a stop to the service main loop */
		case SERVICE_CONTROL_SHUTDOWN:
		case SERVICE_CONTROL_STOP:
			return WSASetEvent(service->ShutdownEvent) ? NO_ERROR : GetLastError();

		/* report the current status */
		case SERVICE_CONTROL_INTERROGATE:
			return SetServiceStatus(service->Handle, &service->Status) ? NO_ERROR : GetLastError();

		/* not implemented */
		default:
			return ERROR_CALL_NOT_IMPLEMENTED;
	}
}

/* wait for any service event within the given time */
DWORD WaitForServiceEvents(LPSERVICE service, DWORD timeout)
{
	return WSAWaitForMultipleEvents(SERVICE_EVENT_MAX, service->Events, FALSE, timeout, FALSE);
}

/* return the given event */
WSAEVENT GetServiceEvent(LPSERVICE service, INT event)
{
	return (event < 0 || event >= SERVICE_EVENT_MAX) ? WSA_INVALID_EVENT : service->Events[event];
}

/* begin a list of tasks to reach a service state */
BOOL BeginServiceStatus(LPSERVICE service, DWORD state, DWORD waitHint)
{
	switch (state)
	{
		case SERVICE_PAUSED:
			service->Status.dwCurrentState = SERVICE_PAUSE_PENDING;
			break;
		case SERVICE_RUNNING:
			service->Status.dwCurrentState = (service->Status.dwCurrentState == SERVICE_PAUSED || service->Status.dwCurrentState == SERVICE_PAUSE_PENDING) ? SERVICE_CONTINUE_PENDING : SERVICE_START_PENDING;
			break;
		case SERVICE_STOPPED:
			service->Status.dwCurrentState = SERVICE_STOP_PENDING;
			break;
		default:
			return FALSE;
	}
	service->Status.dwCheckPoint = 0;
	service->Status.dwWaitHint = waitHint;
	service->TargetState = state;
	return SetServiceStatus(service->Handle, &service->Status);
}

/* inform the SCM that the service is one step closer to the next state */
BOOL ProgressServiceStatus(LPSERVICE service)
{
	if (service->TargetState == 0)
		return FALSE;
	service->Status.dwCheckPoint++;
	return SetServiceStatus(service->Handle, &service->Status);
}

/* signal that all pending tasks have been completed to reach the next state */
BOOL EndServiceStatus(LPSERVICE service)
{
	if (service->TargetState == 0)
		return FALSE;
	service->Status.dwCurrentState = service->TargetState;
	service->Status.dwCheckPoint = 0;
	service->Status.dwWaitHint = 0;
	service->TargetState = 0;
	return SetServiceStatus(service->Handle, &service->Status);
}

/* create a new service structure */
LPSERVICE InitializeService()
{
#define CHECK(condition) { if (!(condition)) goto ON_ERROR; }

	LPSERVICE service;

	/* allocate and reset the structure */
	ALLOC(service);
	service->ShutdownEvent = WSA_INVALID_EVENT;
	service->NetworkEvent = WSA_INVALID_EVENT;
	service->WaveformEvent = WSA_INVALID_EVENT;

	/* create all events */
	CHECK((service->Events[SERVICE_EVENT_SHUTDOWN] = service->ShutdownEvent = WSACreateEvent()) != WSA_INVALID_EVENT);
	CHECK((service->Events[SERVICE_EVENT_NETWORK] = service->NetworkEvent = WSACreateEvent()) != WSA_INVALID_EVENT);
	CHECK((service->Events[SERVICE_EVENT_WAVEFORM] = service->WaveformEvent = WSACreateEvent()) != WSA_INVALID_EVENT);

	/* register the service control handler */
	CHECK((service->Handle = RegisterServiceCtrlHandlerEx(_T(SERVICE_NAME), &Handler, service)) != 0);

	/* initialize and report the status */
	service->Status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	service->Status.dwControlsAccepted = SERVICE_CONTROL_SHUTDOWN | SERVICE_CONTROL_STOP;
	return service;

ON_ERROR:
	/* on error release the created events and the structure */
	if (service->ShutdownEvent != WSA_INVALID_EVENT) WSACloseEvent(service->ShutdownEvent);
	if (service->NetworkEvent != WSA_INVALID_EVENT) WSACloseEvent(service->NetworkEvent);
	if (service->WaveformEvent != WSA_INVALID_EVENT) WSACloseEvent(service->WaveformEvent);
	FREE(service);
	return NULL;

#undef CHECK
}

/* release all service resources */
VOID FreeService(LPSERVICE service, DWORD exitCode)
{
	/* close all created events */
	WSACloseEvent(service->ShutdownEvent);
	WSACloseEvent(service->NetworkEvent);
	WSACloseEvent(service->WaveformEvent);
	service->Status.dwWin32ExitCode = exitCode;
	SetServiceStatus(service->Handle, &service->Status);
	FREE(service);
}
