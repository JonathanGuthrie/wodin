#if !defined(_CAPABILITYHANDLER_HPP_INCLUDED)
#define _CAPABILITYHANDLER_HPP_INCLUDED

#include "imaphandler.hpp"

ImapHandler *capabilityHandler(ImapSession *, INPUT_DATA_STRUCT &input);

class CapabilityHandler : public ImapHandler {
public:
  CapabilityHandler(ImapSession *session)  : ImapHandler(session) {}
  virtual ~CapabilityHandler() {}
  virtual IMAP_RESULTS receiveData(INPUT_DATA_STRUCT &input);
};

#endif // !defined(_CAPABILITYHANDLER_HPP_INCLUDED)
