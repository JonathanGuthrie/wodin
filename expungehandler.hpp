#if !defined(_EXPUNGEHANDLER_HPP_INCLUDED)
#define _EXPUNGEHANDLER_HPP_INCLUDED

#include "imaphandler.hpp"

ImapHandler *expungeHandler(ImapSession *, INPUT_DATA_STRUCT &input);

class ExpungeHandler : public ImapHandler {
public:
  ExpungeHandler(ImapSession *session)  : ImapHandler(session) {}
  virtual ~ExpungeHandler() {}
  virtual IMAP_RESULTS receiveData(INPUT_DATA_STRUCT &input);
};

#endif // !defined(_EXPUNGEHANDLER_HPP_INCLUDED)
