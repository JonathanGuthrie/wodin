#if !defined(_CAPABILITYHANDLER_HPP_INCLUDED)
#define _CAPABILITYHANDLER_HPP_INCLUDED

#include "imaphandler.hpp"

ImapHandler *capabilityHandler(ImapSession *);

class CapabilityHandler : public ImapHandler {
public:
  CapabilityHandler(ImapSession *session)  : ImapHandler(session) {}
  virtual ~CapabilityHandler() {}
  virtual IMAP_RESULTS receiveData(uint8_t* pData, size_t dwDataLen);
};

#endif // !defined(_CAPABILITYHANDLER_HPP_INCLUDED)
