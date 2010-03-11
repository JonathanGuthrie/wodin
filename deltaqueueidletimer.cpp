#include <time.h>

#include "deltaqueueidletimer.hpp"
#include "deltaqueueaction.hpp"
#include "imapmaster.hpp"
#include "internetserver.hpp"
#include "imapsession.hpp"

DeltaQueueIdleTimer::DeltaQueueIdleTimer(int delta, SessionDriver *driver) : DeltaQueueAction(delta, driver) {}


void DeltaQueueIdleTimer::HandleTimeout(bool isPurge) {
    if (!isPurge) {
        ImapMaster *imap_master = dynamic_cast<ImapMaster *>(m_driver->GetMaster());
	const ImapSession *imap_session = dynamic_cast<const ImapSession *>(m_driver->GetSession());
	time_t now = time(NULL);
	unsigned timeout = (ImapNotAuthenticated == imap_session->GetState()) ?
	    imap_master->GetLoginTimeout() :
	    imap_master->GetIdleTimeout();

	if ((now - timeout) > imap_session->GetLastCommandTime()) {
	    std::string bye = "* BYE Idle timeout disconnect\r\n";
	    m_driver->GetSocket()->Send((uint8_t *)bye.data(), bye.size());
	    m_driver->GetServer()->KillSession(m_driver);
	}
	else
	{
	    imap_master->SetIdleTimer(m_driver, (time_t) imap_session->GetLastCommandTime() + timeout + 1 - now);
	}
    }
}
