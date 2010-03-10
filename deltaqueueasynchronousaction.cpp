#include <time.h>

#include "deltaqueueasynchronousaction.hpp"
#include "deltaqueueaction.hpp"
#include "imapmaster.hpp"
#include "imapsession.hpp"

DeltaQueueAsynchronousAction::DeltaQueueAsynchronousAction(int delta, SessionDriver *driver) : DeltaQueueAction(delta, driver) { }


void DeltaQueueAsynchronousAction::HandleTimeout(bool isPurge)
{
    ImapMaster *imap_master = dynamic_cast<ImapMaster *>(m_driver->GetMaster());
    if (!isPurge) {
	m_driver->DoAsynchronousWork();
	imap_master->ScheduleAsynchronousAction(m_driver, imap_master->GetAsynchronousEventTime());
    }
}
