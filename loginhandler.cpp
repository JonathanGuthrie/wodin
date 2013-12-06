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

#include "loginhandler.hpp"
#include "imapmaster.hpp"

ImapHandler *loginHandler(ImapSession *session, INPUT_DATA_STRUCT &input) {
    (void) input;
    return new LoginHandler(session, session->parseBuffer());
}



/*
 * The LOGIN indicates that the session may be closed.  The main command
 * processor gets the word about this because the state is set to "4" which
 * means "login state"
 */
IMAP_RESULTS LoginHandler::execute(void) {
    IMAP_RESULTS result;

    m_session->user(m_session->master()->userInfo(m_parseBuffer->arguments()));
    bool v = m_session->user()->checkCredentials(m_parseBuffer->arguments()+(strlen(m_parseBuffer->arguments())+1));
    if (v) {
	m_session->responseText("");
	// SYZYGY LOG
	// Log("Client %u logged-in user %s %lu\n", m_dwClientNumber, m_pUser->m_szUsername.c_str(), m_pUser->m_nUserID);
	if (NULL == m_session->store()) {
	    m_session->store(m_session->master()->mailStore(m_session));
	}
	// m_session->store()->FixMailstore(); // I only un-comment this, if I have reason to believe that mailboxes are broken

	// SYZYGY LOG
	// Log("User %lu logged in\n", m_pUser->m_nUserID);
	m_session->state(ImapAuthenticated);
	result = IMAP_OK;
    }
    else {
	m_session->user(NULL);
	m_session->responseText("Authentication Failed");
	result = IMAP_NO_WITH_PAUSE;
    }
    return result;
}


/*
 * Okay, the login command's use is officially deprecated.  Instead, you're supposed
 * to use the AUTHENTICATE command with some SASL mechanism.  I will include it,
 * but I'll also include a check of the login_disabled flag, which will set whether or
 * not this command is accepted by the command processor.  Eventually, the master will
 * know whether or not to accept the login command.
 */
IMAP_RESULTS LoginHandler::receiveData(INPUT_DATA_STRUCT &input) {
    IMAP_RESULTS result = IMAP_OK;
    if (m_session->loginDisabled()) {
	m_session->responseText("Login Disabled");
	return IMAP_NO;
    }
    else {
	result = IMAP_OK;
	if (0 < m_parseBuffer->literalLength()) {
	    size_t dataUsed = m_parseBuffer->addLiteralToParseBuffer(input);
	    if (dataUsed <= input.dataLen) {
		if (2 < (input.dataLen - dataUsed)) {
		    // Get rid of the CRLF if I have it
		    input.dataLen -= 2;
		    input.data[input.dataLen] = '\0';  // Make sure it's terminated so strchr et al work
		}
	    }
	    else {
		result = IMAP_IN_LITERAL;
	    }
	}
	if ((0 == m_parseBuffer->literalLength()) && (input.parsingAt < input.dataLen)) {
	    do {
		switch (m_parseBuffer->astring(input, false, NULL)) {
		case ImapStringGood:
		    ++m_parseStage;
		    if ((input.parsingAt < input.dataLen) && (' ' == input.data[input.parsingAt])) {
			++input.parsingAt;
		    }
		    break;

		case ImapStringBad:
		    result = IMAP_BAD;
		    break;

		case ImapStringPending:
		    result = IMAP_NOTDONE;
		    break;
		}
	    } while((IMAP_OK == result) && (input.parsingAt < input.dataLen));
	}
    }

    switch(result) {
    case IMAP_OK:
	if (2 == m_parseStage) {
	    result = execute();
	}
	else {
	    m_session->responseText("Malformed Command");
	    result = IMAP_BAD;
	}
	break;

    case IMAP_NOTDONE:
	m_session->responseText("Ready for Literal");
	break;

    case IMAP_BAD:
	m_session->responseText("Malformed Command");
	break;

    case IMAP_IN_LITERAL:
	break;

    default:
	m_session->responseText("Failed");
	break;
    }
    return result;
}
