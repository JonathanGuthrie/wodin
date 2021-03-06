/*
 * Copyright 2013 Jonathan R. Guthrie
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <time.h>

#include <clotho/internetserver.hpp>
#include <clotho/deltaqueueaction.hpp>

#include "idletimer.hpp"
#include "imapmaster.hpp"
#include "imapsession.hpp"

DeltaQueueIdleTimer::DeltaQueueIdleTimer(int delta, InternetSession *session) : DeltaQueueAction(delta, session) {}


void DeltaQueueIdleTimer::handleTimeout(bool isPurge) {
#if !defined(TEST)
    if (!isPurge) {
	ImapSession *imap_session = dynamic_cast<ImapSession *>(m_session);
	const ImapMaster *imap_master = dynamic_cast<const ImapMaster *>(imap_session->master());
	time_t now = time(NULL);
	unsigned timeout = (ImapNotAuthenticated == imap_session->state()) ?
	    imap_master->loginTimeout() :
	    imap_master->idleTimeout();

	if ((now - timeout) > imap_session->lastCommandTime()) {
	    imap_session->idleTimeout();
	}
	else {
	    imap_session->server()->addTimerAction(new DeltaQueueIdleTimer((time_t) imap_session->lastCommandTime() + timeout + 1 - now, m_session));
	}
    }
#endif // !defined(TEST)
}
