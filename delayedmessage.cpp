#include <clotho/internetserver.hpp>

#include "delayedmessage.hpp"
#include "imapsession.hpp"

DeltaQueueDelayedMessage::DeltaQueueDelayedMessage(int delta, InternetSession *session, const std::string message) : DeltaQueueAction(delta, session), message(message) {}


void DeltaQueueDelayedMessage::handleTimeout(bool isPurge) {
  if (!isPurge) {
    const ImapSession *imap_session = dynamic_cast<const ImapSession *>(m_session);
    imap_session->driver()->wantsToSend(message);
    imap_session->driver()->wantsToReceive();
  }
}
