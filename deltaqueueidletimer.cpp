#include <time.h>

#include <libcppserver/internetserver.hpp>
#include <libcppserver/deltaqueueaction.hpp>

#include "deltaqueueidletimer.hpp"
#include "imapmaster.hpp"
#include "imapsession.hpp"

DeltaQueueIdleTimer::DeltaQueueIdleTimer(int delta, InternetSession *session) : DeltaQueueAction(delta, session) {}


void DeltaQueueIdleTimer::HandleTimeout(bool isPurge) {
    if (!isPurge) {
	const ImapSession *imap_session = dynamic_cast<const ImapSession *>(m_session);
        ImapMaster *imap_master = dynamic_cast<ImapMaster *>(imap_session->GetMaster());
	time_t now = time(NULL);
	unsigned timeout = (ImapNotAuthenticated == imap_session->GetState()) ?
	    imap_master->GetLoginTimeout() :
	    imap_master->GetIdleTimeout();

	if ((now - timeout) > imap_session->GetLastCommandTime()) {
	    std::string bye = "* BYE Idle timeout disconnect\r\n";
	    imap_session->GetDriver()->WantsToSend(bye);
	    imap_session->GetServer()->KillSession(imap_session->GetDriver());
	}
	else {
	    imap_session->GetServer()->AddTimerAction(new DeltaQueueIdleTimer((time_t) imap_session->GetLastCommandTime() + timeout + 1 - now, m_session));
	}
    }
}
