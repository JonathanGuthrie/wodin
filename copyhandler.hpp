#if !defined(_COPYHANDLER_HPP_INCLUDED)
#define _COPYHANDLER_HPP_INCLUDED

#include "imaphandler.hpp"

ImapHandler *copyHandler(ImapSession *, INPUT_DATA_STRUCT &input);
ImapHandler *uidCopyHandler(ImapSession *, INPUT_DATA_STRUCT &input);

class CopyHandler : public ImapHandler {
private:
  ParseBuffer *m_parseBuffer;
  uint32_t m_parseStage;  
  bool m_usingUid;
  IMAP_RESULTS execute(void);

public:
  CopyHandler(ImapSession *session, ParseBuffer *parseBuffer, bool usingUid)  : ImapHandler(session), m_parseBuffer(parseBuffer), m_parseStage(0), m_usingUid(usingUid) {}
  virtual ~CopyHandler() {}
  virtual IMAP_RESULTS receiveData(INPUT_DATA_STRUCT &input);
};

#endif // !defined(_COPYHANDLER_HPP_INCLUDED)
