#include "closehandler.hpp"

ImapHandler *closeHandler(ImapSession *session, INPUT_DATA_STRUCT &input) {
  (void) input;
  return new CloseHandler(session);
}

IMAP_RESULTS CloseHandler::receiveData(INPUT_DATA_STRUCT &input) {
  // If the mailbox is open, close it
  // In IMAP, deleted messages are always purged before a close
  IMAP_RESULTS result = IMAP_TRY_AGAIN;
  if (MailStore::SUCCESS == m_session->store()->lock()) {
    NUMBER_SET purgedMessages;
    m_session->store()->listDeletedMessages(&purgedMessages);
    m_session->purgedMessages(purgedMessages);
    for(NUMBER_SET::iterator i=purgedMessages.begin(); i!=purgedMessages.end(); ++i) {
      m_session->store()->expungeThisUid(*i);
    }
    m_session->store()->mailboxClose();
    m_session->state(ImapAuthenticated);
    m_session->store()->unlock();
    result = IMAP_OK;
  }
  return result;
}
