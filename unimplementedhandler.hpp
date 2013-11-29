#if !defined(_UNIMPLEMENTEDHANDLER_HPP_INCLUDED)
#define _UNIMPLEMENTEDHANDLER_HPP_INCLUDED

#include "imaphandler.hpp"

ImapHandler *unimplementedHandler(ImapSession *, INPUT_DATA_STRUCT &input);

class UnimplementedHandler : public ImapHandler {
public:
  UnimplementedHandler(ImapSession *session)  : ImapHandler(session) {}
  virtual ~UnimplementedHandler() {}
  virtual IMAP_RESULTS receiveData(INPUT_DATA_STRUCT &input);
};

#endif // !defined(_UNIMPLEMENTEDHANDLER_HPP_INCLUDED)
