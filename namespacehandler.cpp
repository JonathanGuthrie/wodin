#include "namespacehandler.hpp"

ImapHandler *namespaceHandler(ImapSession *session, INPUT_DATA_STRUCT &input) {
  (void) input;
  return new NamespaceHandler(session);
}

IMAP_RESULTS NamespaceHandler::receiveData(INPUT_DATA_STRUCT &input) {
  std::string response("* NAMESPACE ");
  response += m_session->store()->listNamespaces() + "\r\n";
  m_session->driver()->wantsToSend(response);
  return IMAP_OK;
}
