#if !defined(_RENAMEHANDLER_HPP_INCLUDED)
#define _RENAMEHANDLER_HPP_INCLUDED

#include "imaphandler.hpp"

ImapHandler *renameHandler(ImapSession *, INPUT_DATA_STRUCT &input);

class RenameHandler : public ImapHandler {
private:
  ParseBuffer *m_parseBuffer;
  uint32_t m_parseStage;  

public:
  RenameHandler(ImapSession *session, ParseBuffer *parseBuffer)  : ImapHandler(session), m_parseBuffer(parseBuffer), m_parseStage(0) {}
  virtual ~RenameHandler() {}
  virtual IMAP_RESULTS receiveData(INPUT_DATA_STRUCT &input);
};

#endif // !defined(_RENAMEHANDLER_HPP_INCLUDED)
