#include <time.h>

#include <clotho/deltaqueueaction.hpp>

#include "asynchronousaction.hpp"
#include "imapmaster.hpp"
#include "imapsession.hpp"


DeltaQueueAsynchronousAction::DeltaQueueAsynchronousAction(int delta, InternetSession *session) : DeltaQueueAction(delta, session) {
}


void DeltaQueueAsynchronousAction::handleTimeout(bool isPurge) {
#if !defined(TEST)
  ImapSession *imap_session = dynamic_cast<ImapSession *>(m_session);
  ImapMaster *imap_master = dynamic_cast<ImapMaster *>(imap_session->master());
  if (!isPurge) {
    imap_session->asynchronousEvent();
    imap_session->server()->addTimerAction(new DeltaQueueAsynchronousAction(imap_master->asynchronousEventTime(), m_session));
    }
#endif // !defined(TEST)
}
