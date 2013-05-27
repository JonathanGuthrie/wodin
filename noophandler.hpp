#if !defined(_NOOPHANDLER_HPP_INCLUDED)
#define _NOOPHANDLER_HPP_INCLUDED

#include "imaphandler.hpp"

ImapHandler *noopHandler(ImapSession *);

class NoopHandler : public ImapHandler {
public:
  NoopHandler(ImapSession *session)  : ImapHandler(session) {}
  virtual ~NoopHandler() {}
  virtual IMAP_RESULTS receiveData(uint8_t* pData, size_t dwDataLen);
};

#endif // !defined(_NOOPHANDLER_HPP_INCLUDED)
