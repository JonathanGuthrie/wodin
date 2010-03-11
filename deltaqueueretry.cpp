#include <time.h>

#include "deltaqueueretry.hpp"
#include "internetserver.hpp"
#include "imapsession.hpp"
#include "imapdriver.hpp"

DeltaQueueRetry::DeltaQueueRetry(int delta, SessionDriver *driver) : DeltaQueueAction(delta, driver) { }


void DeltaQueueRetry::HandleTimeout(bool isPurge) {
  if (!isPurge) {
    ImapDriver *imap_driver = dynamic_cast<ImapDriver *>(m_driver);
    imap_driver->DoRetry();
  }
}
