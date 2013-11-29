#include "logouthandler.hpp"

ImapHandler *logoutHandler(ImapSession *session, INPUT_DATA_STRUCT &input) {
  (void) input;
  return new LogoutHandler(session);
}

/*
 * The LOGOUT indicates that the session may be closed.  The main command
 * processor gets the word about this because the state is set to "4" which
 * means "logout state"
 */
IMAP_RESULTS LogoutHandler::receiveData(INPUT_DATA_STRUCT &input) {
  // If the mailbox is open, close it
  // In IMAP, deleted messages are always purged before a close
  m_session->closeMailbox(ImapLogoff);
  m_session->driver()->wantsToSend("* BYE IMAP4rev1 server closing\r\n");
  return IMAP_OK;
}
