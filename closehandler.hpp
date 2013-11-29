#if !defined(_CLOSEHANDLER_HPP_INCLUDED)
#define _CLOSEHANDLER_HPP_INCLUDED

#include "imaphandler.hpp"

ImapHandler *closeHandler(ImapSession *, INPUT_DATA_STRUCT &input);

class CloseHandler : public ImapHandler {
public:
  CloseHandler(ImapSession *session)  : ImapHandler(session) {}
  virtual ~CloseHandler() {}
  virtual IMAP_RESULTS receiveData(INPUT_DATA_STRUCT &input);
};

#endif // !defined(_CLOSEHANDLER_HPP_INCLUDED)
