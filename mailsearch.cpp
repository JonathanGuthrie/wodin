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

#include "mailsearch.hpp"

#include <algorithm>

MailSearch::MailSearch() {
  m_uidVector.clear();
  m_hasUidVector = false;
  initialize();
}

// initialize initializes everything in the object except for m_uidVector and m_hasUidVector.  The
// reason I need a function separate from the constructor for this is because I also want to call it
// from Evaluate so that I can have this code in just one place.  The reason I don't want to
// initialize m_uidVector and m_hasUidVector here is because Evaluate is supposed to leave the results
// of the evaluation in m_uidVector and m_hasUidVector must be true for that to work
void MailSearch::initialize() {
  m_includeMask = 0;
  m_excludeMask = 0;
  m_forceNoMatches = false;
  m_forceAllMatches = false;
  m_smallestSize = 0;
  m_largestSize = ~0;
  m_beginDate = m_endDate = NULL;
  m_beginInternalDate = m_endInternalDate = NULL;
  m_headerSearchList.clear();
  m_bodySearchList.clear();
  // m_hslSearchList.clear();
}

MailSearch::~MailSearch() {
  finalize();
}



// The reason I need a separate finalize function is so that I can call it from Evaluate.
void MailSearch::finalize(void) {
  if (NULL != m_beginDate) {
    delete m_beginDate;
    m_beginDate = NULL;
  }
  if (NULL != m_endDate) {
    delete m_endDate;
    m_endDate = NULL;
  }
  if (NULL != m_beginInternalDate) {
    delete m_beginInternalDate;
    m_beginInternalDate = NULL;
  }
  if (NULL != m_endInternalDate) {
    delete m_endInternalDate;
    m_endInternalDate = NULL;
  }
  for (TEXT_SEARCH_LIST::iterator i=m_bodySearchList.begin(); i!=m_bodySearchList.end(); ++i) {
    if (NULL != i->badChars) {
      delete[] i->badChars;
    }
    if (NULL != i->suffix) {
      delete[] i->suffix;
    }
  }
  for (TEXT_SEARCH_LIST::iterator i=m_textSearchList.begin(); i!=m_textSearchList.end(); ++i) {
    if (NULL != i->badChars) {
      delete[] i->badChars;
    }
    if (NULL != i->suffix)
      {
	delete[] i->suffix;
      }
  }
  m_headerSearchList.clear();
  m_bodySearchList.clear();
  m_textSearchList.clear();
  m_searchList.clear();
}


void MailSearch::addBccSearch(const insensitiveString &toSearchFor) {
  TEXT_SEARCH_DATA item;
  item.which = BCC;
  item.target = toSearchFor;
  m_headerSearchList.push_back(item);
}

static void build_bm_bad_chars(const insensitiveString &target, int output[256]) {
  int length = target.size();

  for (int i=0; i<256; ++i) {
    output[i] = length;
  }
  for (int i=0; i<length; ++i) {
    output[tolower(target[i])] = length - i - 1;
    output[toupper(target[i])] = length - i - 1;
  }
}

// This finds the amount to shift for a suffix of target found somewhere else, but
// with a different preceeding character, in target.  I'm supposed to search
// case-insensitively, so I assume that target has already been ToLower()ed.
static void find_bm_suffixes(const insensitiveString &target, int output[]) {
  int length = target.size();
  int f, g, i;
 
  output[length - 1] = length;

  g = length - 1;
  // Go through the characters of the target from right to left, starting
  // with the next to last one.
  for (i = length - 2; i >= 0; --i) {
    if ((i > g) && (output[i + length - f - 1] < (i - g))) {
      output[i] = output[i + length - f - 1];
    }
    else {
      if (i < g) {
	g = i;
      }
      f = i;
      while ((g >= 0) && (target[g] == target[g + length - f - 1])) {
	--g;
      }
      output[i] = f - g;
    }
  }
}

// This builds the boyer-moore suffix list, which gives the shift associated
// with a given matching suffix.  Text searches are supposed to be case-insensitive
// so the target is presumed to already by ToLower()ed.
static void build_bm_suffix(const insensitiveString &target, int output[]) {
  int length = target.size();
  int *suffixes;
  suffixes = new int[length];

  find_bm_suffixes(target, suffixes);
  for (int i=0; i<length; ++i) {
    output[i] = length;
  }
  int j = 0;
  for (int i=length-1; i>=-1; --i) {
    if ((-1 == i) || (suffixes[i] == (i + 1))) {
      while (j < (length - i - 1)) {
	if (length == output[j]) {
	  output[j] = length - i - 1;
	}
	++j;
      }
    }
  }
  for (int i=0; i<=(length-2); ++i) {
    output[length - suffixes[i] - 1] = length - i - 1;
  }
  delete[] suffixes;
}

void MailSearch::addBodySearch(const insensitiveString &toSearchFor) {
  TEXT_SEARCH_DATA item;
  item.which = BODY;
  item.target = toSearchFor;
  item.badChars = new int[256];
  build_bm_bad_chars(item.target, item.badChars);
  item.suffix = new int[item.target.size()];
  build_bm_suffix(item.target, item.suffix);
  m_bodySearchList.push_back(item);
}

void MailSearch::addBeforeSearch(DateTime &dateToSearchFor) {
  if (NULL == m_endInternalDate) {
    m_endInternalDate = new DateTime(dateToSearchFor);
  }
  else {
    if (dateToSearchFor < *m_endInternalDate) {
      delete m_endInternalDate;
      m_endInternalDate = new DateTime(dateToSearchFor);
    }
  }
}

void MailSearch::addCcSearch(const insensitiveString &toSearchFor) {
  TEXT_SEARCH_DATA item;
  item.which = CC;
  item.target = toSearchFor;
  m_headerSearchList.push_back(item);
}

void MailSearch::addFromSearch(const insensitiveString &toSearchFor) {
  TEXT_SEARCH_DATA item;
  item.which = FROM;
  item.target = toSearchFor;
  m_headerSearchList.push_back(item);
}

void MailSearch::addOnSearch(DateTime &dateToSearchFor) {
  addSinceSearch(dateToSearchFor);
  dateToSearchFor.addDays(1);
  addBeforeSearch(dateToSearchFor);
}

void MailSearch::addSinceSearch(DateTime &dateToSearchFor) {
  if (NULL == m_beginInternalDate) {
    m_beginInternalDate = new DateTime(dateToSearchFor);
  }
  else {
    if (dateToSearchFor < *m_beginInternalDate) {
      delete m_beginInternalDate;
      m_beginInternalDate = new DateTime(dateToSearchFor);
    }
  }
}

void MailSearch::addSubjectSearch(const insensitiveString &toSearchFor) {
  TEXT_SEARCH_DATA item;
  item.which = SUBJECT;
  item.target = toSearchFor;
  m_headerSearchList.push_back(item);
}

void MailSearch::addTextSearch(const insensitiveString &toSearchFor) {
  TEXT_SEARCH_DATA item;
  item.which = TEXT;
  item.target = toSearchFor;
  item.badChars = new int[256];
  build_bm_bad_chars(item.target, item.badChars);
  item.suffix = new int[item.target.size()];
  build_bm_suffix(item.target, item.suffix);
  m_textSearchList.push_back(item);
}

void MailSearch::addToSearch(const insensitiveString &toSearchFor) {
  TEXT_SEARCH_DATA item;
  item.which = TO;
  item.target = toSearchFor;
  m_headerSearchList.push_back(item);
}

void MailSearch::addSentBeforeSearch(DateTime &dateToSearchFor) {
  if (NULL == m_endDate)  {
    m_endDate = new DateTime(dateToSearchFor);
  }
  else {
    if (dateToSearchFor < *m_endDate) {
      delete m_endDate;
      m_endDate = new DateTime(dateToSearchFor);
    }
  }
}

void MailSearch::addSentSinceSearch(DateTime &dateToSearchFor) {
  if (NULL == m_beginDate) {
    m_beginDate = new DateTime(dateToSearchFor);
  }
  else {
    if (dateToSearchFor < *m_beginDate) {
      delete m_beginDate;
      m_beginDate = new DateTime(dateToSearchFor);
    }
  }
}

void MailSearch::addSentOnSearch(DateTime &dateToSearchFor) {
  addSentSinceSearch(dateToSearchFor);
  dateToSearchFor.addDays(1);
  addSentBeforeSearch(dateToSearchFor);
}

// For the addMsnVector and addUidVector methonds, the outputs are guaranteed to be
// valid message IDs or zero, and the vectors are guaranteed to be sorted, and the
// nonzero numbers in the vector are guaranteed to be unique.
void MailSearch::addMsnVector(MailStore *base, const SEARCH_RESULT &vector) {
  if (!m_hasUidVector) {
    for (SEARCH_RESULT::const_iterator i = vector.begin(); i != vector.end(); ++i) {
      m_uidVector.push_back(base->mailboxMsnToUid(*i));
    }
  }
  else {
    for (SEARCH_RESULT::iterator i = m_uidVector.begin(); i != m_uidVector.end();) {
      unsigned long msn = base->mailboxUidToMsn(*i);

      SEARCH_RESULT::const_iterator elem = find(vector.begin(), vector.end(), msn);
      if (vector.end() == elem) {	
	m_uidVector.erase(i);
      }
      else {
	++i;
      }
    }
  }
  m_hasUidVector = true;
}

void MailSearch::addUidVector(const SEARCH_RESULT &vector) {
  if (!m_hasUidVector) {
    for (SEARCH_RESULT::const_iterator i = vector.begin(); i != vector.end(); ++i) {
      m_uidVector.push_back(*i);
    }
  }
  else {
    int uid = 0;
    for (SEARCH_RESULT::iterator i = m_uidVector.begin(); i != m_uidVector.end();) {
      ++uid;
      SEARCH_RESULT::const_iterator elem = find(vector.begin(), vector.end(), uid);
      if (vector.end() == elem) {	
	m_uidVector.erase(i);
      }
      else {
	++i;
      }
    }
  }
  m_hasUidVector = true;
}

void MailSearch::addSmallestSize(size_t limit) {
  if (limit > m_smallestSize) {
    m_smallestSize = limit;
  }
}

void MailSearch::addLargestSize(size_t limit) {
  if (limit < m_largestSize) {
    m_largestSize = limit;
  }
}

#define MAX(a, b) ((a>b) ? a : b)

// Recursive body search searches for the substring through all the body parts
// it does not search any of the headers
static bool recursiveBodySearch(MESSAGE_BODY root, const MailSearch::TEXT_SEARCH_DATA target, const char *bigBuffer) {
  bool result = false;

  size_t i = root.bodyStartOffset + root.headerOctets;
  int length = target.target.size();

  while (!result && (i < (root.bodyStartOffset + root.bodyOctets - length))) {
    int j;
    for (j = length - 1; (j >= 0) && (target.target[j] == tolower(bigBuffer[i+j])); --j) {
      // NOTHING
    }
    if (j < 0) {
      result = true;
    }
    else {
      i += MAX(target.suffix[j], target.badChars[bigBuffer[i+j]] - length + 1 + j);
    }
  }
  if (!result) {
    if (NULL != root.subparts) {
      for (int i = 0; i < root.subparts->size(); i++) {
	result = recursiveBodySearch((*root.subparts)[i], target, bigBuffer);
      }
    }
  }
  return result;
}

// This stores into The uid vector the list of UID's that match the conditions described by searchSpec
MailStore::MAIL_STORE_RESULT MailSearch::evaluate(MailStore *where) {
  MailStore::MAIL_STORE_RESULT result = MailStore::SUCCESS;

  if (where->isMailboxOpen()) {
    if (m_forceNoMatches) {
      m_hasUidVector = true;
      m_uidVector.clear();
    }
    else {
      if ((0 != (m_includeMask | m_excludeMask)) ||
	  (0 < m_smallestSize) || ((~0) > m_largestSize) ||
	  (NULL != m_beginInternalDate) || (NULL != m_endInternalDate)) {
	if ((0 == (m_includeMask & m_excludeMask)) &&
	    (m_smallestSize <= m_largestSize) && 
	    ((NULL == m_beginInternalDate) || (NULL == m_endInternalDate) ||
	     (*m_beginInternalDate <= *m_endInternalDate))) {
	  const SEARCH_RESULT *temp = where->searchMetaData(m_includeMask, m_includeMask | m_excludeMask, m_smallestSize, m_largestSize, m_beginInternalDate, m_endInternalDate);
	  if (NULL != temp) {
	    addUidVector(*temp);
	    delete temp;
	  }
	  else {
	    result = MailStore::GENERAL_FAILURE;
	  }
	}
	else {
	  // It's requested a condition that logically isn't possible, like if it requires one of the bits to
	  // be both set and unset.  This is not an error, but it will result in no messages found.
	  m_uidVector.clear();
	}
      }

      // At this point, I've checked all the file metadata I can.  If there are any text checks to be made,
      // I have to go through the already generated list of possible candidate messages, and parse each
      //  message file so I can do the text searches
      if ((result == MailStore::SUCCESS) &&
	  ((0 != m_searchList.size()) ||
	   (0 != m_headerSearchList.size()) ||
	   (0 != m_bodySearchList.size()) ||
	   (0 != m_textSearchList.size()) ||
	   (NULL != m_beginDate) || (NULL != m_endDate))) {
	SEARCH_RESULT inputVector, outputVector;
	outputVector.clear();
	/*
	 * reportResults populates m_uidVector (if it hasn't been populated already)
	 * and copies m_uidVector into inputVector
	 */
	reportResults(where, &inputVector);

	for (SEARCH_RESULT::iterator i = inputVector.begin(); i!= inputVector.end(); ++i) {
	  MailMessage *message;
	  bool itIsIn = true;
	  // I'm going to need the message data, so I get it
	  if (MailMessage::SUCCESS == where->messageData(&message, *i)) {
	    /*
	     * Step, the first:
	     * Search for the headers specified in the common header fields
	     */
	    for (TEXT_SEARCH_LIST::const_iterator j=m_headerSearchList.begin(); itIsIn && (j!=m_headerSearchList.end()); ++j) {
	      insensitiveString target = j->target;

	      switch(j->which) {
	      case MailSearch::BCC:
		{
		  insensitiveString line = message->bcc();
		  itIsIn = (std::string::npos != line.find(target));
		}
		break;

	      case MailSearch::CC:
		{
		  insensitiveString line = message->cc();
		  itIsIn = (std::string::npos != line.find(target));
		}
		break;

	      case MailSearch::FROM:
		{
		  insensitiveString line = message->from();
		  itIsIn = (std::string::npos != line.find(target));
		}
		break;

	      case MailSearch::SUBJECT:
		{
		  insensitiveString line = message->subject();
		  itIsIn = (std::string::npos != line.find(target));
		}
		break;

	      case MailSearch::TO:
		{
		  insensitiveString line = message->to();
		  itIsIn = (std::string::npos != line.find(target));
		}
		break;
	      }
	    }

	    /*
	     * Step, the second:
	     * Do the body searches
	     */
	    if (!m_textSearchList.empty() || !m_bodySearchList.empty()) {
	      char messageBuffer[where->bufferLength(*i)+1];
	      int len = where->readMessage(messageBuffer, 0, where->bufferLength(*i));
	      messageBuffer[len] = '\0';

	      for (TEXT_SEARCH_LIST::const_iterator j=m_bodySearchList.begin(); itIsIn && (j!=m_bodySearchList.end()); ++j) {
		itIsIn = recursiveBodySearch(message->messageBody(), *j, messageBuffer);
	      }

	      /*
	       * Step, the third:
	       * Do the text searches
	       * This is a boyer-moore-pratt search body, the initialization is elsewhere
	       */
	      for (TEXT_SEARCH_LIST::const_iterator j=m_textSearchList.begin(); itIsIn && (j!=m_textSearchList.end()); ++j) {
		size_t i = 0;
		insensitiveString finding = j->target;
		int length = finding.size();

		itIsIn = false;
		while (!itIsIn && (i < (message->messageBody().bodyOctets))) {
		  int k;
		  for (k = length - 1; (k >= 0) && (finding[k] == tolower(messageBuffer[i+k])); --k) {
		    // NOTHING
		  }
		  if (k < 0) {
		    itIsIn = true;
		  }
		  else {
		    i += MAX(j->suffix[k], j->badChars[messageBuffer[i+k]] - length + 1 + k);
		  }
		}
	      }
	    }
	    /*
	     * Step, the last:
	     * Do the date searches
	     */
	    if (itIsIn && (NULL != m_beginDate)) {
	      itIsIn = (*m_beginDate <= message->messageTime());
	    }

	    if (itIsIn && (NULL != m_endDate)) {
	      itIsIn = (*m_endDate >= message->messageTime());
	    }

	    if (itIsIn) {
	      outputVector.push_back(*i);
	    }
	  }
	}
	addUidVector(outputVector);
      }
    }
  }
  else {
    result = MailStore::MAILBOX_NOT_OPEN;
  }

  // When where->MessageSearch returns, everything is collapsed into m_uidVector, so I re-initialize the flag
  // values so it won't ever attempt to recalculate them no matter what happens
  finalize();
  initialize();
  return result;
}

void MailSearch::reportResults(const MailStore *where, SEARCH_RESULT *result) {
  if (!m_hasUidVector) {
    where->messageList(m_uidVector); 
  }
  m_hasUidVector = true;
  result->clear();
  for (SEARCH_RESULT::iterator i = m_uidVector.begin(); i!= m_uidVector.end(); ++i) {
    result->push_back(*i);
  }
}

void MailSearch::addGenericHeaderSearch(const insensitiveString &headerName, const insensitiveString &toSearchFor) {
  HEADER_SEARCH_DATA item;
  item.headerField = headerName;
  item.target = toSearchFor;
  m_searchList.push_back(item);
}

