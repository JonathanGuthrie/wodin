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

#if !defined(_STOREHANDLER_HPP_INCLUDED)
#define _STOREHANDLER_HPP_INCLUDED

#include "imaphandler.hpp"

ImapHandler *storeHandler(ImapSession *, INPUT_DATA_STRUCT &input);
ImapHandler *uidStoreHandler(ImapSession *, INPUT_DATA_STRUCT &input);

class StoreHandler : public ImapHandler {
private:
  ParseBuffer *m_parseBuffer;
  bool m_usingUid;

public:
  StoreHandler(ImapSession *session, ParseBuffer *parseBuffer, bool usingUid)  : ImapHandler(session), m_parseBuffer(parseBuffer), m_usingUid(usingUid)  {}
  virtual ~StoreHandler() {}
  virtual IMAP_RESULTS receiveData(INPUT_DATA_STRUCT &input);
};

#endif // !defined(_STOREHANDLER_HPP_INCLUDED)