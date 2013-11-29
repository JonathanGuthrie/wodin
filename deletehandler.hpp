#if !defined(_DELETEHANDLER_HPP_INCLUDED)
#define _DELETEHANDLER_HPP_INCLUDED

#include "imaphandler.hpp"

ImapHandler *deleteHandler(ImapSession *, INPUT_DATA_STRUCT &input);

class DeleteHandler : public ImapHandler {
private:
  ParseBuffer *m_parseBuffer;
  uint32_t m_parseStage;  
  IMAP_RESULTS execute(void);

public:
  DeleteHandler(ImapSession *session, ParseBuffer *parseBuffer)  : ImapHandler(session), m_parseBuffer(parseBuffer), m_parseStage(0) {}
  virtual ~DeleteHandler() {}
  virtual IMAP_RESULTS receiveData(INPUT_DATA_STRUCT &input);
};

#endif // !defined(_DELETEHANDLER_HPP_INCLUDED)
