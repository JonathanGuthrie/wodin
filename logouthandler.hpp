#if !defined(_LOGOUTHANDLER_HPP_INCLUDED)
#define _LOGOUTHANDLER_HPP_INCLUDED

#include "imaphandler.hpp"

ImapHandler *logoutHandler(ImapSession *);

class LogoutHandler : public ImapHandler {
public:
  LogoutHandler(ImapSession *session)  : ImapHandler(session) {}
  virtual ~LogoutHandler() {}
  virtual IMAP_RESULTS receiveData(uint8_t* pData, size_t dwDataLen);
};

#endif // !defined(_LOGOUTHANDLER_HPP_INCLUDED)
