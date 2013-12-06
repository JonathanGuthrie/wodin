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
