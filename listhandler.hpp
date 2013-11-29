#if !defined(_LISTHANDLER_HPP_INCLUDED)
#define _LISTHANDLER_HPP_INCLUDED

#include "imaphandler.hpp"

ImapHandler *lsubHandler(ImapSession *, INPUT_DATA_STRUCT &input);
ImapHandler *listHandler(ImapSession *, INPUT_DATA_STRUCT &input);

class ListHandler : public ImapHandler {
private:
  bool m_listAll;
  ParseBuffer *m_parseBuffer;
  uint32_t m_parseStage;  
  IMAP_RESULTS execute(void);

public:
  ListHandler(ImapSession *session, ParseBuffer *parseBuffer, bool listAll)  : ImapHandler(session), m_listAll(listAll), m_parseBuffer(parseBuffer), m_parseStage(0) {}
  virtual ~ListHandler() {}
  virtual IMAP_RESULTS receiveData(INPUT_DATA_STRUCT &input);
};

#endif // !defined(_LISTHANDLER_HPP_INCLUDED)
