#include <time.h>

#include <libcppserver/deltaqueueaction.hpp>

#include "deltaqueueasynchronousaction.hpp"
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
      imap_session->GetServer()->AddTimerAction(new DeltaQueueAsynchronousAction(imap_master->GetAsynchronousEventTime(), m_session));
    }
}
