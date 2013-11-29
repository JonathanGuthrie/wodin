#include "checkhandler.hpp"

ImapHandler *checkHandler(ImapSession *session, INPUT_DATA_STRUCT &input) {
  (void) input;
  return new CheckHandler(session);
}

IMAP_RESULTS CheckHandler::receiveData(INPUT_DATA_STRUCT &input) {
  // Unlike NOOP, I always call mailboxFlushBuffers because that recalculates the the cached data.
  // That may be the only way to find out that the number of messages or the UIDNEXT value has
  // changed.
  IMAP_RESULTS result = IMAP_OK;
  NUMBER_SET purgedMessages;

  if (MailStore::SUCCESS == m_session->store()->mailboxUpdateStats(&purgedMessages)) {
    m_session->purgedMessages(purgedMessages);
  }
  m_session->store()->mailboxFlushBuffers();

  return result;
}
