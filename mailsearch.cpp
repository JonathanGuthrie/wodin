#include "mailsearch.hpp"

MailSearch::MailSearch() {
    m_uidVector.clear();
    m_hasUidVector = false;
    Initialize();
}

// Initialize initializes everything in the object except for m_uidVector and m_hasUidVector.  The
// reason I need a function separate from the constructor for this is because I also want to call it
// from Evaluate so that I can have this code in just one place.  The reason I don't want to
// initialize m_uidVector and m_hasUidVector here is because Evaluate is supposed to leave the results
// of the evaluation in m_uidVector and m_hasUidVector must be true for that to work
void MailSearch::Initialize()
{
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

MailSearch::~MailSearch()
{
    Finalize();
}



// The reason I need a separate finalize function is so that I can call it from Evaluate.
void MailSearch::Finalize(void) {
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


void MailSearch::AddBccSearch(const std::string &toSearchFor) {
    TEXT_SEARCH_DATA item;
    item.which = BCC;
    item.target = toSearchFor;
    m_headerSearchList.push_back(item);
}

static void build_bm_bad_chars(const std::string &target, int output[256]) {
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
static void find_bm_suffixes(const std::string &target, int output[]) {
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
static void build_bm_suffix(const std::string &target, int output[]) {
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

void MailSearch::AddBodySearch(const std::string &toSearchFor) {
    TEXT_SEARCH_DATA item;
    item.which = BODY;
    item.target = toSearchFor;
    // item.target.ToLower(); // SYZYGY -- what do I do with this?
    item.badChars = new int[256];
    build_bm_bad_chars(item.target, item.badChars);
    item.suffix = new int[item.target.size()];
    build_bm_suffix(item.target, item.suffix);
    m_bodySearchList.push_back(item);
}

void MailSearch::AddBeforeSearch(DateTime &dateToSearchFor) {
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

void MailSearch::AddCcSearch(const std::string &toSearchFor) {
    TEXT_SEARCH_DATA item;
    item.which = CC;
    item.target = toSearchFor;
    m_headerSearchList.push_back(item);
}

void MailSearch::AddFromSearch(const std::string &toSearchFor) {
    TEXT_SEARCH_DATA item;
    item.which = FROM;
    item.target = toSearchFor;
    m_headerSearchList.push_back(item);
}

void MailSearch::AddOnSearch(DateTime &dateToSearchFor) {
    AddSinceSearch(dateToSearchFor);
    dateToSearchFor.AddDays(1);
    AddBeforeSearch(dateToSearchFor);
}

void MailSearch::AddSinceSearch(DateTime &dateToSearchFor) {
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

void MailSearch::AddSubjectSearch(const std::string &toSearchFor) {
    TEXT_SEARCH_DATA item;
    item.which = SUBJECT;
    item.target = toSearchFor;
    m_headerSearchList.push_back(item);
}

void MailSearch::AddTextSearch(const std::string &toSearchFor) {
    TEXT_SEARCH_DATA item;
    item.which = TEXT;
    item.target = toSearchFor;
    // item.target.ToLower(); // SYZYGY -- what do I do with this?
    item.badChars = new int[256];
    build_bm_bad_chars(item.target, item.badChars);
    item.suffix = new int[item.target.size()];
    build_bm_suffix(item.target, item.suffix);
    m_textSearchList.push_back(item);
}

void MailSearch::AddToSearch(const std::string &toSearchFor) {
    TEXT_SEARCH_DATA item;
    item.which = TO;
    item.target = toSearchFor;
    m_headerSearchList.push_back(item);
}

void MailSearch::AddSentBeforeSearch(DateTime &dateToSearchFor) {
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

void MailSearch::AddSentSinceSearch(DateTime &dateToSearchFor) {
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

void MailSearch::AddSentOnSearch(DateTime &dateToSearchFor) {
    AddSentSinceSearch(dateToSearchFor);
    dateToSearchFor.AddDays(1);
    AddSentBeforeSearch(dateToSearchFor);
}

// For the AddMsnVector and AddUidVector methonds, the outputs are guaranteed to be
// valid message IDs or zero, and the vectors are guaranteed to be sorted, and the
// nonzero numbers in the vector are guaranteed to be unique.
void MailSearch::AddMsnVector(MailStore *base, const SEARCH_RESULT &vector) {
    if (!m_hasUidVector) {
	for (int i=0; i<vector.size(); ++i) {
	    m_uidVector.push_back(base->MailboxMsnToUid(vector[i]));
	}
    }
    else {
	int i = 0;
	while (i < m_uidVector.size()) {
	    unsigned long msn = base->MailboxUidToMsn(m_uidVector[i]);

	    SEARCH_RESULT::const_iterator elem = find(vector.begin(), vector.end(), msn);
	    if (vector.end() == elem) {	
		// m_uidVector.RemoveAt(i); // SYZYGY
	    }
	    else {
		++i;
	    }
	}
    }
    m_hasUidVector = true;
}

void MailSearch::AddUidVector(const SEARCH_RESULT &vector) {
    if (!m_hasUidVector) {
	for (int i=0; i<vector.size(); ++i) {
	    m_uidVector.push_back(vector[i]);
	}
    }
    else {
	int i = 0;
	while (i < m_uidVector.size()) {
	    int dummy;

	    SEARCH_RESULT::const_iterator elem = find(vector.begin(), vector.end(), i);
	    if (vector.end() == elem) {	
		// m_uidVector.RemoveAt(i); // SYZYGY
	    }
	    else {
		++i;
	    }
	}
    }
    m_hasUidVector = true;
}

void MailSearch::AddSmallestSize(size_t limit) {
    if (limit > m_smallestSize)
    {
	m_smallestSize = limit;
    }
}

void MailSearch::AddLargestSize(size_t limit) {
    if (limit < m_largestSize) {
	m_largestSize = limit;
    }
}

#define MAX(a, b) ((a>b) ? a : b)

// Recursive body search searches for the substring through all the body parts
// it does not search any of the headers
static bool RecursiveBodySearch(MESSAGE_BODY root, const MailSearch::TEXT_SEARCH_DATA target, const char *bigBuffer) {
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
		result = RecursiveBodySearch((*root.subparts)[i], target, bigBuffer);
	    }
	}
    }
    return result;
}

// This stores into The uid vector the list of UID's that match the conditions described by searchSpec
MailStore::MAIL_STORE_RESULT MailSearch::Evaluate(MailStore *where) {
    MailStore::MAIL_STORE_RESULT result = MailStore::SUCCESS;

    if (where->IsMailboxOpen()) {
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
		    const SEARCH_RESULT *temp = where->SearchMetaData(m_includeMask, m_includeMask | m_excludeMask, m_smallestSize, m_largestSize, m_beginInternalDate, m_endInternalDate);
		    if (NULL != temp) {
			AddUidVector(*temp);
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
		char *messageBuffer;
		SEARCH_RESULT inputVector, outputVector;
		outputVector.clear();
		ReportResults(where, &inputVector);

#if 0 // SYZYGY -- warning SIMDESKISM
		for (int i = 0; i < inputVector.size(); ++i) {
		    MailMessage message;
		    if (CMailMessage::SUCCESS == message.parse(where, input_vector[i], true, true)) {
			// If I can't read it, it doesn't wind up on the list of matching messages
			if ((0 != m_tslBodySearchList.GetSize()) ||
			    (0 != m_tslTextSearchList.GetSize())) {
			    CSimFile infile;
			    DWORD dwDummy;
			    CSimpleDate sdDummy(MMDDYYYY, true);
			    bool bDummy;
			    CStdString csPhysicalFileName = where->GetMessagePhysicalPath(input_vector[i], dwDummy, sdDummy, bDummy);

			    pMessageBuffer = new char[message.GetMessageBody().dwBodyOctets];
			    if (infile.Open(csPhysicalFileName, CSimFile::modeRead)) {
				infile.Read(pMessageBuffer, message.GetMessageBody().dwBodyOctets);
				infile.Close();
			    }
			}
			bool bItIsIn = true;

			for (int j=0; bItIsIn && (j<m_hslSearchList.GetSize()); ++j) {
			    std::string target = m_searchList[j].target;
			    // target.ToUpper(); // SYZYGY -- WTF?
			    bItIsIn = false;
			    std::pair<HEADER_FIELDS::const_iterator, HEADER_FIELDS::const_iterator> headers;
			    headers = message.GetHeaderList().equal_range(m_hslSearchList[j].csHeaderField);
			    for (HEADER_FIELDS::const_iterator k = headers.first; k!=headers.second; ++k) {
				CStdString line = k->second;
				line.ToUpper();
				if (-1 != line.Find(target)) {
				    bItIsIn = true;
				    break;
				}
			    }
			}

			for (int j=0; bItIsIn && (j<m_tslHeaderSearchList.GetSize()); ++j) {
			    CStdString target = m_tslHeaderSearchList[j].target;
			    target.ToUpper();

			    switch(m_tslHeaderSearchList[j].which) {
			    case MailSearch::BCC: 
			    {
				std::string line = message.GetBcc();
				line.ToUpper();
				bItIsIn = (-1 != line.Find(target));
			    }
			    break;

			    case MailSearch::CC:
			    {
				CStdString line = message.GetCc();
				line.ToUpper();
				bItIsIn = (-1 != line.Find(target));
			    }
			    break;

			    case MailSearch::FROM:
			    {
				CStdString line = message.GetFrom();
				line.ToUpper();
				bItIsIn = (-1 != line.Find(target));
			    }
			    break;

			    case MailSearch::SUBJECT:
			    {
				CStdString line = message.GetSubject();
				line.ToUpper();
				bItIsIn = (-1 != line.Find(target));
			    }
			    break;

			    case MailSearch::TO:
			    {
				CStdString line = message.GetTo();
				line.ToUpper();
				bItIsIn = (-1 != line.Find(target));
			    }
			    break;
			    }
			}

			for (int j=0; bItIsIn && (j<m_tslBodySearchList.GetSize()); ++j)
			{
			    bItIsIn = RecursiveBodySearch(message.GetMessageBody(), m_tslBodySearchList[j], pMessageBuffer);
			}

			for (int j=0; bItIsIn && (j<m_tslTextSearchList.GetSize()); ++j)
			{
			    DWORD i = 0;
			    CStdString csFinding = m_tslTextSearchList[j].target;
			    int length = csFinding.GetLength();

			    bItIsIn = false;
			    while (!bItIsIn && (i < (message.GetMessageBody().dwBodyOctets)))
			    {
				int k;
				for (k = length - 1; (k >= 0) && (csFinding[k] == tolower(pMessageBuffer[i+k])); --k)
				{
				    // NOTHING
				}
				if (k < 0)
				{
				    bItIsIn = true;
				}
				else
				{
				    i += MAX(m_tslTextSearchList[j].bm_suffix[k],
					     m_tslTextSearchList[j].bm_bad_chars[pMessageBuffer[i+k]] - length + 1 + k);
				}
			    }
			}

			if (bItIsIn && (NULL != m_dateBeginDate))
			{
			    struct tm msg_time = message.GetMessageTime();
			    time_t date = mktime(&msg_time);
			    bItIsIn = (*m_dateBeginDate <= date);
			}

			if (bItIsIn && (NULL != m_dateEndDate))
			{
			    struct tm msg_time = message.GetMessageTime();
			    time_t date = mktime(&msg_time);
			    bItIsIn = (date <= *m_dateEndDate);
			}

			if (bItIsIn)
			{
			    output_vector.Add(input_vector[i], compare_msns);
			}
			if ((0 != m_tslBodySearchList.GetSize()) ||
			    (0 != m_tslTextSearchList.GetSize()))
			{
			    delete[] pMessageBuffer;
			}
		    }
		}
#endif // 0 SYZYGY -- END SIMDESKISM
		AddUidVector(outputVector);
	    }
	}
    }
    else {
	result = MailStore::MAILBOX_NOT_OPEN;
    }

    // When where->MessageSearch returns, everything is collapsed into m_uidVector, so I re-initialize the flag
    // values so it won't ever attempt to recalculate them no matter what happens
    Finalize();
    Initialize();
    return result;
}

void MailSearch::ReportResults(const MailStore *where, SEARCH_RESULT *result) {
    if (!m_hasUidVector) {
	where->MessageList(m_uidVector); 
    }
    m_hasUidVector = true;
    result->clear();
    for (int i=0; i<m_uidVector.size(); ++i) {
	result->push_back(m_uidVector[i]);
    }
}

void MailSearch::AddGenericHeaderSearch(const std::string &headerName, const std::string &toSearchFor) {
    HEADER_SEARCH_DATA item;
    item.headerField = headerName;
    item.target = toSearchFor;
    m_searchList.push_back(item);
}

