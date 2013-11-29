#if !defined(_STATUSHANDLER_HPP_INCLUDED)
#define _STATUSHANDLER_HPP_INCLUDED

#include "imaphandler.hpp"

ImapHandler *statusHandler(ImapSession *, INPUT_DATA_STRUCT &input);

class StatusHandler : public ImapHandler {
private:
  ParseBuffer *m_parseBuffer;
  uint32_t m_parseStage;
  uint32_t m_mailFlags;

public:
  StatusHandler(ImapSession *session, ParseBuffer *parseBuffer)  : ImapHandler(session), m_parseBuffer(parseBuffer), m_parseStage(0), m_mailFlags(0) {}
  virtual ~StatusHandler() {}
  virtual IMAP_RESULTS receiveData(INPUT_DATA_STRUCT &input);
  static void buildSymbolTable(void);
};

#endif // !defined(_STATUSHANDLER_HPP_INCLUDED)
