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

#include "starttlshandler.hpp"
#include "imapmaster.hpp"

ImapHandler *startTlsHandler(ImapSession *session, INPUT_DATA_STRUCT &input) {
    (void) input;
    return new StartTlsHandler(session);
}

/*--------------------------------------------------------------------------------------*/
/* Handles the STARTTLS command                                                  	*/
/*--------------------------------------------------------------------------------------*/
IMAP_RESULTS StartTlsHandler::receiveData(INPUT_DATA_STRUCT &input) {
    IMAP_RESULTS result;
    (void) input;
    if (m_session->driver()->isConnectionEncrypted()) {
	m_session->responseText("Unrecognized Command");
	result = IMAP_BAD;
    }
    else {
	m_session->driver()->startTls(m_session->master()->keyfile(), m_session->master()->certfile(), m_session->master()->cafile(), m_session->master()->crlfile());
	result = IMAP_OK;
    }
    return result;
}
