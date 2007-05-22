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
	unsigned timeout = (ImapNotAuthenticated == driver->GetSession()->GetState()) ?
	    driver->GetServer()->GetLoginTimeout() :
	    driver->GetServer()->GetIdleTimeout();

	if ((now - timeout) > driver->GetSession()->GetLastCommandTime())
	{
	    std::string bye = "* BYE Idle timeout disconnect\r\n";
	    driver->GetSocket()->Send((uint8_t *)bye.data(), bye.size());
	    driver->GetServer()->KillSession(driver);
	}
	else
	{
	    driver->GetServer()->SetIdleTimer(driver, (time_t) driver->GetSession()->GetLastCommandTime() + timeout + 1 - now);
	}
    }
}
