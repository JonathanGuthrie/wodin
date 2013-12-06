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

#include "searchhandler.hpp"

ImapHandler *searchHandler(ImapSession *session, INPUT_DATA_STRUCT &input) {
    (void) input;
    return new SearchHandler(session, session->parseBuffer(), false);
}

ImapHandler *uidSearchHandler(ImapSession *session, INPUT_DATA_STRUCT &input) {
    (void) input;
    return new SearchHandler(session, session->parseBuffer(), true);
}


typedef enum {
    SSV_ERROR,
    SSV_ALL,
    SSV_ANSWERED,
    SSV_BCC,
    SSV_BEFORE,
    SSV_BODY,
    SSV_CC,
    SSV_DELETED,
    SSV_FLAGGED,
    SSV_FROM,
    SSV_KEYWORD,
    SSV_NEW,
    SSV_OLD,
    SSV_ON,
    SSV_RECENT,
    SSV_SEEN,
    SSV_SINCE,
    SSV_SUBJECT,
    SSV_TEXT,
    SSV_TO,
    SSV_UNANSWERED,
    SSV_UNDELETED,
    SSV_UNFLAGGED,
    SSV_UNKEYWORD,
    SSV_UNSEEN,
    SSV_DRAFT,
    SSV_HEADER,
    SSV_LARGER,
    SSV_NOT,
    SSV_OR,
    SSV_SENTBEFORE,
    SSV_SENTON,
    SSV_SENTSINCE,
    SSV_SMALLER,
    SSV_UID,
    SSV_UNDRAFT,
    SSV_OPAREN,
    SSV_CPAREN
} SEARCH_SYMBOL_VALUES;
typedef std::map<insensitiveString, SEARCH_SYMBOL_VALUES> SEARCH_SYMBOL_T;

static SEARCH_SYMBOL_T searchSymbolTable;

void SearchHandler::buildSymbolTable() {
    // This is the symbol table for the search keywords
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("ALL",        SSV_ALL));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("ANSWERED",   SSV_ANSWERED));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("BCC",        SSV_BCC));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("BEFORE",     SSV_BEFORE));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("BODY",       SSV_BODY));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("CC",         SSV_CC));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("DELETED",    SSV_DELETED));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("FLAGGED",    SSV_FLAGGED));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("FROM",       SSV_FROM));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("KEYWORD",    SSV_KEYWORD));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("NEW",        SSV_NEW));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("OLD",        SSV_OLD));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("ON",         SSV_ON));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("RECENT",     SSV_RECENT));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("SEEN",       SSV_SEEN));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("SINCE",      SSV_SINCE));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("SUBJECT",    SSV_SUBJECT));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("TEXT",       SSV_TEXT));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("TO",         SSV_TO));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("UNANSWERED", SSV_UNANSWERED));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("UNDELETED",  SSV_UNDELETED));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("UNFLAGGED",  SSV_UNFLAGGED));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("UNKEYWORD",  SSV_UNKEYWORD));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("UNSEEN",     SSV_UNSEEN));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("DRAFT",      SSV_DRAFT));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("HEADER",     SSV_HEADER));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("LARGER",     SSV_LARGER));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("NOT",        SSV_NOT));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("OR",         SSV_OR));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("SENTBEFORE", SSV_SENTBEFORE));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("SENTON",     SSV_SENTON));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("SENTSINCE",  SSV_SENTSINCE));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("SMALLER",    SSV_SMALLER));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("UID",        SSV_UID));
    searchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("UNDRAFT",    SSV_UNDRAFT));
}


// updateSearchTerm updates the search specification for one term.  That term may be a 
// parenthesized list of other terms, in which case it calls updateSearchTerms to evaluate
// all of those.  updateSearchTerms will, in turn, call updateSearchTerm to update the 
// searchTerm for each of the terms in the list.
// updateSearchTerm return true if everything went okay or false if there were errors in
// the Search Specification String.
bool SearchHandler::updateSearchTerm(MailSearch &searchTerm, size_t &tokenPointer) {
    bool result = true;

    if ('(' == m_parseBuffer->parseChar(tokenPointer)) {
	tokenPointer += 2;
	result = updateSearchTerms(searchTerm, tokenPointer, true);
    }
    else {
	if (('*' == m_parseBuffer->parseChar(tokenPointer)) || isdigit(m_parseBuffer->parseChar(tokenPointer))) {
	    // It's a list of MSN ranges
	    // How do I treat a list of numbers?  Well, I generate a vector of all the possible values and then 
	    // and it (do a "set intersection" between it and the current vector, if any.

	    // I don't need to check to see if tokenPointer is less than m_parsePointer because this function
	    // wouldn't get called if it wasn't
	    SEARCH_RESULT srVector;
	    result = m_session->msnSequenceSet(srVector, tokenPointer);
	    searchTerm.addUidVector(srVector);
	    tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
	}
	else {
	    SEARCH_SYMBOL_T::iterator found = searchSymbolTable.find((char *)&m_parseBuffer[tokenPointer]);
	    tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
	    if (found != searchSymbolTable.end()) {
		switch (found->second) {
		case SSV_ERROR:
		    result = false;
		    break;

		case SSV_ALL:
		    // I don't do anything unless there are no other terms.  However, since there's an implicit "and all"
		    // around the whole expression, I don't have to do anything explicitly.
		    break;

		case SSV_ANSWERED:
		    searchTerm.addBitsToIncludeMask(MailStore::IMAP_MESSAGE_ANSWERED);
		    break;

		case SSV_BCC:
		    if (tokenPointer < m_parseBuffer->parsePointer()) {
			searchTerm.addBccSearch(m_parseBuffer->parseStr(tokenPointer));
			tokenPointer += strlen(m_parseBuffer->parseStr(tokenPointer)) + 1;
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_BEFORE:
		    if (tokenPointer < m_parseBuffer->parsePointer()) {
			std::string dateString(m_parseBuffer->parseStr(tokenPointer));
			// SYZYGY -- need a try-catch here
			// DateTime date(dateString.c_str());
			DateTime date;
			// date.LooseDate(dateString);
			if (date.isValid()) {
			    tokenPointer += strlen(m_parseBuffer->parseStr(tokenPointer)) + 1;
			    searchTerm.addBeforeSearch(date);
			}
			else {
			    result = false;
			}
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_BODY:
		    if (tokenPointer < m_parseBuffer->parsePointer()) {
			searchTerm.addBodySearch(m_parseBuffer->parseStr(tokenPointer));
			tokenPointer += strlen(m_parseBuffer->parseStr(tokenPointer)) + 1;
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_CC:
		    if (tokenPointer < m_parseBuffer->parsePointer()) {
			searchTerm.addCcSearch(m_parseBuffer->parseStr(tokenPointer));
			tokenPointer += strlen(m_parseBuffer->parseStr(tokenPointer)) + 1;
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_DELETED:
		    searchTerm.addBitsToIncludeMask(MailStore::IMAP_MESSAGE_DELETED);
		    break;

		case SSV_FLAGGED:
		    searchTerm.addBitsToIncludeMask(MailStore::IMAP_MESSAGE_FLAGGED);
		    break;

		case SSV_FROM:
		    if (tokenPointer < m_parseBuffer->parsePointer()) {
			searchTerm.addFromSearch(m_parseBuffer->parseStr(tokenPointer));
			tokenPointer += strlen(m_parseBuffer->parseStr(tokenPointer)) + 1;
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_KEYWORD:
		    // Searching for this cannot return any matches because we don't have keywords to set
		    searchTerm.forceNoMatches();
		    // However, I need to swallow the space and the atom that follows it if any, and it had 
		    // better or it's a syntax error
		    if (tokenPointer < m_parseBuffer->parsePointer()) {
			tokenPointer += strlen(m_parseBuffer->parseStr(tokenPointer)) + 1;
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_NEW:
		    searchTerm.addBitsToExcludeMask(MailStore::IMAP_MESSAGE_SEEN);
		    searchTerm.addBitsToIncludeMask(MailStore::IMAP_MESSAGE_RECENT);
		    break;

		case SSV_OLD:
		    searchTerm.addBitsToExcludeMask(MailStore::IMAP_MESSAGE_RECENT);
		    break;

		case SSV_ON:
		    if (tokenPointer < m_parseBuffer->parsePointer()) {
			std::string dateString(m_parseBuffer->parseStr(tokenPointer));
			// SYZYGY -- need a try-catch here
			// DateTime date(dateString.c_str()); 
			DateTime date;
			// date.LooseDate(dateString);
			if (date.isValid()) {
			    tokenPointer += strlen(m_parseBuffer->parseStr(tokenPointer)) + 1;
			    searchTerm.addOnSearch(date);
			}
			else {
			    result = false;
			}
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_RECENT:
		    searchTerm.addBitsToIncludeMask(MailStore::IMAP_MESSAGE_RECENT);
		    break;

		case SSV_SEEN:
		    searchTerm.addBitsToIncludeMask(MailStore::IMAP_MESSAGE_SEEN);
		    break;

		case SSV_SINCE:
		    if (tokenPointer < m_parseBuffer->parsePointer()) {
			std::string dateString(m_parseBuffer->parseStr(tokenPointer));	
			// SYZYGY -- need a try-catch here
			// DateTime date(dateString);
			DateTime date;
			// date.LooseDate(dateString);
			if (date.isValid()) {
			    tokenPointer += strlen(m_parseBuffer->parseStr(tokenPointer)) + 1;
			    searchTerm.addSinceSearch(date);
			}
			else {
			    result = false;
			}
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_SUBJECT:
		    if (tokenPointer < m_parseBuffer->parsePointer()) {
			searchTerm.addSubjectSearch(m_parseBuffer->parseStr(tokenPointer));
			tokenPointer += strlen(m_parseBuffer->parseStr(tokenPointer)) + 1;
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_TEXT:
		    if (tokenPointer < m_parseBuffer->parsePointer()) {
			searchTerm.addTextSearch(m_parseBuffer->parseStr(tokenPointer));
			tokenPointer += strlen(m_parseBuffer->parseStr(tokenPointer)) + 1;
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_TO:
		    if (tokenPointer < m_parseBuffer->parsePointer()) {
			searchTerm.addToSearch(m_parseBuffer->parseStr(tokenPointer));
			tokenPointer += strlen(m_parseBuffer->parseStr(tokenPointer)) + 1;
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_UNANSWERED:
		    searchTerm.addBitsToExcludeMask(MailStore::IMAP_MESSAGE_ANSWERED);
		    break;

		case SSV_UNDELETED:
		    searchTerm.addBitsToExcludeMask(MailStore::IMAP_MESSAGE_DELETED);
		    break;

		case SSV_UNFLAGGED:
		    searchTerm.addBitsToExcludeMask(MailStore::IMAP_MESSAGE_FLAGGED);
		    break;

		case SSV_UNKEYWORD:
		    // Searching for this matches all messages because we don't have keywords to unset
		    // since all does nothing, this does nothing
		    // However, I need to swallow the space and the atom that follows it if any, and it had 
		    // better or it's a syntax error
		    if (tokenPointer < m_parseBuffer->parsePointer()) {
			tokenPointer += strlen(m_parseBuffer->parseStr(tokenPointer)) + 1;
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_UNSEEN:
		    searchTerm.addBitsToExcludeMask(MailStore::IMAP_MESSAGE_SEEN);
		    break;

		case SSV_DRAFT:
		    searchTerm.addBitsToIncludeMask(MailStore::IMAP_MESSAGE_DRAFT);
		    break;

		case SSV_HEADER:
		    if (tokenPointer < m_parseBuffer->parsePointer()) {
			char *header_name = m_parseBuffer->parseStr(tokenPointer);
			tokenPointer += strlen(m_parseBuffer->parseStr(tokenPointer)) + 1;
			if (tokenPointer < m_parseBuffer->parsePointer()) {
			    searchTerm.addGenericHeaderSearch(header_name, m_parseBuffer->parseStr(tokenPointer));
			    tokenPointer += strlen(m_parseBuffer->parseStr(tokenPointer)) + 1;
			}
			else {
			    result = false;
			}
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_LARGER:
		    if (tokenPointer < m_parseBuffer->parsePointer()) {
			char *s;
			unsigned long value = strtoul(m_parseBuffer->parseStr(tokenPointer), &s, 10);
			if ('\0' == *s) {
			    tokenPointer += strlen(m_parseBuffer->parseStr(tokenPointer)) + 1;
			    searchTerm.addSmallestSize(value + 1);
			}
			else {
			    result = false;
			}
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_NOT:
		    // Okay, not works by evaluating the next term and then doing a symmetric set difference between
		    // that and the whole list of available messages.  The result is then treated as if it was a 
		    // list of numbers
		    if (tokenPointer < m_parseBuffer->parsePointer()) {
			MailSearch termToBeNotted;
			if (updateSearchTerm(termToBeNotted, tokenPointer)) {
			    SEARCH_RESULT vector;
			    // messageList will not return CANNOT_COMPLETE here since I locked the mail store in the caller
			    if (MailStore::SUCCESS ==  m_session->store()->messageList(vector)) {
				if (MailStore::SUCCESS == termToBeNotted.evaluate(m_session->store())) {
				    SEARCH_RESULT rightResult;
				    termToBeNotted.reportResults(m_session->store(), &rightResult);

				    for (SEARCH_RESULT::iterator i=rightResult.begin(); i!=rightResult.end(); ++i) {
					SEARCH_RESULT::iterator elem = find(vector.begin(), vector.end(), *i);
					if (vector.end() != elem) {
					    vector.erase(elem);
					}
				    }
				    searchTerm.addUidVector(vector);
				}
				else {
				    result = false;
				}
			    }
			    else {
				result = false;
			    }
			}
			else {
			    result = false;
			}
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_OR:
		    // Okay, or works by evaluating the next two terms and then doing a union between the two
		    // the result is then treated as if it is a list of numbers
		    if (tokenPointer < m_parseBuffer->parsePointer())
		    {
			MailSearch termLeftSide;
			if (updateSearchTerm(termLeftSide, tokenPointer)) {
			    if (MailStore::SUCCESS == termLeftSide.evaluate(m_session->store())) {
				MailSearch termRightSide;
				if (updateSearchTerm(termRightSide, tokenPointer)) {
				    if (MailStore::SUCCESS == termRightSide.evaluate(m_session->store())) {
					SEARCH_RESULT leftResult;
					termLeftSide.reportResults(m_session->store(), &leftResult);
					SEARCH_RESULT rightResult;
					termRightSide.reportResults(m_session->store(), &rightResult);
					for (size_t i=0; i<rightResult.size(); ++i) {
					    unsigned long uid = rightResult[i];

					    if (leftResult.end() == find(leftResult.begin(), leftResult.end(), uid)) {
						leftResult.push_back(uid);
					    }
					}
					searchTerm.addUidVector(leftResult);
				    }
				    else {
					result = false;
				    }
				}
				else {
				    result = false;
				}
			    }
			    else {
				result = false;
			    }
			}
			else {
			    result = false;
			}
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_SENTBEFORE:
		    if (tokenPointer < m_parseBuffer->parsePointer()) {
			std::string dateString(m_parseBuffer->parseStr(tokenPointer));
			// SYZYGY -- need a try-catch here
			// DateTime date(dateString);
			DateTime date;
			// date.LooseDate(dateString);
			if (date.isValid()) {
			    tokenPointer += strlen(m_parseBuffer->parseStr(tokenPointer)) + 1;
			    searchTerm.addSentBeforeSearch(date);
			}
			else {
			    result = false;
			}
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_SENTON:
		    if (tokenPointer < m_parseBuffer->parsePointer()) {
			std::string dateString(m_parseBuffer->parseStr(tokenPointer));
			// SYZYGY -- need a try-catch here
			// DateTime date(dateString);
			DateTime date;
			// date.LooseDate(dateString);
			if (date.isValid()) {
			    tokenPointer += strlen(m_parseBuffer->parseStr(tokenPointer)) + 1;
			    searchTerm.addSentOnSearch(date);
			}
			else {
			    result = false;
			}
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_SENTSINCE:
		    if (tokenPointer < m_parseBuffer->parsePointer()) {
			std::string dateString(m_parseBuffer->parseStr(tokenPointer));
			// SYZYGY -- need a try-catch here
			// DateTime date(dateString);
			DateTime date;
			// date.LooseDate(dateString);
			if (date.isValid()) {
			    tokenPointer += strlen(m_parseBuffer->parseStr(tokenPointer)) + 1;
			    searchTerm.addSentSinceSearch(date);
			}
			else {
			    result = false;
			}
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_SMALLER:
		    if (tokenPointer < m_parseBuffer->parsePointer()) {
			char *s;
			unsigned long value = strtoul(m_parseBuffer->parseStr(tokenPointer), &s, 10);
			if ('\0' == *s) {
			    tokenPointer += strlen(m_parseBuffer->parseStr(tokenPointer)) + 1;
			    searchTerm.addLargestSize(value - 1);
			}
			else {
			    result = false;
			}
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_UID:
		    // What follows is a UID list
		    if (tokenPointer < m_parseBuffer->parsePointer()) {
			SEARCH_RESULT srVector;

			result = m_session->uidSequenceSet(srVector, tokenPointer);
			searchTerm.addUidVector(srVector);
			tokenPointer += strlen((char *)&m_parseBuffer[tokenPointer]) + 1;
		    }
		    else {
			result = false;
		    }
		    break;

		case SSV_UNDRAFT:
		    searchTerm.addBitsToExcludeMask(MailStore::IMAP_MESSAGE_DRAFT);
		    break;

		default:
		    result = false;
		    break;
		}
	    }
	    else {
		result = false;
	    }
	}
    }
    return result;
}

// updateSearchTerms calls updateSearchTerm until there is an error, it reaches a ")" or it reaches the end of 
// the string.  If there is an error, indicated by updateSearchTerm returning false, or if it reaches a ")" and
// isSubExpression is not true, then false is returned.  Otherwise, true is returned.  
bool SearchHandler::updateSearchTerms(MailSearch &searchTerm, size_t &tokenPointer, bool isSubExpression) {
    bool result;
    do {
	result = updateSearchTerm(searchTerm, tokenPointer);
    } while(result && (tokenPointer < m_parseBuffer->parsePointer()) && (')' != m_parseBuffer->parseChar(tokenPointer)));
    if ((')' == m_parseBuffer->parseChar(tokenPointer)) && !isSubExpression) {
	result = false;
    }
    if (')' == m_parseBuffer->parseChar(tokenPointer)) {
	tokenPointer += 2;
    }
    return result;
}
// SearchHandler::execute assumes that the tokens for the search are in m_parseBuffer from 0 to m_parsePointer and
// that they are stored there as a sequence of ASCIIZ strings.  It is also known that when the system gets
// here that it's time to generate the output
IMAP_RESULTS SearchHandler::execute() {
    SEARCH_RESULT foundVector;
    NUMBER_LIST foundList;
    IMAP_RESULTS result = IMAP_OK;
    MailSearch searchTerm;
    size_t executePointer = 0;

    if (MailStore::SUCCESS == m_session->store()->lock()) {
	// Skip the first two tokens that are the tag and the command
	executePointer += (size_t) strlen((char *)m_parseBuffer) + 1;
	executePointer += (size_t) strlen((char *)&m_parseBuffer[executePointer]) + 1;

	// I do this once again if m_UsingUid is set because the command in that case is UID
	if (m_usingUid) {
	    executePointer += (size_t) strlen((char *)&m_parseBuffer[executePointer]) + 1;
	}

	// This section turns the search spec into something that might work efficiently
	if (updateSearchTerms(searchTerm, executePointer, false)) {
	    m_session->mboxErrorCode(searchTerm.evaluate(m_session->store()));
	    searchTerm.reportResults(m_session->store(), &foundVector);

	    // This section turns the vector of UID numbers into a list of either UID numbers or MSN numbers
	    // whichever I'm supposed to be displaying.  If I were putting the numbers into some sort of 
	    // order, this is where that would happen
	    if (!m_usingUid) {
		for (size_t i=0; i<foundVector.size(); ++i) {
		    if (0 != foundVector[i]) {
			foundList.push_back(m_session->store()->mailboxUidToMsn(foundVector[i]));
		    }
		}
	    }
	    else {
		for (size_t i=0; i<foundVector.size(); ++i) {
		    if (0 != foundVector[i]) {
			foundList.push_back(foundVector[i]);
		    }
		}
	    }
	    // This section writes the generated list of numbers
	    if (MailStore::SUCCESS == m_session->mboxErrorCode()) {
		std::ostringstream ss;
		ss << "* SEARCH";
		for(NUMBER_LIST::iterator i=foundList.begin(); i!=foundList.end(); ++i) {
		    ss << " " << *i;
		}
		ss << "\r\n";
		m_session->driver()->wantsToSend(ss.str());
		result = IMAP_OK;
	    }
	    else {
		result = IMAP_MBOX_ERROR;
	    }
	}
	else {
	    result = IMAP_BAD;
	    m_session->responseText("Malformed Command");
	}
	m_session->store()->unlock();
    }
    else {
	result = IMAP_TRY_AGAIN;
    }

    return result;
}


IMAP_RESULTS SearchHandler::searchKeyParse(INPUT_DATA_STRUCT &input) {
    IMAP_RESULTS result = IMAP_OK;
    while((IMAP_OK == result) && (input.dataLen > input.parsingAt)) {
	if (' ' == input.data[input.parsingAt]) {
	    ++input.parsingAt;
	}
	if ('(' == input.data[input.parsingAt]) {
	    m_parseBuffer->addToParseBuffer(input, 1);
	    m_session->parseBuffer()->argument3Here();
	    if (('*' == input.data[input.parsingAt]) || isdigit(input.data[input.parsingAt])) {
		char *end1 = strchr(((char *)input.data)+input.parsingAt, ' ');
		char *end2 = strchr(((char *)input.data)+input.parsingAt, ')');
		size_t len;
		if (NULL != end2) {
		    if ((NULL == end1) || (end2 < end1)) {
			len = (end2 - (((char *)input.data)+input.parsingAt));
		    }
		    else {
			len = (end1 - (((char *)input.data)+input.parsingAt));
		    }
		}
		else {
		    m_session->responseText("Malformed Command");
		    result = IMAP_BAD;
		}
		if ((0 < len) && (input.dataLen >= (input.parsingAt + len))) {
		    m_parseBuffer->addToParseBuffer(input, len);
		}
		else {
		    m_session->responseText("Malformed Command");
		    result = IMAP_BAD;
		}
	    }
	    else {
		switch(m_parseBuffer->astring(input, true, NULL)) {
		case ImapStringGood:
		    break;

		case ImapStringBad:
		    m_session->responseText("Malformed Command");
		    result = IMAP_BAD;
		    break;

		case ImapStringPending:
		    m_session->responseText("Ready for Literal");
		    result = IMAP_NOTDONE;
		    m_parseStage = 1;
		    break;

		default:
		    m_session->responseText("Internal Search Error");
		    result = IMAP_BAD;
		    break;
		}
	    }
	}
	else {
	    if (('*' == input.data[input.parsingAt]) || isdigit(input.data[input.parsingAt])) {
		char *end1 = strchr(((char *)input.data)+input.parsingAt, ' ');
		char *end2 = strchr(((char *)input.data)+input.parsingAt, ')');
		size_t len;
		if (NULL != end2) {
		    if ((NULL == end1) || (end2 < end1)) {
			len = (end2 - (((char *)input.data)+input.parsingAt));
		    }
		    else {
			len = (end1 - (((char *)input.data)+input.parsingAt));
		    }
		}
		else {
		    if (NULL != end1) {
			len = (end1 - (((char *)input.data)+input.parsingAt));
		    }
		    else {
			len = strlen(((char *)input.data)+input.parsingAt);
		    }
		}
		if ((0 < len) && (input.dataLen >= (input.parsingAt + len))) {
		    m_parseBuffer->addToParseBuffer(input, len);
		}
		else {
		    m_session->responseText("Malformed Command");
		    result = IMAP_BAD;
		}
	    }
	    else {
		switch(m_parseBuffer->astring(input, true, "*")) {
		case ImapStringGood:
		    break;

		case ImapStringBad:
		    m_session->responseText("Malformed Command");
		    result = IMAP_BAD;
		    break;

		case ImapStringPending:
		    m_session->responseText("Ready for Literal");
		    result = IMAP_NOTDONE;
		    m_parseStage = 1;
		    break;

		default:
		    m_session->responseText("Internal Search Error");
		    result = IMAP_BAD;
		    break;
		}
	    }
	}
	if ((input.dataLen >= input.parsingAt)  && (')' == input.data[input.parsingAt])) {
	    m_parseBuffer->addToParseBuffer(input, 1);
	}
    }
    if (IMAP_OK == result) {
	m_parseStage = 3;
	result = execute();
    }
    return result;
}


IMAP_RESULTS SearchHandler::receiveData(INPUT_DATA_STRUCT &input) {
    IMAP_RESULTS result = IMAP_OK;

    size_t savedParsePointer = input.parsingAt;
    switch (m_parseStage) {
    case 0:
	if (('(' == input.data[input.parsingAt]) || ('*' == input.data[input.parsingAt])) {
	    input.parsingAt = savedParsePointer;
	    result = searchKeyParse(input);
	}
	else {
	    m_session->parseBuffer()->atom(input);
	    if (0 == strcmp("CHARSET", m_session->parseBuffer()->arguments())) {
		if ((2 < (input.dataLen - input.parsingAt)) && (' ' == input.data[input.parsingAt])) {
		    ++input.parsingAt;
		    // argument 2 is the charset declaration
		    m_session->parseBuffer()->argument2Here();
		    switch (m_session->parseBuffer()->astring(input, true, NULL)) {
		    case ImapStringGood:
			// I have the charset m_parseBuffer->astring--check to make sure it's "US-ASCII"
			// for the time being, it's the only charset I recognize
			if (0 == strcmp("US-ASCII", m_session->parseBuffer()->argument2())) {
			    m_parseStage = 2;
			    result = searchKeyParse(input);
			}
			else {
			    m_session->responseText("Unrecognized Charset");
			    result = IMAP_BAD;
			}
			break;

		    case ImapStringBad:
			m_session->responseText("Malformed Command");
			result = IMAP_BAD;
			break;

		    case ImapStringPending:
			result = IMAP_NOTDONE;
			m_parseStage = 1;
			m_session->responseText("Ready for Literal");
			break;

		    default:
			m_session->responseText("Failed");
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
		input.parsingAt = savedParsePointer;
		result = searchKeyParse(input);
	    }
	}
	break;

    case 1:
	m_session->responseText("Completed");
	// It was receiving the charset m_parseBuffer->astring
	// for the time being, it's the only charset I recognize
	if ((input.dataLen > m_session->parseBuffer()->literalLength()) && (8 == m_session->parseBuffer()->literalLength())) {
	    for (int i=0; i<8; ++i) {
		input.data[i] = toupper(input.data[i]);
	    }
	    if (11 < input.dataLen) {
		// Get rid of the CRLF if I have it
		input.dataLen -= 2;
		input.data[input.dataLen] = '\0';  // Make sure it's terminated so strchr et al work
	    }
	    if (0 == strncmp("US-ASCII ", (char *)input.data, 9)) {
		result = searchKeyParse(input);
	    }
	    else {
		m_session->responseText("Unrecognized Charset");
		result = IMAP_BAD;
	    }
	}
	else {
	    m_session->responseText("Malformed Command");
	    result = IMAP_BAD;
	}
	break;

    case 2:
    {
	size_t dataUsed = m_session->parseBuffer()->addLiteralToParseBuffer(input);

	m_session->responseText("Completed");
	// It's somewhere in the middle of the search specification
	if (dataUsed <= input.dataLen) {
	    if ((dataUsed < input.dataLen) && (' ' == input.data[dataUsed])) {
		++dataUsed;
	    }
	    if (2 < (input.dataLen - dataUsed)) {
		// Get rid of the CRLF if I have it
		input.dataLen -= 2;
		input.data[input.dataLen] = '\0';  // Make sure it's terminated so strchr et al work
	    }
	    result = searchKeyParse(input);
	}
	else {
	    result = IMAP_IN_LITERAL;
	}
    }
    break;

    default:
	m_session->responseText("Completed");
	result = execute();
	break;
    }

    return result;
}
