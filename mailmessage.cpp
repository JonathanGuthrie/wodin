#include "mailmessage.hpp"
#include "mailstore.hpp"

typedef enum
{
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

void MailMessage::BuildSymbolTable() {
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

// SYZYGY -- this date and time stuff should go into datetime
/*
 * date-time = [ day-of-week "," ] date FWS time [CFWS]
 *
 * day-of-week = ([FWS] day-name) / obs-day-of-week
 *
 * day-name = "Mon" / "Tue" / "Wed" / "Thu" / "Fri" / "Sat" / "Sun"
 *
 * date = day month year
 *
 * year = 4*DIGIT / obs-year
 *
 * month = (FWS month-name FWS) / obs-month
 *
 * month-name = "Jan" / "Feb" / "Mar" / "Apr" /
 *              "May" / "Jun" / "Jul" / "Aug" /
 *              "Sep" / "Oct" / "Nov" / "Dec"
 *
 * day = [FWS] 1*2DIGIT / obs-day
 *
 * time = time-of-day FWS zone
 *
 * time-of-day = hour ":" minute [ ":" second ]
 *
 * hour = 2DIGIT / obs-hour
 *
 * minute = 2DIGIT / obs-minute
 *
 * second = 2DIGIT / obs-second
 *
 * zone = (( "+" / "-") 4DIGIT) / obs-zone
 *
 * [CFWS] day-name [CFWS]
 *
 * obs-year = [CFWS] 2*DIGIT [CFWS]
 *
 * obs-month = CFWS month-name CFWS
 *
 * obs-day = [CFWS] 1*2DIGIT [CFWS]
 *
 * obs-hour = [CFWS] 2DIGIT [CFWS]
 *
 * obs-minute = [CFWS] 2DIGIT [CFWS]
 *
 * obs-second = [CFWS] 2DIGIT [CFWS]
 *
 * obs-zone = "UT" / "GMT" / "EST" / "EDT" / "CST" / "CDT" /
 *            "MST" / "MDT" / "PST" / "PDT" / %d65-73 / %d75-90
 *	      %d97-105 / %d107-122
 *
 * If a two digit year is encoutered, it is interpreted as being the year less 2000 if
 * the digits represent a number less than 50, otherwise it is interpreted as being the
 * year less 1900.  A three digit year is interpreted as being the year less 1900.
 * the single-letter time zones, UT, and GMT are the same as +0000, the EDT is -0400,
 * EST and CDT are -0500, CST and MDT are -0600, MST and PDT are -0700, and PST is -0800
 *
 * Okay, except for the two, three, and four-digit years and the time zone wierdness (and
 * the time zone is important because it's essential for comparing dates) it appears as if
 * the obsolete fields are the same as the current fields.
 */
struct tm ParseDateField(const char *pszField) {
    struct tm result;
    const char *parse_pointer;

    memset(&result, 0, sizeof(struct tm));
    parse_pointer = strchr(pszField, ',');
    if (NULL == parse_pointer) {
        parse_pointer = pszField;
    }
    else {
        ++parse_pointer;
    }
    char *temp_pointer;
    long temp = strtol(parse_pointer, &temp_pointer, 10);
    if (0 < temp) {
        result.tm_mday = temp;
        parse_pointer = temp_pointer;
        while(('\0' != *parse_pointer) && isspace(*parse_pointer)) {
	    ++parse_pointer;
        }
        temp = month_name_to_month(parse_pointer);
        if (0 < temp) {
	    result.tm_mon = temp;
	    parse_pointer += 3;
	    temp = strtol(parse_pointer, &temp_pointer, 10);
	    if (0 < temp) {
		parse_pointer = temp_pointer;
		if (999 < temp) {
		    result.tm_year = temp - 1900;
		}
		else {
		    if ((99 < temp) || (50 <= temp)) {
			result.tm_year = temp;
		    }
		    else {
			result.tm_year = temp + 100;
		    }
		}

		temp = strtol(parse_pointer, &temp_pointer, 10);
		if (':' == *temp_pointer) {
		    result.tm_hour = temp;
		    parse_pointer = temp_pointer + 1;
		    temp = strtol(parse_pointer, &temp_pointer, 10);
		    result.tm_min = temp;
		    if (':' == *temp_pointer) {
			parse_pointer = temp_pointer + 1;
			temp = strtol(parse_pointer, &temp_pointer, 10);
		    }
		    else {
			temp = 0;
		    }
		    result.tm_sec = temp;
		    if (isspace(*temp_pointer)) {
			parse_pointer = temp_pointer;
			while(('\0' != *parse_pointer) && isspace(*parse_pointer)) {
			    ++parse_pointer;
			    if (('-' == *parse_pointer) || ('+' == *parse_pointer)) {
				if ('-' == *parse_pointer) {
				    ++parse_pointer;
				    temp = strtol(parse_pointer, NULL, 10);
				    temp = -temp;
				}
				else {
				    ++parse_pointer;
				    temp = strtol(parse_pointer, NULL, 10);
				}
#if !defined(_WIN32) && !defined(_WWS_SOLARIS_)
				result.tm_gmtoff = temp * 60;
#endif // !defined(_WIN32) && !defined(_WWS_SOLARIS_)
			    }
			}
		    } 
		}
	    }
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
	    --pos;
	    int end = work.find_last_not_of(SPACE, pos);
	    int begin = work.find_first_not_of(SPACE);

	    work = work.substr(begin, end-begin+1);
	}
	else {
	    pos = work.find(';');
	    // if ';' is not found, then pos = std::string:npos, which is always bigger than the string, so it will trim the spaces off the end
	    // of the whole thing.  So, I don't need to see if semicolon failed here
	    int end = work.find_last_not_of(SPACE, pos);
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
bool MailMessage::ProcessHeaderLine(const insensitiveString &line) {
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


void MailMessage::ParseBodyParts(MailStore *store, bool loadBinaryParts, MESSAGE_BODY &parentBody, char messageBuffer[1101],
    		       const char *parentSeparator, size_t sectionStartOffset)
{
    parentBody.bodyMediaType = GetMediaType(parentBody.contentTypeLine);
    switch(parentBody.bodyMediaType) {
    case MIME_TYPE_MULTIPART:
    {
	std::string separator = GetMediaBoundary(parentBody.contentTypeLine);
	if (0 < separator.size()) {
	    // When I get here, I know that I have a read the header for the (sub)part, that the body that I'm
	    // reading is a multipart message, and that I have a valid separator string.  I need to read the 
	    // lines as they come in and process them accordingly
	    int notdone = true;
	    while (notdone) {
		notdone = false;
		if (store->ReadMessageLine(messageBuffer)) {
		    parentBody.bodyOctets += strlen(messageBuffer);
		    parentBody.bodyLines++;
		    if (('-' != messageBuffer[0]) ||
			('-' != messageBuffer[1]) ||
			(0 != strncmp(messageBuffer+2, separator.c_str(), separator.size()))) {
			notdone = true;
		    }
		}
	    }
	    // If I get here, I've seen the beginning of a separator line.  I need to check to see if it marks
	    // the end of the subparts with this separator
	    notdone = true;
	    while (notdone) {
		if ((4 + separator.size() > (int)strlen(messageBuffer)) ||
		    ('-' != messageBuffer[2+separator.size()]) ||
		    ('-' != messageBuffer[3+separator.size()])) {
		    // If I'm here, I know that it is not the final terminator.   I need to accumulate lines of
		    // the subpart header until I see the blank line that ends them.
		    insensitiveString unfoldedLine;
		    MESSAGE_BODY childBody;

		    childBody.subparts = NULL;
		    childBody.bodyLines = 0;
		    childBody.bodyOctets = 0;
		    childBody.headerOctets = 0;
		    childBody.bodyStartOffset = sectionStartOffset + parentBody.bodyOctets;
		    while (store->ReadMessageLine(messageBuffer) &&
			   (('-' != messageBuffer[0]) ||
			    ('-' != messageBuffer[1]) ||
			    (0 != strncmp(messageBuffer+2, separator.c_str(), separator.size())))) {
			childBody.bodyOctets += strlen(messageBuffer);
			childBody.bodyLines++;
			insensitiveString line(messageBuffer);
			if (2 >= line.size()) {
			    ProcessSubpartHeaderLine(unfoldedLine, childBody);
			    if (NULL == parentBody.subparts) {
				parentBody.subparts = new std::vector<MESSAGE_BODY>;
			    }
			    childBody.headerOctets = childBody.bodyOctets;
			    childBody.headerLines = childBody.bodyLines;
			    ParseBodyParts(store, loadBinaryParts, childBody, messageBuffer, separator.c_str(),
					   sectionStartOffset + parentBody.bodyOctets);
			    // This part here is because the separator line is considered to
			    // be "CRLF--<separator>CRLF, so I've got an extra line and an extra
			    // CRLF sequence that I need to not account for twice.
			    childBody.bodyOctets -= 2;
			    childBody.bodyLines--;
			    parentBody.subparts->push_back(childBody);
			    // But I do have to account for it once
			    parentBody.bodyOctets += childBody.bodyOctets + 2;
			    parentBody.bodyLines += childBody.bodyLines + 1;
			    break;
			}
			else {
			    if ((' ' == line[0]) || ('\t' == line[0])) {
				int begin = line.find_first_not_of(SPACE);
				int end = line.find_last_not_of(SPACE);
				if (std::string::npos != begin) {
				    unfoldedLine += ' ' + line.substr(begin, end-begin+1);
				}
				else {
				    unfoldedLine += ' ';
				}
			    }
			    else {
				if (0 < unfoldedLine.size()) {
				    notdone = ProcessSubpartHeaderLine(unfoldedLine, childBody);
				}
				int begin = line.find_first_not_of(SPACE);
				int end = line.find_last_not_of(SPACE);
				if (std::string::npos != begin) {
				    unfoldedLine = line.substr(begin, end-begin+1);
				}
				else {
				    unfoldedLine = "";
				}
			    }
			}
		    }
		    parentBody.bodyOctets += strlen(messageBuffer);
		    parentBody.bodyLines++;
		}
		else {
		    notdone = false;
		}
	    }
	}
    }
    if ((NULL == parentSeparator) || 
	('-' != messageBuffer[0]) ||
	('-' != messageBuffer[1]) ||
	(0 != strncmp(messageBuffer+2, parentSeparator, strlen(parentSeparator)))) {
	while (store->ReadMessageLine(messageBuffer) &&
	       ((NULL == parentSeparator) || 
		('-' != messageBuffer[0]) ||
		('-' != messageBuffer[1]) ||
		(0 != strncmp(messageBuffer+2, parentSeparator, strlen(parentSeparator))))) {
	    parentBody.bodyOctets += strlen(messageBuffer);
	    parentBody.bodyLines++;
	}
    }
    break;

    case MIME_TYPE_MESSAGE:
	// The MIME_TYPE_MESSAGE has characteristics of both the multipart and single part.  It's like a single part
	// because there's no separator associated with the part.  It's like multipart because it's got a potentially
	// multipart message in side it.  Actually, come to think of it, it's like a single part
    {
	// If I'm here, I know that it is not the final terminator.   I need to accumulate lines of
	// the subpart header until I see the blank line that ends them.
	insensitiveString unfoldedLine;
	MESSAGE_BODY childBody;

	childBody.subparts = NULL;
	childBody.bodyLines = 0;
	childBody.bodyOctets = 0;
	childBody.headerOctets = 0;
	childBody.bodyStartOffset = sectionStartOffset + parentBody.bodyOctets;
	while (store->ReadMessageLine(messageBuffer) &&
	       (('-' != messageBuffer[0]) ||
		('-' != messageBuffer[1]) ||
		(0 != strncmp(messageBuffer+2, parentSeparator, strlen(parentSeparator))))) {
	    childBody.bodyOctets += strlen(messageBuffer);
	    childBody.bodyLines++;
	    insensitiveString line(messageBuffer);
	    if (2 >= line.size()) {
		ProcessSubpartHeaderLine(unfoldedLine, childBody);
		if (NULL == parentBody.subparts) {
		    parentBody.subparts = new std::vector<MESSAGE_BODY>;
		}
		childBody.headerOctets = childBody.bodyOctets;
		childBody.headerLines = childBody.bodyLines;
		ParseBodyParts(store, loadBinaryParts, childBody, messageBuffer, parentSeparator,
			       sectionStartOffset + parentBody.bodyOctets);
		childBody.bodyOctets -= 2;
		childBody.bodyLines--;
		parentBody.subparts->push_back(childBody);
		parentBody.bodyOctets += childBody.bodyOctets + 2;
		parentBody.bodyLines += childBody.bodyLines + 1;
		break;
	    }
	    else {
		if ((' ' == line[0]) || ('\t' == line[0])) {
		    int begin = line.find_first_not_of(SPACE);
		    int end = line.find_last_not_of(SPACE);
		    if (std::string::npos != begin) {
			unfoldedLine += ' ' + line.substr(begin, end-begin+1);
		    }
		    else {
			unfoldedLine += ' ';
		    }
		}
		else {
		    if (0 < unfoldedLine.size()) {
			ProcessSubpartHeaderLine(unfoldedLine, childBody);
		    }
		    int begin = line.find_first_not_of(SPACE);
		    int end = line.find_last_not_of(SPACE);
		    if (std::string::npos != begin) {
			unfoldedLine = line.substr(begin, end-begin+1);
		    }
		    else {
			unfoldedLine = "";
		    }
		}
	    }
	}
    }
    break;

    default:
	if (loadBinaryParts || (MIME_TYPE_TEXT == parentBody.bodyMediaType)) {
	    if ((NULL == parentSeparator) || 
		('-' != messageBuffer[0]) ||
		('-' != messageBuffer[1]) ||
		(0 != strncmp(messageBuffer+2, parentSeparator, strlen(parentSeparator)))) {
		while (store->ReadMessageLine(messageBuffer) &&
		       ((NULL == parentSeparator) || 
			('-' != messageBuffer[0]) ||
			('-' != messageBuffer[1]) ||
			(0 != strncmp(messageBuffer+2, parentSeparator, strlen(parentSeparator))))) {
		    parentBody.bodyOctets += strlen(messageBuffer);
		    parentBody.bodyLines++;
		}
	    }
	}
    }
}


MailMessage::MailMessage() {
    m_messageStatus = MailMessage::SUCCESS;
    m_mainBody.bodyMediaType = MIME_TYPE_TEXT;
    m_mainBody.bodyLines = 0;
    m_mainBody.bodyStartOffset = 0;
    m_mainBody.bodyOctets = 0;
    m_mainBody.headerOctets = 0;
    m_mainBody.subparts = NULL;
    m_textLines = 0;
    m_lineBuffPtr = 0;
    m_lineBuffEnd = 0;
}


MailMessage::MAIL_MESSAGE_RESULT MailMessage::Parse(MailStore *store, unsigned long uid, unsigned long msn, bool readBody, bool loadBinaryParts) {
    m_uid = uid;
    m_msn = msn;
    bool notdone = true;
    insensitiveString unfoldedLine;

    while(notdone) {
	char messageBuffer[1101];
	if (store->ReadMessageLine(messageBuffer)) {
	    int lineLength = (int) strlen(messageBuffer);
	    m_mainBody.bodyOctets += lineLength;
	    m_mainBody.bodyLines++;
	    if ((2 <= lineLength) && ('\r' == messageBuffer[lineLength-2]) && ('\n' == messageBuffer[lineLength -1])) {
		if (2 == lineLength) {
		    // process the last unfolded line here
		    ProcessHeaderLine(unfoldedLine);
		    // And then convert the fields that we need converted
		    m_date = ParseDateField(m_dateLine.c_str());
		    m_mainBody.headerOctets = m_mainBody.bodyOctets;
		    m_mainBody.headerLines = m_mainBody.bodyLines;
		    if (readBody) {
			ParseBodyParts(store, loadBinaryParts, m_mainBody, messageBuffer, NULL, 0);
			while(store->ReadMessageLine(messageBuffer)) {
			    m_mainBody.bodyOctets += strlen(messageBuffer);
			    m_mainBody.bodyLines++;
			}
		    }
		    notdone = false;
		}
		else {
		    if ((' ' == messageBuffer[0]) || ('\t' == messageBuffer[0])) {
			insensitiveString temp(messageBuffer);
			int begin = 0;
			while (('\0' != messageBuffer[begin]) && isspace(messageBuffer[begin])) {
			    ++begin;
			}
			int end = strlen(messageBuffer) - 1;
			while ((0 < end) && isspace(messageBuffer[end])) {
			    messageBuffer[end] = '\0';
			    --end;
			}
			unfoldedLine += ' ' + (insensitiveString) (messageBuffer + begin);
		    }
		    else {
			if (0 < unfoldedLine.size()) {
			    ProcessHeaderLine(unfoldedLine);
			}
			int begin = 0;
			while (('\0' != messageBuffer[begin]) && isspace(messageBuffer[begin])) {
			    ++begin;
			}
			int end = strlen(messageBuffer) - 1;
			while ((0 < end) && isspace(messageBuffer[end])) {
			    messageBuffer[end] = '\0';
			    --end;
			}
			unfoldedLine = messageBuffer + begin;
		    }
		}
	    }
	    else {
		notdone = false;
		// m_messageStatus = MailMessage::MESSAGE_MALFORMED_NO_BODY;
		// SYZGY Log
		// store->GetClient()->Log("In MailMessage::parse, message \"%s\" has no body\n", file_name.c_str());
		m_messageStatus = MailMessage::SUCCESS;
	    }
	}
	else {
	    ProcessHeaderLine(unfoldedLine);
	    notdone = false;
	    // m_messageStatus = MailMessage::MESSAGE_MALFORMED_NO_BODY;
	    // SYZYGY Log
	    // store->GetClient()->Log("In MailMessage::parse, message \"%s\" has no body\n", file_name.c_str());
	    m_messageStatus = MailMessage::SUCCESS;
	}
    }
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
