#include <time.h>

#include "deltaqueuecheckmailbox.hpp"
#include "deltaqueueaction.hpp"
#include "imapserver.hpp"
#include "imapsession.hpp"

DeltaQueueCheckMailbox::DeltaQueueCheckMailbox(int delta, SessionDriver *driver) : DeltaQueueAction(delta, driver) { }


void DeltaQueueCheckMailbox::HandleTimeout(bool isPurge)
{
    if (!isPurge) {
	m_driver->DoAsynchronousWork();
	m_driver->GetServer()->ScheduleAsynchronousAction(m_driver, m_driver->GetServer()->GetAsynchronousEventTime());
    }
}
