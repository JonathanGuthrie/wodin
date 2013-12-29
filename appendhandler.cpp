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

#include "appendhandler.hpp"
#include "imapmaster.hpp"

ImapHandler *appendHandler(ImapSession *session, INPUT_DATA_STRUCT &input) {
    (void) input;
    return new AppendHandler(session, session->parseBuffer(), session->store());
}


// Parse stage is 0 if it doesn't have a mailbox name, and 1 if it is waiting for one, 2 if the name has arrived, and 3 if it's receiving message data.
// The thing is, execute is only called when the parse stage is 2 because execute's job is to parse the date and flags, if any, and create a new message
// to which to begin appending data
IMAP_RESULTS AppendHandler::execute(INPUT_DATA_STRUCT &input) {
    IMAP_RESULTS result = IMAP_BAD;
    DateTime messageDateTime;

    if ((2 < (input.dataLen - input.parsingAt)) && (' ' == input.data[input.parsingAt++])) {
	if ('(' == input.data[input.parsingAt]) {
	    ++input.parsingAt;
	    bool flagsOk;
	    m_mailFlags = m_parseBuffer->readEmailFlags(input, flagsOk);
	    if (flagsOk && (2 < (input.dataLen - input.parsingAt)) &&
		(')' == input.data[input.parsingAt]) && (' ' == input.data[input.parsingAt+1])) {
		input.parsingAt += 2;
	    }
	    else {
		m_session->responseText("Malformed Command");
		return IMAP_BAD;
	    }
	}
	else {
	    m_mailFlags = 0;
	}

	if ('"' == input.data[input.parsingAt]) {
	    ++input.parsingAt;
	    // The constructor for DateTime swallows both the leading and trailing
	    // double quote characters
	    if (messageDateTime.parse(input.data, input.dataLen, input.parsingAt, DateTime::IMAP) &&
		('"' == input.data[input.parsingAt]) &&
		(' ' == input.data[input.parsingAt+1])) {
		input.parsingAt += 2;
	    }
	    else {
		m_session->responseText("Malformed Command");
		return IMAP_BAD;
	    }
	}

	if ((2 < (input.dataLen - input.parsingAt)) && ('{' == input.data[input.parsingAt])) {
	    // It's a literal string
	    input.parsingAt++; // Skip the curly brace
	    char *close;
	    // read the number
	    m_parseBuffer->literalLength(strtoul((char *)&input.data[input.parsingAt], &close, 10));
	    // check for the close curly brace and end of message and to see if I can fit all that data
	    // into the buffer
	    size_t lastChar = (size_t) (close - ((char *)input.data));
	    if ((NULL == close) || ('}' != close[0]) || (lastChar != (input.dataLen - 1))) {
		m_session->responseText("Malformed Command");
	    }
	    else {
		m_parseStage = 2;
		result = IMAP_OK;
	    }
	}
	else {
	    m_session->responseText("Malformed Command");
	    return IMAP_BAD;
	}
    }
    else {
	m_session->responseText("Malformed Command");
	return IMAP_BAD;
    }

    MailStore::MAIL_STORE_RESULT mlr;
    m_tempMailboxName = m_parseBuffer->arguments();
    if (MailStore::SUCCESS == (mlr = m_store->lock(m_tempMailboxName))) {
		
	switch (m_store->addMessageToMailbox(m_tempMailboxName, m_session->lineBuffer(), 0, messageDateTime, m_mailFlags, &m_appendingUid)) {
	case MailStore::SUCCESS:
	    m_session->responseText("Ready for the Message Data");
	    m_parseStage = 3;
	    result = IMAP_NOTDONE;
	    break;

	case MailStore::CANNOT_COMPLETE_ACTION:
	    result = IMAP_TRY_AGAIN;
	    break;

	default:
	    result = IMAP_MBOX_ERROR;
	    break;
	}
    }
    else {
	result = IMAP_MBOX_ERROR;
	if (MailStore::CANNOT_COMPLETE_ACTION == mlr) {
	    result = IMAP_TRY_AGAIN;
	}
    }
    return result;
}


IMAP_RESULTS AppendHandler::receiveData(INPUT_DATA_STRUCT &input) {
    IMAP_RESULTS result = IMAP_OK;

    switch (m_parseStage) {
    case 0:
	switch (m_parseBuffer->astring(input, false, NULL)) {
	case ImapStringGood:
	    m_parseStage = 2;
	    result = execute(input);
	    break;

	case ImapStringBad:
	    m_session->responseText("Malformed Command");
	    result = IMAP_BAD;
	    break;

	case ImapStringPending:
	    result = IMAP_NOTDONE;
	    m_parseStage = 1;
	    break;

	default:
	    m_session->responseText("Failed -- internal error");
	    result = IMAP_NO;
	    break;
	}
	break;

    case 1:
	// It's the mailbox name that's arrived
    {
	size_t dataUsed = m_parseBuffer->addLiteralToParseBuffer(input);
	if (0 == m_parseBuffer->literalLength()) {
	    m_parseStage = 2;
	    if (2 < (input.dataLen - dataUsed)) {
		// Get rid of the CRLF if I have it
		input.dataLen -= 2;
		input.data[input.dataLen] = '\0';  // Make sure it's terminated so strchr et al work
	    }
	    result = execute(input);
	}
	else {
	    result = IMAP_IN_LITERAL;
	}
    }
    break;

    case 2:
    {
	result = execute(input);
    }
    break;

    case 3:
    {
	size_t residue;
	// It's the message body that's arrived
	size_t len = MIN(m_parseBuffer->literalLength(), input.dataLen);
	if (m_parseBuffer->literalLength() > input.dataLen) {
	    result = IMAP_IN_LITERAL;
	    residue = m_parseBuffer->literalLength() - input.dataLen;
	}
	else {
	    residue = 0;
	}
	m_parseBuffer->literalLength(residue);
	std::string mailbox(m_parseBuffer->arguments());
	switch (m_store->appendDataToMessage(mailbox, m_appendingUid, input.data, len)) {
	case MailStore::SUCCESS:
	    if (0 == m_parseBuffer->literalLength()) {
		m_parseStage = 4;
		switch(m_store->doneAppendingDataToMessage(mailbox, m_appendingUid)) {
		case MailStore::SUCCESS:
		    if (MailStore::SUCCESS == m_store->unlock(m_tempMailboxName)) {
			result = IMAP_OK;
		    }
		    else {
			m_store->deleteMessage(mailbox, m_appendingUid);
			result = IMAP_MBOX_ERROR;
		    }
		    m_appendingUid = 0;
		    result = IMAP_OK;
		    break;

		case MailStore::CANNOT_COMPLETE_ACTION:
		    result = IMAP_TRY_AGAIN;
		    break;

		default:
		    m_store->deleteMessage(mailbox, m_appendingUid);
		    m_appendingUid = 0;
		    result = IMAP_MBOX_ERROR;
		}
	    }
	    break;

	case MailStore::CANNOT_COMPLETE_ACTION:
	    result = IMAP_TRY_AGAIN;
	    break;

	default:
	    m_store->doneAppendingDataToMessage(mailbox, m_appendingUid);
	    m_store->deleteMessage(mailbox, m_appendingUid);
	    m_store->unlock(m_tempMailboxName);
	    result = IMAP_MBOX_ERROR;
	    m_appendingUid = 0;
	    m_parseBuffer->literalLength(0);
	    break;
	}
    }
    break;

    case 4:
	// I've got the last of the data, and I'm retrying the done appending code
    {
	std::string mailbox(m_parseBuffer->arguments());
	switch(m_store->doneAppendingDataToMessage(mailbox, m_appendingUid)) {
	case MailStore::SUCCESS:
	    if (MailStore::SUCCESS == m_store->unlock(m_tempMailboxName)) {
		result = IMAP_OK;
	    }
	    else {
		m_store->deleteMessage(mailbox, m_appendingUid);
		result = IMAP_MBOX_ERROR;
	    }
	    m_appendingUid = 0;
	    break;

	case MailStore::CANNOT_COMPLETE_ACTION:
	    result = IMAP_TRY_AGAIN;
	    break;

	default:
	    m_store->deleteMessage(mailbox, m_appendingUid);
	    m_appendingUid = 0;
	    result = IMAP_MBOX_ERROR;
	}
	m_tempMailboxName = "";
    }
    break;

    default:
	m_session->responseText("Failed -- internal error");
	result = IMAP_NO;
	break;
    }
    return result;
}
