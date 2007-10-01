#include <time.h>

#include "deltaqueueasynchronousaction.hpp"
#include "deltaqueueaction.hpp"
#include "imapserver.hpp"
#include "imapsession.hpp"

DeltaQueueAsynchronousAction::DeltaQueueAsynchronousAction(int delta, SessionDriver *driver) : DeltaQueueAction(delta, driver) { }


void DeltaQueueAsynchronousAction::HandleTimeout(bool isPurge)
{
    if (!isPurge) {
	m_driver->DoAsynchronousWork();
	m_driver->GetServer()->ScheduleAsynchronousAction(m_driver, m_driver->GetServer()->GetAsynchronousEventTime());
    }
}
