#include <time.h>

#include "deltaqueueidletimer.hpp"
#include "deltaqueueaction.hpp"
#include "imapmaster.hpp"
#include "internetserver.hpp"
#include "imapsession.hpp"

DeltaQueueIdleTimer::DeltaQueueIdleTimer(int delta, SessionDriver *driver) : DeltaQueueAction(delta, driver) {}


void DeltaQueueIdleTimer::HandleTimeout(bool isPurge)
{
    if (!isPurge) {
        ImapMaster *imap_master = dynamic_cast<ImapMaster *>(m_driver->GetMaster());
	time_t now = time(NULL);
	unsigned timeout = (ImapNotAuthenticated == m_driver->GetSession()->GetState()) ?
	    imap_master->GetLoginTimeout() :
	    imap_master->GetIdleTimeout();

	if ((now - timeout) > m_driver->GetSession()->GetLastCommandTime())
	{
	    std::string bye = "* BYE Idle timeout disconnect\r\n";
	    m_driver->GetSocket()->Send((uint8_t *)bye.data(), bye.size());
	    m_driver->GetServer()->KillSession(m_driver);
	}
	else
	{
	    imap_master->SetIdleTimer(m_driver, (time_t) m_driver->GetSession()->GetLastCommandTime() + timeout + 1 - now);
	}
    }
}
