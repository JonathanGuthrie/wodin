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

#include "closehandler.hpp"

ImapHandler *closeHandler(ImapSession *session, INPUT_DATA_STRUCT &input) {
  (void) input;
  return new CloseHandler(session);
}

IMAP_RESULTS CloseHandler::receiveData(INPUT_DATA_STRUCT &input) {
  // If the mailbox is open, close it
  // In IMAP, deleted messages are always purged before a close
  IMAP_RESULTS result = IMAP_TRY_AGAIN;
  if (MailStore::SUCCESS == m_session->store()->lock()) {
    NUMBER_SET purgedMessages;
    m_session->store()->listDeletedMessages(&purgedMessages);
    m_session->purgedMessages(purgedMessages);
    for(NUMBER_SET::iterator i=purgedMessages.begin(); i!=purgedMessages.end(); ++i) {
      m_session->store()->expungeThisUid(*i);
    }
    m_session->store()->mailboxClose();
    m_session->state(ImapAuthenticated);
    m_session->store()->unlock();
    result = IMAP_OK;
  }
  return result;
}
