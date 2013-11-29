#if !defined(_SEARCHHANDLER_HPP_INCLUDED)
#define _SEARCHHANDLER_HPP_INCLUDED

#include "imaphandler.hpp"
#include "mailsearch.hpp"

ImapHandler *searchHandler(ImapSession *, INPUT_DATA_STRUCT &input);
ImapHandler *uidSearchHandler(ImapSession *, INPUT_DATA_STRUCT &input);

class MailSearch;

class SearchHandler : public ImapHandler {
private:
  ParseBuffer *m_parseBuffer;
  uint32_t m_parseStage;  
  bool m_usingUid;
  IMAP_RESULTS execute(void);
  IMAP_RESULTS searchKeyParse(INPUT_DATA_STRUCT &input);
  bool updateSearchTerms(MailSearch &searchTerm, size_t &tokenPointer, bool isSubExpression);
  bool updateSearchTerm(MailSearch &searchTerm, size_t &tokenPointer);


public:
  static void buildSymbolTable(void);
  SearchHandler(ImapSession *session, ParseBuffer *parseBuffer, bool usingUid)  : ImapHandler(session), m_parseBuffer(parseBuffer), m_parseStage(0), m_usingUid(usingUid) {}
  virtual ~SearchHandler() {}
  virtual IMAP_RESULTS receiveData(INPUT_DATA_STRUCT &input);
};

#endif // !defined(_SEARCHHANDLER_HPP_INCLUDED)
