#include "renamehandler.hpp"

ImapHandler *renameHandler(ImapSession *session, INPUT_DATA_STRUCT &input) {
  (void) input;
  return new RenameHandler(session, session->parseBuffer());
}

/*
 * In this function, parse stage 0 is before the name
 * parse stage 1 is during the name
 * parse stage 2 is after the first name, and before the second
 * parse stage 3 is during the second name
 * parse stage 4 is after the second name
 */
IMAP_RESULTS RenameHandler::receiveData(INPUT_DATA_STRUCT &input) {
  IMAP_RESULTS result = IMAP_OK;

  do {
    switch (m_parseStage) {
    case 0:
    case 2:
      switch (m_parseBuffer->astring(input, false, NULL)) {
      case ImapStringGood:
	m_parseStage += 2;
	if ((input.parsingAt < input.dataLen) && (' ' == input.data[input.parsingAt])) {
	  ++input.parsingAt;
	}
	break;

      case ImapStringBad:
	result = IMAP_BAD;
	break;

      case ImapStringPending:
	++m_parseStage;
	result = IMAP_NOTDONE;
	break;
      }
      break;

    case 1:
    case 3:
      {
	size_t dataUsed = m_parseBuffer->addLiteralToParseBuffer(input);
	if (dataUsed <= input.dataLen) {
	  ++m_parseStage;
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

    default: {
      m_parseStage = 5;
      std::string source(m_parseBuffer->arguments());
      std::string destination(m_parseBuffer->arguments()+(strlen(m_parseBuffer->arguments())+1));

      switch (m_session->mboxErrorCode(m_session->store()->renameMailbox(source, destination))) {
      case MailStore::SUCCESS:
	result = IMAP_OK;
	break;

      case MailStore::CANNOT_COMPLETE_ACTION:
	result = IMAP_TRY_AGAIN;
	break;
      }
    }
      break;
    }
  } while ((IMAP_OK == result) && (m_parseStage < 5));

  return result;
}
