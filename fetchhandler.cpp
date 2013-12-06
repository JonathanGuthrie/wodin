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

#include "fetchhandler.hpp"

typedef enum {
    FETCH_ALL,
    FETCH_FAST,
    FETCH_FULL,
    FETCH_BODY,
    FETCH_BODY_PEEK,
    FETCH_BODYSTRUCTURE,
    FETCH_ENVELOPE,
    FETCH_FLAGS,
    FETCH_INTERNALDATE,
    FETCH_RFC822,
    FETCH_RFC822_HEADER,
    FETCH_RFC822_SIZE,
    FETCH_RFC822_TEXT,
    FETCH_UID
} FETCH_NAME_VALUES;
typedef std::map<insensitiveString, FETCH_NAME_VALUES> FETCH_NAME_T;

static FETCH_NAME_T fetchSymbolTable;

typedef enum {
    FETCH_BODY_BODY,
    FETCH_BODY_HEADER,
    FETCH_BODY_FIELDS,
    FETCH_BODY_NOT_FIELDS,
    FETCH_BODY_TEXT,
    FETCH_BODY_MIME
} FETCH_BODY_PARTS;

ImapHandler *fetchHandler(ImapSession *session, INPUT_DATA_STRUCT &input) {
    (void) input;
    return new FetchHandler(session, session->parseBuffer(), false);
}

ImapHandler *uidFetchHandler(ImapSession *session, INPUT_DATA_STRUCT &input) {
    (void) input;
    return new FetchHandler(session, session->parseBuffer(), true);
}


void FetchHandler::buildSymbolTable() {
    fetchSymbolTable.insert(FETCH_NAME_T::value_type("ALL",           FETCH_ALL));
    fetchSymbolTable.insert(FETCH_NAME_T::value_type("FAST",          FETCH_FAST));
    fetchSymbolTable.insert(FETCH_NAME_T::value_type("FULL",          FETCH_FULL));
    fetchSymbolTable.insert(FETCH_NAME_T::value_type("BODY",          FETCH_BODY));
    fetchSymbolTable.insert(FETCH_NAME_T::value_type("BODY.PEEK",     FETCH_BODY_PEEK));
    fetchSymbolTable.insert(FETCH_NAME_T::value_type("BODYSTRUCTURE", FETCH_BODYSTRUCTURE));
    fetchSymbolTable.insert(FETCH_NAME_T::value_type("ENVELOPE",      FETCH_ENVELOPE));
    fetchSymbolTable.insert(FETCH_NAME_T::value_type("FLAGS",         FETCH_FLAGS));
    fetchSymbolTable.insert(FETCH_NAME_T::value_type("INTERNALDATE",  FETCH_INTERNALDATE));
    fetchSymbolTable.insert(FETCH_NAME_T::value_type("RFC822",        FETCH_RFC822));
    fetchSymbolTable.insert(FETCH_NAME_T::value_type("RFC822.HEADER", FETCH_RFC822_HEADER));
    fetchSymbolTable.insert(FETCH_NAME_T::value_type("RFC822.SIZE",   FETCH_RFC822_SIZE));
    fetchSymbolTable.insert(FETCH_NAME_T::value_type("RFC822.TEXT",   FETCH_RFC822_TEXT));
    fetchSymbolTable.insert(FETCH_NAME_T::value_type("UID",           FETCH_UID));
}


void FetchHandler::messageChunk(unsigned long uid, size_t offset, size_t length) {
    if (MailStore::SUCCESS == m_session->store()->openMessageFile(uid)) {
	char *xmitBuffer = new char[length+1];
	size_t charsRead = m_session->store()->readMessage(xmitBuffer, offset, length);
	m_session->store()->closeMessageFile();
	xmitBuffer[charsRead] = '\0';

	std::ostringstream literal;
	literal << "{" << charsRead << "}\r\n";
	m_session->driver()->wantsToSend(literal.str());
	m_session->driver()->wantsToSend((uint8_t *)xmitBuffer, charsRead);
	delete[] xmitBuffer;
    }
}

void FetchHandler::fetchResponseInternalDate(const MailMessage *message) {
    std::string result = "INTERNALDATE \"";
    DateTime when = m_session->store()->messageInternalDate(message->uid());
    when.format(DateTime::IMAP);

    result += when.str();
    result += "\"";
    m_session->driver()->wantsToSend(result);
}

void FetchHandler::fetchResponseRfc822(unsigned long uid, const MailMessage *message) {
    m_session->driver()->wantsToSend("RFC822 ");
    messageChunk(uid, 0, message->messageBody().bodyOctets);
}

void FetchHandler::fetchResponseRfc822Header(unsigned long uid, const MailMessage *message) {
    size_t length;
    if (0 != message->messageBody().headerOctets) {
	length = message->messageBody().headerOctets;
    }
    else {
	length = message->messageBody().bodyOctets;
    }
    m_session->driver()->wantsToSend("RFC822.HEADER ");
    messageChunk(uid, 0, length);
}

void FetchHandler::fetchResponseRfc822Size(const MailMessage *message) {
    std::ostringstream result;
    result << "RFC822.SIZE " << message->messageBody().bodyOctets;
    m_session->driver()->wantsToSend(result.str());
}

void FetchHandler::fetchResponseRfc822Text(unsigned long uid, const MailMessage *message) {
    std::string result;
    if (0 < message->messageBody().headerOctets) {
	size_t length;
	if (message->messageBody().bodyOctets > message->messageBody().headerOctets) {
	    length = message->messageBody().bodyOctets - message->messageBody().headerOctets;
	}
	else {
	    length = 0;
	}
	m_session->driver()->wantsToSend("RFC822.TEXT ");
	messageChunk(uid, message->messageBody().headerOctets, length);
    }
    else {
	m_session->driver()->wantsToSend("RFC822.TEXT {0}\r\n");
    }
}

static insensitiveString quotifyString(const insensitiveString &input) {
    insensitiveString result("\"");

    for (size_t i=0; i<input.size(); ++i) {
	if (('"' == input[i]) || ('\\' == input[i])) {
	    result += '\\';
	}
	result += input[i];
    }
    result += "\"";

    return result;
}


#define SPACE " \t\v\r\n\f"

static insensitiveString rfc822DotAtom(insensitiveString &input) {
    insensitiveString result;

    size_t begin = input.find_first_not_of(SPACE);
    size_t end = input.find_last_not_of(SPACE);
    if (std::string::npos != begin) {
	input = input.substr(begin, end-begin+1);
    }
    else {
	input = "";
    }

    if ('"' == input[0]) {
	input = input.substr(1);
	bool escapeFlag = false;
	size_t i;
	for (i=0; i<input.size(); ++i) {
	    if (escapeFlag) {
		escapeFlag = false;
		result += input[i];
	    }
	    else {
		if ('"' == input[i]) {
		    break;
		}
		else {
		    if ('\\' == input[i]) {
			escapeFlag = true;
		    }
		    else {
			result += input[i];
		    }
		}
	    }
	}
	input = input.substr(i+1);
    }
    else {
	size_t pos = input.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!#$%&'*+-/=?^_`{|}~. ");
	if (std::string::npos != pos) {
	    int begin = input.find_first_not_of(SPACE);
	    int end = input.find_last_not_of(SPACE, pos-1);
	    result = input.substr(begin, end-begin+1);
	    input = input.substr(pos);
	}
	else {
	    result = input;
	    input = "";
	}
    }
    return result;
}


static insensitiveString parseRfc822Domain(insensitiveString &input) {
    insensitiveString result;

    if ('[' == input[1]) {
	input = input.substr(1);
	std::string token;
	bool escapeFlag = false;
	bool notdone = true;
	size_t i;
	for (i=0; (i<input.size()) && notdone; ++i) {
	    if (escapeFlag) {
		token += input[i];
		escapeFlag = false;
	    }
	    else {
		if ('\\' == input[i]) {
		    escapeFlag = true;
		}
		else {
		    if (']' == input[i])  {
			notdone = false;
		    }
		    else {
			token += input[i];
		    }
		}
	    }
	}
	input = input.substr(i+1);
    }
    else {
	insensitiveString token = rfc822DotAtom(input);
	result += quotifyString(token);
    }
    return result;
}

static insensitiveString parseRfc822AddrSpec(insensitiveString &input) {
    insensitiveString result;

    if ('@' != input[0]) {
	result += "NIL ";
    }
    else {
	// It's an old literal path
	size_t pos = input.find(':');
	if (std::string::npos != pos) {
	    insensitiveString temp = quotifyString(input.substr(0, pos));
	    int begin = temp.find_first_not_of(SPACE);
	    int end = temp.find_last_not_of(SPACE, pos-1);
	    result += temp.substr(begin, end-begin+1) + " ";
	    input = input.substr(pos+1);
	}
	else {
	    result += "NIL ";
	}
    }
    insensitiveString token = rfc822DotAtom(input);
    result += quotifyString(token) + " ";
    if ('@' == input[0]) {
	input = input.substr(1);
	result += parseRfc822Domain(input);
    }
    else {
	result += "\"\"";
    }
    return result;
}

static insensitiveString parseMailbox(insensitiveString &input) {
    insensitiveString result("(");
    int begin = input.find_first_not_of(SPACE);
    int end = input.find_last_not_of(SPACE);
    input = input.substr(begin, end-begin+1);
    if ('<' == input[0]) {
	result += "NIL ";

	begin = input.find_first_not_of(SPACE, 1);
	end = input.find_last_not_of(SPACE);
	input = input.substr(begin, end-begin+1);
	// Okay, this is an easy one.  This is an "angle-addr"
	result += parseRfc822AddrSpec(input);
	begin = input.find_first_not_of(SPACE);
	end = input.find_last_not_of(SPACE);
	input = input.substr(begin, end-begin+1);
	if ('>' == input[0]) {
	    begin = input.find_first_not_of(SPACE, 1);
	    end = input.find_last_not_of(SPACE);
	    input = input.substr(begin, end-begin+1);
	}
    }
    else {
	if ('@' == input[0]) {
	    result += "NIL " + parseRfc822AddrSpec(input);
	}
	else {
	    insensitiveString token = rfc822DotAtom(input);
	    begin = input.find_first_not_of(SPACE);
	    end = input.find_last_not_of(SPACE);
	    input = input.substr(begin, end-begin+1);

	    // At this point, I don't know if I've seen a local part or an address_spec
	    // I can tell the difference by the first character.  If it's a at sign or if I'm out of string,
	    // then it's a local part  If it's a greater than, then it was a display name and I'm expecting
	    // an address
	    if ((0 == input.size()) || (',' == input[0])) {
		result += "NIL NIL " + quotifyString(token) + " \"\"";
	    }
	    else {
		if ('@' == input[0]) {
		    input = input.substr(1);
		    result += "NIL NIL " + quotifyString(token) + " " + parseRfc822Domain(input);
		}
		else {
		    if ('<' == input[0]) {
			input = input.substr(1);
		    }
		    result += quotifyString(token) + " " + parseRfc822AddrSpec(input);
		}
	    }
	}
    }
    result += ")";
    return result;
}

// removeRfc822Comments assumes that the lines have already been unfolded
static insensitiveString removeRfc822Comments(const insensitiveString &headerLine) {
    insensitiveString result;
    bool inComment = false;
    bool inQuotedString = false;
    bool escapeFlag = false;
    int len = headerLine.size();

    for(int i=0; i<len; ++i) {
	if (inComment) {
	    if (escapeFlag) {
		escapeFlag = false;
	    }
	    else {
		if (')' == headerLine[i]) {
		    inComment = false;
		}
		else {
		    if ('\\' == headerLine[i]) {
			escapeFlag = true;
		    }
		}
	    }
	}
	else {
	    if (inQuotedString) {
		if(escapeFlag) {
		    escapeFlag = false;
		}
		else {
		    if ('"' == headerLine[i]) {
			inQuotedString = false;
		    }
		    else {
			if ('\\' == headerLine[i]) {
			    escapeFlag = true;
			}
		    }
		}
		result += headerLine[i];
	    }
	    else {
		// Look for quote and paren
		if ('(' == headerLine[i]) {
		    inComment = true;
		}
		else {
		    if ('"' == headerLine[i]) {
			inQuotedString = true;
		    }
		    result += headerLine[i];
		}
	    }
	}
    }
    return result;
}

static insensitiveString parseMailboxList(const insensitiveString &input) {
    insensitiveString result;

    if (0 != input.size()) {
	result = "(";
	insensitiveString work = removeRfc822Comments(input);
	do {
	    if (',' == work[0]) {
		work = work.substr(1);
	    }
	    result += parseMailbox(work);
	    int end = work.find_last_not_of(SPACE);
	    int begin = work.find_first_not_of(SPACE);
	    work = work.substr(begin, end-begin+1);
	} while (',' == work[0]);
	result += ")";
    }
    else {
	result = "NIL";
    }
    return result;
}

// An address is either a mailbox or a group.  A group is a display-name followed by a colon followed
// by a mailbox list followed by a semicolon.  A mailbox is either an address specification or a name-addr.
//  An address specification is a mailbox followed by a 
static insensitiveString parseAddress(insensitiveString &input) {
    insensitiveString result("(");
    size_t end = input.find_last_not_of(SPACE);
    size_t begin = input.find_first_not_of(SPACE);
    input = input.substr(begin, end-begin+1);
    if ('<' == input[0]) {
	result += "NIL ";

	end = input.find_last_not_of(SPACE);
	begin = input.find_first_not_of(SPACE, 1);
	input = input.substr(begin, end-begin+1);
	// Okay, this is an easy one.  This is an "angle-addr"
	result += parseRfc822AddrSpec(input);
	end = input.find_last_not_of(SPACE);
	begin = input.find_first_not_of(SPACE);
	input = input.substr(begin, end-begin+1);
	if ('>' == input[0]) {
	    end = input.find_last_not_of(SPACE);
	    begin = input.find_first_not_of(SPACE, 1);
	    if (std::string::npos == begin) {
		begin = 0;
	    }
	    input = input.substr(begin, end-begin+1);
	}
    }
    else {
	if ('@' == input[0]) {
	    result += "NIL " + parseRfc822AddrSpec(input);
	}
	else {
	    insensitiveString token = rfc822DotAtom(input);

	    end = input.find_last_not_of(SPACE);
	    begin = input.find_first_not_of(SPACE);
	    input = input.substr(begin, end-begin+1);

	    // At this point, I don't know if I've seen a local part, a display name, or an address_spec
	    // I can tell the difference by the first character.  If it's a at sign or if I'm out of string,
	    // then it's a local part  If it's a colon, then it was a display name and I'm reading a group.
	    //  If it's a greater than, then it was a display name and I'm expecting an address
	    if ((0 == input.size()) || (',' == input[0])) {
		result += "NIL NIL " + quotifyString(token) + " \"\"";
	    }
	    else {
		if ('@' == input[0]) {
		    input = input.substr(1);
		    insensitiveString domain = parseRfc822Domain(input);
		    if ('(' == input[0]) {
			input = input.substr(1, input.find(')')-1);
			result += quotifyString(input) + " NIL " + quotifyString(token) + " " + domain;
		    }
		    else {
			result += "NIL NIL " + quotifyString(token) + " " + domain;
		    }
		    // SYZYGY -- 
		    // SYZYGY -- I wonder what I forgot in the previous line
		}
		else {
		    if ('<' == input[0]) {
			input = input.substr(1);
			result += quotifyString(token) + " " + parseRfc822AddrSpec(input);
		    }
		    else {
			if (':' == input[0]) {
			    input = input.substr(1);
			    // It's a group specification.  I mark the start of the group with (NIL NIL <token> NIL)
			    result += "NIL NIL " + quotifyString(token) + " NIL)";
			    // The middle is a mailbox list
			    result += parseMailboxList(input);
			    // I mark the end of the group with (NIL NIL NIL NIL)
			    if (';' == input[0]) {
				input = input.substr(1);
				result += "(NIL NIL NIL NIL";
			    }
			}
			else {
			    result += quotifyString(token) + " " + parseRfc822AddrSpec(input);
			}
		    }
		}
	    }
	}
    }
    result += ")";
    return result;
}

static insensitiveString parseAddressList(const insensitiveString &input) {
    insensitiveString result;

    if (0 != input.size()) {
	result = "(";
	insensitiveString work = input; // removeRfc822Comments(input);
	do {
	    if (',' == work[0]) {
		work = work.substr(1);
	    }
	    result += parseAddress(work);
	    size_t begin = work.find_first_not_of(SPACE);
	    if (std::string::npos != begin) {
		size_t end = work.find_last_not_of(SPACE);
		work = work.substr(begin, end-begin+1);
	    }
	    else {
		work = "";
	    }
	} while (',' == work[0]);
	result += ")";
    }
    else {
	result = "NIL";
    }
    return result;
}

// fetchResponseEnvelope returns the envelope data of the message.
// The envelope data is a parenthesized list with the following fields
// in this order:
// date (string -- taken from m_csDateLine), subject (string -- taken from m_csSubject),
// from (parenthesized list of addresses from m_csFromLine), sender (parenthesized list
// of addresses from m_csSenderLine), reply-to (parenthesized list of addresses from 
// m_csReplyToLine), to (parenthesized list of addresses from m_csToLine), cc (parenthesized
// list of addresses from m_csCcLine), bcc (parenthesized list of addresses from m_csBccLine)
// in-reply-to (string from m_csInReplyTo), and message-id (string from m_csMessageId)
void FetchHandler::fetchResponseEnvelope(const MailMessage *message) {
    insensitiveString result("ENVELOPE ("); 
    result += quotifyString(message->dateLine()) + " ";
    if (0 != message->subject().size()) {
	result += quotifyString(message->subject()) + " ";
    }
    else {
	result += "NIL ";
    }
    insensitiveString from = parseAddressList(message->from()) + " ";
    result += from;
    if (0 != message->sender().size()) {
	result += parseAddressList(message->sender()) + " ";
    }
    else {
	result += from;
    }
    if (0 != message->replyTo().size()) {
	result += parseAddressList(message->replyTo()) + " ";
    }
    else {
	result += from;
    }
    result += parseAddressList(message->to()) + " ";
    result += parseAddressList(message->cc()) + " ";
    result += parseAddressList(message->bcc()) + " ";
    if (0 != message->inReplyTo().size()) {
	result += quotifyString(message->inReplyTo()) + " ";
    }
    else
    {
	result += "NIL ";
    }
    if (0 != message->messageId().size()) {
	result += quotifyString(message->messageId());
    }
    else {
	result += "NIL ";
    }
    result += ")";
    m_session->driver()->wantsToSend(result);
}

// The part before the slash is the body type
// The default type is "TEXT"
static insensitiveString parseBodyType(const insensitiveString &typeLine) {	
    insensitiveString result = removeRfc822Comments(typeLine);
    if (0 == result.size()) {
	result = "\"TEXT\"";
    }
    else {
	size_t pos = result.find('/');
	if (std::string::npos != pos) {
	    result = result.substr(0, pos);
	}
	else {
	    pos = result.find(';');
	    if (std::string::npos != pos) {
		result = result.substr(0, pos);
	    }
	}
	int end = result.find_last_not_of(SPACE);
	int begin = result.find_first_not_of(SPACE);
	result = result.substr(begin, end-begin+1);
	result = "\"" + result + "\"";
    }
    return result;
}

// the part between the slash and the semicolon (if any) is the body subtype
// The default subtype is "PLAIN" for text types and "UNKNOWN" for others
static insensitiveString parseBodySubtype(const insensitiveString &typeLine, MIME_MEDIA_TYPES type) {
    insensitiveString result = removeRfc822Comments(typeLine);
    size_t pos = result.find('/');
    if (std::string::npos == pos) {
	if (MIME_TYPE_TEXT == type) {
	    result = "\"PLAIN\"";
	}
	else {
	    result = "\"UNKNOWN\"";
	}
    }
    else {
	result = result.substr(pos+1);
	pos = result.find(';');
	if (std::string::npos != pos) {
	    result = result.substr(0, pos);
	}
	int end = result.find_last_not_of(SPACE);
	int begin = result.find_first_not_of(SPACE);
	result = result.substr(begin, end-begin+1);
	result = "\"" + result + "\"";
    }
    return result;
}

// The parameters are all after the first semicolon
// The default parameters are ("CHARSET" "US-ASCII") for text types, and
// NIL for everything else
static insensitiveString parseBodyParameters(const insensitiveString &typeLine, MIME_MEDIA_TYPES type) {
    insensitiveString uncommented = removeRfc822Comments(typeLine);
    insensitiveString result;
	
    size_t pos = uncommented.find(';'); 
    if (std::string::npos == pos) {
	if (MIME_TYPE_TEXT == type) {
	    result = "(\"CHARSET\" \"US-ASCII\")";
	}
	else {
	    result = "NIL";
	}
    }
    else {
	insensitiveString residue = uncommented.substr(pos+1);
	bool inQuotedString = false;
	bool escapeFlag = false;

	result = "(\"";
	for (size_t pos=0; pos<residue.size(); ++pos) {
	    if (inQuotedString) {
		if (escapeFlag) {
		    escapeFlag = false;
		    result += residue[pos];
		}
		else {
		    if ('"' == residue[pos]) {
			inQuotedString = false;
		    }
		    else {
			if ('\\' == residue[pos]) {
			    escapeFlag = true;
			}
			else {
			    result += residue[pos];
			}
		    }
		}
	    }
	    else {
		if (' ' != residue[pos]) {
		    if ('"' == residue[pos]) {
			inQuotedString = true;
		    }
		    else {
			if (';' == residue[pos]) {
			    result += "\" \"";
			}
			else {
			    if ('=' == residue[pos]) {
				result += "\" \"";
			    }
			    else {
				result += residue[pos];
			    }
			}
		    }
		}
	    }
	}
	result += "\")";
    }
    return result;
}

// fetchSubpartEnvelope returns the envelope data of an RFC 822 message subpart.
// The envelope data is a parenthesized list with the following fields
// in this order:
// date (string -- taken from m_csDateLine), subject (string -- taken from m_subject),
// from (parenthesized list of addresses from m_fromLine), sender (parenthesized list
// of addresses from m_senderLine), reply-to (parenthesized list of addresses from
// m_csReplyToLine), to (parenthesized list of addresses from m_csToLine), cc (parenthesized
// list of addresses from m_csCcLine), bcc (parenthesized list of addresses from m_csBccLine)
// in-reply-to (string from m_csInReplyTo), and message-id (string from m_csMessageId)
static insensitiveString fetchSubpartEnvelope(const MESSAGE_BODY &body) {
    insensitiveString result("(");
    HEADER_FIELDS::const_iterator field = body.fieldList.find("date");
    if (body.fieldList.end() != field) {
	result += quotifyString(field->second.c_str()) + " ";
    }
    else {
	result += "NIL ";
    }
    field = body.fieldList.find("subject");
    if (body.fieldList.end() != field) {
	result += quotifyString(field->second.c_str()) + " ";
    }
    else {
	result += "NIL ";
    }
    field = body.fieldList.find("from");
    insensitiveString from;
    if (body.fieldList.end() != field) {
	from = parseAddressList(field->second.c_str()) + " ";
    }
    else {
	from = "NIL ";
    }
    result += from;
    field = body.fieldList.find("sender");
    if (body.fieldList.end() != field) {
	result += parseAddressList(field->second.c_str()) + " ";
    }
    else {
	result += from;
    }
    field = body.fieldList.find("reply-to");
    if (body.fieldList.end() != field) {
	result += parseAddressList(field->second.c_str()) + " ";
    }
    else {
	result += from;
    }
    field = body.fieldList.find("to");
    if (body.fieldList.end() != field) {
	result += parseAddressList(field->second.c_str()) + " ";
    }
    else {
	result += "NIL ";
    }
    field = body.fieldList.find("cc");
    if (body.fieldList.end() != field) {
	result += parseAddressList(field->second.c_str()) + " ";
    }
    else {
	result += "NIL ";
    }
    field = body.fieldList.find("bcc");
    if (body.fieldList.end() != field) {
	result += parseAddressList(field->second.c_str()) + " ";
    }
    else {
	result += "NIL ";
    }
    field = body.fieldList.find("in-reply-to");
    if (body.fieldList.end() != field) {
	result += quotifyString(field->second.c_str()) + " ";
    }
    else {
	result += "NIL ";
    }
    field = body.fieldList.find("message-id");
    if (body.fieldList.end() != field) {
	result += quotifyString(field->second.c_str());
    }
    else {
	result += "NIL";
    }
    result += ")";
    return result;
}

// fetchResponseBodyStructure builds a bodystructure response.  The body structure is a representation
// of the MIME structure of the message.  It starts at the main part and goes from there.
// The results for each part are a parenthesized list of values.  For non-multipart parts, the
// fields are the body type (derived from the contentTypeLine), the body subtype (derived from the
// contentTypeLine), the body parameter parenthesized list (derived from the contentTypeLine), the
// body id (from contentIdLine), the body description (from contentDescriptionLine), the body
// encoding (from contentEncodingLine) and the body size (from text.GetLength()).  Text sections
// also have the number of lines (taken from bodyLines)
//
// Multipart parts are the list of subparts followed by the subtype (from contentTypeLine)
// Note that even though I'm passing a parameter about whether or not I want to include the 
// extension data, I'm not actually putting extension data in the response
//
// Message parts are formatted just like text parts except they have the envelope structure
// and the body structure before the number of lines.
static std::string fetchResponseBodyStructureHelper(const MESSAGE_BODY &body, bool includeExtensionData) {
    std::ostringstream result;
    result << "(";
    switch(body.bodyMediaType) {
    case MIME_TYPE_MULTIPART:
	if (NULL != body.subparts) {
	    for (size_t i=0; i<body.subparts->size(); ++i) {
		MESSAGE_BODY part = (*body.subparts)[i];
		result << fetchResponseBodyStructureHelper(part, includeExtensionData);
	    }
	}
	result << " " << parseBodySubtype(body.contentTypeLine, body.bodyMediaType).c_str();
	break;

    case MIME_TYPE_MESSAGE:
	result << parseBodyType(body.contentTypeLine).c_str();
	result << " " << parseBodySubtype(body.contentTypeLine, body.bodyMediaType).c_str(); {
	    insensitiveString uncommented = removeRfc822Comments(body.contentTypeLine);
	    size_t pos = uncommented.find(';');
	    if (std::string::npos == pos) {
		result << " NIL";
	    }
	    else {
		result << " " << parseBodyParameters(body.contentTypeLine, body.bodyMediaType).c_str();
	    }
	}
	if (0 == body.contentIdLine.size()) {
	    result << " NIL";
	}
	else {
	    result << " \"" << body.contentIdLine.c_str() << "\"";
	}
	if (0 == body.contentDescriptionLine.size()) {
	    result << " NIL";
	}
	else {
	    result << " \"" << body.contentDescriptionLine.c_str() << "\"";
	}
	if (0 == body.contentEncodingLine.size()) {
	    result << " \"7BIT\"";
	}
	else {
	    result << " \"" << body.contentEncodingLine.c_str() << "\"";
	}
	if ((NULL != body.subparts) && (0<body.subparts->size())) {
	    result << " " << (body.bodyOctets - body.headerOctets);
	    MESSAGE_BODY part = (*body.subparts)[0];
	    result << " " << fetchSubpartEnvelope(part).c_str();
	    result << " " << fetchResponseBodyStructureHelper(part, includeExtensionData);
	}
	break;

    default:
	result << parseBodyType(body.contentTypeLine).c_str();
	result << " " << parseBodySubtype(body.contentTypeLine, body.bodyMediaType).c_str();
	result << " " << parseBodyParameters(body.contentTypeLine, body.bodyMediaType).c_str();
	if (0 == body.contentIdLine.size()) {
	    result << " NIL";
	}
	else {
	    result << " \"" << body.contentIdLine.c_str() << "\"";
	}
	if (0 == body.contentDescriptionLine.size()) {
	    result << " NIL";
	}
	else {
	    result << " \"" << body.contentDescriptionLine.c_str() << "\"";
	}
	if (0 == body.contentEncodingLine.size()) {
	    result << " \"7BIT\"";
	}
	else {
	    result << " \"" << body.contentEncodingLine.c_str() << "\"";
	}
	result << " " << (body.bodyOctets - body.headerOctets);
	break;
    }
    if ((MIME_TYPE_TEXT == body.bodyMediaType) || (MIME_TYPE_MESSAGE == body.bodyMediaType)) {
	result << " " << (body.bodyLines - body.headerLines);
    }
    result << ")";
    return result.str();
}

void FetchHandler::fetchResponseBodyStructure(const MailMessage *message) {
    std::string result("BODYSTRUCTURE ");
    result +=  fetchResponseBodyStructureHelper(message->messageBody(), true);
    m_session->driver()->wantsToSend(result);
}

void FetchHandler::fetchResponseBody(const MailMessage *message) {
    std::string result("BODY ");
    result +=  fetchResponseBodyStructureHelper(message->messageBody(), false);
    m_session->driver()->wantsToSend(result);
}

#define SendBlank() (blankLen?(m_session->driver()->wantsToSend(" ",1),0):0)

IMAP_RESULTS FetchHandler::execute(void) {
    IMAP_RESULTS finalResult = IMAP_OK;

    size_t executePointer = strlen((char *)m_parseBuffer) + 1;
    executePointer += strlen((char *)&m_parseBuffer[executePointer]) + 1;
    SEARCH_RESULT srVector;

    if (m_usingUid) {
	executePointer += strlen((char *)&m_parseBuffer[executePointer]) + 1;
	m_session->uidSequenceSet(srVector, executePointer);
    }
    else {
	if (!m_session->msnSequenceSet(srVector, executePointer)) {
	    m_session->responseText("Invalid Message Sequence Number");
	    finalResult = IMAP_NO;
	}
    }

    if (MailStore::SUCCESS == m_session->store()->lock()) {
	for (size_t i = 0; (IMAP_BAD != finalResult) && (i < srVector.size()); ++i) {
	    int blankLen = 0;
	    MailMessage *message;

	    MailMessage::MAIL_MESSAGE_RESULT messageReadResult = m_session->store()->messageData(&message, srVector[i]);
	    unsigned long uid = srVector[i];
	    if (MailMessage::SUCCESS == messageReadResult) {
		IMAP_RESULTS result = IMAP_OK;

		bool seenFlag = false;
		size_t specificationBase;
		// v-- This part syntax-checks the fetch request
		specificationBase = executePointer + strlen(m_parseBuffer->parseStr(executePointer)) + 1;
		if (0 == strcmp("(", m_parseBuffer->parseStr(specificationBase))) {
		    specificationBase += strlen(m_parseBuffer->parseStr(specificationBase)) + 1;
		}
		while ((IMAP_OK == result) && (specificationBase < m_parseBuffer->parsePointer())) {
		    FETCH_NAME_T::iterator which = fetchSymbolTable.find(m_parseBuffer->parseStr(specificationBase));
		    if (fetchSymbolTable.end() == which) {
			// This may be an extended BODY name, so I have to check for that.
			char *temp;
			temp = strchr(m_parseBuffer->parseStr(specificationBase), '[');
			if (NULL != temp) {
			    *temp = '\0';
			    FETCH_NAME_T::iterator which = fetchSymbolTable.find((char *)&m_parseBuffer[specificationBase]);
			    specificationBase += strlen(m_parseBuffer->parseStr(specificationBase)) + 1;
			    *temp = '[';
			    if (fetchSymbolTable.end() != which) {
				switch(which->second) {
				case FETCH_BODY:
				case FETCH_BODY_PEEK:
				    break;

				default:
				    m_session->responseText("Malformed Command");
				    result = IMAP_BAD;
				    break;
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
			MESSAGE_BODY body = message->messageBody();
			while ((IMAP_OK == result) && isdigit(m_parseBuffer->parseChar(specificationBase))) {
			    char *end;
			    unsigned long section = strtoul(m_parseBuffer->parseStr(specificationBase), &end, 10);
			    // This part here is because the message types have two headers, one for the
			    // subpart the message is in, one for the message that's contained in the subpart
			    // if I'm looking at a subpart of that subpart, then I have to skip the subparts
			    // header and look at the message's header, which is handled by the next four
			    // lines of code.
			    if ((body.bodyMediaType == MIME_TYPE_MESSAGE) && (1 == body.subparts->size())) {
				body = (*body.subparts)[0];
			    }
			    if (NULL != body.subparts) {
				if ((0 < section) && (section <= (body.subparts->size()))) {
				    body = (*body.subparts)[section-1];
				    specificationBase += (end - m_parseBuffer->parseStr(specificationBase));
				    if ('.' == *end) {
					++specificationBase;
				    }
				}
				else {
				    m_session->responseText("Invalid Subpart");
				    result = IMAP_BAD;
				}
			    }
			    else {
				if ((1 == section) && (']' == *end)) {
				    specificationBase += (end - m_parseBuffer->parseStr(specificationBase));
				}
				else {
				    m_session->responseText("Invalid Subpart");
				    result = IMAP_BAD;
				}
			    }
			}
			// When I get here, and if everything's okay, then I'm looking for a possibly nonzero length of characters
			// representing the section-msgtext
			FETCH_BODY_PARTS whichPart;
			if ((IMAP_OK == result) && (m_parseBuffer->parsePointer() > specificationBase)) {
			    // If I'm out of string and the next string begins with ']', then I'm returning the whole body of
			    // whatever subpart I'm at
			    if (']' == m_parseBuffer->parseChar(specificationBase)) {
				whichPart = FETCH_BODY_BODY;
			    }
			    else {
				insensitiveString part(m_parseBuffer->parseStr(specificationBase));
				// I'm looking for HEADER[.FIELDS[.NOT]], TEXT, or MIME
				if (part.substr(0, 6) == "header") {
				    if (']' == part[6]) {
					specificationBase += 6;
					whichPart = FETCH_BODY_HEADER;
				    }
				    else {
					if (part.substr(6, 7) == ".FIELDS") {
					    if (13 == part.size()) {
						specificationBase += strlen(m_parseBuffer->parseStr(specificationBase)) + 1;
						whichPart = FETCH_BODY_FIELDS;
					    }
					    else {
						if (part.substr(13) == ".NOT") {
						    specificationBase +=  strlen(m_parseBuffer->parseStr(specificationBase)) + 1;
						    whichPart = FETCH_BODY_NOT_FIELDS;
						}
						else {
						    m_session->responseText("Malformed Command");
						    result = IMAP_BAD;
						}
					    }
					}
					else {
					    m_session->responseText("Malformed Command");
					    result = IMAP_BAD;
					}
				    }
				}
				else {
				    if (part.substr(0, 5) == "TEXT]") {
					specificationBase += 4;
					whichPart = FETCH_BODY_TEXT;
				    }
				    else {
					if (part.substr(0, 5) == "MIME]") {
					    specificationBase += 4;
					    whichPart = FETCH_BODY_MIME;
					}
					else {
					    m_session->responseText("Malformed Command");
					    result = IMAP_BAD;
					}
				    }
				}
			    }
			}
			// Now look for the list of headers to return
			std::vector<std::string> fieldList;
			if ((IMAP_OK == result) && (m_parseBuffer->parsePointer() > specificationBase) &&
			    ((FETCH_BODY_FIELDS == whichPart) || (FETCH_BODY_NOT_FIELDS == whichPart))) {
			    if (0 == strcmp(m_parseBuffer->parseStr(specificationBase), "(")) {
				specificationBase +=  strlen(m_parseBuffer->parseStr(specificationBase)) + 1;
				while((IMAP_BUFFER_LEN > specificationBase) &&
				      (0 != strcmp(m_parseBuffer->parseStr(specificationBase), ")"))) {
				    specificationBase += strlen(m_parseBuffer->parseStr(specificationBase)) + 1;
				}
				if (0 == strcmp(m_parseBuffer->parseStr(specificationBase), ")")) {
				    specificationBase += strlen(m_parseBuffer->parseStr(specificationBase)) + 1;
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

			// Okay, look for the end square bracket
			if ((IMAP_OK == result) && (']' == m_parseBuffer->parseChar(specificationBase))) {
			    specificationBase++;
			}
			else {
			    m_session->responseText("Malformed Command");
			    result = IMAP_BAD;
			}

			// Okay, get the limits, if any.
			if ((IMAP_OK == result) && (m_parseBuffer->parsePointer() > specificationBase)) {
			    if ('<' == m_parseBuffer->parseChar(specificationBase))
			    {
				++specificationBase;
				char *end;
				strtoul(m_parseBuffer->parseStr(specificationBase), &end, 10);
				if ('.' == *end) {
				    strtoul(end+1, &end, 10);
				}
				if ('>' != *end) {
				    m_session->responseText("Malformed Command");
				    result = IMAP_BAD;
				}
			    }
			}

			if (IMAP_OK == result) {
			    switch(whichPart) {
			    case FETCH_BODY_BODY:
			    case FETCH_BODY_MIME:
			    case FETCH_BODY_HEADER:
			    case FETCH_BODY_FIELDS:
			    case FETCH_BODY_NOT_FIELDS:
			    case FETCH_BODY_TEXT:
				break;

			    default:
				m_session->responseText("Malformed Command");
				result = IMAP_BAD;
				break;
			    }
			}
		    }
		    specificationBase += strlen(m_parseBuffer->parseStr(specificationBase)) + 1;
		    // SYZYGY -- Valgrind sometimes flags this as uninitialized
		    // SYZYGY -- I suppose it's running off the end of m_parseBuffer
		    if (0 == strcmp(")", m_parseBuffer->parseStr(specificationBase))) {
			specificationBase += strlen(m_parseBuffer->parseStr(specificationBase)) + 1;
		    }
		}
		// ^-- This part syntax-checks the fetch request
		// v-- This part executes the fetch request
		if (IMAP_OK == result) {
		    specificationBase = executePointer + strlen(m_parseBuffer->parseStr(executePointer)) + 1;
		    if (0 == strcmp("(", m_parseBuffer->parseStr(specificationBase))) {
			specificationBase += strlen(m_parseBuffer->parseStr(specificationBase)) + 1;
		    }
		    std::ostringstream fetchResult;
		    fetchResult << "* " << message->msn() << " FETCH (";
		    m_session->driver()->wantsToSend(fetchResult.str());
		}
		while ((IMAP_OK == result) && (specificationBase < m_parseBuffer->parsePointer())) {
		    SendBlank();
		    FETCH_NAME_T::iterator which = fetchSymbolTable.find(m_parseBuffer->parseStr(specificationBase));
		    if (fetchSymbolTable.end() != which) {
			blankLen = 1;
			switch(which->second) {
			case FETCH_ALL:
			    m_session->fetchResponseFlags(message->messageFlags());
			    SendBlank();
			    fetchResponseInternalDate(message);
			    SendBlank();
			    fetchResponseRfc822Size(message);
			    SendBlank();
			    fetchResponseEnvelope(message);
			    break;

			case FETCH_FAST:
			    m_session->fetchResponseFlags(message->messageFlags());
			    SendBlank();
			    fetchResponseInternalDate(message);
			    SendBlank();
			    fetchResponseRfc822Size(message);
			    break;

			case FETCH_FULL:
			    m_session->fetchResponseFlags(message->messageFlags());
			    SendBlank();
			    fetchResponseInternalDate(message);
			    SendBlank();
			    fetchResponseRfc822Size(message);
			    SendBlank();
			    fetchResponseEnvelope(message);
			    SendBlank();
			    fetchResponseBody(message);
			    break;

			case FETCH_BODY_PEEK:
			case FETCH_BODY:
			    fetchResponseBody(message);
			    break;

			case FETCH_BODYSTRUCTURE:
			    fetchResponseBodyStructure(message);
			    break;

			case FETCH_ENVELOPE:
			    fetchResponseEnvelope(message);
			    break;

			case FETCH_FLAGS:
			    m_session->fetchResponseFlags(message->messageFlags());
			    break;

			case FETCH_INTERNALDATE:
			    fetchResponseInternalDate(message);
			    break;

			case FETCH_RFC822:
			    fetchResponseRfc822(uid, message);
			    seenFlag = true;
			    break;
						
			case FETCH_RFC822_HEADER:
			    fetchResponseRfc822Header(uid, message);
			    break;

			case FETCH_RFC822_SIZE:
			    fetchResponseRfc822Size(message);
			    break;

			case FETCH_RFC822_TEXT:
			    fetchResponseRfc822Text(uid, message);
			    seenFlag = true;
			    break;

			case FETCH_UID:
			    m_session->fetchResponseUid(uid);
			    break;

			default:
			    m_session->responseText("Internal Logic Error");
			    //    					result = IMAP_NO;
			    break;
			}
		    }
		    else {
			// This may be an extended BODY name, so I have to check for that.
			char *temp;
			temp = strchr((char *)&m_parseBuffer[specificationBase], '[');
			if (NULL != temp) {
			    *temp = '\0';
			    FETCH_NAME_T::iterator which = fetchSymbolTable.find((char *)&m_parseBuffer[specificationBase]);
			    specificationBase += strlen((char *)&m_parseBuffer[specificationBase]) + 1;
			    *temp = '[';
			    if (fetchSymbolTable.end() != which) {
				switch(which->second) {
				case FETCH_BODY:
				    seenFlag = true;
				    // NOTE NO BREAK!
				case FETCH_BODY_PEEK:
				    m_session->driver()->wantsToSend("BODY[");
				    break;

				default:
				    m_session->responseText("Malformed Command");
				    result = IMAP_BAD;
				    break;
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
			MESSAGE_BODY body = message->messageBody();
			// I need a part number flag because, for a single part message, body[1] is
			// different from body[], but later on, when I determine what part to fetch,
			// I won't know whether I've got body[1] or body[], and partNumberFlag
			// records the difference
			bool partNumberFlag = false;
			while ((IMAP_OK == result) && isdigit(m_parseBuffer->parseChar(specificationBase))) {
			    partNumberFlag = true;
			    char *end;
			    unsigned long section = strtoul(m_parseBuffer->parseStr(specificationBase), &end, 10);
			    // This part here is because the message types have two headers, one for the
			    // subpart the message is in, one for the message that's contained in the subpart
			    // if I'm looking at a subpart of that subpart, then I have to skip the subparts
			    // header and look at the message's header, which is handled by the next four
			    // lines of code.
			    if ((body.bodyMediaType == MIME_TYPE_MESSAGE) && (1 == body.subparts->size())) {
				body = (*body.subparts)[0];
			    }
			    if (NULL != body.subparts) {
				if ((0 < section) && (section <= (body.subparts->size()))) {
				    std::ostringstream sectionString;
				    sectionString << section;
				    m_session->driver()->wantsToSend(sectionString.str());
				    body = (*body.subparts)[section-1];
				    specificationBase += (end - ((char *)&m_parseBuffer[specificationBase]));
				    if ('.' == *end) {
					m_session->driver()->wantsToSend(".");
					++specificationBase;
				    }
				}
				else {
				    m_session->responseText("Invalid Subpart");
				    result = IMAP_NO;
				}
			    }
			    else {
				std::ostringstream sectionString;
				sectionString << section;
				m_session->driver()->wantsToSend(sectionString.str());
				// SYZYGY -- Danger!  Will Robinson!
				// How do I know that specificationBase is relative to the same
				// character array?  I don't unless I put other conditions on it
				specificationBase += (end - m_parseBuffer->parseStr(specificationBase));
			    }
			}
			// When I get here, and if everything's okay, then I'm looking for a possibly nonzero length of characters
			// representing the section-msgtext
			FETCH_BODY_PARTS whichPart;
			if ((IMAP_OK == result) && (m_parseBuffer->parsePointer() > specificationBase)) {
			    // If I'm out of string and the next string begins with ']', then I'm returning the whole body of
			    // whatever subpart I'm at
			    if (']' == m_parseBuffer->parseChar(specificationBase)) {
				// Unless it's a single part message -- in which case I return the text
				if (!partNumberFlag || (NULL != body.subparts)) {
				    whichPart = FETCH_BODY_BODY;
				}
				else {
				    whichPart = FETCH_BODY_TEXT;
				}
			    }
			    else {
				insensitiveString part((char *)&m_parseBuffer[specificationBase]);
				// I'm looking for HEADER[.FIELDS[.NOT]], TEXT, or MIME
				if (part.substr(0, 6) == "header") {
				    if (']' == part[6]) {
					specificationBase += 6;
					m_session->driver()->wantsToSend("HEADER");
					whichPart = FETCH_BODY_HEADER;
				    }
				    else {
					if (part.substr(6,7) == ".FIELDS") {
					    if (13 == part.size()) {
						specificationBase += strlen((char *)&m_parseBuffer[specificationBase]) + 1;
						m_session->driver()->wantsToSend("HEADER.FIELDS");
						whichPart = FETCH_BODY_FIELDS;
					    }
					    else {
						if (part.substr(13) == ".NOT") {
						    specificationBase += strlen((char *)&m_parseBuffer[specificationBase]) + 1;
						    m_session->driver()->wantsToSend("HEADER.FIELDS.NOT");
						    whichPart = FETCH_BODY_NOT_FIELDS;
						}
						else {
						    m_session->responseText("Malformed Command");
						    result = IMAP_BAD;
						}
					    }
					}
					else {
					    m_session->responseText("Malformed Command");
					    result = IMAP_BAD;
					}
				    }
				}
				else {
				    if (part.substr(0, 5) == "TEXT]") {
					specificationBase += 4;
					m_session->driver()->wantsToSend("TEXT");
					whichPart = FETCH_BODY_TEXT;
				    }
				    else {
					if (part.substr(0, 5) == "MIME]") {
					    specificationBase += 4;
					    m_session->driver()->wantsToSend("MIME");
					    whichPart = FETCH_BODY_MIME;
					}
					else {
					    m_session->responseText("Malformed Command");
					    result = IMAP_BAD;
					}
				    }
				}
			    }
			}
			// Now look for the list of headers to return
			std::vector<insensitiveString> fieldList;
			if ((IMAP_OK == result) && (m_parseBuffer->parsePointer() > specificationBase) &&
			    ((FETCH_BODY_FIELDS == whichPart) || (FETCH_BODY_NOT_FIELDS == whichPart))) {
			    if (0 == strcmp(m_parseBuffer->parseStr(specificationBase), "(")) {
				m_session->driver()->wantsToSend(" (");
				specificationBase += strlen(m_parseBuffer->parseStr(specificationBase)) + 1;
				int blankLen = 0;
				while((IMAP_BUFFER_LEN > specificationBase) &&
				      (0 != strcmp(m_parseBuffer->parseStr(specificationBase), ")"))) {
				    int len = (int) strlen(m_parseBuffer->parseStr(specificationBase));
				    SendBlank();
				    m_session->driver()->wantsToSend(m_parseBuffer->parseStr(specificationBase), len);
				    blankLen = 1;
				    fieldList.push_back((char*)&m_parseBuffer[specificationBase]);
				    specificationBase += len + 1;
				}
				if (0 == strcmp((char *)&m_parseBuffer[specificationBase], ")")) {
				    m_session->driver()->wantsToSend(")");
				    specificationBase += strlen((char *)&m_parseBuffer[specificationBase]) + 1;
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

			// Okay, look for the end square bracket
			if ((IMAP_OK == result) && (']' == m_parseBuffer->parseChar(specificationBase))) {
			    m_session->driver()->wantsToSend("]");
			    specificationBase++;
			}
			else {
			    m_session->responseText("Malformed Command");
			    result = IMAP_BAD;
			}

			// Okay, get the limits, if any.
			size_t firstByte = 0, maxLength = ~0;
			if ((IMAP_OK == result) && (m_parseBuffer->parsePointer() > specificationBase)) {
			    if ('<' == m_parseBuffer->parseChar(specificationBase)) {
				++specificationBase;
				char *end;
				firstByte = strtoul((char *)&m_parseBuffer[specificationBase], &end, 10);
				std::ostringstream limit;
				limit << "<" << firstByte << ">";
				m_session->driver()->wantsToSend(limit.str());
				if ('.' == *end) {
				    maxLength = strtoul(end+1, &end, 10);
				}
				if ('>' != *end) {
				    m_session->responseText("Malformed Command");
				    result = IMAP_BAD;
				}
			    }
			}

			if (IMAP_OK == result) {
			    size_t length;

			    switch(whichPart) {
			    case FETCH_BODY_BODY:
				// This returns all the data header and text, but it's limited by first_byte and max_length
				if ((body.bodyMediaType == MIME_TYPE_MESSAGE) || (partNumberFlag && (body.bodyMediaType == MIME_TYPE_MULTIPART))) {
				    // message parts are special, I have to skip over the mime header before I begin
				    firstByte += body.headerOctets;
				}
				if (firstByte < body.bodyOctets) {
				    length = MIN(body.bodyOctets - firstByte, maxLength);
				    m_session->driver()->wantsToSend(" ");
				    messageChunk(uid, firstByte + body.bodyStartOffset, length);
				}
				else {
				    m_session->driver()->wantsToSend(" {0}\r\n");
				}
				break;

			    case FETCH_BODY_HEADER:
				if (!partNumberFlag || ((body.bodyMediaType == MIME_TYPE_MESSAGE) && (1 == body.subparts->size()))) {
				    if (partNumberFlag) {
					// It's a subpart
					body = (*body.subparts)[0];
				    }
				    if (firstByte < body.headerOctets) {
					length = MIN(body.headerOctets - firstByte, maxLength);
					m_session->driver()->wantsToSend(" ");
					messageChunk(uid, firstByte + body.bodyStartOffset, length);
				    }
				    else {
					m_session->driver()->wantsToSend(" {0}\r\n");
				    }
				}
				else {
				    // If it's not a message subpart, it doesn't have a header.
				    m_session->driver()->wantsToSend(" {0}\r\n");
				}
				break;

			    case FETCH_BODY_MIME:
				if (firstByte < body.headerOctets) {
				    length = MIN(body.headerOctets - firstByte, maxLength);
				    m_session->driver()->wantsToSend(" ");
				    messageChunk(uid, firstByte + body.bodyStartOffset, length);
				}
				else {
				    m_session->driver()->wantsToSend(" {0}\r\n");
				}
				break;

			    case FETCH_BODY_FIELDS:
				if ((body.bodyMediaType == MIME_TYPE_MESSAGE) && (1 == body.subparts->size())) {
				    body = (*body.subparts)[0];
				}
				{
				    std::string interesting;

				    // For each element in fieldList, look up the corresponding entries in body.fieldList
				    // and display
				    for(size_t i=0; i<fieldList.size(); ++i) {
					for (HEADER_FIELDS::const_iterator iter = body.fieldList.lower_bound(fieldList[i]);
					     body.fieldList.upper_bound(fieldList[i]) != iter; ++iter) {
					    interesting += iter->first.c_str();
					    interesting += ": ";
					    interesting += iter->second.c_str();
					    interesting += "\r\n";
					}
				    }
				    interesting += "\r\n";
				    if (firstByte < interesting.size()) {
					length = MIN(interesting.size() - firstByte, maxLength);

					std::ostringstream headerFields;

					headerFields << " {" << length << "}\r\n";
					headerFields << interesting.substr(firstByte, length);
					m_session->driver()->wantsToSend(headerFields.str());
				    }
				    else {
					m_session->driver()->wantsToSend(" {0}\r\n");
				    }
				}
				break;

			    case FETCH_BODY_NOT_FIELDS:
				if ((body.bodyMediaType == MIME_TYPE_MESSAGE) && (1 == body.subparts->size())) {
				    body = (*body.subparts)[0];
				}
				{
				    insensitiveString interesting;

				    for (HEADER_FIELDS::const_iterator i = body.fieldList.begin(); body.fieldList.end() != i; ++i) {
					if (fieldList.end() == std::find(fieldList.begin(), fieldList.end(), i->first)) {
					    interesting += i->first;
					    interesting += ": ";
					    interesting += i->second;
					    interesting += "\r\n";
					}
				    }
				    interesting += "\r\n";
				    if (firstByte < interesting.size()) {
					length = MIN(interesting.size() - firstByte, maxLength);

					std::ostringstream headerFields;
					headerFields << " {" << length << "}\r\n";
					headerFields << interesting.substr(firstByte, length).c_str();
					m_session->driver()->wantsToSend(headerFields.str());
				    }
				    else {
					m_session->driver()->wantsToSend(" {0}\r\n");
				    }
				}
				break;

			    case FETCH_BODY_TEXT:
				if ((body.bodyMediaType == MIME_TYPE_MESSAGE) && (1 == body.subparts->size())) {
				    body = (*body.subparts)[0];
				}
				if (0 < body.headerOctets) {
				    std::string result;
				    size_t length;
				    if (body.bodyOctets > body.headerOctets) {
					length = body.bodyOctets - body.headerOctets;
				    }
				    else {
					length = 0;
				    }
				    if (firstByte < length) {
					length = MIN(length - firstByte, maxLength);
					m_session->driver()->wantsToSend(" ");
					messageChunk(uid, firstByte + body.bodyStartOffset + body.headerOctets, length);
				    }
				    else {
					m_session->driver()->wantsToSend(" {0}\r\n");
				    }
				}
				else {
				    m_session->driver()->wantsToSend(" {0}\r\n");
				}
				break;

			    default:
				m_session->responseText("Malformed Command");
				result = IMAP_BAD;
				break;
			    }
			}
		    }
		    blankLen = 1;
		    specificationBase += strlen((char *)&m_parseBuffer[specificationBase]) + 1;
		    if (0 == strcmp(")", (char *)&m_parseBuffer[specificationBase])) {
			specificationBase += strlen((char *)&m_parseBuffer[specificationBase]) + 1;
		    }
		}
		// Here, I update the SEEN flag, if that is necessary, and send the flags if that flag has
		// changed state
		if (IMAP_OK == result) {
		    if (seenFlag && (0 == (MailStore::IMAP_MESSAGE_SEEN & message->messageFlags()))) {
			uint32_t updatedFlags;
			if (MailStore::SUCCESS == m_session->store()->messageUpdateFlags(srVector[i], ~0, MailStore::IMAP_MESSAGE_SEEN, updatedFlags)) {
			    SendBlank();
			    m_session->fetchResponseFlags(updatedFlags);
			    blankLen = 1;
			}
		    }
		    if (m_usingUid) {
			SendBlank();
			m_session->fetchResponseUid(uid);
			blankLen = 1;
		    }
		    m_session->driver()->wantsToSend(")\r\n");
		    blankLen = 0;
		}
		else {
		    finalResult = result;
		}
	    }
	    else {
		finalResult = IMAP_NO;
		if (MailMessage::MESSAGE_DOESNT_EXIST != messageReadResult) {
		    m_session->responseText("Internal Mailstore Error");
		}
		else {
		    m_session->responseText("Message Doesn't Exist");
		}
	    }
	    // ^-- This part executes the fetch request
	}
	m_session->store()->unlock();
    }
    else {
	finalResult = IMAP_TRY_AGAIN;
    }
    return finalResult;
}

// The problem is that I'm using m_parseStage to do the parenthesis balance, so this needs to be redesigned
IMAP_RESULTS FetchHandler::receiveData(INPUT_DATA_STRUCT &input) {
    IMAP_RESULTS result = IMAP_OK;

    // The first thing up is a sequence set.  This is a sequence of digits, commas, and colons, followed by
    // a space
    switch (m_parseStage) {
    case 0: {
	size_t len = strspn(((char *)input.data)+input.parsingAt, "0123456789,:*");
	if ((0 < len) && (input.dataLen >= (input.parsingAt + len + 2)) && (' ' == input.data[input.parsingAt+len]) && 
	    (IMAP_BUFFER_LEN > (m_parseBuffer->parsePointer()+len+1)))  {
	    m_parseBuffer->addToParseBuffer(input, len);
	    input.parsingAt++;

	    m_parseStage = 1;
	    if ('(' == input.data[input.parsingAt]) {
		m_parseBuffer->addToParseBuffer(input, 1);
		++m_parseStage;
		while ((IMAP_OK == result) && (input.dataLen > input.parsingAt) && (1 < m_parseStage)) {
		    if((input.dataLen > input.parsingAt) && (' ' == input.data[input.parsingAt])) {
			++input.parsingAt;
		    }
		    if((input.dataLen > input.parsingAt) && ('(' == input.data[input.parsingAt])) {
			m_parseBuffer->addToParseBuffer(input, 1);
			++m_parseStage;
		    }
		    switch (m_parseBuffer->astring(input, true, NULL)) {
		    case ImapStringGood:
			result = IMAP_OK;
			break;

		    case ImapStringBad:
			m_session->responseText("Malformed Command");
			result = IMAP_BAD;
			break;

		    case ImapStringPending:
			m_session->responseText("Ready for Literal");
			result = IMAP_NOTDONE;
			break;

		    default:
			m_session->responseText("Failed");
			result = IMAP_BAD;
			break;
		    }
		    if ((input.dataLen > input.parsingAt) && (')' == input.data[input.parsingAt])) {
			m_parseBuffer->addToParseBuffer(input, 1);
			--m_parseStage;
		    }
		}
		if ((IMAP_OK == result) && (1 == m_parseStage)) {
		    if (input.dataLen == input.parsingAt) {
			m_parseStage = 2;
			result = execute();
		    }
		    else {
			m_session->responseText("Malformed Command");
			result = IMAP_BAD;
		    }
		}
	    }
	    else {
		while ((IMAP_OK == result) && (input.dataLen > input.parsingAt)) {
		    if (' ' == input.data[input.parsingAt]) {
			++input.parsingAt;
		    }
		    if ((input.dataLen > input.parsingAt) && ('(' == input.data[input.parsingAt])) {
			m_parseBuffer->addToParseBuffer(input, 1);
		    }
		    if ((input.dataLen > input.parsingAt) && (')' == input.data[input.parsingAt])) {
			m_parseBuffer->addToParseBuffer(input, 1);
		    }
		    switch (m_parseBuffer->astring(input, true, NULL)) {
		    case ImapStringGood:
			break;

		    case ImapStringBad:
			m_session->responseText("Malformed Command");
			result = IMAP_BAD;
			break;

		    case ImapStringPending:
			++m_parseStage;
			m_session->responseText("Ready for Literal");
			result = IMAP_NOTDONE;
			break;

		    default:
			m_session->responseText("Failed");
			result = IMAP_BAD;
			break;
		    }
		}
		if (IMAP_OK == result) {
		    result = execute();
		}
	    }
	}
	else {
	    m_session->responseText("Malformed Command");
	    result = IMAP_BAD;
	}
    }
	break;

    case 1:
	m_parseBuffer->addLiteralToParseBuffer(input);

	// If there was more data than needed for the literal
	if (0 == m_parseBuffer->literalLength()) {
	    IMAP_RESULTS result = IMAP_OK;
	
	    // loop through the what's left on the line
	    while ((IMAP_OK == result) && (input.dataLen > input.parsingAt)) {
		// begin by clearing off any preceeding spaces
		if((input.dataLen > input.parsingAt) && (' ' == input.data[input.parsingAt])) {
		    ++input.parsingAt;
		}
		// If there's a parenthesis, then go past it, too
		if((input.dataLen > input.parsingAt) && ('(' == input.data[input.parsingAt])) {
		    m_parseBuffer->addToParseBuffer(input, 1);
		}
		// If the parens aren't empty, then I must have an astring, so I can copy it to the parse buffer
		if ((input.dataLen > input.parsingAt) && (')' != input.data[input.parsingAt])) {
		    switch (m_parseBuffer->astring(input, true, NULL)) {
		    case ImapStringGood:
			result = IMAP_OK;
			break;

			// of course, if the astring is malformed, then signal an error
		    case ImapStringBad:
			m_session->responseText("Malformed Command");
			result = IMAP_BAD;
			break;

		    case ImapStringPending:
			m_session->responseText("Ready for Literal");
			result = IMAP_NOTDONE;
			break;

		    default:
			m_session->responseText("Failed");
			result = IMAP_BAD;
			break;
		    }
		}
		// At this point, I go past the the closing parenthesis
		if ((input.dataLen > input.parsingAt) && (')' == input.data[input.parsingAt])) {
		    m_parseBuffer->addToParseBuffer(input, 1);
		}
	    }
	    // When I get here, I've gotten past the parsing and I can get on with stage 2, 
	    // execution
	    if (IMAP_OK == result) {
		m_parseStage = 2;
		result = execute();
	    }
	}
	else {
	    result = IMAP_IN_LITERAL;
	}
	break;

	// the original case 1
#if 0
    case 1:
	if (dataLen >= m_parseBuffer->literalLength()) {
	    m_parseBuffer->addToParseBuffer(data, m_parseBuffer->literalLength());
	    size_t i = m_parseBuffer->literalLength();
	    m_parseBuffer->literalLength(0);
	    if ((i < dataLen) && (' ' == data[i])) {
		++i;
	    }
	    if (2 < dataLen) {
		// Get rid of the CRLF if I have it
		dataLen -= 2;
		data[dataLen] = '\0';  // Make sure it's terminated so strchr et al work
	    }
	    if (1 == m_parseStage) {
		IMAP_RESULTS result = IMAP_OK;
		while ((IMAP_OK == result) && (dataLen > i)) {
		    if((dataLen > i) && (' ' == data[i])) {
			++i;
		    }
		    if((dataLen > i) && ('(' == data[i])) {
			m_parseBuffer->addToParseBuffer(&data[i], 1);
			++i;
		    }
		    if ((dataLen > i) && (')' != data[i])) {
			switch (m_parseBuffer->astring(data, dataLen, i, true, NULL)) {
			case ImapStringGood:
			    result = IMAP_OK;
			    break;

			case ImapStringBad:
			    strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
			    result = IMAP_BAD;
			    break;

			case ImapStringPending:
			    strncpy(m_responseText, "Ready for Literal", MAX_RESPONSE_STRING_LENGTH);
			    result = IMAP_NOTDONE;
			    break;

			default:
			    strncpy(m_responseText, "Failed", MAX_RESPONSE_STRING_LENGTH);
			    result = IMAP_BAD;
			    break;
			}
		    }
		    if ((dataLen > i) && (')' == data[i])) {
			m_parseBuffer->addToParseBuffer(&data[i], 1);
			++i;
		    }
		}
		if (IMAP_OK == result) {
		    m_parseStage = 2;
		    result = fetchHandlerExecute(m_usesUid);
		}
	    }
	    else {
		IMAP_RESULTS result = IMAP_OK;
		while ((IMAP_OK == result) && (dataLen > i) && (')' != data[i]) && (1 < m_parseStage)) {
		    if((dataLen > i) && (' ' == data[i])) {
			++i;
		    }
		    if((dataLen > i) && ('(' == data[i])) {
			m_parseBuffer->addToParseBuffer(&data[i], 1);
			++i;
			++m_parseStage;
		    }
		    switch (m_parseBuffer->astring(data, dataLen, i, true, NULL)) {
		    case ImapStringGood:
			result = IMAP_OK;
			break;

		    case ImapStringBad:
			strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_BAD;
			break;

		    case ImapStringPending:
			strncpy(m_responseText, "Ready for Literal", MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_NOTDONE;
			break;

		    default:
			strncpy(m_responseText, "Failed", MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_BAD;
			break;
		    }
		    if ((dataLen > i) && (')' == data[i])) {
			m_parseBuffer->addToParseBuffer(&data[i], 1);
			++i;
			--m_parseStage;
		    }
		}
		if ((IMAP_OK == result) && (1 == m_parseStage)) {
		    if (dataLen == i) {
			result = fetchHandlerExecute(m_usesUid);
		    }
		    else {
			strncpy(m_responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_BAD;
		    }
		}
		if (IMAP_NOTDONE != result) {
		    result = fetchHandlerExecute(m_usesUid);
		}
	    }
	}
	else {
	    result = IMAP_IN_LITERAL;
	    m_parseBuffer->addToParseBuffer(data, dataLen, false);
	    m_parseBuffer->literalLength(m_parseBuffer->literalLength() - dataLen);
	}
	break;
#endif // 0
    default:
	result = execute();
	break;
    }

    return result;
}

