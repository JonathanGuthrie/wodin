#include <time.h>

#include "deltaqueueretry.hpp"
#include "internetserver.hpp"
#include "imapsession.hpp"

DeltaQueueRetry::DeltaQueueRetry(int delta, SessionDriver *driver) : DeltaQueueAction(delta, driver) { }


void DeltaQueueRetry::HandleTimeout(bool isPurge)
{
    if (!isPurge) {
	m_driver->DoRetry();
    }
}
