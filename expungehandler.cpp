#include "expungehandler.hpp"

ImapHandler *expungeHandler(ImapSession *session, INPUT_DATA_STRUCT &input) {
  (void) input;
  return new ExpungeHandler(session);
}

IMAP_RESULTS ExpungeHandler::receiveData(INPUT_DATA_STRUCT &input) {
  IMAP_RESULTS result = IMAP_MBOX_ERROR;
  NUMBER_SET purgedMessages;

  switch (m_session->mboxErrorCode(m_session->store()->listDeletedMessages(&purgedMessages))) {
  case MailStore::SUCCESS:
    m_session->purgedMessages(purgedMessages);
    result = IMAP_OK;
    break;

  case MailStore::CANNOT_COMPLETE_ACTION:
    result = IMAP_TRY_AGAIN;
    break;
  }

  return result;
}
