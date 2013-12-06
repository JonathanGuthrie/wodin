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

#if !defined(_FETCHHANDLER_HPP_INCLUDED)
#define _FETCHHANDLER_HPP_INCLUDED

#include "imaphandler.hpp"

ImapHandler *fetchHandler(ImapSession *, INPUT_DATA_STRUCT &input);
ImapHandler *uidFetchHandler(ImapSession *, INPUT_DATA_STRUCT &input);

class FetchHandler : public ImapHandler {
private:
    ParseBuffer *m_parseBuffer;
    uint32_t m_parseStage;  
    bool m_usingUid;
    IMAP_RESULTS execute(void);
    void fetchResponseInternalDate(const MailMessage *message);
    void fetchResponseRfc822(unsigned long uid, const MailMessage *message);
    void fetchResponseRfc822Header(unsigned long uid, const MailMessage *message);
    void fetchResponseRfc822Size(const MailMessage *message);
    void fetchResponseRfc822Text(unsigned long uid, const MailMessage *message);
    void fetchResponseEnvelope(const MailMessage *message);
    void fetchResponseBodyStructure(const MailMessage *message);
    void fetchResponseBody(const MailMessage *message);
    void messageChunk(unsigned long uid, size_t offset, size_t length);

public:
    static void buildSymbolTable(void);
    FetchHandler(ImapSession *session, ParseBuffer *parseBuffer, bool usingUid)  : ImapHandler(session), m_parseBuffer(parseBuffer), m_parseStage(0), m_usingUid(usingUid) {}
    virtual ~FetchHandler() {}
    virtual IMAP_RESULTS receiveData(INPUT_DATA_STRUCT &input);
};

#endif // !defined(_FETCHHANDLER_HPP_INCLUDED)
