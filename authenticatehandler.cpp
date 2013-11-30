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

#include "authenticatehandler.hpp"
#include "imapmaster.hpp"

ImapHandler *authenticateHandler(ImapSession *session, INPUT_DATA_STRUCT &input) {
  (void) input;
  return new AuthenticateHandler(session, session->parseBuffer());
}

IMAP_RESULTS AuthenticateHandler::receiveData(INPUT_DATA_STRUCT &input) {
  IMAP_RESULTS result; 
  switch (m_parseStage) {
  case 0:
    m_session->user(m_session->master()->userInfo(m_parseBuffer->arguments()));
    m_parseBuffer->atom(input);
    m_auth = SaslChooser(m_session->master(), m_parseBuffer->arguments());
    if (NULL != m_auth) {
      m_auth->sendChallenge(m_session->responseCode());
      m_session->responseText();
      m_parseStage = 1;
      result = IMAP_NOTDONE;
    }
    else {
      m_session->responseText("Unrecognized Authentication Method");
      result = IMAP_NO;
    }
    break;

  case 1:
    std::string str((char*)input.data, input.dataLen - 2);

    switch (m_auth->receiveResponse(str)) {
    case Sasl::ok:
      result = IMAP_OK;
      // user->GetUserFileSystem();
      m_session->user(m_session->master()->userInfo(m_auth->getUser().c_str()));

      if (NULL == m_session->store()) {
	m_session->store(m_session->master()->mailStore(m_session));
      }

      // Log("Client %u logged-in user %s %lu\n", m_dwClientNumber, m_pUser->m_szUsername.c_str(), m_pUser->m_nUserID);

      m_session->state(ImapAuthenticated);
      break;

    case Sasl::no:
      result = IMAP_NO_WITH_PAUSE;
      m_session->state(ImapNotAuthenticated);
      m_session->responseText("Authentication Failed");
      break;

    case Sasl::bad:
    default:
      result = IMAP_BAD;
      m_session->state(ImapNotAuthenticated);
      m_session->responseText("Authentication Cancelled");
      break;
    }
    delete m_auth;
    m_auth = NULL;
    break;
  }
  return result;
}
