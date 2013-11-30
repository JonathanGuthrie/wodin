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
