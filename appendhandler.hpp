#if !defined(_APPENDHANDLER_HPP_INCLUDED)
#define _APPENDHANDLER_HPP_INCLUDED

#include "imaphandler.hpp"

ImapHandler *appendHandler(ImapSession *, INPUT_DATA_STRUCT &input);

class AppendHandler : public ImapHandler {
private:
  ParseBuffer *m_parseBuffer;
  uint32_t m_parseStage;  
  IMAP_RESULTS execute(INPUT_DATA_STRUCT &input);
  uint32_t m_mailFlags;
  std::string m_tempMailboxName;
  size_t m_appendingUid;
  Namespace *m_store;

public:
  AppendHandler(ImapSession *session, ParseBuffer *parseBuffer, Namespace *store)  : ImapHandler(session), m_parseBuffer(parseBuffer), m_parseStage(0), m_tempMailboxName(""),m_appendingUid(-1),m_store(store) {}
  virtual ~AppendHandler() {}
  virtual IMAP_RESULTS receiveData(INPUT_DATA_STRUCT &input);
};

#endif // !defined(_APPENDHANDLER_HPP_INCLUDED)
