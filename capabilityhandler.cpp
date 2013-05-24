#include "capabilityhandler.hpp"

ImapHandler *capabilityHandler(ImapSession *session) {
  return new CapabilityHandler(session);
}

IMAP_RESULTS CapabilityHandler::receiveData(uint8_t* data, size_t dataLen) {
  std::string response("* ");
  response += m_session->capabilityString() + "\r\n";
  m_session->driver()->wantsToSend(response);
  return IMAP_OK;
}
