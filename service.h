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

#ifndef _SERVICE_H
#define _SERVICE_H

/* event offsets */
#define SERVICE_EVENT_SHUTDOWN 0
#define SERVICE_EVENT_NETWORK  1
#define SERVICE_EVENT_WAVEFORM 2
#define SERVICE_EVENT_MAX      3

/* transparent service structure */
typedef struct tagSERVICE SERVICE, *LPSERVICE;

/* wait for any service event within the given time */
extern DWORD WaitForServiceEvents(LPSERVICE, DWORD);

/* return the given event */
extern WSAEVENT GetServiceEvent(LPSERVICE, INT);

/* begin a list of tasks to reach a service state */
extern BOOL BeginServiceStatus(LPSERVICE, DWORD, DWORD);

/* inform the SCM that the service is one step closer to the next state */
extern BOOL ProgressServiceStatus(LPSERVICE);

/* signal that all pending tasks have been completed to reach the next state */
extern BOOL EndServiceStatus(LPSERVICE);

/* create a new service structure */
extern LPSERVICE InitializeService();

/* release all service resources */
extern VOID FreeService(LPSERVICE, DWORD);

#endif
