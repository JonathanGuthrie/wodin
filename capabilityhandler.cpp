#include "capabilityhandler.hpp"

ImapHandler *capabilityHandler(ImapSession *session, INPUT_DATA_STRUCT &input) {
  (void) input;
  return new CapabilityHandler(session);
}

IMAP_RESULTS CapabilityHandler::receiveData(INPUT_DATA_STRUCT &input) {
  std::string response("* ");
  response += m_session->capabilityString() + "\r\n";
  m_session->driver()->wantsToSend(response);
  return IMAP_OK;
}
