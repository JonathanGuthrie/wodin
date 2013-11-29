#if !defined(_CHECKHANDLER_HPP_INCLUDED)
#define _CHECKHANDLER_HPP_INCLUDED

#include "imaphandler.hpp"

ImapHandler *checkHandler(ImapSession *, INPUT_DATA_STRUCT &input);

class CheckHandler : public ImapHandler {
public:
  CheckHandler(ImapSession *session)  : ImapHandler(session) {}
  virtual ~CheckHandler() {}
  virtual IMAP_RESULTS receiveData(INPUT_DATA_STRUCT &input);
};

#endif // !defined(_CHECKHANDLER_HPP_INCLUDED)
