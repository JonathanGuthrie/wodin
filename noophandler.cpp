#include "noophandler.hpp"

ImapHandler *noopHandler(ImapSession *session) {
  return new NoopHandler(session);
}

/*
 * The NOOP may be used by the server to trigger things like checking for
 * new messages although the server doesn't have to wait for this to do
 * things like that
 */
IMAP_RESULTS NoopHandler::receiveData(uint8_t* data, size_t dataLen) {
  // This command literally doesn't do anything.  If there was an update, it was found
  // asynchronously and the updated info will be printed in formatTaggedResponse

  return IMAP_OK;
}
