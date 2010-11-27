#include <time.h>

#include <clotho/internetserver.hpp>
#include <clotho/deltaqueueaction.hpp>

#include "deltaqueueidletimer.hpp"
#include "imapmaster.hpp"
#include "imapsession.hpp"

DeltaQueueIdleTimer::DeltaQueueIdleTimer(int delta, InternetSession *session) : DeltaQueueAction(delta, session) {}


void DeltaQueueIdleTimer::HandleTimeout(bool isPurge) {
    if (!isPurge) {
        ImapSession *imap_session = dynamic_cast<ImapSession *>(m_session);
        const ImapMaster *imap_master = dynamic_cast<const ImapMaster *>(imap_session->GetMaster());
	time_t now = time(NULL);
	unsigned timeout = (ImapNotAuthenticated == imap_session->GetState()) ?
	    imap_master->loginTimeout() :
	    imap_master->idleTimeout();

	if ((now - timeout) > imap_session->GetLastCommandTime()) {
	    imap_session->IdleTimeout();
	}
	else {
	    imap_session->GetServer()->AddTimerAction(new DeltaQueueIdleTimer((time_t) imap_session->GetLastCommandTime() + timeout + 1 - now, m_session));
	}
    }
}
