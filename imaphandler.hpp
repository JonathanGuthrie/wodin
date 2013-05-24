#if !defined(_IMAPHANDLER_HPP_INCLUDED)
#define _IMAPHANDLER_HPP_INCLUDED

#include "imapsession.hpp"

class ImapHandler {
protected:
  ImapSession *m_session;

public:
  ImapHandler(ImapSession *session) : m_session(session) {};
  virtual ~ImapHandler() {};
  virtual IMAP_RESULTS receiveData(uint8_t* pData, size_t dwDataLen) = 0;
};

#endif // !defined(_IMAPHANDLER_HPP_INCLUDED)
