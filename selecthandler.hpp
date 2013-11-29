#if !defined(_SELECTHANDLER_HPP_INCLUDED)
#define _SELECTHANDLER_HPP_INCLUDED

#include "imaphandler.hpp"

ImapHandler *examineHandler(ImapSession *, INPUT_DATA_STRUCT &input);
ImapHandler *selectHandler(ImapSession *, INPUT_DATA_STRUCT &input);

class SelectHandler : public ImapHandler {
private:
  bool m_readOnly;
  ParseBuffer *m_parseBuffer;
  uint32_t m_parseStage;  
  IMAP_RESULTS execute(void);

public:
  SelectHandler(ImapSession *session, ParseBuffer *parseBuffer, bool readOnly)  : ImapHandler(session), m_readOnly(readOnly), m_parseBuffer(parseBuffer), m_parseStage(0) {}
  virtual ~SelectHandler() {}
  virtual IMAP_RESULTS receiveData(INPUT_DATA_STRUCT &input);
};

#endif // !defined(_SELECTHANDLER_HPP_INCLUDED)
