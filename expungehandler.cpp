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

#include "expungehandler.hpp"

ImapHandler *expungeHandler(ImapSession *session, INPUT_DATA_STRUCT &input) {
    (void) input;
    return new ExpungeHandler(session);
}

IMAP_RESULTS ExpungeHandler::receiveData(INPUT_DATA_STRUCT &input) {
    (void) input;

    IMAP_RESULTS result = IMAP_MBOX_ERROR;
    NUMBER_SET purgedMessages;

    switch (m_session->mboxErrorCode(m_session->store()->listDeletedMessages(&purgedMessages))) {
    case MailStore::SUCCESS:
	m_session->purgedMessages(purgedMessages);
	result = IMAP_OK;
	break;

    case MailStore::CANNOT_COMPLETE_ACTION:
	result = IMAP_TRY_AGAIN;
	break;

    default:
	result = IMAP_NO;
	break;
    }

    return result;
}
