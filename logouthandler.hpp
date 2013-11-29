#if !defined(_LOGOUTHANDLER_HPP_INCLUDED)
#define _LOGOUTHANDLER_HPP_INCLUDED

#include "imaphandler.hpp"

ImapHandler *logoutHandler(ImapSession *, INPUT_DATA_STRUCT &input);

class LogoutHandler : public ImapHandler {
public:
  LogoutHandler(ImapSession *session)  : ImapHandler(session) {}
  virtual ~LogoutHandler() {}
  virtual IMAP_RESULTS receiveData(INPUT_DATA_STRUCT &input);
};

#endif // !defined(_LOGOUTHANDLER_HPP_INCLUDED)
