#include "unimplementedhandler.hpp"

ImapHandler *unimplementedHandler(ImapSession *session, INPUT_DATA_STRUCT &input) {
  (void) input;
  return new UnimplementedHandler(session);
}

/*--------------------------------------------------------------------------------------*/
/* Gets called whenever the system doesn't know what else to do.  It signals the client */
/* that the command was not recognized							*/
/*--------------------------------------------------------------------------------------*/
IMAP_RESULTS UnimplementedHandler::receiveData(INPUT_DATA_STRUCT &input) {
  m_session->responseText("Unrecognized Command");
  return IMAP_BAD;
}
