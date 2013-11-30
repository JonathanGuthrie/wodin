/*
 * Copyright 2013 Jonathan R. Guthrie
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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