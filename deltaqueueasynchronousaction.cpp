#include <time.h>

#include "deltaqueueasynchronousaction.hpp"
#include "deltaqueueaction.hpp"
#include "imapmaster.hpp"
#include "imapsession.hpp"
#include "imapdriver.hpp"


DeltaQueueAsynchronousAction::DeltaQueueAsynchronousAction(int delta, SessionDriver *driver) : DeltaQueueAction(delta, driver) {
}


void DeltaQueueAsynchronousAction::HandleTimeout(bool isPurge)
{
    ImapMaster *imap_master = dynamic_cast<ImapMaster *>(m_driver->GetMaster());
    ImapDriver *imap_driver = dynamic_cast<ImapDriver *>(m_driver);
    if (!isPurge) {
      imap_driver->DoAsynchronousWork();
      imap_master->ScheduleAsynchronousAction(imap_driver, imap_master->GetAsynchronousEventTime());
    }
}
