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

#if !defined(_AUTHENTICATEHANDLER_HPP_INCLUDED)
#define _AUTHENTICATEHANDLER_HPP_INCLUDED

#include "imaphandler.hpp"

ImapHandler *authenticateHandler(ImapSession *, INPUT_DATA_STRUCT &input);

class AuthenticateHandler : public ImapHandler {
private:
    uint32_t m_parseStage;
    Sasl *m_auth;
    ParseBuffer *m_parseBuffer;

public:
    AuthenticateHandler(ImapSession *session, ParseBuffer *parseBuffer)  : ImapHandler(session), m_parseStage(0), m_auth(NULL), m_parseBuffer(parseBuffer) {}
    virtual ~AuthenticateHandler() {}
    virtual IMAP_RESULTS receiveData(INPUT_DATA_STRUCT &input);
};

#endif // !defined(_AUTHENTICATEHANDLER_HPP_INCLUDED)
