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

#include "storehandler.hpp"
#include "mailstore.hpp"

ImapHandler *storeHandler(ImapSession *session, INPUT_DATA_STRUCT &input) {
  (void) input;
  return new StoreHandler(session, session->parseBuffer(), false);
}

ImapHandler *uidStoreHandler(ImapSession *session, INPUT_DATA_STRUCT &input) {
  (void) input;
  return new StoreHandler(session, session->parseBuffer(), true);
}

IMAP_RESULTS StoreHandler::receiveData(INPUT_DATA_STRUCT &input) {
  IMAP_RESULTS result = IMAP_OK;
  size_t executePointer = m_parseBuffer->parsePointer();
  int update = '=';

  while((input.parsingAt < input.dataLen) && (' ' != input.data[input.parsingAt])) {
    m_parseBuffer->addToParseBuffer(input, 1, false);
  }
  m_parseBuffer->addToParseBuffer(input, 0);
  if ((input.parsingAt < input.dataLen) && (' ' == input.data[input.parsingAt])) {
    ++input.parsingAt;
  }
  else {
    m_session->responseText("Malformed Command");
    result = IMAP_BAD;
  }

  if ((IMAP_OK == result) && (('-' == input.data[input.parsingAt]) || ('+' == input.data[input.parsingAt]))) {
    update = input.data[input.parsingAt++];
  }
  while((IMAP_OK == result) && (input.parsingAt < input.dataLen) && (' ' != input.data[input.parsingAt])) {
    INPUT_DATA_STRUCT temp;
    temp.parsingAt = 0;
    temp.dataLen = 1;
    temp.data[0] = toupper(input.data[input.parsingAt++]);
    m_parseBuffer->addToParseBuffer(temp, 1, false);
  }
  m_parseBuffer->addToParseBuffer(input, 0);
  if ((input.parsingAt < input.dataLen) && (' ' == input.data[input.parsingAt])) {
    ++input.parsingAt;
  }
  else {
    m_session->responseText("Malformed Command");
    result = IMAP_BAD;
  }

  size_t myDataLen = input.dataLen;
  if ((IMAP_OK == result) && (input.parsingAt < input.dataLen) && ('(' == input.data[input.parsingAt])) {
    ++input.parsingAt;
    if (')' == input.data[input.dataLen-1]) {
      --myDataLen;
    }
    else {
      m_session->responseText("Malformed Command");
      result = IMAP_BAD;
    }
  }

  while((IMAP_OK == result) && (input.parsingAt < myDataLen)) {
    // These can only be spaces, backslashes, or alpha
    if (isalpha(input.data[input.parsingAt]) || (' ' == input.data[input.parsingAt]) || ('\\' == input.data[input.parsingAt])) {
      if (' ' == input.data[input.parsingAt]) {
	++input.parsingAt;
	m_parseBuffer->addToParseBuffer(input, 0);
      }
      else {
	INPUT_DATA_STRUCT temp;
	temp.parsingAt = 0;
	temp.dataLen = 1;
	temp.data[0] = toupper(input.data[input.parsingAt++]);
	m_parseBuffer->addToParseBuffer(temp, 1, false);
      }
    }
    else {
      m_session->responseText("Malformed Command");
      result = IMAP_BAD;
    }
  }
  m_parseBuffer->addToParseBuffer(input, 0);
  if (input.parsingAt != myDataLen) {
    m_session->responseText("Malformed Command");
    result = IMAP_BAD;
  }

  if (IMAP_OK == result) {
    SEARCH_RESULT srVector;
    bool sequenceOk;
    if (m_usingUid) {
      sequenceOk = m_session->uidSequenceSet(srVector, executePointer);
    }
    else {
      sequenceOk = m_session->msnSequenceSet(srVector, executePointer);
    }
    if (sequenceOk) {
      executePointer += strlen((char *)&m_parseBuffer[executePointer]) + 1;
      if (0 == strncmp((char *)&m_parseBuffer[executePointer], "FLAGS", 5)) {
	bool silentFlag = false;
	// I'm looking for "FLAGS.SILENT and the "+5" is because FLAGS is five characters long
	if ('.' == m_parseBuffer->parseChar(executePointer+5)) {
	  if (0 == strcmp(m_parseBuffer->parseStr(executePointer+6), "SILENT")) {
	    silentFlag = true;
	  }
	  else {
	    m_session->responseText("Malformed Command");
	    result = IMAP_BAD;
	  }
	}
	if (IMAP_OK == result) {
	  uint32_t flagSet = 0;

	  executePointer += strlen(m_parseBuffer->parseStr(executePointer)) + 1;
	  while((IMAP_OK == result) && (m_parseBuffer->parsePointer() > executePointer)) {
	    if ('\\' == m_parseBuffer->parseChar(executePointer)) {
	      FLAG_SYMBOL_T::iterator found = flagSymbolTable.find((char *)&m_parseBuffer[executePointer+1]);
	      if (flagSymbolTable.end() != found) {
		flagSet |= found->second;
	      }
	      else {
		m_session->responseText("Malformed Command");
		result = IMAP_BAD;
	      }
	    }
	    else {
	      m_session->responseText("Malformed Command");
	      result = IMAP_BAD;
	    }
	    executePointer += strlen((char *)&m_parseBuffer[executePointer]) + 1;
	  }
	  if (IMAP_OK == result) {
	    uint32_t andMask, orMask;
	    switch (update) {
	    case '+':
	      // Add these flags
	      andMask = ~0;
	      orMask = flagSet;
	      break;

	    case '-':
	      // Remove these flags
	      andMask = ~flagSet;
	      orMask = 0;
	      break;

	    default:
	      // Set these flags
	      andMask = 0;
	      orMask = flagSet;
	      break;
	    }
	    for (unsigned i=0; i < srVector.size(); ++i) {
	      if (0 != srVector[i]) {
		uint32_t flags;
		if (MailStore::SUCCESS == m_session->store()->messageUpdateFlags(srVector[i], andMask, orMask, flags)) {
		  if (!silentFlag) {
		    std::ostringstream fetch;
		    fetch << "* " << m_session->store()->mailboxUidToMsn(srVector[i]) << " FETCH (";
		    m_session->driver()->wantsToSend(fetch.str());
		    m_session->fetchResponseFlags(flags);
		    if (m_usingUid) {
		      m_session->driver()->wantsToSend(" ");
		      m_session->fetchResponseUid(srVector[i]);
		    }
		    m_session->driver()->wantsToSend(")\r\n");
		  }
		}
		else {
		  result = IMAP_MBOX_ERROR;
		}
	      }
	    }
	  }
	}
      }
      else {
	m_session->responseText("Malformed Command");
	result = IMAP_BAD;
      }
    }
    else {
      m_session->responseText("Malformed Command");
      result = IMAP_BAD;
    }
  }
  return result;
}
