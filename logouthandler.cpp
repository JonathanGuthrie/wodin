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

#include "logouthandler.hpp"

ImapHandler *logoutHandler(ImapSession *session, INPUT_DATA_STRUCT &input) {
  (void) input;
  return new LogoutHandler(session);
}

/*
 * The LOGOUT indicates that the session may be closed.  The main command
 * processor gets the word about this because the state is set to "4" which
 * means "logout state"
 */
IMAP_RESULTS LogoutHandler::receiveData(INPUT_DATA_STRUCT &input) {
  // If the mailbox is open, close it
  // In IMAP, deleted messages are always purged before a close
  m_session->closeMailbox(ImapLogoff);
  m_session->driver()->wantsToSend("* BYE IMAP4rev1 server closing\r\n");
  return IMAP_OK;
}
