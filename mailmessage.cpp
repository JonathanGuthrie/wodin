#include "iostream"

#include "mailmessage.hpp"
#include "mailstore.hpp"

#include <string.h>

typedef enum {
  DATE_FIELD,
  SUBJECT_FIELD,
  IN_REPLY_TO_FIELD,
  MESSAGE_ID_FIELD,
  FROM_FIELD,
  SENDER_FIELD,
  REPLY_TO_FIELD,
  TO_FIELD,
  CC_FIELD,
  BCC_FIELD,
  MIME_VERSION_FIELD,
  CONTENT_TYPE_FIELD,
  CONTENT_ENCODING_FIELD,
  CONTENT_ID_FIELD,
  CONTENT_DESCRIPTION_FIELD
} KNOWN_FIELDS;

typedef std::map<insensitiveString, KNOWN_FIELDS> FIELD_SYMBOL_T;
static FIELD_SYMBOL_T fieldSymbolTable;

typedef std::map<insensitiveString, MIME_MEDIA_TYPES> MEDIA_SYMBOL_T;
static MEDIA_SYMBOL_T mediaTypeSymbolTable;

void MailMessage::buildSymbolTable() {
  fieldSymbolTable.insert(FIELD_SYMBOL_T::value_type("DATE",         DATE_FIELD));
  fieldSymbolTable.insert(FIELD_SYMBOL_T::value_type("SUBJECT",      SUBJECT_FIELD));
  fieldSymbolTable.insert(FIELD_SYMBOL_T::value_type("IN-REPLY-TO",  IN_REPLY_TO_FIELD));
  fieldSymbolTable.insert(FIELD_SYMBOL_T::value_type("MESSAGE-ID",   MESSAGE_ID_FIELD));
  fieldSymbolTable.insert(FIELD_SYMBOL_T::value_type("FROM",         FROM_FIELD));
  fieldSymbolTable.insert(FIELD_SYMBOL_T::value_type("SENDER",       SENDER_FIELD));
  fieldSymbolTable.insert(FIELD_SYMBOL_T::value_type("REPLY-TO",     REPLY_TO_FIELD));
  fieldSymbolTable.insert(FIELD_SYMBOL_T::value_type("TO",           TO_FIELD));
  fieldSymbolTable.insert(FIELD_SYMBOL_T::value_type("CC",           CC_FIELD));
  fieldSymbolTable.insert(FIELD_SYMBOL_T::value_type("BCC",          BCC_FIELD));
  fieldSymbolTable.insert(FIELD_SYMBOL_T::value_type("MIME-VERSION", MIME_VERSION_FIELD));
  fieldSymbolTable.insert(FIELD_SYMBOL_T::value_type("CONTENT-TYPE", CONTENT_TYPE_FIELD));
  fieldSymbolTable.insert(FIELD_SYMBOL_T::value_type("CONTENT-TRANSFER-ENCODING", CONTENT_ENCODING_FIELD));
  fieldSymbolTable.insert(FIELD_SYMBOL_T::value_type("CONTENT-ID",   CONTENT_ID_FIELD));
  fieldSymbolTable.insert(FIELD_SYMBOL_T::value_type("CONTENT-DESCRIPTION", CONTENT_DESCRIPTION_FIELD));

  mediaTypeSymbolTable.insert(MEDIA_SYMBOL_T::value_type("TEXT",        MIME_TYPE_TEXT));
  mediaTypeSymbolTable.insert(MEDIA_SYMBOL_T::value_type("IMAGE",       MIME_TYPE_IMAGE));
  mediaTypeSymbolTable.insert(MEDIA_SYMBOL_T::value_type("AUDIO",       MIME_TYPE_AUDIO));
  mediaTypeSymbolTable.insert(MEDIA_SYMBOL_T::value_type("VIDEO",       MIME_TYPE_VIDEO));
  mediaTypeSymbolTable.insert(MEDIA_SYMBOL_T::value_type("APPLICATION", MIME_TYPE_APPLICATION));
  mediaTypeSymbolTable.insert(MEDIA_SYMBOL_T::value_type("MULTIPART",   MIME_TYPE_MULTIPART));
  mediaTypeSymbolTable.insert(MEDIA_SYMBOL_T::value_type("MESSAGE",     MIME_TYPE_MESSAGE));
}

int month_name_to_month(const char *name) {
  int result = -1;
  if (2 < strlen(name)) {
    switch(toupper(name[0])) {
    case 'J':
      // It's jan, jun, or jul
      if ('U' == toupper(name[1])) {
	if ('L' == toupper(name[2])) {
	  result = 6;
	}
	else {
	  if ('N' == toupper(name[2])) {
	    result = 5;
	  }
	}
      }
      else {
	if (('A' == toupper(name[1])) && ('N' == toupper(name[2]))) {
	  result = 0;
	}
      }
      break;

    case 'F':
      if (('E' == toupper(name[1])) && ('B' == toupper(name[2]))) {
	result = 1;
      }
      break;

    case 'M':
      // it's mar or may
      if ('A' == toupper(name[1])) {
	if ('R' == toupper(name[2])) {
	  result = 2;
	}
	else {
	  if ('Y' == toupper(name[2])) {
	    result = 4;
	  }
	}
      }
      break;

    case 'A':
      // it's apr or aug
      if ('U' == toupper(name[1])) {
	if ('G' == toupper(name[2])) {
	  result = 7;
	}
      }
      else {
	if (('P' == toupper(name[1])) && ('R' == toupper(name[2]))) {
	  result = 3;
	}
      }
      break;

    case 'S':
      // it's sep
      if (('E' == toupper(name[1])) && ('P' == toupper(name[2]))) {
	result = 8;
      }
      break;

    case 'O':
      // it's oct
      if (('C' == toupper(name[1])) && ('T' == toupper(name[2]))) {
	result = 9;
      }
      break;

    case 'N':
      // it's nov
      if (('O' == toupper(name[1])) && ('V' == toupper(name[2]))) {
	result = 10;
      }
      break;

    case 'D':
      // it's dec
      if (('E' == toupper(name[1])) && ('C' == toupper(name[2]))) {
	result = 11;
      }
      break;

    default:
      break;
    }
  }
  return result;
}

#define SPACE " \t\v\r\n\f"

static MIME_MEDIA_TYPES GetMediaType(const insensitiveString &contentTypeLine) {
  MIME_MEDIA_TYPES result;

  if (0 < contentTypeLine.size()) {
    insensitiveString work = contentTypeLine;
    int pos = work.find('/');
    if (std::string::npos != pos) {
      int end = work.find_last_not_of(SPACE, pos-1);
      int begin = work.find_first_not_of(SPACE);

      work = work.substr(begin, end-begin+1);
    }
    else {
      pos = work.find(';');
      // if ';' is not found, then pos = std::string:npos, which is always bigger than the string, so it will trim the spaces off the end
      // of the whole thing.  So, I don't need to see if semicolon failed here
      int end = work.find_last_not_of(SPACE, pos-1);
      int begin = work.find_first_not_of(SPACE);
      work = work.substr(begin, end-begin+1);
    }

    MEDIA_SYMBOL_T::iterator which = mediaTypeSymbolTable.find(work);
    if (mediaTypeSymbolTable.end() != which) {
      result = which->second;
    }
    else {
      result = MIME_TYPE_UNKNOWN;
    }
  }
  else {
    result = MIME_TYPE_TEXT;
  }
  return result;
}

std::string GetMediaBoundary(const insensitiveString &contentTypeLine) {
  std::string result;
  insensitiveString work = contentTypeLine;

  int pos = work.find("boundary=");
  if (std::string::npos != pos) {
    int end = work.find_last_not_of(SPACE);
    int begin = work.find_first_not_of(SPACE, pos+9);

    result = contentTypeLine.substr(begin, end-begin+1).c_str();
    pos = result.find(';');
    if (std::string::npos != pos) {
      result = result.substr(0, pos);
    }
    if ('"' == result[0]) {
      result = result.substr(1, result.size()-2);
    }
  }
  else {
    result = "";
  }
  return result;
}

// ProcessHeaderLine figures out whether or not the header line in question is something
// I consider special.  If it is, then the string that is the value part of the line
// is saved.  It isn't converted to anything, just saved.
bool MailMessage::processHeaderLine(const insensitiveString &line) {
  bool result = true;

  if (0 != line.size()) {
    int colon = line.find(':');

    if (std::string::npos != colon) {
      int end = line.find_last_not_of(SPACE);
      int begin = line.find_first_not_of(SPACE, colon+1);
      insensitiveString right;
      if (std::string::npos != begin) {
	right = line.substr(begin, end-begin+1);
      }
      else {
	right = "";
      }
      FIELD_SYMBOL_T::iterator field = fieldSymbolTable.find(line.substr(0, colon));
      if (field != fieldSymbolTable.end()) {
	// None of these fields permit multiple values.
	switch(field->second) {
	case DATE_FIELD:
	  m_dateLine = right;
	  break;

	case SUBJECT_FIELD:
	  m_subject = right;
	  break;

	case IN_REPLY_TO_FIELD:
	  m_inReplyTo = right;
	  break;

	case MESSAGE_ID_FIELD:
	  m_messageId = right;
	  break;

	case FROM_FIELD:
	  m_fromLine = right;
	  break;

	case SENDER_FIELD:
	  m_senderLine = right;
	  break;

	case REPLY_TO_FIELD:
	  m_replyToLine = right;
	  break;

	case TO_FIELD:
	  m_toLine = right;
	  break;

	case CC_FIELD:
	  m_ccLine = right;
	  break;

	case BCC_FIELD:
	  m_bccLine = right;
	  break;

	case CONTENT_TYPE_FIELD:
	  m_mainBody.contentTypeLine = right;
	  break;

	case CONTENT_ENCODING_FIELD:
	  m_mainBody.contentEncodingLine = right;
	  break;

	case CONTENT_ID_FIELD:
	  m_mainBody.contentIdLine = right;
	  break;

	case CONTENT_DESCRIPTION_FIELD:
	  m_mainBody.contentDescriptionLine = right;
	  break;

	default:
	  break;
	}
      }
      // No matter what, always add it to the generic field list
      m_mainBody.fieldList.insert(HEADER_FIELDS::value_type(line.substr(0, colon), right));
    }
    else {
      result = false;
    }
  }
  return result;
}

bool ProcessSubpartHeaderLine(const insensitiveString &line, MESSAGE_BODY &body) {
  bool result = true;

  if (0 != line.size()) {
    int colon = line.find(':');
    if (std::string::npos != colon) {
      int begin = line.find_first_not_of(SPACE, colon+1);
      int end = line.find_last_not_of(SPACE);
      insensitiveString right;
      if (std::string::npos != begin) {
	right = line.substr(begin, end-begin+1);
      }
      else {
	right = "";
      }
      FIELD_SYMBOL_T::iterator field = fieldSymbolTable.find(line.substr(0, colon));
      if (field != fieldSymbolTable.end()) {
	// None of these fields permit multiple values.
	switch(field->second) {
	case CONTENT_TYPE_FIELD:
	  body.contentTypeLine = right;
	  break;

	case CONTENT_ENCODING_FIELD:
	  body.contentEncodingLine = right;
	  break;

	case CONTENT_ID_FIELD:
	  body.contentIdLine = right;
	  break;

	case CONTENT_DESCRIPTION_FIELD:
	  body.contentDescriptionLine = right;
	  break;

	default:
	  break;
	}
      }
      // No matter what, always add it to the generic field list
      body.fieldList.insert(HEADER_FIELDS::value_type(line.substr(0, colon), right));
    }
    else {
      result = false;
    }
  }
  return result;
}


// When this is called, it parsePointer points to the first line of the body
// When it returns, parsePointer must point to the line after the end of the body.  If it's a subpart, then parsePointer points to the
//    first character of the separator line that ended everything.
void MailMessage::parseBodyParts(bool loadBinaryParts, MESSAGE_BODY &parentBody, char *messageBuffer, size_t &parsePointer,
				 const char *parentSeparator, int nestingDepth) {
  parentBody.bodyMediaType = GetMediaType(parentBody.contentTypeLine);
  switch(parentBody.bodyMediaType) {
  case MIME_TYPE_MULTIPART: {
    char *eol;
    std::string separator = GetMediaBoundary(parentBody.contentTypeLine);
    if (0 < separator.size()) {
      // When I get here, I know that I have a read the header for the (sub)part, that the body that I'm
      // reading is a multipart message, and that I have a valid separator string.  I need to read the 
      // lines as they come in and process them accordingly

      // This part happens before I see the first separator.  When it's done, all the characters (including
      // those for the separator line) are added to the bodyOctets total of the parent and all the lines
      // (including the one for the separator line) are added to the bodyLines total of the parent and
      // parsePointer should point at the beginning of the separator line.  The next part will determine whether
      // it's the end separator or not.
      int notdone = true;
      while (notdone) {
	notdone = false;
	if ((NULL != (eol = strchr(&messageBuffer[parsePointer], '\r')) && ('\n' == eol[1]))) {
	  int lineLength = 2 + (eol - &messageBuffer[parsePointer]);
	  parentBody.bodyOctets += lineLength;
	  parentBody.bodyLines++;
	  if (('-' != messageBuffer[parsePointer]) ||
	      ('-' != messageBuffer[parsePointer+1]) ||
	      (0 != strncmp(&messageBuffer[parsePointer+2], separator.c_str(), separator.size()))) {
	    notdone = true;
	    parsePointer += lineLength;
	  }
	}
      }

      // If I get here, I've seen the beginning of a separator line.  I need to check to see if it marks
      // the end of the subparts with this separator.  At this point, parsePointer points to the
      // beginning of the separator line, but the characters in that line have already been charged to the
      // parent.
      notdone = (NULL != (eol = strchr(&messageBuffer[parsePointer], '\r')) && ('\n' == eol[1]));
      while (notdone) {
	// std::cout << "Parsing at at octet " << parsePointer << std::endl;
	int lineLength = 2 + (eol - &messageBuffer[parsePointer]);
	if ((6 + separator.size() != lineLength) ||
	    ('-' != messageBuffer[parsePointer+2+separator.size()]) ||
	    ('-' != messageBuffer[parsePointer+3+separator.size()])) {
	  // If I'm here, I know that it is not the final terminator.   I need to accumulate lines of
	  // the subpart header until I see the blank line that ends them.  I've already accounted for
	  // the characters in the separator, what I have to do now is skip past the end and begin
	  // processing data.
	  insensitiveString unfoldedLine;
	  MESSAGE_BODY childBody;

	  parsePointer += lineLength;
	  childBody.subparts = NULL;
	  childBody.bodyLines = 0;
	  childBody.bodyOctets = 0;
	  childBody.headerOctets = 0;
	  // childBody.bodyStartOffset = sectionStartOffset + parentBody.bodyOctets;
	  childBody.bodyStartOffset = parsePointer;
	  // When I get here, parsePointer has to point to a line that hasn't been looked at
	  // before.
	  while ((NULL != (eol = strchr(&messageBuffer[parsePointer], '\r')) && ('\n' == eol[1])) &&
		 (('-' != messageBuffer[parsePointer]) ||
		  ('-' != messageBuffer[parsePointer+1]) ||
		  (0 != strncmp(&messageBuffer[parsePointer+2], separator.c_str(), separator.size())))) {
	    int lineLength = 2 + (eol - &messageBuffer[parsePointer]);
	    childBody.bodyOctets += lineLength;
	    childBody.bodyLines++;
	    if (2 >= lineLength) {
	      // Handle the body of the part.  First, move past the end of the header and parsePointer points
	      // to the first character of the line before the body
	      parsePointer += lineLength;
	      // now parsePointer points to the first character of the body
	      // I've still got the last header line to deal with, so I deal with it now.
	      ProcessSubpartHeaderLine(unfoldedLine, childBody);
	      // if I don't yet have any subparts, I create a new vector to hold them
	      if (NULL == parentBody.subparts) {
		parentBody.subparts = new std::vector<MESSAGE_BODY>;
	      }
	      // Since the body includes the header, and since I'm now done with the header, the
	      // size of the header can be recorded as the current size of the body.
	      childBody.headerOctets = childBody.bodyOctets;
	      childBody.headerLines = childBody.bodyLines;
	      parseBodyParts(loadBinaryParts, childBody, messageBuffer, parsePointer, separator.c_str(), nestingDepth+1);
	      // When I get here, I've seen a separator, the characters in the separator have been included
	      // in the count for the child body, but parsePointer points to the CRLF before the separator
	      // line.
	      parentBody.subparts->push_back(childBody);
	      // Everything in the child is also part of the parent
	      parentBody.bodyOctets += childBody.bodyOctets;
	      parentBody.bodyLines += childBody.bodyLines;

	      if (('\r' == messageBuffer[parsePointer]) &&
		  ('\n' == messageBuffer[parsePointer+1]) &&
		  (NULL != (eol = strchr(&messageBuffer[parsePointer+2], '\r'))) &&
		  ('\n' == eol[1])) {
		parentBody.bodyOctets += 2 + (eol - &messageBuffer[parsePointer]);
		parentBody.bodyLines += 2;
		// I adjust parsePointer, but only to advance it past the CRLF before
		// the separator.  The separator itself is advanced past elsewhere
		parsePointer += 2;
	      }
	      eol = strchr(&messageBuffer[parsePointer], '\r');
	      notdone = (eol != NULL);
	      break;
	    }
	    else {
	      // Handle the header of the part
	      if ((' ' == messageBuffer[parsePointer]) || ('\t' == messageBuffer[parsePointer])) {
		// The initial value of "end" should point to the CR, which causes the
		// string to be terminated
		int end = lineLength - 2;
		while ((0 < end) && isspace(messageBuffer[end+parsePointer])) {
		  messageBuffer[parsePointer+end] = '\0';
		  --end;
		}
		while (('\0' != messageBuffer[parsePointer]) && isspace(messageBuffer[parsePointer])) {
		  // Replace the tabs and such with spaces
		  messageBuffer[parsePointer++] = ' ';
		}
		// Then back up one so the line will begin with a single space character
		--parsePointer;
		unfoldedLine += &messageBuffer[parsePointer];
		// I can't use the lineLength value here because it's been messed up by the foregoing
		parsePointer += 2 + (eol - &messageBuffer[parsePointer]);
	      }
	      else {
		if (0 < unfoldedLine.size()) {
		  ProcessSubpartHeaderLine(unfoldedLine, childBody);
		}
		// The initial value of "end" should point to the CR, which causes the
		// string to be terminated
		int end = lineLength - 2;
		while ((0 < end) && isspace(messageBuffer[end+parsePointer])) {
		  messageBuffer[parsePointer+end] = '\0';
		  --end;
		}
		unfoldedLine = &messageBuffer[parsePointer];
		parsePointer += lineLength;
	      }
	    }
	  }
	  // If I get here by breaking out of the loop, then parsePointer points to the beginning of a
	  // separator line, but the characters in the separator line have not been accounted for in the
	  // child body (and, therefore, in the parent body.)
	  // If I get here by falling through the loop, then parsePointer points to the beginning of
	  // a separator line (and there was only a header) and the characters in the string haven't
	  // been accounted for.
	  // notdone = (NULL != (eol = strchr(&messageBuffer[parsePointer], '\r')) && ('\n' == eol[1]));
	  eol = strchr(&messageBuffer[parsePointer], '\r');
	}
	else {
	  notdone = false;
	}
      }
    }
    // This is all the stuff in a multipart body after the last separator.  It's outside the block because
    // I don't care about the separator for the body part I've been parsing, I only care about the separator
    // for the parent, if any.
    // When this finishes, parsePointer must point to the line after the end of the body.  If it's a subpart,
    //    then parsePointer points to the first character of the separator line that ended everything.
    // I need to not count the end header line here as I've already counted for it above
    // but I haven't yet moved the parse beyond the line
    eol = strchr(&messageBuffer[parsePointer], '\r');
    if ((NULL != eol) && ('\n' == eol[1])) {
      parsePointer += 2 + (eol - &messageBuffer[parsePointer]);
    }
    if ((NULL == parentSeparator) ||
	('-' != messageBuffer[parsePointer]) ||
	('-' != messageBuffer[parsePointer+1]) ||
	(0 != strncmp(&messageBuffer[parsePointer+2], parentSeparator, strlen(parentSeparator)))) {
      while ((NULL != (eol = strchr(&messageBuffer[parsePointer], '\r'))) && ('\n' == eol[1])
	     && ((NULL == parentSeparator) || ('-' != messageBuffer[parsePointer]) || ('-' != messageBuffer[parsePointer+1])
		 || (0 != strncmp(&messageBuffer[parsePointer+2], parentSeparator, strlen(parentSeparator))))) {
	int lineLength = 2 + (eol - &messageBuffer[parsePointer]);
	parentBody.bodyOctets += lineLength;
	parentBody.bodyLines++;
	parsePointer += lineLength;
      }
    }
    if ((NULL != parentSeparator) &&
	('-' == messageBuffer[parsePointer]) &&
	('-' == messageBuffer[parsePointer+1]) &&
	(0 == strncmp(&messageBuffer[parsePointer+2], parentSeparator, strlen(parentSeparator)))) {
      // I need to back up two characters because the CRLF that is immediately before this is considered
      // to be part of the parent, not of the child
      parentBody.bodyOctets -= 2;
      parentBody.bodyLines--;
      parsePointer -= 2;
    }
  }
    break;

  case MIME_TYPE_MESSAGE:
    // SYZYGY working here I need a function to dump the body structure

    // The MIME_TYPE_MESSAGE has characteristics of both the multipart and single part.  It's like a single part
    // because there's no separator associated with the part.  It's like multipart because it's got a potentially
    // multipart message in side it.  Actually, come to think of it, it's like a single part
    {
      // At this point, parsePointer points to the first character of the body which, oddly enough, is actually the
      // first character of the header.
      insensitiveString unfoldedLine;
      MESSAGE_BODY childBody;
      char *eol;

      childBody.subparts = NULL;
      childBody.bodyLines = 0;
      childBody.bodyOctets = 0;
      childBody.headerOctets = 0;
      // childBody.bodyStartOffset = sectionStartOffset + parentBody.bodyOctets;
      childBody.bodyStartOffset = parsePointer;
      while ((NULL != (eol = strchr(&messageBuffer[parsePointer], '\r'))) && ('\n' == eol[1]) &&
	     (('-' != messageBuffer[parsePointer]) ||
	      ('-' != messageBuffer[parsePointer+1]) ||
	      (0 != strncmp(&messageBuffer[parsePointer+2], parentSeparator, strlen(parentSeparator))))) {
	int lineLength = 2 + (eol - &messageBuffer[parsePointer]);
	childBody.bodyOctets += lineLength;
	childBody.bodyLines++;
	if (2 >= lineLength) {
	  parsePointer += lineLength;
	  ProcessSubpartHeaderLine(unfoldedLine, childBody);
	  if (NULL == parentBody.subparts) {
	    parentBody.subparts = new std::vector<MESSAGE_BODY>;
	  }
	  childBody.headerOctets = childBody.bodyOctets;
	  childBody.headerLines = childBody.bodyLines;
	  parseBodyParts(loadBinaryParts, childBody, messageBuffer, parsePointer, parentSeparator, nestingDepth+1);
	  // When I get here, I've seen a separator, the characters in the separator have been included
	  // in the count for the child body, but parsePointer points to the CRLF before the separator
	  // line.
	  parentBody.subparts->push_back(childBody);
	  // Everything in the child is also part of the parent
	  parentBody.bodyOctets += childBody.bodyOctets;
	  parentBody.bodyLines += childBody.bodyLines;
	  break;
	}
	else {
	  if ((' ' == messageBuffer[parsePointer]) || ('\t' == messageBuffer[parsePointer])) {
	    // The initial value of "end" should point to the CR, which causes the
	    // string to be terminated
	    int end = lineLength - 2;
	    while ((0 < end) && isspace(messageBuffer[end+parsePointer])) {
	      messageBuffer[parsePointer+end] = '\0';
	      --end;
	    }
	    while (('\0' != messageBuffer[parsePointer]) && isspace(messageBuffer[parsePointer])) {
	      // Replace the tabs and such with spaces
	      messageBuffer[parsePointer++] = ' ';
	    }
	    // Then back up one so the line will begin with a single space character
	    --parsePointer;
	    unfoldedLine += &messageBuffer[parsePointer];
	    // I can't use the lineLength value here because it's been messed up by the foregoing
	    parsePointer += 2 + (eol - &messageBuffer[parsePointer]);
	  }
	  else {
	    if (0 < unfoldedLine.size()) {
	      ProcessSubpartHeaderLine(unfoldedLine, childBody);
	    }
	    // The initial value of "end" should point to the CR, which causes the
	    // string to be terminated
	    int end = lineLength - 2;
	    while ((0 < end) && isspace(messageBuffer[end+parsePointer])) {
	      messageBuffer[parsePointer+end] = '\0';
	      --end;
	    }
	    unfoldedLine = &messageBuffer[parsePointer];
	    parsePointer += lineLength;
	  }
	}
      }
    }
    break;

  default:
    if (loadBinaryParts || (MIME_TYPE_TEXT == parentBody.bodyMediaType)) {
      if ((NULL == parentSeparator) || 
	  ('-' != messageBuffer[parsePointer]) ||
	  ('-' != messageBuffer[parsePointer+1]) ||
	  (0 != strncmp(&messageBuffer[parsePointer+2], parentSeparator, strlen(parentSeparator)))) {
	char *eol;
	while ((NULL != (eol = strchr(&messageBuffer[parsePointer], '\r'))) && ('\n' == eol[1])
	       && ((NULL == parentSeparator) || ('-' != messageBuffer[parsePointer]) || ('-' != messageBuffer[parsePointer+1])
		   || (0 != strncmp(&messageBuffer[parsePointer+2], parentSeparator, strlen(parentSeparator))))) {
	  int lineLength = 2 + (eol - &messageBuffer[parsePointer]);
	  parentBody.bodyOctets += lineLength;
	  parentBody.bodyLines++;
	  parsePointer += lineLength;
	}
      }
      if ((NULL != parentSeparator) &&
	  ('-' == messageBuffer[parsePointer]) &&
	  ('-' == messageBuffer[parsePointer+1]) &&
	  (0 == strncmp(&messageBuffer[parsePointer+2], parentSeparator, strlen(parentSeparator)))) {
	// I need to back up two characters because the CRLF that is immediately before this is considered
	// to be part of the parent, not of the child
	parentBody.bodyOctets -= 2;
	parentBody.bodyLines--;
	parsePointer -= 2;
      }
    }
  }
}


MailMessage::MailMessage(unsigned long uid, unsigned long msn) : m_uid(uid), m_msn(msn) {
  m_messageStatus = MailMessage::SUCCESS;
  m_mainBody.bodyMediaType = MIME_TYPE_TEXT;
  m_mainBody.bodyLines = 0;
  m_mainBody.bodyStartOffset = 0;
  m_mainBody.bodyOctets = 0;
  m_mainBody.headerOctets = 0;
  m_mainBody.headerLines = 0;
  m_mainBody.subparts = NULL;
  m_textLines = 0;
  m_lineBuffPtr = 0;
  m_lineBuffEnd = 0;
}


void dumpMessageBodies(const MessageBody &body, int depth);

MailMessage::MAIL_MESSAGE_RESULT MailMessage::parse(MailStore *store, bool readBody, bool loadBinaryParts) {
  insensitiveString unfoldedLine;

  size_t bufferLength = store->bufferLength(m_uid);
  char *messageBuffer = new char[bufferLength+1];
  if (MailStore::SUCCESS == store->openMessageFile(m_uid)) {
    size_t messageSize;

    if (0 < (messageSize = store->readMessage(messageBuffer, 0, bufferLength))) {
      bool notdone = true;
      size_t parsePointer = 0;
      store->closeMessageFile();
      messageBuffer[messageSize] = '\0';
      // std::cout << "The message is " << messageSize << " octets long" << std::endl;

      // When I get here, the entire message, of size messageSize, is in messageBuffer
      while (notdone) {
	char *eol;
	if ((NULL != (eol = strchr(&messageBuffer[parsePointer], '\r')) && ('\n' == eol[1]))) {
	  int lineLength = 2 + (eol - &messageBuffer[parsePointer]);
	  m_mainBody.bodyOctets += lineLength;
	  m_mainBody.bodyLines += 1;
	  if (2 == lineLength) {
	    parsePointer += lineLength;
	    // process the last unfolded line here
	    processHeaderLine(unfoldedLine);
	    // And then convert the fields that we need converted
	    m_date.parse(m_dateLine, DateTime::RFC822);
	    m_mainBody.headerOctets = m_mainBody.bodyOctets;
	    m_mainBody.headerLines = m_mainBody.bodyLines;
	    if (readBody) {
	      // Do I need this?  Wouldn't the trailing terminator be accounted for as part of the multipart
	      // section?  Shouldn't ParseBodyParts ALWAYS read the entire buffer, if it's called at all?
	      parseBodyParts(loadBinaryParts, m_mainBody, messageBuffer, parsePointer, NULL, 1);
	      // while(store->ReadMessageLine(messageBuffer)) {
	    }
	    notdone = false;
	  }
	  else {
	    if ((' ' == messageBuffer[parsePointer]) || ('\t' == messageBuffer[parsePointer])) {
	      // The initial value of "end" should point to the CR, which causes the
	      // string to be terminated
	      int end = lineLength - 2;
	      while ((0 < end) && isspace(messageBuffer[end+parsePointer])) {
		messageBuffer[parsePointer+end] = '\0';
		--end;
	      }
	      while (('\0' != messageBuffer[parsePointer]) && isspace(messageBuffer[parsePointer])) {
		// Replace the tabs and such with spaces
		messageBuffer[parsePointer++] = ' ';
	      }
	      // Then back up one so the line will begin with a single space character
	      --parsePointer;
	      unfoldedLine += &messageBuffer[parsePointer];
	      // I can't use the lineLength value here because it's been messed up by the foregoing
	      parsePointer += 2 + (eol - &messageBuffer[parsePointer]);
	    }
	    else {
	      if (0 < unfoldedLine.size()) {
		processHeaderLine(unfoldedLine);
	      }
	      // The initial value of "end" should point to the CR, which causes the
	      // string to be terminated
	      int end = lineLength - 2;
	      while ((0 < end) && isspace(messageBuffer[end+parsePointer])) {
		messageBuffer[parsePointer+end] = '\0';
		--end;
	      }
	      unfoldedLine = &messageBuffer[parsePointer];
	      parsePointer += lineLength;
	    }
	  }
	}
	else {
	  // last line
	  notdone = false;
	}
      }
      // dumpMessageBodies(m_mainBody, 0);
    }
    else {
      store->closeMessageFile();
    }
  }
  else {
    m_messageStatus = MESSAGE_FILE_READ_FAILED;
  }
  delete[] messageBuffer;
  return m_messageStatus;
}

void
subpart_destructor(BODY_PARTS partsList) {
  if (NULL != partsList) {
    for (int i=0; i<partsList->size(); ++i) {
      subpart_destructor((*partsList)[i].subparts);
    }
    delete partsList;
  }
}

MailMessage::~MailMessage() {
  subpart_destructor(m_mainBody.subparts);
}


void dumpMessageBodies(const MessageBody &body, int depth) {
  static const char *types[] = {
    "UNKNOWN",
    "TEXT",
    "IMAGE",
    "AUDIO",
    "VIDEO",
    "APPLICATION",
    "MULTIPART",
    "MESSAGE"};

  std::cout << "+++++++++++++++++++" << std::endl;
  for (int i=0; i<depth; ++i) {
    std::cout << "!-";
  }
  std::cout << body.contentDescriptionLine.c_str() << std::endl;
  for (int i=0; i<depth; ++i) {
    std::cout << "!-";
  }
  std::cout << types[body.bodyMediaType] << std::endl;
  for (int i=0; i<depth; ++i) {
    std::cout << "!-";
  }
  std::cout << "header lines " << body.headerLines << " octets " << body.headerOctets << std::endl;
  for (int i=0; i<depth; ++i) {
    std::cout << "!-";
  }
  std::cout << "lines " << body.bodyLines << " octets " << body.bodyOctets << " start offset " << body.bodyStartOffset << std::endl;
  if (NULL != body.subparts) {
    for (std::vector<MESSAGE_BODY>::const_iterator child = body.subparts->begin(); child!=body.subparts->end(); ++child) {
      dumpMessageBodies(*child, depth+1);
    }
  }
  std::cout << "-------------------" << std::endl;
}
