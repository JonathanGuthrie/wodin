#include "createhandler.hpp"

ImapHandler *createHandler(ImapSession *session, INPUT_DATA_STRUCT &input) {
  (void) input;
  return new CreateHandler(session, session->parseBuffer());
}

IMAP_RESULTS CreateHandler::execute(void) {
  IMAP_RESULTS result = IMAP_MBOX_ERROR;
  // SYZYGY -- check to make sure that the argument list has just the one argument
  // SYZYGY -- parsingAt should point to '\0'
  std::string mailbox(m_parseBuffer->arguments());
  switch (m_session->mboxErrorCode(m_session->store()->createMailbox(mailbox))) {
  case MailStore::SUCCESS:
    result = IMAP_OK;
    break;

  case MailStore::CANNOT_COMPLETE_ACTION:
    result = IMAP_TRY_AGAIN;
    break;
  }
  return result;
}


/*
 * In this function, parse stage 0 is before the name
 * parse stage 1 is during the name
 * parse stage 2 is after the second name
 */
IMAP_RESULTS CreateHandler::receiveData(INPUT_DATA_STRUCT &input) {
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
