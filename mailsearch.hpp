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

#if !defined(_MAILSEARCH_HPP_INCLUDED_)
#define _MAILSEARCH_HPP_INCLUDED_

#include <list>
#include <string>

#include "datetime.hpp"
#include "imapsession.hpp"
#include "mailstore.hpp"

class MailSearch {
public:
    typedef enum {
	BCC,
	BODY,
	CC,
	FROM,
	SUBJECT,
	TEXT,
	TO
    } SEARCH_FIELD;
    typedef struct {
	SEARCH_FIELD which;
	insensitiveString target;
	int *badChars;
	int *suffix;
    } TEXT_SEARCH_DATA;
    typedef std::list<TEXT_SEARCH_DATA> TEXT_SEARCH_LIST;
    typedef struct
    {
	insensitiveString headerField;
	insensitiveString target;
    } HEADER_SEARCH_DATA;
    typedef std::list<HEADER_SEARCH_DATA> HEADER_SEARCH_LIST;
    MailSearch();
    ~MailSearch();

    void addBitsToIncludeMask(uint32_t bitsToAdd) { m_includeMask |= bitsToAdd; }
    void addBitsToExcludeMask(uint32_t bitsToAdd) { m_excludeMask |= bitsToAdd; }
    void addBccSearch(const insensitiveString &csToSearchFor);
    void addBodySearch(const insensitiveString &csToSearchFor);
    void addBeforeSearch(DateTime &dateToSearchFor);
    void addCcSearch(const insensitiveString &csToSearchFor);
    void addFromSearch(const insensitiveString &csToSearchFor);
    void forceNoMatches() { m_forceNoMatches = true; }
    void addOnSearch(DateTime &dateToSearchFor);
    void addSinceSearch(DateTime &dateToSearchFor);
    void addSubjectSearch(const insensitiveString &csToSearchFor);
    void addTextSearch(const insensitiveString &csToSearchFor);
    void addToSearch(const insensitiveString &csToSearchFor);
    void addSentBeforeSearch(DateTime &dateToSearchFor);
    void addSentSinceSearch(DateTime &dateToSearchFor);
    void addSentOnSearch(DateTime &dateToSearchFor);
    void addMsnVector(MailStore *base, const SEARCH_RESULT &vector);
    void addUidVector(const SEARCH_RESULT &vector);
    void addSmallestSize(size_t limit);
    void addLargestSize(size_t limit);
    void addGenericHeaderSearch(const insensitiveString &headerName, const insensitiveString &toSearchFor);
    void addAllSearch() { m_forceAllMatches = true; }
    void clearUidVector() { m_uidVector.clear(); }
     
    // Evaluate collapses all the conditions into a vector of numbers and resets all the other
    // conditions
    MailStore::MAIL_STORE_RESULT evaluate(MailStore *where);
    void reportResults(const MailStore *where, SEARCH_RESULT *result);

private:
    void initialize();
    void finalize();
    bool m_forceNoMatches, m_forceAllMatches;
    uint32_t m_includeMask, m_excludeMask;
    size_t m_smallestSize, m_largestSize;

    DateTime *m_beginDate, *m_endDate;
    DateTime *m_beginInternalDate, *m_endInternalDate;
    TEXT_SEARCH_LIST m_headerSearchList, m_bodySearchList, m_textSearchList;
    HEADER_SEARCH_LIST m_searchList;
    SEARCH_RESULT m_uidVector;
    bool m_hasUidVector;
}; 

#endif // _MAILSEARCH_HPP_INCLUDED_
