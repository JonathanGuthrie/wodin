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

    void AddBitsToIncludeMask(uint32_t bitsToAdd) { m_includeMask |= bitsToAdd; }
    void AddBitsToExcludeMask(uint32_t bitsToAdd) { m_excludeMask |= bitsToAdd; }
    void AddBccSearch(const insensitiveString &csToSearchFor);
    void AddBodySearch(const insensitiveString &csToSearchFor);
    void AddBeforeSearch(DateTime &dateToSearchFor);
    void AddCcSearch(const insensitiveString &csToSearchFor);
    void AddFromSearch(const insensitiveString &csToSearchFor);
    void ForceNoMatches() { m_forceNoMatches = true; }
    void AddOnSearch(DateTime &dateToSearchFor);
    void AddSinceSearch(DateTime &dateToSearchFor);
    void AddSubjectSearch(const insensitiveString &csToSearchFor);
    void AddTextSearch(const insensitiveString &csToSearchFor);
    void AddToSearch(const insensitiveString &csToSearchFor);
    void AddSentBeforeSearch(DateTime &dateToSearchFor);
    void AddSentSinceSearch(DateTime &dateToSearchFor);
    void AddSentOnSearch(DateTime &dateToSearchFor);
    void AddMsnVector(MailStore *base, const SEARCH_RESULT &vector);
    void AddUidVector(const SEARCH_RESULT &vector);
    void AddSmallestSize(size_t limit);
    void AddLargestSize(size_t limit);
    void AddGenericHeaderSearch(const insensitiveString &headerName, const insensitiveString &toSearchFor);
    void AddAllSearch() { m_forceAllMatches = true; }
    void ClearUidVector() { m_uidVector.clear(); }
     
    // Evaluate collapses all the conditions into a vector of numbers and resets all the other
    // conditions
    MailStore::MAIL_STORE_RESULT Evaluate(MailStore *where);
    void ReportResults(const MailStore *where, SEARCH_RESULT *result);

private:
    void Initialize();
    void Finalize();
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
