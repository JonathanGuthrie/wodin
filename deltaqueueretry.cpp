#include <time.h>

#include <clotho/internetserver.hpp>

#include "deltaqueueretry.hpp"
#include "imapsession.hpp"

DeltaQueueRetry::DeltaQueueRetry(int delta, InternetSession *session) : DeltaQueueAction(delta, session) { }


void DeltaQueueRetry::HandleTimeout(bool isPurge) {
  if (!isPurge) {
    ImapSession *imap_session = dynamic_cast<ImapSession *>(m_session);
    imap_session->DoRetry();
  }
}
