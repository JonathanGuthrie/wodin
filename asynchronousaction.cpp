#include <time.h>

#include <clotho/deltaqueueaction.hpp>

#include "asynchronousaction.hpp"
#include "imapmaster.hpp"
#include "imapsession.hpp"


DeltaQueueAsynchronousAction::DeltaQueueAsynchronousAction(int delta, InternetSession *session) : DeltaQueueAction(delta, session) {
}


void DeltaQueueAsynchronousAction::HandleTimeout(bool isPurge)
{
    ImapSession *imap_session = dynamic_cast<ImapSession *>(m_session);
    ImapMaster *imap_master = dynamic_cast<ImapMaster *>(imap_session->GetMaster());
    if (!isPurge) {
      imap_session->AsynchronousEvent();
      imap_session->GetServer()->AddTimerAction(new DeltaQueueAsynchronousAction(imap_master->asynchronousEventTime(), m_session));
    }
}
