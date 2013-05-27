#if !defined(_UNIMPLEMENTEDHANDLER_HPP_INCLUDED)
#define _UNIMPLEMENTEDHANDLER_HPP_INCLUDED

#include "imaphandler.hpp"

ImapHandler *unimplementedHandler(ImapSession *);

class UnimplementedHandler : public ImapHandler {
public:
  UnimplementedHandler(ImapSession *session)  : ImapHandler(session) {}
  virtual ~UnimplementedHandler() {}
  virtual IMAP_RESULTS receiveData(uint8_t* pData, size_t dwDataLen);
};

#endif // !defined(_UNIMPLEMENTEDHANDLER_HPP_INCLUDED)
