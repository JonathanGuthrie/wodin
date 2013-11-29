#if !defined(_SUBSCRIBEHANDLER_HPP_INCLUDED)
#define _SUBSCRIBEHANDLER_HPP_INCLUDED

#include "imaphandler.hpp"

ImapHandler *unsubscribeHandler(ImapSession *, INPUT_DATA_STRUCT &input);
ImapHandler *subscribeHandler(ImapSession *, INPUT_DATA_STRUCT &input);

class SubscribeHandler : public ImapHandler {
private:
  bool m_subscribe;
  ParseBuffer *m_parseBuffer;
  uint32_t m_parseStage;  
  IMAP_RESULTS execute(void);

public:
  SubscribeHandler(ImapSession *session, ParseBuffer *parseBuffer, bool subscribe)  : ImapHandler(session), m_subscribe(subscribe), m_parseBuffer(parseBuffer), m_parseStage(0) {}
  virtual ~SubscribeHandler() {}
  virtual IMAP_RESULTS receiveData(INPUT_DATA_STRUCT &input);
};

#endif // !defined(_SUBSCRIBEHANDLER_HPP_INCLUDED)
