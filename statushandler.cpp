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

#include "statushandler.hpp"

ImapHandler *statusHandler(ImapSession *session, INPUT_DATA_STRUCT &input) {
    (void) input;
    return new StatusHandler(session, session->parseBuffer());
}

typedef enum {
    SAV_MESSAGES = 1,
    SAV_RECENT = 2,
    SAV_UIDNEXT = 4,
    SAV_UIDVALIDITY = 8,
    SAV_UNSEEN = 16
} STATUS_ATT_VALUES;
typedef std::map<insensitiveString, STATUS_ATT_VALUES> STATUS_SYMBOL_T;

static STATUS_SYMBOL_T statusSymbolTable;


void StatusHandler::buildSymbolTable() {
    statusSymbolTable.insert(STATUS_SYMBOL_T::value_type("MESSAGES",	SAV_MESSAGES));
    statusSymbolTable.insert(STATUS_SYMBOL_T::value_type("RECENT",	SAV_RECENT));
    statusSymbolTable.insert(STATUS_SYMBOL_T::value_type("UIDNEXT",	SAV_UIDNEXT));
    statusSymbolTable.insert(STATUS_SYMBOL_T::value_type("UIDVALIDITY",	SAV_UIDVALIDITY));
    statusSymbolTable.insert(STATUS_SYMBOL_T::value_type("UNSEEN",	SAV_UNSEEN));
}


IMAP_RESULTS StatusHandler::receiveData(INPUT_DATA_STRUCT &input) {
    IMAP_RESULTS result = IMAP_OK;
    m_mailFlags = 0;

    switch (m_parseStage) {
    case 0:
	switch (m_parseBuffer->astring(input, false, NULL)) {
	case ImapStringGood:
	    m_parseStage = 2;
	    break;

	case ImapStringBad:
	    m_session->responseText("Malformed Command");
	    result = IMAP_BAD;
	    break;

	case ImapStringPending:
	    result = IMAP_NOTDONE;
	    m_session->responseText("Ready for Literal");
	    break;

	default:
	    m_session->responseText("Failed");
	    break;
	}
	break;

    case 1:
	if (0 < m_parseBuffer->literalLength()) {
	    size_t dataUsed = m_parseBuffer->addLiteralToParseBuffer(input);
	    if (dataUsed <= input.dataLen) {
		m_parseStage = 2;
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
	break;

    default:
	break;
    }
    if (2 == m_parseStage) {
	// At this point, I've seen the mailbox name.  The next stuff are all in the pData buffer.
	// I expect to see a space then a paren
	if ((2 < (input.dataLen - input.parsingAt)) && (' ' == input.data[input.parsingAt]) && ('(' == input.data[input.parsingAt+1])) {
	    input.parsingAt += 2;
	    while (input.parsingAt < (input.dataLen-1)) {
		size_t len, next;
		char *p = strchr((char *)&input.data[input.parsingAt], ' ');
		if (NULL != p) {
		    len = (p - ((char *)&input.data[input.parsingAt]));
		    next = input.parsingAt + len + 1; // This one needs to skip the space
		}
		else {
		    len = input.dataLen - input.parsingAt - 1;
		    next = input.parsingAt + len;     // this one needs to keep the parenthesis
		}
		std::string s((char *)&input.data[input.parsingAt], len);
		STATUS_SYMBOL_T::iterator i = statusSymbolTable.find(s.c_str());
		if (i != statusSymbolTable.end()) {
		    m_mailFlags |= i->second;
		}
		else {
		    m_session->responseText("Malformed Command");
		    result = IMAP_BAD;
		    // I don't need to see any more, it's all bogus
		    break;
		}
		input.parsingAt = next;
	    }
	}
	if ((result == IMAP_OK) && (')' == input.data[input.parsingAt]) && ('\0' == input.data[input.parsingAt+1])) {
	    m_parseStage = 3;
	}
	else {
	    m_session->responseText("Malformed Command");
	    result = IMAP_BAD;
	}
    }
    
    if (3 == m_parseStage) {
	std::string mailbox(m_parseBuffer->arguments());
	std::string mailboxExternal = mailbox;
	unsigned messageCount, recentCount, uidNext, uidValidity, firstUnseen;

	switch (m_session->mboxErrorCode(m_session->store()->getMailboxCounts(mailbox, m_mailFlags, messageCount, recentCount, uidNext, uidValidity, firstUnseen))) {
	case MailStore::SUCCESS:
	{
	    std::ostringstream response;
	    response << "* STATUS ";
	    response << mailboxExternal << " ";
	    char separator = '(';
	    if (m_mailFlags & SAV_MESSAGES) {
		response << separator << "MESSAGES " << messageCount;
		separator = ' ';
	    } 
	    if (m_mailFlags & SAV_RECENT) {
		response << separator << "RECENT " << recentCount;
		separator = ' ';
	    }
	    if (m_mailFlags & SAV_UIDNEXT) {
		response << separator << "UIDNEXT " << uidNext;
		separator = ' ';
	    }
	    if (m_mailFlags & SAV_UIDVALIDITY) {
		response << separator << "UIDVALIDITY " << uidValidity;
		separator = ' ';
	    }
	    if (m_mailFlags & SAV_UNSEEN) {
		response << separator << "UNSEEN " << firstUnseen;
		separator = ' ';
	    }
	    response << ")\r\n";
	    m_session->driver()->wantsToSend(response.str());
	    result = IMAP_OK;
	}
	break;

	case MailStore::CANNOT_COMPLETE_ACTION:
	    result = IMAP_TRY_AGAIN;
	    break;

	default:
	    result = IMAP_MBOX_ERROR;
	    break;
	}
    }
    return result;
}
