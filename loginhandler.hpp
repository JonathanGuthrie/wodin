#if !defined(_LOGINHANDLER_HPP_INCLUDED)
#define _LOGINHANDLER_HPP_INCLUDED

#include "imaphandler.hpp"

ImapHandler *loginHandler(ImapSession *, INPUT_DATA_STRUCT &input);

class LoginHandler : public ImapHandler {
private:
  ParseBuffer *m_parseBuffer;
  uint32_t m_parseStage;  
  IMAP_RESULTS execute(void);

public:
  LoginHandler(ImapSession *session, ParseBuffer *parseBuffer)  : ImapHandler(session), m_parseBuffer(parseBuffer), m_parseStage(0) {}
  virtual ~LoginHandler() {}
  virtual IMAP_RESULTS receiveData(INPUT_DATA_STRUCT &input);
};

#endif // !defined(_LOGINHANDLER_HPP_INCLUDED)
