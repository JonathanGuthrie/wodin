#if !defined(_CREATEHANDLER_HPP_INCLUDED)
#define _CREATEHANDLER_HPP_INCLUDED

#include "imaphandler.hpp"

ImapHandler *createHandler(ImapSession *, INPUT_DATA_STRUCT &input);

class CreateHandler : public ImapHandler {
private:
  ParseBuffer *m_parseBuffer;
  uint32_t m_parseStage;  
  IMAP_RESULTS execute(void);

public:
  CreateHandler(ImapSession *session, ParseBuffer *parseBuffer)  : ImapHandler(session), m_parseBuffer(parseBuffer), m_parseStage(0) {}
  virtual ~CreateHandler() {}
  virtual IMAP_RESULTS receiveData(INPUT_DATA_STRUCT &input);
};

#endif // !defined(_CREATEHANDLER_HPP_INCLUDED)
