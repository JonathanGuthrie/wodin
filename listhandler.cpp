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

#include "listhandler.hpp"

ImapHandler *lsubHandler(ImapSession *session, INPUT_DATA_STRUCT &input) {
  (void) input;
  return new ListHandler(session, session->parseBuffer(), false);
}

ImapHandler *listHandler(ImapSession *session, INPUT_DATA_STRUCT &input) {
  (void) input;
  return new ListHandler(session, session->parseBuffer(), true);
}

static std::string imapQuoteString(const std::string &to_be_quoted) {
  std::string result("\"");
  for (int i=0; i<to_be_quoted.size(); ++i) {
    if ('\\' == to_be_quoted[i]) {
      result += '\\';
    }
    result += to_be_quoted[i];
  }
  result += '"';
  return result;
}


// Generate the mailbox flags for the list command here
static std::string genMailboxFlags(const uint32_t attr) {
  std::string result;

  if (0 != (attr & MailStore::IMAP_MBOX_NOSELECT)) {
    result = "(\\Noselect ";
  }
  else {
    if (0 != (attr & MailStore::IMAP_MBOX_MARKED)) {
      result += "(\\Marked ";
    }
    else {
      if (0 != (attr & MailStore::IMAP_MBOX_UNMARKED)) {
	result += "(\\UnMarked ";
      }
      else {
	result += "(";
      }
    }
  }
  if (0 != (attr & MailStore::IMAP_MBOX_NOINFERIORS)) {
    result += "\\NoInferiors)";
  }
  else {
    if (0 != (attr & MailStore::IMAP_MBOX_HASCHILDREN)) {
      result += "\\HasChildren)";
    }
    else {
      result += "\\HasNoChildren)";
    }
  }
  return result;
}

IMAP_RESULTS ListHandler::execute(void) {
  IMAP_RESULTS result = IMAP_MBOX_ERROR;

  std::string reference = m_parseBuffer->arguments();
  std::string mailbox = m_parseBuffer->arguments()+(strlen(m_parseBuffer->arguments())+1);

  if ((0 == reference.size()) && (0 == mailbox.size())) {
    if (m_listAll) {
      // It's a request to return the base and the hierarchy delimiter
      m_session->driver()->wantsToSend("* LIST () \"/\" \"\"\r\n");
    }
  }
  else {
    /*
     * What I think I want to do here is combine the strings by checking the last character of the 
     * reference for '.', adding it if it's not there, and then just combining the two strings.
     *
     * No, I can't do that.  I don't know what the separator is until I know what namespace it's in.
     * the best I can do is merge them and hope for the best.
     */
    reference += mailbox;
    MAILBOX_LIST mailboxList;
    m_session->store()->mailboxList(reference, &mailboxList, m_listAll);
    std::string response;
    for (MAILBOX_LIST::const_iterator i = mailboxList.begin(); i != mailboxList.end(); ++i) {
      if (m_listAll) {
	response = "* LIST ";
      }
      else {
	response = "* LSUB ";
      }
      MAILBOX_NAME which = *i;
      response += genMailboxFlags(i->attributes) + " \"/\" " + imapQuoteString(i->name) + "\r\n";
      m_session->driver()->wantsToSend(response);
    }
  }
  return IMAP_OK;
}


IMAP_RESULTS ListHandler::receiveData(INPUT_DATA_STRUCT &input) {
  IMAP_RESULTS result = IMAP_OK;
  if (0 == m_parseStage) {
    do {
      enum ImapStringState state;
      if (0 == m_parseStage) {
	state = m_parseBuffer->astring(input, false, NULL);
      }
      else {
	state = m_parseBuffer->astring(input, false, "%*");
      }
      switch (state) {
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

    default:
      m_session->responseText("Failed");
      break;
    }
  }
  else {
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
    if (0 == m_parseBuffer->literalLength()) {
      while((2 > m_parseStage) && (IMAP_OK == result) && (input.parsingAt < input.dataLen)) {
	enum ImapStringState state;
	if (0 == m_parseStage) {
	  state = m_parseBuffer->astring(input, false, NULL);
	}
	else {
	  state = m_parseBuffer->astring(input, false, "%*");
	}
	switch (state) {
	case ImapStringGood:
	  ++m_parseStage;
	  if ((input.parsingAt < input.dataLen) && (' ' == input.data[input.parsingAt])) {
	    ++input.parsingAt;
	  }
	  break;

	case ImapStringBad:
	  m_session->responseText("Malformed Command");
	  result = IMAP_BAD;
	  break;

	case ImapStringPending:
	  m_session->responseText("Ready for Literal");
	  result = IMAP_NOTDONE;
	  break;
	}
      }
      if (IMAP_OK == result) {
	if (2 == m_parseStage) {
	  result = execute();
	}
	else {
	  m_session->responseText("Malformed Command");
	  result = IMAP_BAD;
	}
      }
    }
    else {
      result = IMAP_IN_LITERAL;
      m_parseBuffer->addToParseBuffer(input, input.dataLen, false);
      m_parseBuffer->literalLength(m_parseBuffer->literalLength() - input.dataLen);
    }
  }
  return result;
}
