#if !defined(_NOOPHANDLER_HPP_INCLUDED)
#define _NOOPHANDLER_HPP_INCLUDED

#include "imaphandler.hpp"

ImapHandler *noopHandler(ImapSession *, INPUT_DATA_STRUCT &input);

class NoopHandler : public ImapHandler {
public:
  NoopHandler(ImapSession *session)  : ImapHandler(session) {}
  virtual ~NoopHandler() {}
  virtual IMAP_RESULTS receiveData(INPUT_DATA_STRUCT &input);
};

#endif // !defined(_NOOPHANDLER_HPP_INCLUDED)
