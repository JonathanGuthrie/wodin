#include <time.h>

#include "deltaqueueidletimer.hpp"
#include "deltaqueueaction.hpp"
#include "imapserver.hpp"
#include "imapsession.hpp"

DeltaQueueIdleTimer::DeltaQueueIdleTimer(int delta, SessionDriver *driver) : DeltaQueueAction(delta, driver) {}


void DeltaQueueIdleTimer::HandleTimeout(bool isPurge)
{
    if (!isPurge) {
	time_t now = time(NULL);
	unsigned timeout = (ImapNotAuthenticated == m_driver->GetSession()->GetState()) ?
	    m_driver->GetServer()->GetLoginTimeout() :
	    m_driver->GetServer()->GetIdleTimeout();

	if ((now - timeout) > m_driver->GetSession()->GetLastCommandTime())
	{
	    std::string bye = "* BYE Idle timeout disconnect\r\n";
	    m_driver->GetSocket()->Send((uint8_t *)bye.data(), bye.size());
	    m_driver->GetServer()->KillSession(m_driver);
	}
	else
	{
	    m_driver->GetServer()->SetIdleTimer(m_driver, (time_t) m_driver->GetSession()->GetLastCommandTime() + timeout + 1 - now);
	}
    }
}
