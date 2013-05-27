#include "unimplementedhandler.hpp"

ImapHandler *unimplementedHandler(ImapSession *session) {
  return new UnimplementedHandler(session);
}

/*--------------------------------------------------------------------------------------*/
/* Gets called whenever the system doesn't know what else to do.  It signals the client */
/* that the command was not recognized							*/
/*--------------------------------------------------------------------------------------*/
IMAP_RESULTS UnimplementedHandler::receiveData(uint8_t* data, size_t dataLen) {
  m_session->responseText("Unrecognized Command");
  return IMAP_BAD;
}
