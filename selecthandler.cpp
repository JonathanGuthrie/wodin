#include "selecthandler.hpp"

ImapHandler *examineHandler(ImapSession *session, INPUT_DATA_STRUCT &input) {
  (void) input;
  return new SelectHandler(session, session->parseBuffer(), true);
}

ImapHandler *selectHandler(ImapSession *session, INPUT_DATA_STRUCT &input) {
  (void) input;
  return new SelectHandler(session, session->parseBuffer(), false);
}

IMAP_RESULTS SelectHandler::execute(void) {
  IMAP_RESULTS result = IMAP_MBOX_ERROR;

  if (ImapSelected == m_session->state()) {
    // If the mailbox is open, close it
    if (MailStore::CANNOT_COMPLETE_ACTION == m_session->store()->mailboxClose()) {
      result = IMAP_TRY_AGAIN;
    }
    else {
      m_session->state(ImapAuthenticated);
    }
  }
  if (ImapAuthenticated == m_session->state()) {
    std::string mailbox = m_parseBuffer->arguments();
    switch (m_session->mboxErrorCode(m_session->store()->mailboxOpen(mailbox, !m_readOnly))) {
    case MailStore::SUCCESS:
      m_session->selectData(mailbox, !m_readOnly);
      if (!m_readOnly) {
	m_session->responseCode("[READ-WRITE]");
      }
      else {
	m_session->responseCode("[READ-ONLY]");
      }
      result = IMAP_OK;
      m_session->state(ImapSelected);
      break;

    case MailStore::CANNOT_COMPLETE_ACTION:
      result = IMAP_TRY_AGAIN;
      break;
    }
  }

  return result;
}


IMAP_RESULTS SelectHandler::receiveData(INPUT_DATA_STRUCT &input) {
  IMAP_RESULTS result = IMAP_OK;

  switch (m_parseStage) {
  case 0:
    switch (m_parseBuffer->astring(input, false, NULL)) {
    case ImapStringGood:
      m_parseStage = 2;
      result = execute();
      break;

    case ImapStringBad:
      m_session->responseText("Malformed Command");
      result = IMAP_BAD;
      break;

    case ImapStringPending:
      result = IMAP_NOTDONE;
      ++m_parseStage;
      m_session->responseText("Ready for Literal");
      break;

    default:
      m_session->responseText("Failed");
      break;
    }
    break;

  case 1:
    {
      size_t dataUsed = m_parseBuffer->addLiteralToParseBuffer(input);
      if (dataUsed <= input.dataLen) {
	m_parseStage = 2;
	if (2 < (input.dataLen - dataUsed)) {
	  // Get rid of the CRLF if I have it
	  input.dataLen -= 2;
	  input.data[input.dataLen] = '\0';  // Make sure it's terminated so strchr et al work
	}
	result = execute();
      }
      else {
	result = IMAP_IN_LITERAL;
      }
    }
    break;

  default:
    result = execute();
    break;
  }

  return result;
}
