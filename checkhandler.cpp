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

#include "checkhandler.hpp"

ImapHandler *checkHandler(ImapSession *session, INPUT_DATA_STRUCT &input) {
  (void) input;
  return new CheckHandler(session);
}

IMAP_RESULTS CheckHandler::receiveData(INPUT_DATA_STRUCT &input) {
  // Unlike NOOP, I always call mailboxFlushBuffers because that recalculates the the cached data.
  // That may be the only way to find out that the number of messages or the UIDNEXT value has
  // changed.
  IMAP_RESULTS result = IMAP_OK;
  NUMBER_SET purgedMessages;

  if (MailStore::SUCCESS == m_session->store()->mailboxUpdateStats(&purgedMessages)) {
    m_session->purgedMessages(purgedMessages);
  }
  m_session->store()->mailboxFlushBuffers();

  return result;
}
