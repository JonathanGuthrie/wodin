#if !defined(_AUTHENTICATEHANDLER_HPP_INCLUDED)
#define _AUTHENTICATEHANDLER_HPP_INCLUDED

#include "imaphandler.hpp"

ImapHandler *authenticateHandler(ImapSession *, INPUT_DATA_STRUCT &input);

class AuthenticateHandler : public ImapHandler {
private:
  uint32_t m_parseStage;
  Sasl *m_auth;
  ParseBuffer *m_parseBuffer;

public:
  AuthenticateHandler(ImapSession *session, ParseBuffer *parseBuffer)  : ImapHandler(session), m_parseStage(0), m_auth(NULL), m_parseBuffer(parseBuffer) {}
  virtual ~AuthenticateHandler() {}
  virtual IMAP_RESULTS receiveData(INPUT_DATA_STRUCT &input);
};

#endif // !defined(_AUTHENTICATEHANDLER_HPP_INCLUDED)
