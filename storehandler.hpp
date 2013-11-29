#if !defined(_STOREHANDLER_HPP_INCLUDED)
#define _STOREHANDLER_HPP_INCLUDED

#include "imaphandler.hpp"

ImapHandler *storeHandler(ImapSession *, INPUT_DATA_STRUCT &input);
ImapHandler *uidStoreHandler(ImapSession *, INPUT_DATA_STRUCT &input);

class StoreHandler : public ImapHandler {
private:
  ParseBuffer *m_parseBuffer;
  bool m_usingUid;

public:
  StoreHandler(ImapSession *session, ParseBuffer *parseBuffer, bool usingUid)  : ImapHandler(session), m_parseBuffer(parseBuffer), m_usingUid(usingUid)  {}
  virtual ~StoreHandler() {}
  virtual IMAP_RESULTS receiveData(INPUT_DATA_STRUCT &input);
};

#endif // !defined(_STOREHANDLER_HPP_INCLUDED)
