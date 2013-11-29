#include "copyhandler.hpp"

ImapHandler *copyHandler(ImapSession *session, INPUT_DATA_STRUCT &input) {
  (void) input;
  return new CopyHandler(session, session->parseBuffer(), false);
}

ImapHandler *uidCopyHandler(ImapSession *session, INPUT_DATA_STRUCT &input) {
  (void) input;
  return new CopyHandler(session, session->parseBuffer(), true);
}

IMAP_RESULTS CopyHandler::execute() {
  IMAP_RESULTS result = IMAP_OK;
  size_t execute_pointer = 0;

  // Skip the first two tokens that are the tag and the command
  execute_pointer += strlen(m_parseBuffer->tag()) + 1;
  execute_pointer += strlen(m_parseBuffer->parsingAt()) + 1;

  // I do this once again if m_usingUid is set because the command in that case is UID
  if (m_usingUid) {
    execute_pointer += strlen(m_parseBuffer->parsingAt()) + 1;
  }

  SEARCH_RESULT vector;
  bool sequenceOk;
  if (m_usingUid) {
    sequenceOk = m_session->uidSequenceSet(vector, execute_pointer);
  }
  else {
    sequenceOk = m_session->msnSequenceSet(vector, execute_pointer);
  }
  if (sequenceOk) {
    NUMBER_LIST messageUidsAdded;
    MailStore::MAIL_STORE_RESULT lock1Result = MailStore::SUCCESS;
    MailStore::MAIL_STORE_RESULT lock2Result = MailStore::SUCCESS;

    execute_pointer += strlen(m_parseBuffer->parsingAt()) + 1;
    std::string mailboxName(m_parseBuffer->parsingAt());
    if ((MailStore::SUCCESS == (lock1Result = m_session->store()->lock())) &&
	(MailStore::SUCCESS == (lock2Result = m_session->store()->lock(mailboxName)))) {
      for (SEARCH_RESULT::iterator i=vector.begin(); (i!=vector.end()) && (IMAP_OK==result); ++i) {
	MailMessage *message;

	MailMessage::MAIL_MESSAGE_RESULT messageReadResult = m_session->store()->messageData(&message, *i);
	if (MailMessage::SUCCESS == messageReadResult) {
	  size_t newUid;
	  DateTime when = m_session->store()->messageInternalDate(*i);
	  uint32_t flags = message->messageFlags();

	  char buffer[m_session->store()->bufferLength(*i)+1];
	  switch (m_session->mboxErrorCode(m_session->store()->openMessageFile(*i))) {
	  case MailStore::SUCCESS:
	    {
	      size_t size = m_session->store()->readMessage(buffer, 0, m_session->store()->bufferLength(*i));
	      switch (m_session->mboxErrorCode(m_session->store()->addMessageToMailbox(mailboxName, (uint8_t *)buffer, size, when, flags, &newUid))) {
	      case MailStore::SUCCESS:
		m_session->store()->doneAppendingDataToMessage(mailboxName, newUid);
		messageUidsAdded.push_back(newUid);
		result = IMAP_OK;
		break;

	      case MailStore::CANNOT_COMPLETE_ACTION:
		result = IMAP_TRY_AGAIN;
		break;

	      default:
		result = IMAP_MBOX_ERROR;
		break;
	      }
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
      }
      if (IMAP_OK != result) {
	for (NUMBER_LIST::iterator i=messageUidsAdded.begin(); i!=messageUidsAdded.end(); ++i) {
	  m_session->store()->deleteMessage(mailboxName, *i);
	}
      }
    }
    else {
      if (((MailStore::CANNOT_COMPLETE_ACTION == lock1Result) ||
	   (MailStore::SUCCESS == lock1Result)) &&
	  ((MailStore::CANNOT_COMPLETE_ACTION == lock2Result) ||
	   (MailStore::SUCCESS == lock2Result))) {
	result = IMAP_TRY_AGAIN;
      }
      else {
	result = IMAP_MBOX_ERROR;
      }
    }
    m_session->store()->unlock();
    m_session->store()->unlock(mailboxName);
  }
  else {
    m_session->responseText("Malformed Command");
    result = IMAP_BAD;
  }

  return result;
}


IMAP_RESULTS CopyHandler::receiveData(INPUT_DATA_STRUCT &input) {
  IMAP_RESULTS result = IMAP_OK;

  if (0 == m_parseStage) {
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

    if (IMAP_OK == result) {
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
    }
  }
  else {
    result = IMAP_OK;
    if (1 == m_parseStage) {
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
  }
  if (IMAP_OK == result) {
    result = execute();
  }
  return result;
}
