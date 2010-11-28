#include <time.h>

#include <clotho/internetserver.hpp>
#include <clotho/deltaqueueaction.hpp>

#include "idletimer.hpp"
#include "imapmaster.hpp"
#include "imapsession.hpp"

DeltaQueueIdleTimer::DeltaQueueIdleTimer(int delta, InternetSession *session) : DeltaQueueAction(delta, session) {}


void DeltaQueueIdleTimer::handleTimeout(bool isPurge) {
#if !defined(TEST)
  if (!isPurge) {
    ImapSession *imap_session = dynamic_cast<ImapSession *>(m_session);
    const ImapMaster *imap_master = dynamic_cast<const ImapMaster *>(imap_session->master());
    time_t now = time(NULL);
    unsigned timeout = (ImapNotAuthenticated == imap_session->state()) ?
      imap_master->loginTimeout() :
      imap_master->idleTimeout();

    if ((now - timeout) > imap_session->lastCommandTime()) {
      imap_session->idleTimeout();
    }
    else {
      imap_session->server()->addTimerAction(new DeltaQueueIdleTimer((time_t) imap_session->lastCommandTime() + timeout + 1 - now, m_session));
    }
  }
#endif // !defined(TEST)
}
