///////////////////////////////////////////////////////////////////////////////
//
// imapsession.cpp : Implementation of ImapSession
//
// Simdesk, Inc. © 2002-2004
// Author: Jonathan Guthrie
//
///////////////////////////////////////////////////////////////////////////////


// SYZYGY -- I need to have some sort of timer that can fire and handle several
// different cases:
//   Look for updated data for asynchronous notifies
//   ***DONE*** Delay for failed LOGIN's or AUTHENTICATIONS
//   ***DONE*** Handle idle timeouts, both for IDLE mode and inactivity in other modes.


// SYZYGY -- I should allow the setting of the keepalive socket option

#include <time.h>
#include <sys/fsuid.h>

#include "imapsession.hpp"
#include "imapserver.hpp"
#include "sasl.hpp"

#ifndef MIN
#define MIN(a, b)  (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))
#endif

// #include <iostream>
#include <list>

typedef enum
{
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

static SEARCH_SYMBOL_T sSearchSymbolTable;

typedef enum
{
    SAV_MESSAGES = 1,
    SAV_RECENT = 2,
    SAV_UIDNEXT = 4,
    SAV_UIDVALIDITY = 8,
    SAV_UNSEEN = 16
} STATUS_ATT_VALUES;
typedef std::map<insensitiveString, STATUS_ATT_VALUES> STATUS_SYMBOL_T;

static STATUS_SYMBOL_T sStatusSymbolTable;

typedef std::map<insensitiveString, MailStore::MAIL_STORE_FLAGS> FLAG_SYMBOL_T;

static FLAG_SYMBOL_T sFlagSymbolTable;

typedef enum
{
    UID_STORE,
    UID_FETCH,
    UID_SEARCH,
    UID_COPY
} UID_SUBCOMMAND_VALUES;
typedef std::map<std::string, UID_SUBCOMMAND_VALUES> UID_SUBCOMMAND_T;

static UID_SUBCOMMAND_T sUidSymbolTable;

typedef enum
{
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

static FETCH_NAME_T sFetchSymbolTable;

typedef enum
{
    FETCH_BODY_BODY,
    FETCH_BODY_HEADER,
    FETCH_BODY_FIELDS,
    FETCH_BODY_NOT_FIELDS,
    FETCH_BODY_TEXT,
    FETCH_BODY_MIME
} FETCH_BODY_PARTS;

bool ImapSession::anonymousEnabled = false;

IMAPSYMBOLS ImapSession::symbols; 

void ImapSession::BuildSymbolTables()
{
#if 0
    CMailMessage::BuildSymbolTable();
#endif // 0

    symbol symbolToInsert;
    symbolToInsert.levels[0] = true;
    symbolToInsert.levels[1] = true;
    symbolToInsert.levels[2] = true;
    symbolToInsert.levels[3] = true;
    symbolToInsert.handler = &ImapSession::CapabilityHandler;
    symbols.insert(IMAPSYMBOLS::value_type("CAPABILITY", symbolToInsert));
    symbolToInsert.handler = &ImapSession::NoopHandler;
    symbols.insert(IMAPSYMBOLS::value_type("NOOP", symbolToInsert));
    symbolToInsert.handler = &ImapSession::LogoutHandler;
    symbols.insert(IMAPSYMBOLS::value_type("LOGOUT", symbolToInsert));

    symbolToInsert.levels[0] = true;
    symbolToInsert.levels[1] = false;
    symbolToInsert.levels[2] = false;
    symbolToInsert.levels[3] = false;
    symbolToInsert.handler = &ImapSession::StarttlsHandler;
    symbols.insert(IMAPSYMBOLS::value_type("STARTTTLS", symbolToInsert)); 
    symbolToInsert.handler = &ImapSession::AuthenticateHandler;
    symbols.insert(IMAPSYMBOLS::value_type("AUTHENTICATE", symbolToInsert));
    symbolToInsert.handler = &ImapSession::LoginHandler;
    symbols.insert(IMAPSYMBOLS::value_type("LOGIN", symbolToInsert));

    symbolToInsert.levels[0] = false;
    symbolToInsert.levels[1] = true;
    symbolToInsert.levels[2] = true;
    symbolToInsert.levels[3] = false;
#if 0
    symbolToInsert.handler = &ImapSession::NamespaceHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type(_T("NAMESPACE"), symbolToInsert));
    symbolToInsert.handler = &ImapSession::SelectHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type(_T("SELECT"), symbolToInsert));
    symbolToInsert.handler = &ImapSession::ExamineHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type(_T("EXAMINE"), symbolToInsert));
#endif // 0
    symbolToInsert.handler = &ImapSession::CreateHandler;
    symbols.insert(IMAPSYMBOLS::value_type("CREATE", symbolToInsert));
#if 0
    symbolToInsert.handler = &ImapSession::DeleteHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type(_T("DELETE"), symbolToInsert));
    symbolToInsert.handler = &ImapSession::RenameHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type(_T("RENAME"), symbolToInsert));
#endif // 0
    symbolToInsert.handler = &ImapSession::SubscribeHandler;
    symbols.insert(IMAPSYMBOLS::value_type("SUBSCRIBE", symbolToInsert));
    symbolToInsert.handler = &ImapSession::UnsubscribeHandler;
    symbols.insert(IMAPSYMBOLS::value_type("UNSUBSCRIBE", symbolToInsert));
    symbolToInsert.handler = &ImapSession::ListHandler;
    symbols.insert(IMAPSYMBOLS::value_type("LIST", symbolToInsert));
    symbolToInsert.handler = &ImapSession::LsubHandler;
    symbols.insert(IMAPSYMBOLS::value_type("LSUB", symbolToInsert));
#if 0
    symbolToInsert.handler = &ImapSession::StatusHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type(_T("STATUS"), symbolToInsert));
    symbolToInsert.handler = &ImapSession::AppendHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type(_T("APPEND"), symbolToInsert));

    symbolToInsert.levels[0] = false;
    symbolToInsert.levels[1] = false;
    symbolToInsert.levels[2] = true;
    symbolToInsert.levels[3] = false;
    symbolToInsert.handler = &ImapSession::CheckHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type(_T("CHECK"), symbolToInsert));
    symbolToInsert.handler = &ImapSession::CloseHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type(_T("CLOSE"), symbolToInsert)); 
    symbolToInsert.handler = &ImapSession::ExpungeHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type(_T("EXPUNGE"), symbolToInsert));
    symbolToInsert.handler = &ImapSession::SearchHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type(_T("SEARCH"), symbolToInsert));
    symbolToInsert.handler = &ImapSession::FetchHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type(_T("FETCH"), symbolToInsert));
    symbolToInsert.handler = &ImapSession::StoreHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type(_T("STORE"), symbolToInsert));
    symbolToInsert.handler = &ImapSession::CopyHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type(_T("COPY"), symbolToInsert));
    symbolToInsert.handler = &ImapSession::UidHandler;
    m_symbols.insert(IMAPSYMBOLS::value_type(_T("UID"), symbolToInsert));
#endif // 0

    // This is the symbol table for the search keywords
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("ALL",        SSV_ALL));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("ANSWERED",   SSV_ANSWERED));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("BCC",        SSV_BCC));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("BEFORE",     SSV_BEFORE));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("BODY",       SSV_BODY));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("CC",         SSV_CC));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("DELETED",    SSV_DELETED));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("FLAGGED",    SSV_FLAGGED));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("FROM",       SSV_FROM));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("KEYWORD",    SSV_KEYWORD));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("NEW",        SSV_NEW));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("OLD",        SSV_OLD));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("ON",         SSV_ON));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("RECENT",     SSV_RECENT));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("SEEN",       SSV_SEEN));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("SINCE",      SSV_SINCE));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("SUBJECT",    SSV_SUBJECT));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("TEXT",       SSV_TEXT));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("TO",         SSV_TO));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("UNANSWERED", SSV_UNANSWERED));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("UNDELETED",  SSV_UNDELETED));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("UNFLAGGED",  SSV_UNFLAGGED));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("UNKEYWORD",  SSV_UNKEYWORD));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("UNSEEN",     SSV_UNSEEN));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("DRAFT",      SSV_DRAFT));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("HEADER",     SSV_HEADER));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("LARGER",     SSV_LARGER));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("NOT",        SSV_NOT));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("OR",         SSV_OR));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("SENTBEFORE", SSV_SENTBEFORE));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("SENTON",     SSV_SENTON));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("SENTSINCE",  SSV_SENTSINCE));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("SMALLER",    SSV_SMALLER));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("UID",        SSV_UID));
    sSearchSymbolTable.insert(SEARCH_SYMBOL_T::value_type("UNDRAFT",    SSV_UNDRAFT));

    sStatusSymbolTable.insert(STATUS_SYMBOL_T::value_type("MESSAGES",	 SAV_MESSAGES));
    sStatusSymbolTable.insert(STATUS_SYMBOL_T::value_type("RECENT",	 SAV_RECENT));
    sStatusSymbolTable.insert(STATUS_SYMBOL_T::value_type("UIDNEXT",	 SAV_UIDNEXT));
    sStatusSymbolTable.insert(STATUS_SYMBOL_T::value_type("UIDVALIDITY", SAV_UIDVALIDITY));
    sStatusSymbolTable.insert(STATUS_SYMBOL_T::value_type("UNSEEN",	 SAV_UNSEEN));

    // These are only the standard flags.  User-defined flags must be handled differently
    sFlagSymbolTable.insert(FLAG_SYMBOL_T::value_type("ANSWERED", MailStore::IMAP_MESSAGE_ANSWERED));
    sFlagSymbolTable.insert(FLAG_SYMBOL_T::value_type("FLAGGED",  MailStore::IMAP_MESSAGE_FLAGGED));
    sFlagSymbolTable.insert(FLAG_SYMBOL_T::value_type("DELETED",  MailStore::IMAP_MESSAGE_DELETED));
    sFlagSymbolTable.insert(FLAG_SYMBOL_T::value_type("SEEN",     MailStore::IMAP_MESSAGE_SEEN));
    sFlagSymbolTable.insert(FLAG_SYMBOL_T::value_type("DRAFT",    MailStore::IMAP_MESSAGE_DRAFT));

    sUidSymbolTable.insert(UID_SUBCOMMAND_T::value_type("STORE",  UID_STORE));
    sUidSymbolTable.insert(UID_SUBCOMMAND_T::value_type("FETCH",  UID_FETCH));
    sUidSymbolTable.insert(UID_SUBCOMMAND_T::value_type("SEARCH", UID_SEARCH));
    sUidSymbolTable.insert(UID_SUBCOMMAND_T::value_type("COPY",   UID_COPY));

    sFetchSymbolTable.insert(FETCH_NAME_T::value_type("ALL",           FETCH_ALL));
    sFetchSymbolTable.insert(FETCH_NAME_T::value_type("FAST",          FETCH_FAST));
    sFetchSymbolTable.insert(FETCH_NAME_T::value_type("FULL",          FETCH_FULL));
    sFetchSymbolTable.insert(FETCH_NAME_T::value_type("BODY",          FETCH_BODY));
    sFetchSymbolTable.insert(FETCH_NAME_T::value_type("BODY.PEEK",     FETCH_BODY_PEEK));
    sFetchSymbolTable.insert(FETCH_NAME_T::value_type("BODYSTRUCTURE", FETCH_BODYSTRUCTURE));
    sFetchSymbolTable.insert(FETCH_NAME_T::value_type("ENVELOPE",      FETCH_ENVELOPE));
    sFetchSymbolTable.insert(FETCH_NAME_T::value_type("FLAGS",         FETCH_FLAGS));
    sFetchSymbolTable.insert(FETCH_NAME_T::value_type("INTERNALDATE",  FETCH_INTERNALDATE));
    sFetchSymbolTable.insert(FETCH_NAME_T::value_type("RFC822",        FETCH_RFC822));
    sFetchSymbolTable.insert(FETCH_NAME_T::value_type("RFC822.HEADER", FETCH_RFC822_HEADER));
    sFetchSymbolTable.insert(FETCH_NAME_T::value_type("RFC822.SIZE",   FETCH_RFC822_SIZE));
    sFetchSymbolTable.insert(FETCH_NAME_T::value_type("RFC822.TEXT",   FETCH_RFC822_TEXT));
    sFetchSymbolTable.insert(FETCH_NAME_T::value_type("UID",           FETCH_UID));
}

ImapSession::ImapSession(Socket *sock, ImapServer *server, SessionDriver *driver, unsigned failedLoginPause) : s(sock), server(server), driver(driver), failedLoginPause(failedLoginPause)
{
    state = ImapNotAuthenticated;
    userData = NULL;
    m_LoginDisabled = false;
    lineBuffer = NULL;	// going with a static allocation for now
    lineBuffPtr = 0;
    lineBuffLen = 0;
    parseBuffer = NULL;
    parseBuffLen = 0;
    literalLength = 0;
    std::string response("* OK [");
    response += BuildCapabilityString() + "] IMAP4rev1 server ready\r\n";
    s->Send((uint8_t *) response.c_str(), response.size());
    lastCommandTime = time(NULL);
}

ImapSession::~ImapSession()
{
    if (NULL != userData)
    {
	delete userData;
    }
    if (ImapSelected == state)
    {
	store->MailboxClose();
    }
    std::string bye = "* BYE SimDesk IMAP4rev1 server shutting down\r\n";
    s->Send((uint8_t *)bye.data(), bye.size());
}


void ImapSession::atom(uint8_t *data, const size_t dataLen, size_t &parsingAt)
{
    static char include_list[] = "!#$&'+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[^_`abcdefghijklmnopqrstuvwxyz|}~";
 
    while (parsingAt < dataLen)
    {
	if (NULL != strchr(include_list, data[parsingAt]))
	{
	    uint8_t temp = toupper(data[parsingAt]);
	    AddToParseBuffer(&temp, 1, false);
	}
	else
	{
	    AddToParseBuffer(NULL, 0);
	    break;
	}
	++parsingAt;
    }
}

enum ImapStringState ImapSession::astring(uint8_t *data, const size_t dataLen, size_t &parsingAt, bool makeUppercase,
					  const char *additionalChars)
{
    ImapStringState result = ImapStringGood;
    if (parsingAt < dataLen)
    {
	if ('"' == data[parsingAt])
	{
	    parsingAt++; // Skip the opening quote
	    bool notdone = true, escape_flag = false;
	    do
	    {
		switch(data[parsingAt])
		{
		case '"':
		    if (escape_flag)
		    {
			AddToParseBuffer(&data[parsingAt], 1, false);
			escape_flag = false;
		    }
		    else
		    {
			AddToParseBuffer(NULL, 0);
			notdone = false;
		    }
		    break;

		case '\\':
		    if (escape_flag)
		    {
			AddToParseBuffer(&data[parsingAt], 1, false);
		    }
		    escape_flag = !escape_flag;
		    break;

		default:
		    escape_flag = false;
		    if (makeUppercase)
		    {
			uint8_t temp = toupper(data[parsingAt]);
			AddToParseBuffer(&temp, 1, false);
		    }
		    else
		    {
			AddToParseBuffer(&data[parsingAt], 1, false);
		    }
		}
		++parsingAt;
		// Check to see if I've run off the end of the input buffer
		if ((parsingAt > dataLen) || ((parsingAt == dataLen) && notdone))
		{
		    notdone = false;
		    result = ImapStringBad;
		}
	    } while(notdone);
	}
	else
	{
	    if ('{' == data[parsingAt])
	    {
		// It's a literal string
		result = ImapStringPending;
		parsingAt++; // Skip the curly brace
		char *close;
		// read the number
		literalLength = strtoul((char *)&data[parsingAt], &close, 10);
		// check for the close curly brace and end of message and to see if I can fit all that data
		// into the buffer
		size_t lastChar = (size_t) (close - ((char *)data));
		if ((NULL == close) || ('}' != close[0]) || (lastChar != (dataLen - 1)))
		{
		    result = ImapStringBad;
		}
	    }
	    else
	    {
		// It looks like an atom
		static char include_list[] = "!#$&'+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[]^_`abcdefghijklmnopqrstuvwxyz|}~";
		result = ImapStringBad;

		while (parsingAt < dataLen)
		{
		    if ((NULL == strchr(include_list, data[parsingAt])) &&
			((NULL == additionalChars) || (NULL == strchr(additionalChars, data[parsingAt]))))
		    {
			break;
		    }
		    result = ImapStringGood;
		    if (makeUppercase)
		    {
			uint8_t temp = toupper(data[parsingAt]);
			AddToParseBuffer(&temp, 1, false);
		    }
		    else
		    {
			AddToParseBuffer(&data[parsingAt], 1, false);
		    }
		    ++parsingAt;
		}
		AddToParseBuffer(NULL, 0);
	    }
	}
    }
    return result;
}


void ImapSession::AddToParseBuffer(const uint8_t *data, size_t length, bool bNulTerminate)
{
    // I need the +1 because I'm going to add a '\0' to the end of the string
    if ((parsePointer + length + 1) > parseBuffLen)
    {
	size_t dwNewSize = MAX(parsePointer + length + 1, 2 * parseBuffLen);
	uint8_t *temp = new uint8_t[dwNewSize];
	uint8_t *debug = parseBuffer;
	memcpy(temp, parseBuffer, parsePointer);
	delete[] parseBuffer;
	parseBuffLen = dwNewSize;
	parseBuffer = temp;
    }
    if (0 < length)
    {
	memcpy(&parseBuffer[parsePointer], data, length);
    }
    parsePointer += length;
    parseBuffer[parsePointer] = '\0';
    if (bNulTerminate)
    {
	parsePointer++;
    }
}

/*--------------------------------------------------------------------------------------*/
/* ReceiveData										*/
/*--------------------------------------------------------------------------------------*/
int ImapSession::ReceiveData(uint8_t *data, size_t dataLen)
{
    lastCommandTime = time(NULL);
    if (NULL != userData) {
	setfsuid(1000); // SYZYGY -- how do I set the privileges I need to access the files?
    }

    uint32_t newDataBase = 0;
    int result = 0;
    // Okay, I need to organize the data into lines.  Each line is however much data ended by a
    // CRLF.  In order to support this, I need a buffer to put data into.

    // First deal with the characters left over from the last call to ReceiveData, if any
    if (0 < lineBuffPtr)
    {
	bool crFlag = ('\r' == lineBuffer[lineBuffPtr]); 

	for(; newDataBase<dataLen; ++newDataBase)
	{
	    // check lineBuffPtr against its current size
	    if (lineBuffPtr >= lineBuffLen)
	    {
		// If it's currently not big enough, check its size against the biggest allowable
		if (lineBuffLen > IMAP_BUFFER_LEN)
		{
		    // The buffer is too big so I assume there is crap in the buffer, so I can ignore it.
		    // At this point, there is no demonstrably "right" thing to do, so anything I can do 
		    // is likely to be wrong, so doing something simple and wrong is slightly better than
		    // doing something complicated and wrong.
		    lineBuffPtr = 0;
		    delete lineBuffer;
		    lineBuffer = NULL;
		    lineBuffLen = 0;
		    break;
		}
		else
		{
		    // The buffer hasn't reached the absolute maximum size for processing, so I grow it
		    uint8_t *temp;
		    temp = new uint8_t[2 * lineBuffLen];
		    memcpy(temp, lineBuffer, lineBuffPtr);
		    delete lineBuffer;
		    lineBuffer = temp;
		    lineBuffLen *= 2;
		}

		// Eventually, I'll make the buffer bigger up to a point
	    }
	    lineBuffer[lineBuffPtr++] = data[newDataBase];
	    if (crFlag && ('\n' == data[newDataBase]))
	    {
		result = HandleOneLine(lineBuffer, lineBuffPtr);
		lineBuffPtr = 0;
		delete lineBuffer;
		lineBuffer = NULL;
		lineBuffLen = 0;
		++newDataBase;  // This handles the increment to skip over the character that we would be doing,
		// in the for header, but we're breaking out of the loop so it has to be done
		// explicitly 
		break;
	    }
	    crFlag = ('\r' == data[newDataBase]);
	}
    }

    // Next, deal with all the whole lines that have arrived in the current call to Receive Data
    // I also deal with literal strings here, because I don't have to break them into lines
    bool crFlag = false;
    bool notDone = true;
    uint32_t currentCommandStart = newDataBase;
    do
    {
	if (0 < literalLength)
	{
	    size_t bytesToFeed = literalLength;
	    if (bytesToFeed >= (dataLen - currentCommandStart))
	    {
		bytesToFeed = dataLen - currentCommandStart;
		notDone = false;
		result = HandleOneLine(&data[currentCommandStart], bytesToFeed);
		currentCommandStart += bytesToFeed;
		newDataBase = currentCommandStart;
	    }
	    else
	    {
		// If I get here, then I know that I've got at least as many characters in
		// the input buffer as I need to fulfill the literal request.  However, I 
		// can't just pass that many characters to HandleOneLine because it may not
		// be the end of a line.  I deal with this by using the usual end-of-line logic
		// starting with the beginning of the string or two bytes before the end.
		if (bytesToFeed > 2)
		{
		    newDataBase += bytesToFeed - 2;
		}
		crFlag = false;
		for(; newDataBase<dataLen; ++newDataBase)
		{
		    if (crFlag && ('\n' == data[newDataBase]))
		    {
			// The "+1"s here are due to the fact that I want to include the current character in the 
			// line to be handled
			result = HandleOneLine(&data[currentCommandStart], newDataBase - currentCommandStart + 1);
			currentCommandStart = newDataBase + 1;
			crFlag = false;
			break;
		    }
		    crFlag = ('\r' == data[newDataBase]);
		}
		notDone = (currentCommandStart < dataLen) && (newDataBase < dataLen);
	    }
	}
	else
	{
	    for(; newDataBase<dataLen; ++newDataBase)
	    {
		if (crFlag && ('\n' == data[newDataBase]))
		{
		    // The "+1"s here are due to the fact that I want to include the current character in the 
		    // line to be handled
		    result = HandleOneLine(&data[currentCommandStart], newDataBase - currentCommandStart + 1);
		    currentCommandStart = newDataBase + 1;
		    crFlag = false;
		    break;
		}
		crFlag = ('\r' == data[newDataBase]);
	    }
	    notDone = (currentCommandStart < dataLen) && (newDataBase < dataLen);
	}
    } while (notDone);

    // Lastly, save any leftover data from the current call to ReceiveData
    // Eventually, I'll size the buffer for this result
    // for now, assume the message is garbage if it's too big
    if ((0 < (newDataBase - currentCommandStart)) && (IMAP_BUFFER_LEN > (newDataBase - currentCommandStart)))
    {
	lineBuffPtr = newDataBase - currentCommandStart;
	lineBuffLen = 2 * lineBuffPtr;
	lineBuffer = new uint8_t[lineBuffLen];
	memcpy(lineBuffer, &data[currentCommandStart], lineBuffPtr);
    }
    return result;
}

std::string ImapSession::FormatTaggedResponse(IMAP_RESULTS status)
{
    std::string response;

    switch(status)
    {
    case IMAP_OK:
	response = (char *)parseBuffer;
	response += " OK";
	response += responseCode[0] == '\0' ? "" : " ";
	response += (char *)responseCode;
	response += " ";
	response += (char *)&parseBuffer[commandString];
	response += " ";
	response += (char *)responseText;
	response += "\r\n";
	break;
 
    case IMAP_NO:
    case IMAP_NO_WITH_PAUSE:
	response = (char *)parseBuffer;
	response += " NO";
	response += responseCode[0] == '\0' ? "" : " ";
	response += (char *)responseCode;
	response += " ";
	response += (char *)&parseBuffer[commandString];
	response += " ";
	response += (char *)responseText;
	response += "\r\n";
	break;

    case IMAP_NOTDONE:
	response += "+ ";
	response += responseCode[0] == '\0' ? "" : " ";
	response += (char *)responseCode;
	response += " ";
	response += (char *)responseText;
	response += "\r\n";
	break;

    default:
    case IMAP_BAD:
	response = (char *)parseBuffer;
	response += " BAD";
	response += responseCode[0] == '\0' ? "" : " ";
	response += (char *)responseCode;
	response += " ";
	response += (char *)&parseBuffer[commandString];
	response += " ";
	response += (char *)responseText;
	response += "\r\n";
	break; 
    }

    return response;
}

// This function assumes that it is passed a single entire line each time.
// It returns true if it's expecting more data, and false otherwise
int ImapSession::HandleOneLine(uint8_t *data, size_t dataLen)
{
    std::string response;
    int result = 0;

    // Some commands accumulate data over multiple lines.  Since the system
    // gives us data one line at a time, I have to put a state here for each
    // command that is processed over multiple calls to ReceiveData
    switch (inProgress)
    {
    case ImapCommandNone:
    default:
    {
	size_t i;
	dataLen -= 2;  // It's not literal data, so I can strip the CRLF off the end
	data[dataLen] = '\0';  // Make sure it's terminated so strchr et al work
	// There's nothing magic about 10, it just seems a convenient size
	// The initial value of parseBuffLen should be bigger than dwDataLen because
	// if it's the same size or smaller, it WILL have to be reallocated at some point.
	parseBuffLen = dataLen + 10;
	parseBuffer = new uint8_t[parseBuffLen];
	uint8_t *debug = parseBuffer;
	parsePointer = 0; // It's a new command, so I need to reset the pointer

	char *tagEnd = strchr((char *)data, ' ');
	if (NULL != tagEnd)
	{
	    i = (size_t) (tagEnd - ((char *)data));
	}
	else
	{
	    i = dataLen;
	}
	AddToParseBuffer(data, i);
	if (i < dataLen)
	{
	    ++i;  // Lose the space separating the tag from the command
	    commandString = parsePointer;
	    tagEnd = strchr((char *)&data[i], ' ');
	    if (NULL != tagEnd)
	    {
		AddToParseBuffer(&data[i], (size_t) (tagEnd - ((char *)&data[i])));
		i = (size_t) (tagEnd - ((char *)data));
	    }
	    else
	    {
		AddToParseBuffer(&data[i], dataLen - i);
		i = dataLen;
	    }

	    arguments = parsePointer;
	    if (i < dataLen)
	    {
		++i;  // Lose the space between the command and the arguments
	    }
	    IMAPSYMBOLS::iterator found = symbols.find((char *)&parseBuffer[commandString]);
	    if ((found != symbols.end()) && found->second.levels[state])
	    {
		IMAP_RESULTS status;
		responseCode[0] = '\0';
		strncpy(responseText, "Completed", MAX_RESPONSE_STRING_LENGTH);
		status = (this->*found->second.handler)(data, dataLen, i);
		response = FormatTaggedResponse(status);
		if (IMAP_NO_WITH_PAUSE == status)
		{
		    result = 1;
		    GetServer()->DelaySend(driver, failedLoginPause, response);
		}
		else
		{
		    s->Send((uint8_t *)response.data(), response.size());
		}
	    }
	    else
	    {
		response = (char *)parseBuffer;
		response += " BAD ";
		response += (char *)&parseBuffer[commandString];
		response += " Unrecognized Command\r\n";
		s->Send((uint8_t *)response.data(), response.size());
	    }
	    if (ImapLogoff <= state)
	    {
		// If I'm logged off, I'm obviously not expecting any more data
		result = -1;
	    }
	}
	else
	{
	    response = (char *)parseBuffer;
	    response += " BAD No Command\r\n";
	    s->Send((uint8_t *)response.data(), (int)response.size());
	}
    }
    break;

#if 0
    case ImapCommandAuthenticate:
    {
	IMAP_RESULTS result;
	CStdString s((char*)pData, dwDataLen - 2);

	switch (m_cAuth->ReceiveResponse(s))
	{
	case CSasl::ok:
	    result = IMAP_OK;
	    m_pUser->GetUserFileSystem();

	    if (NULL == m_msStore)
	    {
		m_msStore = new CMailStore(this, m_pUser);
	    }
	    m_msStore->CreateMailbox(_T("INBOX"));

	    Log("Client %u logged-in user %s %lu\n", m_dwClientNumber, m_pUser->m_szUsername.c_str(), m_pUser->m_nUserID);

	    m_eState = ImapAuthenticated;
	    break;

	case CSasl::no:
	    result = IMAP_NO_WITH_PAUSE;
	    m_eState = ImapNotAuthenticated;
	    strncpy(m_pResponseText, _T("Authentication Failed"), MAX_RESPONSE_STRING_LENGTH);
	    break;

	case CSasl::bad:
	default:
	    result = IMAP_BAD;
	    m_eState = ImapNotAuthenticated;
	    strncpy(m_pResponseText, _T("Authentication Cancelled"), MAX_RESPONSE_STRING_LENGTH);
	    break;
	}
	delete m_cAuth;
	response = FormatTaggedResponse(result);
	Send((void *)response.data(), (int)response.size());
	m_eInProgress = ImapCommandNone;
    }
    break;

    case ImapCommandAppend:
	// If I get here, I am waiting for a bunch of characters to arrive
	if (0 == m_dwParseStage)
	{
	    // It's the mailbox name that's arrived
	    if (dwDataLen >= m_dwLiteralLength)
	    {
		AddToParseBuffer(pData, m_dwLiteralLength);
		++m_dwParseStage;
		DWORD i = m_dwLiteralLength;
		m_dwLiteralLength = 0;
		if (2 < dwDataLen)
		{
		    // Get rid of the CRLF if I have it
		    dwDataLen -= 2;
		    pData[dwDataLen] = '\0';  // Make sure it's terminated so strchr et al work
		}
		m_pResponseText[0] = '\0';
		CStdString response = FormatTaggedResponse(AppendHandlerExecute(pData, dwDataLen, i));
		Send((void *)response.data(), (int)response.size());
	    }
	    else
	    {
		AddToParseBuffer(pData, m_dwLiteralLength, false);
		m_dwLiteralLength -= dwDataLen;
	    }
	}
	else
	{
	    IMAP_RESULTS result = IMAP_OK;
	    if (1 == m_dwParseStage)
	    {
		DWORD residue;
		// It's the message body that's arrived
		DWORD len = MIN(m_dwLiteralLength, dwDataLen);
		if (m_dwLiteralLength < dwDataLen)
		{
		    residue = dwDataLen - m_dwLiteralLength;
		}
		else
		{
		    residue = 0;
		}
		CStdString csMailbox((char *)&m_pParseBuffer[m_dwArguments]);
		csMailbox.Replace('.', '\\');
		m_eInProgress = ImapCommandNone;
		switch(m_msStore->AppendDataToMessage(csMailbox, m_dwAppendingUid, pData, len))
		{
		case CMailStore::SUCCESS:
		    m_dwLiteralLength -= len;
		    if (0 == m_dwLiteralLength)
		    {
			m_dwParseStage = 2;
			m_dwAppendingUid = 0;
		    }
		    m_eInProgress = ImapCommandAppend;
		    break;
	
		case CMailStore::MESSAGE_FILE_OPEN_FAILED:
		    m_msStore->DeleteMessage(csMailbox, m_dwAppendingUid);
		    m_dwAppendingUid = 0;
		    strncpy(m_pResponseText, _T("File Open Failed"), MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_NO;
		    m_dwLiteralLength = 0;
		    break;

		case CMailStore::MESSAGE_FILE_WRITE_FAILED:
		    m_msStore->DeleteMessage(csMailbox, m_dwAppendingUid);
		    m_dwAppendingUid = 0;
		    strncpy(m_pResponseText, _T("File Write Failed"), MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_NO;
		    m_dwLiteralLength = 0;
		    break;
	
		default:
		    m_msStore->DeleteMessage(csMailbox, m_dwAppendingUid);
		    m_dwAppendingUid = 0;
		    strncpy(m_pResponseText, _T("General Failure"), MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_NO;
		    m_dwLiteralLength = 0;
		    break;
		}
	    }
	    else
	    {
		m_eInProgress = ImapCommandNone;
	    }
	    if (ImapCommandNone == m_eInProgress)
	    {
		CStdString response = FormatTaggedResponse(result);
		Send((void *)response.data(), (int)response.size());
	    }
	}
	break;

    case ImapCommandLogin:
	// If I get here, I know that I'm in a login command and a string literal is
	// being sent by the client.  I need to accumulate characters into the parse
	// buffer until I get to astrings read, when I do the work.
	m_eInProgress = ImapCommandNone;
	if (dwDataLen >= m_dwLiteralLength)
	{
	    ++m_dwParseStage;
	    AddToParseBuffer(pData, m_dwLiteralLength);
	    DWORD i = m_dwLiteralLength;
	    m_dwLiteralLength = 0;
	    if ((i < dwDataLen) && (' ' == pData[i]))
	    {
		++i;
	    }
	    if (2 < dwDataLen)
	    {
		// Get rid of the CRLF if I have it
		dwDataLen -= 2;
		pData[dwDataLen] = '\0';  // Make sure it's terminated so strchr et al work
	    }
	    IMAP_RESULTS status = IMAP_OK;
	    while((2 > m_dwParseStage) && (IMAP_OK == status) && (i < dwDataLen))
	    {
		switch (astring(pData, dwDataLen, i, false, NULL))
		{
		case ImapStringGood:
		    ++m_dwParseStage;
		    if ((i < dwDataLen) && (' ' == pData[i]))
		    {
			++i;
		    }
		    break;

		case ImapStringBad:
		    strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
		    status = IMAP_BAD;
		    break;

		case ImapStringPending:
		    m_eInProgress = ImapCommandLogin;
		    strncpy(m_pResponseText, _T("Ready for Literal"), MAX_RESPONSE_STRING_LENGTH);
		    status = IMAP_NOTDONE;
		    break;
		}
	    }
	    if (IMAP_OK == status)
	    {
		if (2 == m_dwParseStage)
		{
		    status = LoginHandlerExecute();
		}
		else
		{
		    strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
		    status = IMAP_BAD;
		}
	    }
	    CStdString response = FormatTaggedResponse(status);
	    Send((void *)response.data(), (int)response.size());
	}
	else
	{
	    AddToParseBuffer(pData, dwDataLen, false);
	    m_dwLiteralLength -= dwDataLen;
	}
	break;

    case ImapCommandSelect:
	m_eInProgress = ImapCommandNone;
	if (dwDataLen >= m_dwLiteralLength)
	{
	    ++m_dwParseStage;
	    AddToParseBuffer(pData, m_dwLiteralLength);
	    DWORD i = m_dwLiteralLength;
	    m_dwLiteralLength = 0;
	    if ((i < dwDataLen) && (' ' == pData[i]))
	    {
		++i;
	    }
	    if (2 < dwDataLen)
	    {
		// Get rid of the CRLF if I have it
		dwDataLen -= 2;
		pData[dwDataLen] = '\0';  // Make sure it's terminated so strchr et al work
	    }
	    m_pResponseText[0] = '\0';
	    CStdString response = FormatTaggedResponse(SelectHandlerExecute(true));
	    Send((void *)response.data(), (int)response.size());
	}
	else
	{
	    AddToParseBuffer(pData, dwDataLen, false);
	    m_dwLiteralLength -= dwDataLen;
	}
	break;

    case ImapCommandExamine:
	m_eInProgress = ImapCommandNone;
	if (dwDataLen >= m_dwLiteralLength)
	{
	    ++m_dwParseStage;
	    AddToParseBuffer(pData, m_dwLiteralLength);
	    DWORD i = m_dwLiteralLength;
	    m_dwLiteralLength = 0;
	    if ((i < dwDataLen) && (' ' == pData[i]))
	    {
		++i;
	    }
	    if (2 < dwDataLen)
	    {
		// Get rid of the CRLF if I have it
		dwDataLen -= 2;
		pData[dwDataLen] = '\0';  // Make sure it's terminated so strchr et al work
	    }
	    m_pResponseText[0] = '\0';
	    CStdString response = FormatTaggedResponse(SelectHandlerExecute(false));
	    Send((void *)response.data(), (int)response.size());
	}
	else
	{
	    AddToParseBuffer(pData, dwDataLen, false);
	    m_dwLiteralLength -= dwDataLen;
	}
	break;
#endif // 0

    case ImapCommandCreate:
	inProgress = ImapCommandNone;
	if (dataLen >= literalLength)
	{
	    ++parseStage;
	    AddToParseBuffer(data, literalLength);
	    size_t i = literalLength;
	    literalLength = 0;
	    if ((i < dataLen) && (' ' == data[i]))
	    {
		++i;
	    }
	    if (2 < dataLen)
	    {
		// Get rid of the CRLF if I have it
		dataLen -= 2;
		data[dataLen] = '\0';  // Make sure it's terminated so strchr et al work
	    }
	    responseText[0] = '\0';
	    std::string response = FormatTaggedResponse(CreateHandlerExecute());
	    s->Send((uint8_t *)response.data(), (int)response.size());
	}
	else
	{
	    AddToParseBuffer(data, dataLen, false);
	    literalLength -= dataLen;
	}
	break;

#if 0
    case ImapCommandDelete:
	m_eInProgress = ImapCommandNone;
	if (dwDataLen >= m_dwLiteralLength)
	{
	    ++m_dwParseStage;
	    AddToParseBuffer(pData, m_dwLiteralLength);
	    DWORD i = m_dwLiteralLength;
	    m_dwLiteralLength = 0;
	    if ((i < dwDataLen) && (' ' == pData[i]))
	    {
		++i;
	    }
	    if (2 < dwDataLen)
	    {
		// Get rid of the CRLF if I have it
		dwDataLen -= 2;
		pData[dwDataLen] = '\0';  // Make sure it's terminated so strchr et al work
	    }
	    m_pResponseText[0] = '\0';
	    CStdString response = FormatTaggedResponse(DeleteHandlerExecute());
	    Send((void *)response.data(), (int)response.size());
	}
	else
	{
	    AddToParseBuffer(pData, dwDataLen, false);
	    m_dwLiteralLength -= dwDataLen;
	}
	break;

    case ImapCommandRename:
	m_eInProgress = ImapCommandNone;
	if (dwDataLen >= m_dwLiteralLength)
	{
	    ++m_dwParseStage;
	    AddToParseBuffer(pData, m_dwLiteralLength);
	    DWORD i = m_dwLiteralLength;
	    m_dwLiteralLength = 0;
	    if ((i < dwDataLen) && (' ' == pData[i]))
	    {
		++i;
	    }
	    if (2 < dwDataLen)
	    {
		// Get rid of the CRLF if I have it
		dwDataLen -= 2;
		pData[dwDataLen] = '\0';  // Make sure it's terminated so strchr et al work
	    }
	    IMAP_RESULTS status = IMAP_OK;
	    while((2 > m_dwParseStage) && (IMAP_OK == status) && (i < dwDataLen))
	    {
		switch (astring(pData, dwDataLen, i, false, NULL))
		{
		case ImapStringGood:
		    ++m_dwParseStage;
		    if ((i < dwDataLen) && (' ' == pData[i]))
		    {
			++i;
		    }
		    break;

		case ImapStringBad:
		    strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
		    status = IMAP_BAD;
		    break;

		case ImapStringPending:
		    m_eInProgress = ImapCommandRename;
		    strncpy(m_pResponseText, _T("Ready for Literal"), MAX_RESPONSE_STRING_LENGTH);
		    status = IMAP_NOTDONE;
		    break;
		}
	    }
	    if (IMAP_OK == status)
	    {
		if (2 == m_dwParseStage)
		{
		    status = RenameHandlerExecute();
		}
		else
		{
		    strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
		    status = IMAP_BAD;
		}
	    }
	    CStdString response = FormatTaggedResponse(status);
	    Send((void *)response.data(), (int)response.size());
	}
	else
	{
	    AddToParseBuffer(pData, dwDataLen, false);
	    m_dwLiteralLength -= dwDataLen;
	}
	break;
#endif // 0

    case ImapCommandSubscribe:
	inProgress = ImapCommandNone;
	if (dataLen >= literalLength)
	{
	    ++parseStage;
	    AddToParseBuffer(data, literalLength);
	    size_t i = literalLength;
	    literalLength = 0;
	    if ((i < dataLen) && (' ' == data[i]))
	    {
		++i;
	    }
	    if (2 < dataLen)
	    {
		// Get rid of the CRLF if I have it
		dataLen -= 2;
		data[dataLen] = '\0';  // Make sure it's terminated so strchr et al work
	    }
	    responseText[0] = '\0';
	    std::string response = FormatTaggedResponse(SubscribeHandlerExecute(true));
	    s->Send((uint8_t *)response.data(), (int)response.size());
	}
	else
	{
	    AddToParseBuffer(data, dataLen, false);
	    literalLength -= dataLen;
	}
	break;

    case ImapCommandUnsubscribe:
	inProgress = ImapCommandNone;
	if (dataLen >= literalLength)
	{
	    ++parseStage;
	    AddToParseBuffer(data, literalLength);
	    size_t i = literalLength;
	    literalLength = 0;
	    if ((i < dataLen) && (' ' == data[i]))
	    {
		++i;
	    }
	    if (2 < dataLen)
	    {
		// Get rid of the CRLF if I have it
		dataLen -= 2;
		data[dataLen] = '\0';  // Make sure it's terminated so strchr et al work
	    }
	    responseText[0] = '\0';
	    std::string response = FormatTaggedResponse(SubscribeHandlerExecute(false));
	    s->Send((uint8_t *)response.data(), (int)response.size());
	}
	else
	{
	    AddToParseBuffer(data, dataLen, false);
	    literalLength -= dataLen;
	}
	break;

    case ImapCommandList:
	inProgress = ImapCommandNone;
	if (dataLen >= literalLength)
	{
	    ++parseStage;
	    AddToParseBuffer(data, literalLength);
	    size_t i = literalLength;
	    literalLength = 0;
	    if ((i < dataLen) && (' ' == data[i]))
	    {
		++i;
	    }
	    if (2 < dataLen)
	    {
		// Get rid of the CRLF if I have it
		dataLen -= 2;
		data[dataLen] = '\0';  // Make sure it's terminated so strchr et al work
	    }
	    IMAP_RESULTS status = IMAP_OK;
	    while((2 > parseStage) && (IMAP_OK == status) && (i < dataLen))
	    {
		enum ImapStringState state;
		if (0 == parseStage)
		{
		    state = astring(data, dataLen, i, false, NULL);
		}
		else
		{
		    state = astring(data, dataLen, i, false, "%*");
		}
		switch (state) 
		{
		case ImapStringGood:
		    ++parseStage;
		    if ((i < dataLen) && (' ' == data[i]))
		    {
			++i;
		    }
		    break;

		case ImapStringBad:
		    strncpy(responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
		    status = IMAP_BAD;
		    break;

		case ImapStringPending:
		    inProgress = ImapCommandList;
		    strncpy(responseText, "Ready for Literal", MAX_RESPONSE_STRING_LENGTH);
		    status = IMAP_NOTDONE;
		    break;
		}
	    }
	    if (IMAP_OK == status)
	    {
		if (2 == parseStage)
		{
		    status = ListHandlerExecute(true);
		}
		else
		{
		    strncpy(responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
		    status = IMAP_BAD;
		}
	    }
	    std::string response = FormatTaggedResponse(status);
	    s->Send((uint8_t *)response.data(), (int)response.size());
	}
	else
	{
	    AddToParseBuffer(data, dataLen, false);
	    literalLength -= dataLen;
	}
	break;

    case ImapCommandLsub:
	inProgress = ImapCommandNone;
	if (dataLen >= literalLength)
	{
	    ++parseStage;
	    AddToParseBuffer(data, literalLength);
	    size_t i = literalLength;
	    literalLength = 0;
	    if ((i < dataLen) && (' ' == data[i]))
	    {
		++i;
	    }
	    if (2 < dataLen)
	    {
		// Get rid of the CRLF if I have it
		dataLen -= 2;
		data[dataLen] = '\0';  // Make sure it's terminated so strchr et al work
	    }
	    IMAP_RESULTS status = IMAP_OK;
	    while((2 > parseStage) && (IMAP_OK == status) && (i < dataLen))
	    {
		enum ImapStringState state;
		if (0 == parseStage)
		{
		    state = astring(data, dataLen, i, false, NULL);
		}
		else
		{
		    state = astring(data, dataLen, i, false, "%*");
		}
		switch (state) 
		{
		case ImapStringGood:
		    ++parseStage;
		    if ((i < dataLen) && (' ' == data[i]))
		    {
			++i;
		    }
		    break;

		case ImapStringBad:
		    strncpy(responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
		    status = IMAP_BAD;
		    break;

		case ImapStringPending:
		    inProgress = ImapCommandLsub;
		    strncpy(responseText, "Ready for Literal", MAX_RESPONSE_STRING_LENGTH);
		    status = IMAP_NOTDONE;
		    break;
		}
	    }
	    if (IMAP_OK == status)
	    {
		if (2 == parseStage)
		{
		    status = ListHandlerExecute(false);
		}
		else
		{
		    strncpy(responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
		    status = IMAP_BAD;
		}
	    }
	    std::string response = FormatTaggedResponse(status);
	    s->Send((uint8_t *)response.data(), (int)response.size());
	}
	else
	{
	    AddToParseBuffer(data, dataLen, false);
	    literalLength -= dataLen;
	}
	break;

#if 0
    case ImapCommandStatus:
	m_eInProgress = ImapCommandNone;
	if (dwDataLen >= m_dwLiteralLength)
	{
	    ++m_dwParseStage;
	    AddToParseBuffer(pData, m_dwLiteralLength);
	    DWORD i = m_dwLiteralLength;
	    m_dwLiteralLength = 0;
	    if (2 < dwDataLen)
	    {
		// Get rid of the CRLF if I have it
		dwDataLen -= 2;
		pData[dwDataLen] = '\0';  // Make sure it's terminated so strchr et al work
	    }
	    m_pResponseText[0] = '\0';
	    CStdString response = FormatTaggedResponse(StatusHandlerExecute(pData, dwDataLen, i));
	    Send((void *)response.data(), (int)response.size());
	}
	else
	{
	    AddToParseBuffer(pData, dwDataLen, false);
	    m_dwLiteralLength -= dwDataLen;
	}
	break;

    case ImapCommandSearch:
	m_eInProgress = ImapCommandNone;
	strncpy(m_pResponseText, _T("Completed"), MAX_RESPONSE_STRING_LENGTH);
	if (0 == m_dwParseStage)
	{
	    IMAP_RESULTS status;

	    // It was receiving the charset astring
	    // for the time being, it's the only charset I recognize
	    if ((dwDataLen > m_dwLiteralLength) && (8 == m_dwLiteralLength))
	    {
		for (int i=0; i<8; ++i)
		{
		    pData[i] = toupper(pData[i]);
		}
		if (11 < dwDataLen)
		{
		    // Get rid of the CRLF if I have it
		    dwDataLen -= 2;
		    pData[dwDataLen] = '\0';  // Make sure it's terminated so strchr et al work
		}
		if (0 == strncmp(_T("US-ASCII "), (char *)pData, 9))
		{
		    DWORD i = 9;
		    status = SearchKeyParse(pData, dwDataLen, i);
		}
		else
		{
		    strncpy(m_pResponseText, _T("Unrecognized Charset"), MAX_RESPONSE_STRING_LENGTH);
		    status = IMAP_BAD;
		}
	    }
	    else
	    {
		strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
		status = IMAP_BAD;
	    }
	    CStdString response = FormatTaggedResponse(status);
	    Send((void *)response.data(), (int)response.size());
	}
	else
	{
	    // It's somewhere in the middle of the search specification
	    if (dwDataLen >= m_dwLiteralLength)
	    {
		AddToParseBuffer(pData, m_dwLiteralLength);
		DWORD i = m_dwLiteralLength;
		m_dwLiteralLength = 0;
		if (2 < dwDataLen)
		{
		    // Get rid of the CRLF if I have it
		    dwDataLen -= 2;
		    pData[dwDataLen] = '\0';  // Make sure it's terminated so strchr et al work
		}
		CStdString response = FormatTaggedResponse(SearchKeyParse(pData, dwDataLen, i));
		Send((void *)response.data(), (int)response.size());
	    }
	    else
	    {
		AddToParseBuffer(pData, dwDataLen, false);
		m_dwLiteralLength -= dwDataLen;
	    }
	}
	break;

    case ImapCommandFetch:
	if (dwDataLen >= m_dwLiteralLength)
	{
	    AddToParseBuffer(pData, m_dwLiteralLength);
	    DWORD i = m_dwLiteralLength;
	    m_dwLiteralLength = 0;
	    if ((i < dwDataLen) && (' ' == pData[i]))
	    {
		++i;
	    }
	    if (2 < dwDataLen)
	    {
		// Get rid of the CRLF if I have it
		dwDataLen -= 2;
		pData[dwDataLen] = '\0';  // Make sure it's terminated so strchr et al work
	    }
	    if (0 == m_dwParseStage)
	    {
		IMAP_RESULTS result = IMAP_OK;
		while ((IMAP_OK == result) && (dwDataLen > i))
		{
		    if((dwDataLen > i) && (' ' == pData[i]))
		    {
			++i;
		    }
		    if((dwDataLen > i) && ('(' == pData[i]))
		    {
			AddToParseBuffer(&pData[i], 1);
			++i;
		    }
		    if ((dwDataLen > i) && (')' != pData[i]))
		    {
			switch (astring(pData, dwDataLen, i, true, NULL))
			{
			case ImapStringGood:
			    result = IMAP_OK;
			    break;

			case ImapStringBad:
			    strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
			    result = IMAP_BAD;
			    break;

			case ImapStringPending:
			    strncpy(m_pResponseText, _T("Ready for Literal"), MAX_RESPONSE_STRING_LENGTH);
			    result = IMAP_NOTDONE;
			    break;

			default:
			    strncpy(m_pResponseText, _T("Failed"), MAX_RESPONSE_STRING_LENGTH);
			    result = IMAP_BAD;
			    break;
			}				
		    }
		    if ((dwDataLen > i) && (')' == pData[i]))
		    {
			AddToParseBuffer(&pData[i], 1);
			++i;
		    }
		}
		if (IMAP_OK == result)
		{
		    m_eInProgress = ImapCommandNone;
		    result = FetchHandlerExecute(m_bUsesUid);
		}
		CStdString response = FormatTaggedResponse(result);
		Send((void *)response.data(), (int)response.size());
	    }
	    else
	    {
		IMAP_RESULTS result = IMAP_OK;
		while ((IMAP_OK == result) && (dwDataLen > i) && (')' != pData[i]) && (0 < m_dwParseStage))
		{
		    if((dwDataLen > i) && (' ' == pData[i]))
		    {
			++i;
		    }
		    if((dwDataLen > i) && ('(' == pData[i]))
		    {
			AddToParseBuffer(&pData[i], 1);
			++i;
			++m_dwParseStage;
		    }
		    switch (astring(pData, dwDataLen, i, true, NULL))
		    {
		    case ImapStringGood:
			result = IMAP_OK;
			break;

		    case ImapStringBad:
			strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_BAD;
			break;

		    case ImapStringPending:
			strncpy(m_pResponseText, _T("Ready for Literal"), MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_NOTDONE;
			break;

		    default:
			strncpy(m_pResponseText, _T("Failed"), MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_BAD;
			break;
		    }
		    if ((dwDataLen > i) && (')' == pData[i]))
		    {
			AddToParseBuffer(&pData[i], 1);
			++i;
			--m_dwParseStage;
		    }
		}
		if ((IMAP_OK == result) && (0 == m_dwParseStage))
		{
		    if (dwDataLen == i)
		    {
			result = FetchHandlerExecute(m_bUsesUid);
		    }
		    else
		    {
			strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_BAD;
		    }
		}
		if (IMAP_NOTDONE != result)
		{
		    m_eInProgress = ImapCommandNone;
		    CStdString response = FormatTaggedResponse(FetchHandlerExecute(m_bUsesUid));
		    Send((void *)response.data(), (int)response.size());
		}
	    }
	}
	else
	{
	    AddToParseBuffer(pData, dwDataLen, false);
	    m_dwLiteralLength -= dwDataLen;
	}
	break;

    case ImapCommandCopy:
	m_eInProgress = ImapCommandNone;
	if (dwDataLen >= m_dwLiteralLength)
	{
	    ++m_dwParseStage;
	    AddToParseBuffer(pData, m_dwLiteralLength);
	    DWORD i = m_dwLiteralLength;
	    m_dwLiteralLength = 0;
	    if ((i < dwDataLen) && (' ' == pData[i]))
	    {
		++i;
	    }
	    if (2 < dwDataLen)
	    {
		// Get rid of the CRLF if I have it
		dwDataLen -= 2;
		pData[dwDataLen] = '\0';  // Make sure it's terminated so strchr et al work
	    }
	    m_pResponseText[0] = '\0';
	    CStdString response = FormatTaggedResponse(CopyHandlerExecute(m_bUsesUid));
	    Send((void *)response.data(), (int)response.size());
	}
	else
	{
	    AddToParseBuffer(pData, dwDataLen, false);
	    m_dwLiteralLength -= dwDataLen;
	}
	break;
#endif // 0
    }
    if (ImapCommandNone == inProgress)
    {
	literalLength = 0;
	if (NULL != parseBuffer)
	{
	    delete[] parseBuffer;
	    parseBuffer = NULL;
	    parseBuffLen = 0;
	}
    }
    return result;
}


/*--------------------------------------------------------------------------------------*/
/* ImapSession::UnimplementedHandler													*/
/*--------------------------------------------------------------------------------------*/
/* Gets called whenever the system doesn't know what else to do.  It signals the client */
/* that the command was not recognized													*/
/*--------------------------------------------------------------------------------------*/
IMAP_RESULTS ImapSession::UnimplementedHandler()
{
    strncpy(responseText, "Unrecognized Command",  MAX_RESPONSE_STRING_LENGTH);
    return IMAP_BAD;
}


std::string ImapSession::BuildCapabilityString()
{
    std::string capability;
#if 0 // SYZYGY AUTH ANONYMOUS LOGINDISABLED all are determined by the server state as much as the session
    capability.Format("CAPABILITY IMAP4rev1 CHILDREN NAMESPACE AUTH=PLAIN%s%s",
		      m_AnonymousEnabled ? " AUTH=ANONYMOUS" : "",
		      m_LoginDisabled ? " LOGINDISABLED" : "");
#else // !0
    capability = "CAPABILITY IMAP4rev1 CHILDREN NAMESPACE AUTH=PLAIN";
#endif // !0

    return capability;
}


/*
 * The capability handler looks simple, but there's some complexity lurking
 * there.  For starters, I'm going to return a constant string with not very
 * much in it.  However, later on the string might change depending on the
 * context.  Further, since the capability string might be sent by multiple
 * different commands, that actual work will be done by a worker function.
 */
IMAP_RESULTS ImapSession::CapabilityHandler(uint8_t *pData, size_t dwDataLen, size_t &r_dwParsingAt)
{
    std::string response("* ");
    response += BuildCapabilityString() + "\r\n";
    s->Send((uint8_t *)response.c_str(), response.size());
    return IMAP_OK;
}

/*
 * The NOOP may be used by the server to trigger things like checking for
 * new messages although the server doesn't have to wait for this to do
 * things like that
 */
IMAP_RESULTS ImapSession::NoopHandler(uint8_t *data, size_t dataLen, size_t &parsingAt)
{
    // I want to re-read the cached data if I'm in the selected state and
    // either the number of messages or the UIDNEXT value changes
    if (ImapSelected == state)
    {
	if ((currentNextUid != store->GetSerialNumber()) ||
	    (currentMessageCount != store->MailboxMessageCount(store->GetMailboxUserPath())))
	{
	    NUMBER_LIST purgedMessages;
	    std::string untagged;

	    if (MailStore::SUCCESS == store->MailboxUpdateStats(&purgedMessages))
	    {
		for (NUMBER_LIST::iterator i=purgedMessages.begin() ; i!= purgedMessages.end(); ++i)
		{
		    int message_number;
		    std::string untagged;

		    message_number = *i;
		    // SYZYGY FORMAT
		    // untagged.Format(_T("* %d EXPUNGE\r\n"), message_number);
		    s->Send((uint8_t *)untagged.c_str(), untagged.size());
		}
	    }
	    if (currentNextUid != store->GetSerialNumber())
	    {
		currentNextUid = store->GetSerialNumber();
		// SYZYGY FORMAT
		// untagged.Format(_T("* OK [UIDNEXT %d]\r\n"), m_dwCurrentNextUid);
		s->Send((uint8_t *)untagged.c_str(), untagged.size());
	    }
	    if (currentMessageCount != store->MailboxMessageCount())
	    {
		currentMessageCount = store->MailboxMessageCount();
		// SYZYGY FORMAT
		// untagged.Format(_T("* %d EXISTS\r\n"), m_dwCurrentMessageCount);
		s->Send((uint8_t *)untagged.c_str(), untagged.size());
	    }
	    if (currentRecentCount != store->MailboxRecentCount())
	    {
		currentRecentCount = store->MailboxRecentCount();
		// SYZYGY FORMAT
		// untagged.Format(_T("* %d RECENT\r\n"), m_dwCurrentRecentCount);
		s->Send((uint8_t *)untagged.c_str(), untagged.size());
	    }
	    if (currentUnseen != store->MailboxFirstUnseen())
	    {
		currentUnseen = store->MailboxFirstUnseen();
		if (0 < currentUnseen)
		{
		    // SYZYGY FORMAT
		    // untagged.Format(_T("* OK [UNSEEN %d]\r\n"), m_dwCurrentUnseen);
		    s->Send((uint8_t *)untagged.c_str(), untagged.size());
		}
	    }
	    if (currentUidValidity != store->GetUidValidityNumber())
	    {
		currentUidValidity = store->GetUidValidityNumber();
		// SYZYGY FORMAT
		// untagged.Format(_T("* OK [UIDVALIDITY %d]\r\n"), m_dwCurrentUidValidity);
		s->Send((uint8_t *)untagged.c_str(), untagged.size());
	    }
	}
    }

    return IMAP_OK;
}

/*
 * The LOGOUT indicates that the session may be closed.  The main command
 * processor gets the word about this because the state is set to "4" which
 * means "logout state"
 */
IMAP_RESULTS ImapSession::LogoutHandler(uint8_t *pData, size_t dwDataLen, size_t &r_dwParsingAt)
{
    // If the mailbox is open, close it
    // In IMAP, deleted messages are always purged before a close
    if (ImapSelected == state)
    {
	store->MailboxClose();
    }
    if (ImapNotAuthenticated < state)
    {
	if (NULL != store)
	{
	    delete store;
	    store = NULL;
	}
    }
    state = ImapLogoff;
    std::string bye = "* BYE IMAP4rev1 server closing\r\n";
    s->Send((uint8_t *)bye.data(), bye.size());
    return IMAP_OK;
}

IMAP_RESULTS ImapSession::StarttlsHandler(uint8_t *data, size_t dataLen, size_t &parsingAt)
{
    return UnimplementedHandler();
}

IMAP_RESULTS ImapSession::AuthenticateHandler(uint8_t *data, size_t dataLen, size_t &parsingAt)
{
    IMAP_RESULTS result; 
    userData = server->GetUserInfo((char *)&parseBuffer[arguments]);
    atom(data, dataLen, parsingAt);
    auth = SaslChooser(this, (char *)&parseBuffer[arguments]);
    if (NULL != auth)
    {
	inProgress = ImapCommandAuthenticate;
	auth->SendChallenge(responseCode);
	responseText[0] = '\0';
	result = IMAP_NOTDONE;
    }
    else
    {
	strncpy(responseText, "Unrecognized Authentication Method", MAX_RESPONSE_STRING_LENGTH);
	result = IMAP_NO;
    }
    return result;
}

IMAP_RESULTS ImapSession::LoginHandlerExecute()
{
    IMAP_RESULTS result;

    userData = server->GetUserInfo((char *)&parseBuffer[arguments]);
    bool v = userData->CheckCredentials((char *)&parseBuffer[arguments+(strlen((char *)&parseBuffer[arguments])+1)]);
    if (v)
    {
	// SYZYGY LOG
	// Log("Client %u logged-in user %s %lu\n", m_dwClientNumber, m_pUser->m_szUsername.c_str(), m_pUser->m_nUserID);
	if (NULL == store)
	{
	    store = server->GetMailStore(userData);
	}
	store->CreateMailbox("INBOX");
	// store->FixMailstore(); // I only un-comment this, if I have reason to believe that mailboxes are broken

	// SYZYGY LOG
	// Log("User %lu logged in\n", m_pUser->m_nUserID);
	state = ImapAuthenticated;
	result = IMAP_OK;
    }
    else
    {
	delete userData;
	userData = NULL;
	strncpy(responseText, "Authentication Failed", MAX_RESPONSE_STRING_LENGTH);
	result = IMAP_NO_WITH_PAUSE;
    }
    return result;
}

/*
 * Okay, the login command's use is officially deprecated.  Instead, you're supposed
 * to use the AUTHENTICATE command with some SASL mechanism.  Since I'm trying to
 * duplicate the functionality of SimDesk's current IMAP server, I will include it,
 * but I'll also include a check of the login_disabled flag, which will set whether or
 * not this command is accepted by the command processor
 */
IMAP_RESULTS ImapSession::LoginHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt)
{
    IMAP_RESULTS result = IMAP_OK;
    if (m_LoginDisabled)
    {
	strncpy(responseText, "Login Disabled", MAX_RESPONSE_STRING_LENGTH);
	return IMAP_NO;
    }
    else
    {
	parseStage = 0;
	do
	{
	    switch (astring(data, dataLen, parsingAt, false, NULL))
	    {
	    case ImapStringGood:
		++parseStage;
		if ((parsingAt < dataLen) && (' ' == data[parsingAt]))
		{
		    ++parsingAt;
		}
		break;

	    case ImapStringBad:
		result = IMAP_BAD;
		break;

	    case ImapStringPending:
		result = IMAP_NOTDONE;
		break;
	    }
	} while((IMAP_OK == result) && (parsingAt < dataLen));
    }
    switch(result)
    {
    case IMAP_OK:
	if (2 == parseStage)
	{
	    result = LoginHandlerExecute();
	}
	else
	{
	    strncpy(responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	    result = IMAP_BAD;
	}
	break;

    case IMAP_NOTDONE:
	inProgress = ImapCommandLogin;
	strncpy(responseText, "Ready for Literal", MAX_RESPONSE_STRING_LENGTH);
	break;

    case IMAP_BAD:
	strncpy(responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	break;

    default:
	strncpy(responseText, "Failed", MAX_RESPONSE_STRING_LENGTH);
	break;
    }
    return result;
}

#if 0
IMAP_RESULTS ImapSession::NamespaceHandler(byte *pData, DWORD dwDataLen, DWORD &r_dwParsingAt)
{
    char *s = _T("* NAMESPACE ((\"\" \".\")) NIL NIL\r\n");
    Send(s, (int)strlen(s));
    return IMAP_OK;
}


void ImapSession::SendSelectData(const CStdString &mailbox, bool bIsReadWrite)
{
    m_dwCurrentNextUid = m_msStore->GetSerialNumber();
    m_dwCurrentUidValidity = m_msStore->GetUidValidityNumber();
    m_dwCurrentMessageCount = m_msStore->MailboxMessageCount();
    m_dwCurrentRecentCount = m_msStore->MailboxRecentCount();
    m_dwCurrentUnseen = m_msStore->MailboxFirstUnseen();

    CStdString csResponse;
    // The select commands sends
    // an untagged flags response
    csResponse = _T("* FLAGS (\\Draft \\Answered \\Flagged \\Deleted \\Seen)\r\n");
    Send((void *)csResponse.c_str(), csResponse.GetLength());
    // an untagged exists response
    // the exists response needs the number of messages in the message base
    csResponse.Format(_T("* %d EXISTS\r\n"), m_dwCurrentMessageCount);
    Send((void *)csResponse.c_str(), csResponse.GetLength());
    // an untagged recent response
    // the recent response needs the count of recent messages
    csResponse.Format(_T("* %d RECENT\r\n"), m_dwCurrentRecentCount);
    Send((void *)csResponse.c_str(), csResponse.GetLength());
    // an untagged okay unseen response
    if (0 < m_dwCurrentUnseen)
    {
	csResponse.Format(_T("* OK [UNSEEN %d]\r\n"), m_dwCurrentUnseen); 
	Send((void *)csResponse.c_str(), csResponse.GetLength());
    }
    // an untagged okay permanent flags response
    if (bIsReadWrite)
    {
	csResponse = _T("* OK [PERMANENTFLAGS (\\Draft \\Answered \\Flagged \\Deleted \\Seen)]\r\n");
	Send((void *)csResponse.c_str(), csResponse.GetLength());
    }
    else
    {
	csResponse = _T("* OK [PERMANENTFLAGS ()]\r\n");
	Send((void *)csResponse.c_str(), csResponse.GetLength());
    }
    // an untagged okay uidnext response
    csResponse.Format(_T("* OK [UIDNEXT %d]\r\n"), m_dwCurrentNextUid);
    Send((void *)csResponse.c_str(), csResponse.GetLength());
    // an untagged okay uidvalidity response
    csResponse.Format(_T("* OK [UIDVALIDITY %d]\r\n"), m_dwCurrentUidValidity);
    Send((void *)csResponse.c_str(), csResponse.GetLength());
}

IMAP_RESULTS ImapSession::SelectHandlerExecute(bool bOpenReadWrite)
{
    IMAP_RESULTS result = IMAP_OK;

    if (ImapSelected == m_eState)
    {
	// If the mailbox is open, close it
	m_msStore->MailboxClose();
    }
    m_eState = ImapAuthenticated;
    CStdString mailbox = (char *)&m_pParseBuffer[m_dwArguments];
    mailbox.Replace('.', '\\');
    CMailStore::MAIL_STORE_RESULT r = m_msStore->MailboxOpen(mailbox, bOpenReadWrite);
    switch(r)
    {
    case CMailStore::SUCCESS:
	SendSelectData(mailbox, bOpenReadWrite);
	if (bOpenReadWrite)
	{
	    strncpy(m_pResponseCode, _T("[READ-WRITE]"), MAX_RESPONSE_STRING_LENGTH);
	}
	else
	{
	    strncpy(m_pResponseCode, _T("[READ-ONLY]"), MAX_RESPONSE_STRING_LENGTH);
	}
	result = IMAP_OK;
	m_eState = ImapSelected;
	break;

    case CMailStore::MAILBOX_NOT_SELECTABLE:
    case CMailStore::MAILBOX_DOES_NOT_EXIST:
	// result = ((CStdString)"NO ") + command.ToUpper() + " Mailbox \"" + mailbox + "\" does not exist or is not selectable";
	strncpy(m_pResponseText, _T("Mailbox not selectable"), MAX_RESPONSE_STRING_LENGTH);
	result = IMAP_NO;
	break;

    default:
	strncpy(m_pResponseText, _T("General Error Opening Mailbox"), MAX_RESPONSE_STRING_LENGTH);
	result = IMAP_NO;
	break;
    }

    return result;
}

IMAP_RESULTS ImapSession::SelectHandler(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt)
{
    IMAP_RESULTS result = IMAP_OK;

    switch (astring(pData, dwDataLen, r_dwParsingAt, true, NULL))
    {
    case ImapStringGood:
	result = SelectHandlerExecute(true);
	break;

    case ImapStringBad:
	strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
	result = IMAP_BAD;
	break;

    case ImapStringPending:
	result = IMAP_NOTDONE;
	m_eInProgress = ImapCommandSelect;
	strncpy(m_pResponseText, _T("Ready for Literal"), MAX_RESPONSE_STRING_LENGTH);
	break;

    default:
	strncpy(m_pResponseText, _T("Failed"), MAX_RESPONSE_STRING_LENGTH);
	break;
    }
    return result;
}

IMAP_RESULTS ImapSession::ExamineHandler(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt) {
    IMAP_RESULTS result = IMAP_OK;

    switch (astring(pData, dwDataLen, r_dwParsingAt, true, NULL))
    {
    case ImapStringGood:
	result = SelectHandlerExecute(false);
	break;

    case ImapStringBad:
	strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
	result = IMAP_BAD;
	break;

    case ImapStringPending:
	result = IMAP_NOTDONE;
	m_eInProgress = ImapCommandExamine;
	strncpy(m_pResponseText, _T("Ready for Literal"), MAX_RESPONSE_STRING_LENGTH);
	break;

    default:
	strncpy(m_pResponseText, _T("Failed"), MAX_RESPONSE_STRING_LENGTH);
	break;
    }
    return result;
}
#endif // 0

IMAP_RESULTS ImapSession::CreateHandlerExecute()
{
    IMAP_RESULTS result = IMAP_OK;
    std::string mailbox((char *)&parseBuffer[arguments]);
    MailStore::MAIL_STORE_RESULT r = store->CreateMailbox(mailbox);
    switch(r)
    {
    case MailStore::SUCCESS:
	break;

    case MailStore::MAILBOX_ALREADY_EXISTS:
	strncpy(responseText, "Mailbox already exists", MAX_RESPONSE_STRING_LENGTH);
	result = IMAP_NO;
	break;

    default:
	strncpy(responseText, "General Error Creating Mailbox", MAX_RESPONSE_STRING_LENGTH);
	result = IMAP_NO;
	break;
    }
    return result;
}

IMAP_RESULTS ImapSession::CreateHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt) {
    IMAP_RESULTS result = IMAP_OK;

    switch (astring(data, dataLen, parsingAt, false, NULL))
    {
    case ImapStringGood:
	result = CreateHandlerExecute();
	break;

    case ImapStringBad:
	strncpy(responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	result = IMAP_BAD;
	break;

    case ImapStringPending:
	result = IMAP_NOTDONE;
	inProgress = ImapCommandCreate;
	strncpy(responseText, "Ready for Literal", MAX_RESPONSE_STRING_LENGTH);
	break;

    default:
	strncpy(responseText, "Failed", MAX_RESPONSE_STRING_LENGTH);
	break;
    }
    return result;
}

#if 0
IMAP_RESULTS ImapSession::DeleteHandlerExecute()
{
    IMAP_RESULTS result = IMAP_OK;
    CStdString mailbox((char *)&m_pParseBuffer[m_dwArguments]);
    mailbox.Replace('.', '\\');
    if(!mailbox.Equals(_T("INBOX")))
    {
	CMailStore::MAIL_STORE_RESULT r = m_msStore->DeleteMailbox(mailbox);
	switch(r)
	{
	case CMailStore::SUCCESS:
	    break;

	case CMailStore::MAILBOX_UNABLE_TO_UPDATE_CATALOG:
	    result = IMAP_NO;
	    strncpy(m_pResponseText, _T("Database Error Deleting Mailbox"), MAX_RESPONSE_STRING_LENGTH);
	    break;

	case CMailStore::MAILBOX_DOES_NOT_EXIST:
	    result = IMAP_NO;
	    strncpy(m_pResponseText, _T("Mailbox Does Not Exist"), MAX_RESPONSE_STRING_LENGTH);
	    break;

	default:
	    result = IMAP_NO;
	    strncpy(m_pResponseText, _T("General Error Deleting Mailbox"), MAX_RESPONSE_STRING_LENGTH);
	    break;
	}
    }
    else
    {
	result = IMAP_NO;
	strncpy(m_pResponseText, _T("Cannot Delete INBOX"), MAX_RESPONSE_STRING_LENGTH);
    }
    return result;
}

IMAP_RESULTS ImapSession::DeleteHandler(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt) {
    IMAP_RESULTS result = IMAP_OK;

    switch (astring(pData, dwDataLen, r_dwParsingAt, true, NULL))
    {
    case ImapStringGood:
	result = DeleteHandlerExecute();
	break;

    case ImapStringBad:
	strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
	result = IMAP_BAD;
	break;

    case ImapStringPending:
	result = IMAP_NOTDONE;
	m_eInProgress = ImapCommandDelete;
	strncpy(m_pResponseText, _T("Ready for Literal"), MAX_RESPONSE_STRING_LENGTH);
	break;

    default:
	strncpy(m_pResponseText, _T("Failed"), MAX_RESPONSE_STRING_LENGTH);
	break;
    }
    return result;
}

IMAP_RESULTS ImapSession::RenameHandlerExecute()
{
    IMAP_RESULTS result = IMAP_OK;

    CStdString source((char *)&m_pParseBuffer[m_dwArguments]);
    CStdString destination((char *)&m_pParseBuffer[m_dwArguments+(strlen((char *)&m_pParseBuffer[m_dwArguments])+1)]);

    source.Replace('.', '\\');
    destination.Replace('.', '\\');
    switch(m_msStore->RenameMailbox(source, destination))
    {
    case CMailStore::SUCCESS:
	break;

    case CMailStore::MAILBOX_DOES_NOT_EXIST:
	result = IMAP_NO;
	strncpy(m_pResponseText, _T("Source Mailbox Does Not Exist"), MAX_RESPONSE_STRING_LENGTH);
	break;

    case CMailStore::MAILBOX_ALREADY_EXISTS:
	result = IMAP_NO;
	strncpy(m_pResponseText, _T("Destination Mailbox Already Exists"), MAX_RESPONSE_STRING_LENGTH);
	break;

    default:
	result = IMAP_NO;
	strncpy(m_pResponseText, _T("General Error Renaming Mailbox"), MAX_RESPONSE_STRING_LENGTH);
	break;
    }
    return result;
}

IMAP_RESULTS ImapSession::RenameHandler(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt)
{
    IMAP_RESULTS result = IMAP_OK;
    m_dwParseStage = 0;
    do
    {
	switch (astring(pData, dwDataLen, r_dwParsingAt, false, NULL))
	{
	case ImapStringGood:
	    ++m_dwParseStage;
	    if ((r_dwParsingAt < dwDataLen) && (' ' == pData[r_dwParsingAt]))
	    {
		++r_dwParsingAt;
	    }
	    break;

	case ImapStringBad:
	    result = IMAP_BAD;
	    break;

	case ImapStringPending:
	    result = IMAP_NOTDONE;
	    break;
	}
    } while((IMAP_OK == result) && (r_dwParsingAt < dwDataLen));
    switch(result)
    {
    case IMAP_OK:
	if (2 == m_dwParseStage)
	{
	    result = RenameHandlerExecute();
	}
	else
	{
	    strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
	    result = IMAP_BAD;
	}
	break;

    case IMAP_NOTDONE:
	m_eInProgress = ImapCommandRename;
	strncpy(m_pResponseText, _T("Ready for Literal"), MAX_RESPONSE_STRING_LENGTH);
	break;

    case IMAP_BAD:
	strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
	break;

    default:
	strncpy(m_pResponseText, _T("Failed"), MAX_RESPONSE_STRING_LENGTH);
	break;
    }
    return result;
}
#endif // 0

IMAP_RESULTS ImapSession::SubscribeHandlerExecute(bool isSubscribe)
{
    IMAP_RESULTS result = IMAP_OK;
    std::string mailbox((char *)&parseBuffer[arguments]);
    MailStore::MAIL_STORE_RESULT r = store->SubscribeMailbox(mailbox, isSubscribe);
    switch(r)
    {
    case MailStore::SUCCESS:
	break;

    case MailStore::MAILBOX_DOES_NOT_EXIST:
	strncpy(responseText, "Mailbox Does Not Exist", MAX_RESPONSE_STRING_LENGTH);
	strncpy(responseCode, "[TRYCREATE]", MAX_RESPONSE_STRING_LENGTH);
	result = IMAP_NO;
	break;

    case MailStore::MAILBOX_ALREADY_SUBSCRIBED:
	strncpy(responseText, "Already Subscribed to Mailbox ", MAX_RESPONSE_STRING_LENGTH);
	strncat(responseText, mailbox.c_str(), MAX_RESPONSE_STRING_LENGTH - strlen(responseText));
	result = IMAP_NO;
	break;

    case MailStore::MAILBOX_NOT_SUBSCRIBED:
	strncpy(responseText, "Not Subscribed to Mailbox ", MAX_RESPONSE_STRING_LENGTH - strlen(responseText));
	strncat(responseText, mailbox.c_str(), MAX_RESPONSE_STRING_LENGTH);
	result = IMAP_NO;
	break;

    default:
	strncpy(responseText, "General Error Subscribing Mailbox", MAX_RESPONSE_STRING_LENGTH);
	result = IMAP_NO;
	break;
    }
    return result;
}

IMAP_RESULTS ImapSession::SubscribeHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt)
{
    IMAP_RESULTS result = IMAP_OK;

    switch (astring(data, dataLen, parsingAt, false, NULL))
    {
    case ImapStringGood:
	result = SubscribeHandlerExecute(true);
	break;

    case ImapStringBad:
	strncpy(responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	result = IMAP_BAD;
	break;

    case ImapStringPending:
	result = IMAP_NOTDONE;
	inProgress = ImapCommandSubscribe;
	strncpy(responseText, "Ready for Literal", MAX_RESPONSE_STRING_LENGTH);
	break;

    default:
	strncpy(responseText, "Failed", MAX_RESPONSE_STRING_LENGTH);
	break;
    }
    return result;
}

IMAP_RESULTS ImapSession::UnsubscribeHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt)
{
    IMAP_RESULTS result = IMAP_OK;

    switch (astring(data, dataLen, parsingAt, false, NULL))
    {
    case ImapStringGood:
	result = SubscribeHandlerExecute(false);
	break;

    case ImapStringBad:
	strncpy(responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	result = IMAP_BAD;
	break;

    case ImapStringPending:
	result = IMAP_NOTDONE;
	inProgress = ImapCommandUnsubscribe;
	strncpy(responseText, "Ready for Literal", MAX_RESPONSE_STRING_LENGTH);
	break;

    default:
	strncpy(responseText, "Failed", MAX_RESPONSE_STRING_LENGTH);
	break;
    }
    return result;
}

static std::string ImapQuoteString(const std::string &to_be_quoted)
{
    std::string result("\"");
    for (int i=0; i<to_be_quoted.size(); ++i)
    {
	if ('\\' == to_be_quoted[i])
	{
	    result += '\\';
	}
	result += to_be_quoted[i];
    }
    result += '"';
    return result;
}

// Generate the mailbox flags for the list command here
static std::string GenMailboxFlags(const uint32_t attr)
{
    std::string result;

    if (0 != (attr & MailStore::IMAP_MBOX_NOSELECT))
    {
	result = "(\\Noselect ";
    }
    else
    {
	if (0 != (attr & MailStore::IMAP_MBOX_MARKED))
	{
	    result += "(\\Marked ";
	}
	else
	{
	    if (0 != (attr & MailStore::IMAP_MBOX_UNMARKED))
	    {
		result += "(\\UnMarked ";
	    }
	    else
	    {
		result += "(";
	    }
	}
    }
    if (0 != (attr & MailStore::IMAP_MBOX_NOINFERIORS))
    {
	result += "\\NoInferiors)";
    }
    else
    {
	if (0 != (attr & MailStore::IMAP_MBOX_HASCHILDREN))
	{
	    result += "\\HasChildren)";
	}
	else
	{
	    result += "\\HasNoChildren)";
	}
    }
    return result;
}

IMAP_RESULTS ImapSession::ListHandlerExecute(bool listAll)
{
    IMAP_RESULTS result = IMAP_OK;

    const char *reference = (char *)&parseBuffer[arguments];
    const char *mailbox = (char *)&parseBuffer[arguments+(strlen((char *)&parseBuffer[arguments])+1)];

    if (('\0' == reference[0]) && ('\0' == mailbox[0]))
    {
	if (listAll)
	{
	    // It's a request to return the base and the hierarchy delimiter
	    std::string response("* LIST () \"/\" \"\"\r\n");
	    s->Send((uint8_t *)response.c_str(), response.size());
	}
    }
    else
    {
	/*
	 * What I think I want to do here is combine the strings by checking the last character of the 
	 * reference for '.', adding it if it's not there, and then just combining the two strings.
	 */
	/*
	 * SYZYGY -- that's dumb.  The namespace should actually be separate.  However, I'm not going
	 * SYZYGY -- to fix that right now.  I can do it at any time.  However, the name spaces need to
	 * be defined elsewhere, like in the server class
	 */
	MAILBOX_LIST mailboxList;
	store->BuildMailboxList(reference, mailbox, &mailboxList, listAll);
	std::string response;
	for (MAILBOX_LIST::const_iterator i = mailboxList.begin(); i != mailboxList.end(); ++i)
	{
	    if (listAll)
	    {
		response = "* LIST ";
	    }
	    else
	    {
		response = "* LSUB ";
	    }
	    MAILBOX_NAME which = *i;
	    response += GenMailboxFlags(i->attributes) + " \"/\" " + ImapQuoteString(i->name) + "\r\n";
	    s->Send((uint8_t *)response.c_str(), response.size());
	}
    }
    return IMAP_OK;
}

IMAP_RESULTS ImapSession::ListHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt)
{
    IMAP_RESULTS result = IMAP_OK;
    parseStage = 0;
    do
    {
	enum ImapStringState state;
	if (0 == parseStage)
	{
	    state = astring(data, dataLen, parsingAt, false, NULL);
	}
	else
	{
	    state = astring(data, dataLen, parsingAt, false, "%*");
	}
	switch (state) 
	{
	case ImapStringGood:
	    ++parseStage;
	    if ((parsingAt < dataLen) && (' ' == data[parsingAt]))
	    {
		++parsingAt;
	    }
	    break;

	case ImapStringBad:
	    result = IMAP_BAD;
	    break;

	case ImapStringPending:
	    result = IMAP_NOTDONE;
	    break;
	}
    } while((IMAP_OK == result) && (parsingAt < dataLen));
    switch(result)
    {
    case IMAP_OK:
	if (2 == parseStage)
	{
	    result = ListHandlerExecute(true);
	}
	else
	{
	    strncpy(responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	    result = IMAP_BAD;
	}
	break;

    case IMAP_NOTDONE:
	inProgress = ImapCommandList;
	strncpy(responseText, "Ready for Literal", MAX_RESPONSE_STRING_LENGTH);
	break;

    case IMAP_BAD:
	strncpy(responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	break;

    default:
	strncpy(responseText, "Failed", MAX_RESPONSE_STRING_LENGTH);
	break;
    }
    return result;
}

IMAP_RESULTS ImapSession::LsubHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt)
{
    IMAP_RESULTS result = IMAP_OK;
    parseStage = 0;
    do
    {
	enum ImapStringState state;
	if (0 == parseStage)
	{
	    state = astring(data, dataLen, parsingAt, false, NULL);
	}
	else
	{
	    state = astring(data, dataLen, parsingAt, false, "%*");
	}
	switch (state)
	{
	case ImapStringGood:
	    ++parseStage;
	    if ((parsingAt < dataLen) && (' ' == data[parsingAt]))
	    {
		++parsingAt;
	    }
	    break;

	case ImapStringBad:
	    result = IMAP_BAD;
	    break;

	case ImapStringPending:
	    result = IMAP_NOTDONE;
	    break;
	}
    } while((IMAP_OK == result) && (parsingAt < dataLen));
    switch(result)
    {
    case IMAP_OK:
	if (2 == parseStage)
	{
	    result = ListHandlerExecute(false);
	}
	else
	{
	    strncpy(responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	    result = IMAP_BAD;
	}
	break;

    case IMAP_NOTDONE:
	inProgress = ImapCommandLsub;
	strncpy(responseText, "Ready for Literal", MAX_RESPONSE_STRING_LENGTH);
	break;

    case IMAP_BAD:
	strncpy(responseText, "Malformed Command", MAX_RESPONSE_STRING_LENGTH);
	break;

    default:
	strncpy(responseText, "Failed", MAX_RESPONSE_STRING_LENGTH);
	break;
    }
    return result;
}

#if 0
IMAP_RESULTS ImapSession::StatusHandlerExecute(byte *pData, const DWORD dwDataLen, DWORD dwParsingAt)
{
    IMAP_RESULTS result = IMAP_OK;

    CStdString mailbox((char *)&m_pParseBuffer[m_dwArguments]);
    CStdString mailboxExternal = mailbox;
    mailbox.Replace('.', '\\');
	
    DWORD flagset = 0;

    // At this point, I've seen the mailbox name.  The next stuff are all in the pData buffer.
    // I expect to see a space then a paren
    if ((2 < (dwDataLen - dwParsingAt)) && (' ' == pData[dwParsingAt]) && ('(' == pData[dwParsingAt+1]))
    {
	dwParsingAt += 2;
    	while (dwParsingAt < (dwDataLen-1))
    	{
	    DWORD len, next;
	    char *p = strchr((char *)&pData[dwParsingAt], ' ');
	    if (NULL != p)
	    {
		len = (DWORD) (p - ((char *)&pData[dwParsingAt]));
		next = dwParsingAt + len + 1; // This one needs to skip the space
	    }
	    else
	    {
		len = dwDataLen - dwParsingAt - 1;
		next = dwParsingAt + len;     // this one needs to keep the parenthesis
	    }
	    CStdString s((char *)&pData[dwParsingAt], len);
	    STATUS_SYMBOL_T::iterator i = sStatusSymbolTable.find(s);
	    if (i != sStatusSymbolTable.end())
	    {
		flagset |= i->second;
	    }
	    else
	    {
		result = IMAP_BAD;
		// I don't need to see any more, it's all bogus
		break;
	    }
	    dwParsingAt = next;
    	}
    }
    if ((result == IMAP_OK) && (')' == pData[dwParsingAt]))
    {
	int messages_value = 0, recent_value = 0, uidnext_value = 0;
	int uidvalidity_value = 0, unseen_value = 0;

	CStdString response(_T("* STATUS "));
	response += mailboxExternal + _T(" ");
	char separator = '(';
	if (flagset & SAV_MESSAGES)
	{
	    messages_value = m_msStore->MailboxMessageCount(mailbox);
	    response.AppendFormat(_T("%cMESSAGES %d"), separator, messages_value);
	    separator = ' ';
	} 
	if (flagset & SAV_RECENT) 
	{
	    recent_value = m_msStore->MailboxRecentCount(mailbox);
	    response.AppendFormat(_T("%cRECENT %d"), separator, recent_value);
	    separator = ' ';
	}
	if (flagset & SAV_UIDNEXT)
	{
	    uidnext_value = m_msStore->GetSerialNumber();
	    response.AppendFormat(_T("%cUIDNEXT %d"), separator, uidnext_value);
	    separator = ' ';
	}
	if (flagset & SAV_UIDVALIDITY)
	{
	    uidvalidity_value = m_msStore->GetUidValidityNumber();
	    response.AppendFormat(_T("%cUIDVALIDITY %d"), separator, uidvalidity_value);
	    separator = ' ';
	}
	if (flagset & SAV_UNSEEN)
	{
	    unseen_value = m_msStore->MailboxFirstUnseen(mailbox);
	    response.AppendFormat(_T("%cUNSEEN %d"), separator, unseen_value);
	    separator = ' ';
	}
	response += _T(")\r\n");
	Send((void *)response.data(), response.GetLength());
	result = IMAP_OK;
    }
    else
    {
	strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
	result = IMAP_BAD;
    }
    return result;
}

IMAP_RESULTS ImapSession::StatusHandler(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt)
{
    IMAP_RESULTS result = IMAP_OK;

    switch (astring(pData, dwDataLen, r_dwParsingAt, false, NULL))
    {
    case ImapStringGood:
	result = StatusHandlerExecute(pData, dwDataLen, r_dwParsingAt);
	break;

    case ImapStringBad:
	strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
	result = IMAP_BAD;
	break;

    case ImapStringPending:
	result = IMAP_NOTDONE;
	m_eInProgress = ImapCommandStatus;
	strncpy(m_pResponseText, _T("Ready for Literal"), MAX_RESPONSE_STRING_LENGTH);
	break;

    default:
	strncpy(m_pResponseText, _T("Failed"), MAX_RESPONSE_STRING_LENGTH);
	break;
    }
    return result;
}

DWORD ImapSession::ReadEmailFlags(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt, bool &r_okay)
{
    r_okay = true;
    DWORD result = 0;

    // Read atoms until I run out of space or the next character is a ')' instead of a ' '
    while ((r_dwParsingAt <= dwDataLen) && (')' != pData[r_dwParsingAt]))
    {
	char *s = (char *)&m_pParseBuffer[m_dwParsePointer];
	if ('\\' == pData[r_dwParsingAt++])
	{
	    atom(pData, dwDataLen, r_dwParsingAt);
	    FLAG_SYMBOL_T::iterator i = sFlagSymbolTable.find(s);
	    if (sFlagSymbolTable.end() != i)
	    {
		result |= i->second;
	    }
	    else
	    {
		r_okay = false;
		break;
	    }
	}
	else
	{
	    r_okay = false;
	    break;
	}
	if ((r_dwParsingAt < dwDataLen) && (' ' == pData[r_dwParsingAt]))
	{
	    ++r_dwParsingAt;
	}
    }
    return result;
}

IMAP_RESULTS ImapSession::AppendHandlerExecute(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt)
{
    IMAP_RESULTS result = IMAP_BAD;
    m_eInProgress = ImapCommandNone;
    CImapDateTime cdMessageDateTime;

    if ((2 < (dwDataLen - r_dwParsingAt)) && (' ' == pData[r_dwParsingAt++]))
    {
	if ('(' == pData[r_dwParsingAt])
	{
	    ++r_dwParsingAt;
	    bool flagsOk;
	    m_dwMailFlags = ReadEmailFlags(pData, dwDataLen, r_dwParsingAt, flagsOk);
	    if (flagsOk && (2 < (dwDataLen - r_dwParsingAt)) &&
		(')' == pData[r_dwParsingAt]) && (' ' == pData[r_dwParsingAt +1]))
	    {
		r_dwParsingAt += 2;
	    }
	    else
	    {
		strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
	    }
	}
	else
	{
	    m_dwMailFlags = 0;
	}

	if ('"' == pData[r_dwParsingAt])
	{
	    // The constructor for CImapDateTime swallows both the leading and trailing
	    // double quote characters
	    CImapDateTime temp_date_time(pData, dwDataLen, r_dwParsingAt);
	    if (temp_date_time.IsValid() && (1 < (dwDataLen - r_dwParsingAt)) && (' ' == pData[r_dwParsingAt]))
	    {
		cdMessageDateTime = temp_date_time;
		++r_dwParsingAt;
	    }
	    else
	    {
		strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
	    }
	}

	if ((2 < (dwDataLen - r_dwParsingAt)) && ('{' == pData[r_dwParsingAt]))
	{
	    // It's a literal string
	    r_dwParsingAt++; // Skip the curly brace
	    char *close;
	    // read the number
	    m_dwLiteralLength = strtoul((char *)&pData[r_dwParsingAt], &close, 10);
	    // check for the close curly brace and end of message and to see if I can fit all that data
	    // into the buffer
	    DWORD lastChar = (DWORD) (close - ((char *)pData));
	    if ((NULL == close) || ('}' != close[0]) || (lastChar != (dwDataLen - 1)))
	    {
		strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
	    }
	    else
	    {
		CStdString csMailbox((char *)&m_pParseBuffer[m_dwArguments]);
		csMailbox.Replace('.', '\\');
		CMailStore::MAIL_STORE_RESULT r = m_msStore->AddMessageToMailbox(csMailbox, m_pLineBuffer,
										 0,
										 cdMessageDateTime,
										 m_dwMailFlags, &m_dwAppendingUid);
		switch (r)
		{
		case CMailStore::SUCCESS:
		    strncpy(m_pResponseText, _T("Ready for the Message Data"), MAX_RESPONSE_STRING_LENGTH);
		    m_dwParseStage = 1;
		    m_eInProgress = ImapCommandAppend;
		    result = IMAP_NOTDONE;
		    break;

		case CMailStore::MAILBOX_DOES_NOT_EXIST:
		case CMailStore::MAILBOX_NOT_SELECTABLE:
		    strncpy(m_pResponseText, _T("cannot append to mailbox"), MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_NO;
		    break;

		default:
		    strncpy(m_pResponseText, _T("general error"), MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_NO;
		    break;
		}
	    }
	}
	else
	{
	    strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
	}
    }
    else
    {
	strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
    }
    return result;
}

IMAP_RESULTS ImapSession::AppendHandler(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt) {
    IMAP_RESULTS result = IMAP_OK;

    switch (astring(pData, dwDataLen, r_dwParsingAt, true, NULL))
    {
    case ImapStringGood:
	result = AppendHandlerExecute(pData, dwDataLen, r_dwParsingAt);
	break;

    case ImapStringBad:
	strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
	result = IMAP_BAD;
	break;

    case ImapStringPending:
	result = IMAP_NOTDONE;
	m_dwParseStage = 0;
	m_eInProgress = ImapCommandAppend;
	break;

    default:
	strncpy(m_pResponseText, _T("Failed"), MAX_RESPONSE_STRING_LENGTH);
	break;
    }
    return result;
}

// The CheckHandler is there because the spec says it is and because I may need the ability to
// do housekeeping at some point.  If there's no housekeeping to do, it's supposed to do the
// same thing as NOOP
IMAP_RESULTS ImapSession::CheckHandler(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt)
{
    return NoopHandler(pData, dwDataLen, r_dwParsingAt); 
}

IMAP_RESULTS ImapSession::CloseHandler(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt)
{
    // If the mailbox is open, close it
    // In IMAP, deleted messages are always purged before a close
    NUMBER_LIST dummy;
    m_msStore->PurgeDeletedMessages(&dummy);
    m_msStore->MailboxClose();
    m_eState = ImapAuthenticated;
    return IMAP_OK;
}

IMAP_RESULTS ImapSession::ExpungeHandler(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt)
{
    IMAP_RESULTS result = IMAP_OK;
    NUMBER_LIST purged_messages;

    CMailStore::MAIL_STORE_RESULT purge_status = m_msStore->PurgeDeletedMessages(&purged_messages);
    switch(purge_status)
    {
    case CMailStore::SUCCESS:
	for (int i=0; i<purged_messages.GetSize(); ++i)
	{
	    DWORD message_number;
	    CStdString untagged;

	    message_number = purged_messages[i];
	    untagged.Format(_T("* %d EXPUNGE\r\n"), message_number);
	    Send((void *)untagged.c_str(), untagged.GetLength());
	}
	break;

    case CMailStore::MAILBOX_DOES_NOT_EXIST:
	result = IMAP_NO;
	strncpy(m_pResponseText, _T("Mailbox Does Not Exist"), MAX_RESPONSE_STRING_LENGTH);
	break;

    case CMailStore::MAILBOX_READ_ONLY:
	result = IMAP_NO;
	strncpy(m_pResponseText, _T("Mailbox Opened Read-Only"), MAX_RESPONSE_STRING_LENGTH);
	break;

    default:
	break;

    }
    return result;
}

// A sequence set is a sequence of range values, each one of which is separated by commas
// A range value is one of a number, a star, or two of either of those separated by 
// a colon.  A star represents the last value in the range.  Two values separated by
// a colon represent the entire range of valid numbers between and including those two
// values.  A star is always valid unless the mail store has no messages in it.
//
// The resulting list of values must be sorted or nothing else works
bool ImapSession::MsnSequenceSet(SEARCH_RESULT &r_srVector, DWORD &r_dwTokenPointer)
{
    bool result = true;

    DWORD mbox_count = m_msStore->MailboxMessageCount();
    char *s, *e;
    s = (char *)&m_pParseBuffer[r_dwTokenPointer];
    do
    {
	DWORD first, second;
	if ('*' != s[0])
	{
	    first = strtoul(s, &e, 10);
	}
	else
	{
	    first = mbox_count;
	    e = &s[1];
	}
	if (':' == *e)
	{
	    s = e+1;
	    if ('*' != s[0])
	    {
		second = strtoul(s, &e, 10);
	    }
	    else
	    {
		second = mbox_count;
		e = &s[1];
	    }
	}
	else
	{
	    second = first;
	}
	if (second < first)
	{
	    DWORD temp = first;
	    first = second;
	    second = temp;
	}
	if (second > mbox_count)
	{
	    second = mbox_count;
	}
	for (DWORD i=first; i<=second; ++i)
	{
	    if (i <= mbox_count)
	    {
		r_srVector.Add(m_msStore->MailboxMsnToUid(i), compare_msns);
	    }
	    else
	    {
		result = false;
	    }
	}    
	s = e;
	if (',' == *s)
	{
	    ++s;
	}
	else
	{
	    if ('\0' != *s)
	    {
		result = false;
	    }
	}
    } while (result && ('\0' != *s));
    return result;
}

bool ImapSession::UidSequenceSet(SEARCH_RESULT &r_srVector, DWORD &r_dwTokenPointer)
{
    bool result = true;

    DWORD mbox_count = m_msStore->MailboxMessageCount();
    DWORD mbox_max_uid;
    if (0 != mbox_count)
    {
	mbox_max_uid = m_msStore->MailboxMsnToUid(m_msStore->MailboxMessageCount());
    }
    else
    {
	mbox_max_uid = 0; 
    }
    DWORD dwMboxMinUid;
    if (0 != mbox_count)
    {
	dwMboxMinUid = m_msStore->MailboxMsnToUid(1);
    }
    else
    {
	dwMboxMinUid = 0;
    }
    char *s, *e;
    s = (char *)&m_pParseBuffer[r_dwTokenPointer];
    do
    {
	DWORD first, second;
	if ('*' != s[0])
	{
	    first = strtoul(s, &e, 10);
	}
	else
	{
	    first = mbox_max_uid;
	    e= &s[1];
	}
	if (':' == *e)
	{
	    s = e+1;
	    if ('*' != s[0])
	    {
		second = strtoul(s, &e, 10);
	    }
	    else
	    {
		second = mbox_max_uid;
		e= &s[1];
	    }
	}
	else
	{
	    second = first;
	}
	if (second < first)
	{
	    DWORD temp = first;
	    first = second;
	    second = temp;
	}
	if (first < dwMboxMinUid)
	{
	    first = dwMboxMinUid;
	}
	if (second > mbox_max_uid)
	{
	    second = mbox_max_uid;
	}
	for (DWORD i=first; i<=second; ++i)
	{
	    if (0 < m_msStore->MailboxUidToMsn(i))
	    {
		r_srVector.Add(i, compare_msns);
	    }
	}    
	s = e;
	if (',' == *s)
	{
	    ++s;
	}
	else
	{
	    if ('\0' != *s)
	    {
		result = false;
	    }
	}
    } while (result && ('\0' != *s));
    return result;
}

// UpdateSearchTerm updates the search specification for one term.  That term may be a 
// parenthesized list of other terms, in which case it calls UpdateSearchTerms to evaluate
// all of those.  UpdateSearchTerms will, in turn, call UpdateSearchTerm to update the 
// searchTerm for each of the terms in the list.
// UpdateSearchTerm return true if everything went okay or false if there were errors in
// the Search Specification String.
bool ImapSession::UpdateSearchTerm(CMailSearch &searchTerm, DWORD &r_dwTokenPointer)
{
    bool result = true;

    if ('(' == m_pParseBuffer[r_dwTokenPointer])
    {
	r_dwTokenPointer += 2;
	result = UpdateSearchTerms(searchTerm, r_dwTokenPointer, true);
    }
    else
    {
	if (('*' == m_pParseBuffer[r_dwTokenPointer]) || isdigit(m_pParseBuffer[r_dwTokenPointer]))
	{
	    // It's a list of MSN ranges
	    // How do I treat a list of numbers?  Well, I generate a vector of all the possible values and then 
	    // and it (do a "set intersection" between it and the current vector, if any.

	    // I don't need to check to see if r_dwTokenPointer is less than m_dwParsePointer because this function
	    // wouldn't get called if it wasn't
	    SEARCH_RESULT srVector;
	    result = MsnSequenceSet(srVector, r_dwTokenPointer);
	    searchTerm.AddUidVector(srVector);
	    r_dwTokenPointer += (DWORD) strlen((char *)&m_pParseBuffer[r_dwTokenPointer]) + 1;
	}
	else
	{
	    SEARCH_SYMBOL_T::iterator found = sSearchSymbolTable.find((char *)&m_pParseBuffer[r_dwTokenPointer]);
	    r_dwTokenPointer += (DWORD) strlen((char *)&m_pParseBuffer[r_dwTokenPointer]) + 1;
	    if (found != sSearchSymbolTable.end())
	    {
		switch (found->second)
		{
		case SSV_ERROR:
		    result = false;
		    break;

		case SSV_ALL:
		    // I don't do anything unless there are no other terms.  However, since there's an implicit "and all"
		    // around the whole expression, I don't have to do anything explicitly.
		    break;

		case SSV_ANSWERED:
		    searchTerm.AddBitsToIncludeMask(IMAP_MESSAGE_ANSWERED);
		    break;

		case SSV_BCC:
		    if (r_dwTokenPointer < m_dwParsePointer)
		    {
			searchTerm.AddBccSearch((char *)&m_pParseBuffer[r_dwTokenPointer]);
			r_dwTokenPointer += (DWORD) strlen((char *)&m_pParseBuffer[r_dwTokenPointer]) + 1;
		    }
		    else
		    {
			result = false;
		    }
		    break;

		case SSV_BEFORE:
		    if (r_dwTokenPointer < m_dwParsePointer)
		    {
			CStdString csDate((char *)&m_pParseBuffer[r_dwTokenPointer]);
			CImapDate date(csDate);
			if (date.IsValid())
			{
			    r_dwTokenPointer += (DWORD) strlen((char *)&m_pParseBuffer[r_dwTokenPointer]) + 1;
			    searchTerm.AddBeforeSearch(date);
			}
			else
			{
			    result = false;
			}
		    }
		    else
		    {
			result = false;
		    }
		    break;

		case SSV_BODY:
		    if (r_dwTokenPointer < m_dwParsePointer)
		    {
			searchTerm.AddBodySearch((char *)&m_pParseBuffer[r_dwTokenPointer]);
			r_dwTokenPointer += (DWORD) strlen((char *)&m_pParseBuffer[r_dwTokenPointer]) + 1;
		    }
		    else
		    {
			result = false;
		    }
		    break;

		case SSV_CC:
		    if (r_dwTokenPointer < m_dwParsePointer)
		    {
			searchTerm.AddCcSearch((char *)&m_pParseBuffer[r_dwTokenPointer]);
			r_dwTokenPointer += (DWORD) strlen((char *)&m_pParseBuffer[r_dwTokenPointer]) + 1;
		    }
		    else
		    {
			result = false;
		    }
		    break;

		case SSV_DELETED:
		    searchTerm.AddBitsToIncludeMask(IMAP_MESSAGE_DELETED);
		    break;

		case SSV_FLAGGED:
		    searchTerm.AddBitsToIncludeMask(IMAP_MESSAGE_FLAGGED);
		    break;

		case SSV_FROM:
		    if (r_dwTokenPointer < m_dwParsePointer)
		    {
			searchTerm.AddFromSearch((char *)&m_pParseBuffer[r_dwTokenPointer]);
			r_dwTokenPointer += (DWORD) strlen((char *)&m_pParseBuffer[r_dwTokenPointer]) + 1;
		    }
		    else
		    {
			result = false;
		    }
		    break;

		case SSV_KEYWORD:
		    // Searching for this cannot return any matches because we don't have keywords to set
		    searchTerm.ForceNoMatches();
		    // However, I need to swallow the space and the atom that follows it if any, and it had 
		    // better or it's a syntax error
		    if (r_dwTokenPointer < m_dwParsePointer)
		    {
			r_dwTokenPointer += (DWORD) strlen((char *)&m_pParseBuffer[r_dwTokenPointer]) + 1;
		    }
		    else
		    {
			result = false;
		    }
		    break;

		case SSV_NEW:
		    searchTerm.AddBitsToExcludeMask(IMAP_MESSAGE_SEEN);
		    searchTerm.AddSearchForRecent();
		    break;

		case SSV_OLD:
		    searchTerm.AddSearchForNotRecent();
		    break;

		case SSV_ON:
		    if (r_dwTokenPointer < m_dwParsePointer)
		    {
			CStdString csDate((char *)&m_pParseBuffer[r_dwTokenPointer]);
			CImapDate date(csDate);
			if (date.IsValid())
			{
			    r_dwTokenPointer += (DWORD) strlen((char *)&m_pParseBuffer[r_dwTokenPointer]) + 1;
			    searchTerm.AddOnSearch(date);
			}
			else
			{
			    result = false;
			}
		    }
		    else
		    {
			result = false;
		    }
		    break;

		case SSV_RECENT:
		    searchTerm.AddSearchForRecent();
		    break;

		case SSV_SEEN:
		    searchTerm.AddBitsToIncludeMask(IMAP_MESSAGE_SEEN);
		    break;

		case SSV_SINCE:
		    if (r_dwTokenPointer < m_dwParsePointer)
		    {
			CStdString csDate((char *)&m_pParseBuffer[r_dwTokenPointer]);
			CImapDate date(csDate);
			if (date.IsValid())
			{
			    r_dwTokenPointer += (DWORD) strlen((char *)&m_pParseBuffer[r_dwTokenPointer]) + 1;
			    searchTerm.AddSinceSearch(date);
			}
			else
			{
			    result = false;
			}
		    }
		    else
		    {
			result = false;
		    }
		    break;

		case SSV_SUBJECT:
		    if (r_dwTokenPointer < m_dwParsePointer)
		    {
			searchTerm.AddSubjectSearch((char *)&m_pParseBuffer[r_dwTokenPointer]);
			r_dwTokenPointer += (DWORD) strlen((char *)&m_pParseBuffer[r_dwTokenPointer]) + 1;
		    }
		    else
		    {
			result = false;
		    }
		    break;

		case SSV_TEXT:
		    if (r_dwTokenPointer < m_dwParsePointer)
		    {
			searchTerm.AddTextSearch((char *)&m_pParseBuffer[r_dwTokenPointer]);
			r_dwTokenPointer += (DWORD) strlen((char *)&m_pParseBuffer[r_dwTokenPointer]) + 1;
		    }
		    else
		    {
			result = false;
		    }
		    break;

		case SSV_TO:
		    if (r_dwTokenPointer < m_dwParsePointer)
		    {
			searchTerm.AddToSearch((char *)&m_pParseBuffer[r_dwTokenPointer]);
			r_dwTokenPointer += (DWORD) strlen((char *)&m_pParseBuffer[r_dwTokenPointer]) + 1;
		    }
		    else
		    {
			result = false;
		    }
		    break;

		case SSV_UNANSWERED:
		    searchTerm.AddBitsToExcludeMask(IMAP_MESSAGE_ANSWERED);
		    break;

		case SSV_UNDELETED:
		    searchTerm.AddBitsToExcludeMask(IMAP_MESSAGE_DELETED);
		    break;

		case SSV_UNFLAGGED:
		    searchTerm.AddBitsToExcludeMask(IMAP_MESSAGE_FLAGGED);
		    break;

		case SSV_UNKEYWORD:
		    // Searching for this matches all messages because we don't have keywords to unset
		    // since all does nothing, this does nothing
		    // However, I need to swallow the space and the atom that follows it if any, and it had 
		    // better or it's a syntax error
		    if (r_dwTokenPointer < m_dwParsePointer)
		    {
			r_dwTokenPointer += (DWORD) strlen((char *)&m_pParseBuffer[r_dwTokenPointer]) + 1;
		    }
		    else
		    {
			result = false;
		    }
		    break;

		case SSV_UNSEEN:
		    searchTerm.AddBitsToExcludeMask(IMAP_MESSAGE_SEEN);
		    break;

		case SSV_DRAFT:
		    searchTerm.AddBitsToIncludeMask(IMAP_MESSAGE_DRAFT);
		    break;

		case SSV_HEADER:
		    if (r_dwTokenPointer < m_dwParsePointer)
		    {
			char *header_name = (char *)&m_pParseBuffer[r_dwTokenPointer];
			r_dwTokenPointer += (DWORD) strlen((char *)&m_pParseBuffer[r_dwTokenPointer]) + 1;
			if (r_dwTokenPointer < m_dwParsePointer)
			{
			    searchTerm.AddGenericHeaderSearch(header_name, (char *)&m_pParseBuffer[r_dwTokenPointer]);
			    r_dwTokenPointer += (DWORD) strlen((char *)&m_pParseBuffer[r_dwTokenPointer]) + 1;
			}
			else
			{
			    result = false;
			}
		    }
		    else
		    {
			result = false;
		    }
		    break;

		case SSV_LARGER:
		    if (r_dwTokenPointer < m_dwParsePointer)
		    {
			char *s;
			DWORD value = strtoul((char *)&m_pParseBuffer[r_dwTokenPointer], &s, 10);
			if ('\0' == *s)
			{
			    r_dwTokenPointer += (DWORD) strlen((char *)&m_pParseBuffer[r_dwTokenPointer]) + 1;
			    searchTerm.AddSmallestSize(value + 1);
			}
			else
			{
			    result = false;
			}
		    }
		    else
		    {
			result = false;
		    }
		    break;

		case SSV_NOT:
		    // Okay, not works by evaluating the next term and then doing a symmetric set difference between
		    // that and the whole list of available messages.  The result is then treated as if it was a 
		    // list of numbers
		    if (r_dwTokenPointer < m_dwParsePointer)
		    {
			CMailSearch termToBeNotted;
			if (UpdateSearchTerm(termToBeNotted, r_dwTokenPointer))
			{
			    SEARCH_RESULT vector;
			    if (CMailStore::SUCCESS ==  m_msStore->MessageList(vector))
			    {
				if (CMailStore::SUCCESS == termToBeNotted.Evaluate(m_msStore))
				{
				    SEARCH_RESULT rightResult;
				    termToBeNotted.ReportResults(m_msStore, &rightResult);
				    for (int i=0; i<rightResult.GetSize(); ++i)
				    {
					vector.Remove(rightResult[i], compare_msns);
				    }
				    searchTerm.AddUidVector(vector);
				}
				else
				{
				    result = false;
				}
			    }
			    else
			    {
				result = false;
			    }
			}
			else
			{
			    result = false;
			}
		    }
		    else
		    {
			result = false;
		    }
		    break;

		case SSV_OR:
		    // Okay, or works by evaluating the next two terms and then doing a union between the two
		    // the result is then treated as if it is a list of numbers
		    if (r_dwTokenPointer < m_dwParsePointer)
		    {
			CMailSearch termLeftSide;
			if (UpdateSearchTerm(termLeftSide, r_dwTokenPointer))
			{
			    if (CMailStore::SUCCESS == termLeftSide.Evaluate(m_msStore))
			    {
				CMailSearch termRightSide;
				if (UpdateSearchTerm(termRightSide, r_dwTokenPointer))
				{
				    if (CMailStore::SUCCESS == termRightSide.Evaluate(m_msStore))
				    {
					SEARCH_RESULT leftResult;
					termLeftSide.ReportResults(m_msStore, &leftResult);
					SEARCH_RESULT rightResult;
					termRightSide.ReportResults(m_msStore, &rightResult);
					for (int i=0; i<rightResult.GetSize(); ++i)
					{
					    DWORD uid = rightResult[i];
					    int which;

					    if (!leftResult.Find(uid, which, compare_msns))
					    {
						leftResult.Add(uid, compare_msns);
					    }
					}
					searchTerm.AddUidVector(leftResult);
				    }
				    else
				    {
					result = false;
				    }
				}
				else
				{
				    result = false;
				}
			    }
			    else
			    {
				result = false;
			    }
			}
			else
			{
			    result = false;
			}
		    }
		    else
		    {
			result = false;
		    }
		    break;

		case SSV_SENTBEFORE:
		    if (r_dwTokenPointer < m_dwParsePointer)
		    {
			CStdString csDate((char *)&m_pParseBuffer[r_dwTokenPointer]);
			CImapDate date(csDate);
			if (date.IsValid())
			{
			    r_dwTokenPointer += (DWORD) strlen((char *)&m_pParseBuffer[r_dwTokenPointer]) + 1;
			    searchTerm.AddSentBeforeSearch(date);
			}
			else
			{
			    result = false;
			}
		    }
		    else
		    {
			result = false;
		    }
		    break;

		case SSV_SENTON:
		    if (r_dwTokenPointer < m_dwParsePointer)
		    {
			CStdString csDate((char *)&m_pParseBuffer[r_dwTokenPointer]);
			CImapDate date(csDate);
			if (date.IsValid())
			{
			    r_dwTokenPointer += (DWORD) strlen((char *)&m_pParseBuffer[r_dwTokenPointer]) + 1;
			    searchTerm.AddSentOnSearch(date);
			}
			else
			{
			    result = false;
			}
		    }
		    else
		    {
			result = false;
		    }
		    break;

		case SSV_SENTSINCE:
		    if (r_dwTokenPointer < m_dwParsePointer)
		    {
			CStdString csDate((char *)&m_pParseBuffer[r_dwTokenPointer]);
			CImapDate date(csDate);
			if (date.IsValid())
			{
			    r_dwTokenPointer += (DWORD) strlen((char *)&m_pParseBuffer[r_dwTokenPointer]) + 1;
			    searchTerm.AddSentSinceSearch(date);
			}
			else
			{
			    result = false;
			}
		    }
		    else
		    {
			result = false;
		    }
		    break;

		case SSV_SMALLER:
		    if (r_dwTokenPointer < m_dwParsePointer)
		    {
			char *s;
			DWORD value = strtoul((char *)&m_pParseBuffer[r_dwTokenPointer], &s, 10);
			if ('\0' == *s)
			{
			    r_dwTokenPointer += (DWORD) strlen((char *)&m_pParseBuffer[r_dwTokenPointer]) + 1;
			    searchTerm.AddLargestSize(value - 1);
			}
			else
			{
			    result = false;
			}
		    }
		    else
		    {
			result = false;
		    }
		    break;

		case SSV_UID:
		    // What follows is a UID list
		    if (r_dwTokenPointer < m_dwParsePointer)
		    {
			SEARCH_RESULT srVector;

			result = UidSequenceSet(srVector, r_dwTokenPointer);
			searchTerm.AddUidVector(srVector);
			r_dwTokenPointer += (DWORD) strlen((char *)&m_pParseBuffer[r_dwTokenPointer]) + 1;
		    }
		    else
		    {
			result = false;
		    }
		    break;

		case SSV_UNDRAFT:
		    searchTerm.AddBitsToExcludeMask(IMAP_MESSAGE_DRAFT);
		    break;

		default:
		    result = false;
		    break;
		}
	    }
	    else
	    {
		result = false;
	    }
	}
    }
    return result;
}

// UpdateSearchTerms calls UpdateSearchTerm until there is an error, it reaches a ")" or it reaches the end of 
// the string.  If there is an error, indicated by UpdateSearchTerm returning false, or if it reaches a ")" and
// isSubExpression is not true, then false is returned.  Otherwise, true is returned.  
bool ImapSession::UpdateSearchTerms(CMailSearch &searchTerm, DWORD &r_dwTokenPointer, bool isSubExpression)
{
    bool notdone = true;
    bool result;
    do
    {
	result = UpdateSearchTerm(searchTerm, r_dwTokenPointer);
    } while(result && (r_dwTokenPointer < m_dwParsePointer) && (')' != m_pParseBuffer[r_dwTokenPointer]));
    if ((')' == m_pParseBuffer[r_dwTokenPointer]) && !isSubExpression)
    {
	result = false;
    }
    if (')' == m_pParseBuffer[r_dwTokenPointer])
    {
	r_dwTokenPointer += 2;
    }
    return result;
}

// SearchHandlerExecute assumes that the tokens for the search are in m_pParseBuffer from 0 to m_dwParsePointer and
// that they are stored there as a sequence of ASCIIZ strings.  It is also known that when the system gets
// here that it's time to generate the output
IMAP_RESULTS ImapSession::SearchHandlerExecute(bool bUsingUid)
{
    SEARCH_RESULT found_vector;
    NUMBER_LIST found_list;
    CMailStore::MAIL_STORE_RESULT msresult;
    CStdString response(_T("* SEARCH"));
    IMAP_RESULTS result;
    CMailSearch searchTerm;
    bool hasNoMatches = false;
    DWORD execute_pointer = 0;

    // Skip the first two tokens that are the tag and the command
    execute_pointer += (DWORD) strlen((char *)m_pParseBuffer) + 1;
    execute_pointer += (DWORD) strlen((char *)&m_pParseBuffer[execute_pointer]) + 1;

    // I do this once again if bUsingUid is set because the command in that case is UID
    if (bUsingUid)
    {
	execute_pointer += (DWORD) strlen((char *)&m_pParseBuffer[execute_pointer]) + 1;
    }

    // This section turns the search spec into something that might work efficiently
    if (UpdateSearchTerms(searchTerm, execute_pointer, false))
    {
	msresult = searchTerm.Evaluate(m_msStore);
	searchTerm.ReportResults(m_msStore, &found_vector);

    	// This section turns the vector of UID numbers into a list of either UID numbers or MSN numbers
    	// whichever I'm supposed to be displaying.  If I were putting the numbers into some sort of 
    	// order, this is where that would happen
    	if (!bUsingUid)
    	{
	    for (int i=0; i<found_vector.GetSize(); ++i)
	    {
		if (0 != found_vector[i])
		{
		    found_list.Add(m_msStore->MailboxUidToMsn(found_vector[i]));
		}
	    }
    	}
    	else
    	{
	    for (int i=0; i<found_vector.GetSize(); ++i)
	    {
		if (0 != found_vector[i])
		{
		    found_list.Add(found_vector[i]);
		}
	    }
    	}
	// This section writes the generated list of numbers
	switch(msresult)
	{
	case CMailStore::SUCCESS:
	{
	    for(int i=0; i<found_list.GetSize(); ++i)
	    {
		response.AppendFormat(_T(" %d"), found_list[i]);
	    }
	    response += _T("\r\n");
	    Send((void *)response.c_str(), (int)response.GetLength());
	    result = IMAP_OK;
	}
	break;

	case CMailStore::MAILBOX_NOT_OPEN:
	    result = IMAP_NO;
	    strncpy(m_pResponseText, _T("Mailbox Not Open"), MAX_RESPONSE_STRING_LENGTH);
	    break;

	default:
	    result = IMAP_NO;
	    strncpy(m_pResponseText, _T("General Error Searching"), MAX_RESPONSE_STRING_LENGTH);
	    break;
	}
    }
    else
    {
	result = IMAP_BAD;
	strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
    }
    return result;
}

IMAP_RESULTS ImapSession::SearchKeyParse(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt)
{
    m_dwParseStage = 1;
    IMAP_RESULTS result = IMAP_OK;
    while((IMAP_OK == result) && (dwDataLen > r_dwParsingAt))
    {
	if (' ' == pData[r_dwParsingAt])
	{
	    ++r_dwParsingAt;
	}
	if ('(' == pData[r_dwParsingAt])
	{
	    AddToParseBuffer(&pData[r_dwParsingAt], 1);
	    ++r_dwParsingAt;
	    if (('*' == pData[r_dwParsingAt]) || isdigit(pData[r_dwParsingAt]))
	    {
		char *end1 = strchr(((char *)pData)+r_dwParsingAt, ' ');
		char *end2 = strchr(((char *)pData)+r_dwParsingAt, ')');
		size_t len;
		if (NULL != end2)
		{
		    if ((NULL == end1) || (end2 < end1))
		    {
			len = (end2 - (((char *)pData)+r_dwParsingAt));
		    }
		    else
		    {
			len = (end1 - (((char *)pData)+r_dwParsingAt));
		    }
		}
		else
		{
		    strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_BAD;
		}
		if ((0 < len) && (dwDataLen >= (r_dwParsingAt + len)))
		{
		    AddToParseBuffer(&pData[r_dwParsingAt], (DWORD) len);
		    r_dwParsingAt += (DWORD) len;
		}
		else
		{
		    strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_BAD;
		}
	    }
	    else
	    {
		switch(astring(pData, dwDataLen, r_dwParsingAt, true, NULL))
		{
		case ImapStringGood:
		    break;

		case ImapStringBad:
		    strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_BAD;
		    break;

		case ImapStringPending:
		    strncpy(m_pResponseText, _T("Ready for Literal"), MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_NOTDONE;
		    m_dwParseStage = 1;
		    m_eInProgress = ImapCommandSearch;
		    break;

		default:
		    strncpy(m_pResponseText, _T("Internal Search Error"), MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_BAD;
		    break;
		}
	    }
	}
	else
	{
	    if (('*' == pData[r_dwParsingAt]) || isdigit(pData[r_dwParsingAt]))
	    {
		char *end1 = strchr(((char *)pData)+r_dwParsingAt, ' ');
		char *end2 = strchr(((char *)pData)+r_dwParsingAt, ')');
		size_t len;
		if (NULL != end2)
		{
		    if ((NULL == end1) || (end2 < end1))
		    {
			len = (end2 - (((char *)pData)+r_dwParsingAt));
		    }
		    else
		    {
			len = (end1 - (((char *)pData)+r_dwParsingAt));
		    }
		}
		else
		{
		    if (NULL != end1)
		    {
			len = (end1 - (((char *)pData)+r_dwParsingAt));
		    }
		    else
		    {
			len = strlen(((char *)pData)+r_dwParsingAt);
		    }
		}
		if ((0 < len) && (dwDataLen >= (r_dwParsingAt + len)))
		{
		    AddToParseBuffer(&pData[r_dwParsingAt], (DWORD) len);
		    r_dwParsingAt += (DWORD) len;
		}
		else
		{
		    strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_BAD;
		}
	    }
	    else
	    {
		switch(astring(pData, dwDataLen, r_dwParsingAt, true, _T("*")))
		{
		case ImapStringGood:
		    break;

		case ImapStringBad:
		    strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_BAD;
		    break;

		case ImapStringPending:
		    strncpy(m_pResponseText, _T("Ready for Literal"), MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_NOTDONE;
		    m_dwParseStage = 1;
		    m_eInProgress = ImapCommandSearch;
		    break;

		default:
		    strncpy(m_pResponseText, _T("Internal Search Error"), MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_BAD;
		    break;
		}
	    }
	}
	if ((dwDataLen >= r_dwParsingAt)  && (')' == pData[r_dwParsingAt]))
	{
	    AddToParseBuffer(&pData[r_dwParsingAt], 1);
	    ++r_dwParsingAt;
	}
    }
    if (IMAP_OK == result)
    {
	result = SearchHandlerExecute(m_bUsesUid);
    }
    return result;
}

IMAP_RESULTS ImapSession::SearchHandlerInternal(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt, bool bUsingUid)
{
    IMAP_RESULTS result = IMAP_OK;

    m_bUsesUid = bUsingUid;
    DWORD dwFirstArgOffset = m_dwParsePointer;
    DWORD dwSavedParsePointer = r_dwParsingAt;
    if (('(' == pData[r_dwParsingAt]) || ('*' == pData[r_dwParsingAt]))
    {
	m_dwParsePointer = dwFirstArgOffset;
	r_dwParsingAt = dwSavedParsePointer;
	result = SearchKeyParse(pData, dwDataLen, r_dwParsingAt);
    }
    else
    {
	atom(pData, dwDataLen, r_dwParsingAt);
	if (0 == strcmp(_T("CHARSET"), (char *)&m_pParseBuffer[dwFirstArgOffset]))
	{
	    if ((2 < (dwDataLen - r_dwParsingAt)) && (' ' == pData[r_dwParsingAt]))
	    {
		++r_dwParsingAt;
		m_dwParsePointer = dwFirstArgOffset;
		switch (astring(pData, dwDataLen, r_dwParsingAt, true, NULL))
		{
		case ImapStringGood:
		    // I have the charset astring--check to make sure it's "US-ASCII"
		    // for the time being, it's the only charset I recognize
		    if (0 == strcmp(_T("US-ASCII"), (char *)&m_pParseBuffer[dwFirstArgOffset]))
		    {
			m_dwParsePointer = dwFirstArgOffset;
			result = SearchKeyParse(pData, dwDataLen, r_dwParsingAt);
		    }
		    else
		    {
			strncpy(m_pResponseText, _T("Unrecognized Charset"), MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_BAD;
		    }
		    break;

		case ImapStringBad:
		    strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_BAD;
		    break;

		case ImapStringPending:
		    result = IMAP_NOTDONE;
		    m_dwParseStage = 0;
		    m_eInProgress = ImapCommandSearch;
		    strncpy(m_pResponseText, _T("Ready for Literal"), MAX_RESPONSE_STRING_LENGTH);
		    break;

		default:
		    strncpy(m_pResponseText, _T("Failed"), MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_BAD;
		    break;
		}
	    }
	    else
	    {
		strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
		result = IMAP_BAD;
	    }
	}
	else
	{
	    m_dwParsePointer = dwFirstArgOffset;
	    r_dwParsingAt = dwSavedParsePointer;
	    result = SearchKeyParse(pData, dwDataLen, r_dwParsingAt);
	}
    }
    return result;
}

IMAP_RESULTS ImapSession::SearchHandler(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt)
{
    return SearchHandlerInternal(pData, dwDataLen, r_dwParsingAt, false);
}

// RemoveRfc822Comments assumes that the lines have already been unfolded
static CStdString RemoveRfc822Comments(const CStdString header_line)
{
    CStdString result;
    bool inComment = false;
    bool inQuotedString = false;
    bool escapeFlag = false;
    int len = header_line.GetLength();

    for(int i=0; i<len; ++i)
    {
	if (inComment)
	{
	    if (escapeFlag)
	    {
		escapeFlag = false;
	    }
	    else
	    {
		if (')' == header_line[i])
		{
		    inComment = false;
		}
		else
		{
		    if ('\\' == header_line[i])
		    {
			escapeFlag = true;
		    }
		}
	    }
	}
	else
	{
	    if (inQuotedString)
	    {
		if(escapeFlag)
		{
		    escapeFlag = false;
		}
		else
		{
		    if ('"' == header_line[i])
		    {
			inQuotedString = false;
		    }
		    else
		    {
			if ('\\' == header_line[i])
			{
			    escapeFlag = true;
			}
		    }
		}
		result += header_line[i];
	    }
	    else
	    {
		// Look for quote and paren
		if ('(' == header_line[i])
		{
		    inComment = true;
		}
		else
		{
		    if ('"' == header_line[i])
		    {
			inQuotedString = true;
		    }
		    result += header_line[i];
		}
	    }
	}
    }
    return result;
}

void ImapSession::SendMessageChunk(DWORD uid, DWORD offset, DWORD length)
{
    CSimFile message;
    DWORD dwDummy;
    CSimpleDate sdDummy(MMDDYYYY, true);
    bool bDummy;
    CStdString csPhysicalFileName = m_msStore->GetMessagePhysicalPath(uid, dwDummy, sdDummy, bDummy);

    if (message.Open(csPhysicalFileName, CSimFile::modeRead))
    {
	DWORD buffLen = offset;
	if (buffLen < length)
	{
	    buffLen = length;
	}
	char *buffer;

	buffer = new char[buffLen];
	message.Read(buffer, offset);

	DWORD dwAmountRead;
	dwAmountRead = message.Read(buffer, length);

	CStdString csLiteral;
	csLiteral.Format(_T("{%u}\r\n"), dwAmountRead);
	Send((void *)csLiteral.c_str(), csLiteral.GetLength());
	Send(buffer, dwAmountRead);
	delete[] buffer;
    }
}

void ImapSession::FetchResponseFlags(DWORD flags, bool isRecent)
{
    CStdString result(_T("FLAGS (")), separator;
    if (IMAP_MESSAGE_SEEN & flags)
    {
	result += _T("\\Seen");
	separator = _T(" ");
    }
    if (IMAP_MESSAGE_ANSWERED & flags)
    {
	result += separator;
	result += _T("\\Answered");
	separator = _T(" ");
    }
    if (IMAP_MESSAGE_FLAGGED & flags)
    {
	result += separator;
	result += _T("\\Flagged");
	separator = _T(" ");
    }
    if (IMAP_MESSAGE_DELETED & flags)
    {
	result += separator;
	result += _T("\\Deleted");
	separator = _T(" ");
    }
    if (IMAP_MESSAGE_DRAFT & flags)
    {
	result += separator;
	result += _T("\\Draft");
	separator = _T(" ");
    }
    if (isRecent)
    {
	result += separator;
	result += _T("\\Recent");
    }
    result += _T(")");
    Send((void *)result.c_str(), result.GetLength());
}

void ImapSession::FetchResponseInternalDate(CMailMessage &message)
{
    CStdString result;
    CSimpleDate when = message.GetInternalDate(); 

    when.SetFormat(IMAPDATEFORMAT);
    result.Format(_T("INTERNALDATE \"%s\""), (LPCSTR)when);
    Send((void *)result.c_str(), result.GetLength());
}

void ImapSession::FetchResponseRfc822(DWORD uid, const CMailMessage &message)
{
    Send((void *)_T("RFC822 "), 7);
    SendMessageChunk(uid, 0, message.GetMessageBody().dwBodyOctets);
}

void ImapSession::FetchResponseRfc822Header(DWORD uid, const CMailMessage &message)
{
    DWORD length;
    if (0 != message.GetMessageBody().dwHeaderOctets)
    {
	length = message.GetMessageBody().dwHeaderOctets;
    }
    else
    {
	length = message.GetMessageBody().dwBodyOctets;
    }
    Send((void *)_T("RFC822.HEADER "), 14);
    SendMessageChunk(uid, 0, length);
}

void ImapSession::FetchResponseRfc822Size(const CMailMessage &message)
{
    CStdString result;
    result.Format(_T("RFC822.SIZE %d"), message.GetMessageBody().dwBodyOctets);
    Send((void *)result.c_str(), result.GetLength());
}

void ImapSession::FetchResponseRfc822Text(DWORD uid, const CMailMessage &message)
{
    CStdString result;
    if (0 < message.GetMessageBody().dwHeaderOctets)
    {
	DWORD length;
	if (message.GetMessageBody().dwBodyOctets > message.GetMessageBody().dwHeaderOctets)
	{
	    length = message.GetMessageBody().dwBodyOctets - message.GetMessageBody().dwHeaderOctets;
	}
	else
	{
	    length = 0;
	}
	Send((void *)_T("RFC822.TEXT "), 12);
	SendMessageChunk(uid, message.GetMessageBody().dwHeaderOctets, length);
    }
    else
    {
	Send((void *)_T("RFC822.TEXT {0}\r\n"), 17);
    }
}

static CStdString QuotifyString(const CStdString &input)
{
    CStdString result(_T("\""));

    for (int i=0; i<input.GetLength(); ++i)
    {
	if (('"' == input[i]) || ('\\' == input[i]))
	{
	    result += '\\';
	}
	result += input[i];
    }
    result += _T("\"");

    return result;
}


static CStdString Rfc822DotAtom(CStdString &r_input)
{
    CStdString result;
    bool notdone = true;
    r_input.Trim();

    if ('"' == r_input[0])
    {
	r_input = r_input.Mid(1);
	bool escapeFlag = false;
	int i;
	for (i=0; i<r_input.GetLength(); ++i)
	{
	    if (escapeFlag)
	    {
		escapeFlag = false;
		result += r_input[i];
	    }
	    else
	    {
		if ('"' == r_input[i])
		{
		    break;
		}
		else
		{
		    if ('\\' == r_input[i])
		    {
			escapeFlag = true;
		    }
		    else
		    {
			result += r_input[i];
		    }
		}
	    }
	}
	r_input = r_input.Mid(i+1);
    }
    else
    {
	SS_SIZETYPE pos = r_input.find_first_not_of(_T("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!#$%&'*+-/=?^_`{|}~. "));
	if (CStdString::npos != pos)
	{
	    result = r_input.Left((int) pos).Trim();
	    r_input = r_input.Mid((int) pos);
	}
	else
	{
	    result = r_input;
	    r_input = _T("");
	}
    }
    return result;
}


static CStdString ParseRfc822Domain(CStdString &r_input)
{
    CStdString result;

    if ('[' == r_input[1])
    {
	r_input = r_input.Mid(1);
	CStdString token;
	bool escapeFlag = false;
	bool notdone = true;
	int i;
	for (i=0; (i<r_input.GetLength()) && notdone; ++i)
	{
	    if (escapeFlag)
	    {
		token += r_input[i];
		escapeFlag = false;
	    }
	    else
	    {
		if ('\\' == r_input[i])
		{
		    escapeFlag = true;
		}
		else
		{
		    if (']' == r_input[i])
		    {
			notdone = false;
		    }
		    else
		    {
			token += r_input[i];
		    }
		}
	    }
	}
	r_input = r_input.Mid(i+1);
    }
    else
    {
	CStdString token = Rfc822DotAtom(r_input);
	result += QuotifyString(token);
    }
    return result;
}

static CStdString ParseRfc822AddrSpec(CStdString &r_input)
{
    CStdString result;

    if ('@' != r_input[0])
    {
	result += _T("NIL ");
    }
    else
    {
	// It's an old literal path
	int pos = r_input.Find(':');
	if (-1 != pos)
	{
	    result += QuotifyString(r_input.Left(pos).Trim()) + _T(" ");
	    r_input = r_input.Mid(pos+1);
	}
	else
	{
	    result += _T("NIL ");
	}
    }
    CStdString token = Rfc822DotAtom(r_input);
    result += QuotifyString(token) + _T(" ");
    if ('@' == r_input[0])
    {
	r_input = r_input.Mid(1);
	result += ParseRfc822Domain(r_input);
    }
    else
    {
	result += _T("\"\"");
    }
    return result;
}

static CStdString ParseMailbox(CStdString &r_input)
{
    CStdString result(_T("("));
    r_input.Trim();
    if ('<' == r_input[0])
    {
	result += _T("NIL ");

	r_input = r_input.Mid(1).Trim();
	// Okay, this is an easy one.  This is an "angle-addr"
	result += ParseRfc822AddrSpec(r_input);
	r_input.Trim();
	if ('>' == r_input[0])
	{
	    r_input = r_input.Mid(1).Trim();
	}
    }
    else
    {
	if ('@' == r_input[0])
	{
	    result += _T("NIL ") + ParseRfc822AddrSpec(r_input);
	}
	else
	{
	    CStdString token = Rfc822DotAtom(r_input);
	    r_input.Trim();

	    // At this point, I don't know if I've seen a local part or an address_spec
	    // I can tell the difference by the first character.  If it's a at sign or if I'm out of string,
	    // then it's a local part  If it's a greater than, then it was a display name and I'm expecting
	    // an address
	    if ((0 == r_input.GetLength()) || (',' == r_input[0]))
	    {
		result += _T("NIL NIL ") + QuotifyString(token) + _T(" \"\"");
	    }
	    else
	    {
		if ('@' == r_input[0])
		{
		    r_input = r_input.Mid(1);
		    result += _T("NIL NIL ") + QuotifyString(token) + _T(" ") + ParseRfc822Domain(r_input);
		}
		else
		{
		    if ('<' == r_input[0])
		    {
			r_input = r_input.Mid(1);
		    }
		    result += QuotifyString(token) + _T(" ") + ParseRfc822AddrSpec(r_input);
		}
	    }
	}
    }
    result += _T(")");
    return result;
}

static CStdString ParseMailboxList(const CStdString &input)
{
    CStdString result;

    if (0 != input.GetLength())
    {
	result = _T("(");
	CStdString work = RemoveRfc822Comments(input);
	do {
	    if (',' == work[0])
	    {
		work = work.Mid(1);
	    }
	    result += ParseMailbox(work);
	    work.Trim();
	} while (',' == work[0]);
	result += _T(")");
    }
    else
    {
	result = _T("NIL");
    }
    return result;
}

// An address is either a mailbox or a group.  A group is a display-name followed by a colon followed
// by a mailbox list followed by a semicolon.  A mailbox is either an address specification or a name-addr.
//  An address specification is a mailbox followed by a 
static CStdString ParseAddress(CStdString &r_input)
{
    CStdString result(_T("("));
    r_input.Trim();
    if ('<' == r_input[0])
    {
	result += _T("NIL ");

	r_input = r_input.Mid(1).Trim();
	// Okay, this is an easy one.  This is an "angle-addr"
	result += ParseRfc822AddrSpec(r_input);
	r_input.Trim();
	if ('>' == r_input[0])
	{
	    r_input = r_input.Mid(1).Trim();
	}
    }
    else
    {
	if ('@' == r_input[0])
	{
	    result += _T("NIL ") + ParseRfc822AddrSpec(r_input);
	}
	else
	{
	    CStdString token = Rfc822DotAtom(r_input);
	    r_input.Trim();

	    // At this point, I don't know if I've seen a local part, a display name, or an address_spec
	    // I can tell the difference by the first character.  If it's a at sign or if I'm out of string,
	    // then it's a local part  If it's a colon, then it was a display name and I'm reading a group.
	    //  If it's a greater than, then it was a display name and I'm expecting an address
	    if ((0 == r_input.GetLength()) || (',' == r_input[0]))
	    {
		result += _T("NIL NIL ") + QuotifyString(token) + _T(" \"\"");
	    }
	    else
	    {
		if ('@' == r_input[0])
		{
		    r_input = r_input.Mid(1);
		    CStdString csDomain = ParseRfc822Domain(r_input);
		    if ('(' == r_input[0])
		    {
			r_input = r_input.Mid(1, r_input.Find(')')-1);
			result += QuotifyString(r_input) + _T(" NIL ") + QuotifyString(token) + _T(" ") + csDomain;
		    }
		    else
		    {
			result += _T("NIL NIL ") + QuotifyString(token) + _T(" ") + csDomain;
		    }
		    // SYZYGY -- 
		}
		else
		{
		    if ('<' == r_input[0])
		    {
			r_input = r_input.Mid(1);
			result += QuotifyString(token) + _T(" ") + ParseRfc822AddrSpec(r_input);
		    }
		    else
		    {
			if (':' == r_input[0])
			{
			    r_input = r_input.Mid(1);
			    // It's a group specification.  I mark the start of the group with (NIL NIL <token> NIL)
			    result += _T("NIL NIL ") + QuotifyString(token) + _T(" NIL)");
			    // The middle is a mailbox list
			    result += ParseMailboxList(r_input);
			    // I mark the end of the group with (NIL NIL NIL NIL)
			    if (';' == r_input[0])
			    {
				r_input = r_input.Mid(1);
				result += _T("(NIL NIL NIL NIL");
			    }
			}
			else
			{
			    result += QuotifyString(token) + _T(" ") + ParseRfc822AddrSpec(r_input);
			}
		    }
		}
	    }
	}
    }
    result += _T(")");
    return result;
}

static CStdString ParseAddressList(const CStdString &input)
{
    CStdString result;

    if (0 != input.GetLength())
    {
	result = _T("(");
	CStdString work = input; // RemoveRfc822Comments(input);
	do {
	    if (',' == work[0])
	    {
		work = work.Mid(1);
	    }
	    result += ParseAddress(work);
	    work.Trim();
	} while (',' == work[0]);
	result += _T(")");
    }
    else
    {
	result = _T("NIL");
    }
    return result;
}

// FetchResponseEnvelope returns the envelope data of the message.
// The envelope data is a parenthesized list with the following fields
// in this order:
// date (string -- taken from m_csDateLine), subject (string -- taken from m_csSubject),
// from (parenthesized list of addresses from m_csFromLine), sender (parenthesized list
// of addresses from m_csSenderLine), reply-to (parenthesized list of addresses from 
// m_csReplyToLine), to (parenthesized list of addresses from m_csToLine), cc (parenthesized
// list of addresses from m_csCcLine), bcc (parenthesized list of addresses from m_csBccLine)
// in-reply-to (string from m_csInReplyTo), and message-id (string from m_csMessageId)
void ImapSession::FetchResponseEnvelope(const CMailMessage &message)
{
    CStdString result(_T("ENVELOPE (")); 
    result += QuotifyString(message.GetDateLine()) + _T(" ");
    if (0 != message.GetSubject().GetLength())
    {
	result += QuotifyString(message.GetSubject()) + _T(" ");
    }
    else
    {
	result += _T("NIL ");
    }
    CStdString csFrom = ParseAddressList(message.GetFrom()) + _T(" ");
    result += csFrom;
    if (0 != message.GetSender().GetLength())
    {
	result += ParseAddressList(message.GetSender()) + _T(" ");
    }
    else
    {
	result += csFrom;
    }
    if (0 != message.GetReplyTo().GetLength())
    {
        result += ParseAddressList(message.GetReplyTo()) + _T(" ");
    }
    else
    {
	result += csFrom;
    }
    result += ParseAddressList(message.GetTo()) + _T(" ");
    result += ParseAddressList(message.GetCc()) + _T(" ");
    result += ParseAddressList(message.GetBcc()) + _T(" ");
    if (0 != message.GetInReplyTo().GetLength())
    {
	result += QuotifyString(message.GetInReplyTo()) + _T(" ");
    }
    else
    {
	result += _T("NIL ");
    }
    if (0 != message.GetMessageId().GetLength())
    {
	result += QuotifyString(message.GetMessageId());
    }
    else
    {
	result += _T("NIL ");
    }
    result += _T(")");
    Send((void *)result.c_str(), result.GetLength());
}

// The part before the slash is the body type
// The default type is "TEXT"
static CStdString ParseBodyType(const CStdString type_line)
{	
    CStdString result = RemoveRfc822Comments(type_line);
    if (0 == result.GetLength())
    {
	result = _T("\"TEXT\"");
    }
    else
    {
	int pos = result.Find('/');
	if (-1 != pos)
	{
	    result = result.Left(pos);
	}
	else
	{
	    pos = result.Find(';');
	    if (-1 != pos)
	    {
		result = result.Left(pos);
	    }
	}
	result.Trim();
	result = _T("\"") + result + _T("\"");
    }
    return result;
}

// the part between the slash and the semicolon (if any) is the body subtype
// The default subtype is "PLAIN" for text types and "UNKNOWN" for others
static CStdString ParseBodySubtype(const CStdString type_line, MIME_MEDIA_TYPES mmtType)
{
    CStdString result = RemoveRfc822Comments(type_line);
    int pos = result.Find('/');
    if (-1 == pos)
    {
	if (MIME_TYPE_TEXT == mmtType)
	{
	    result = _T("\"PLAIN\"");
	}
	else
	{
	    result = _T("\"UNKNOWN\"");
	}
    }
    else
    {
	result = result.Mid(pos+1);
	pos = result.Find(';');
	if (-1 != pos)
	{
	    result = result.Left(pos);
	}
	result.Trim();
	result = _T("\"") + result + _T("\"");
    }
    return result;
}

// The parameters are all after the first semicolon
// The default parameters are ("CHARSET" "US-ASCII") for text types, and
// NIL for everything else
static CStdString ParseBodyParameters(const CStdString type_line, MIME_MEDIA_TYPES mmtType)
{
    CStdString uncommented = RemoveRfc822Comments(type_line);
    CStdString result;
	
    int pos = uncommented.Find(';');
    if (-1 == pos)
    {
	if (MIME_TYPE_TEXT == mmtType)
	{
	    result = _T("(\"CHARSET\" \"US-ASCII\")");
	}
	else
	{
	    result = _T("NIL");
	}
    }
    else
    {
	CStdString residue = uncommented.Mid(pos+1);
	bool inQuotedString = false;
	bool escapeFlag = false;

	result = "(\"";
	for (int pos=0; pos<residue.GetLength(); ++pos)
	{
	    if (inQuotedString)
	    {
		if (escapeFlag)
		{
		    escapeFlag = false;
		    result += residue[pos];
		}
		else
		{
		    if ('"' == residue[pos])
		    {
			inQuotedString = false;
		    }
		    else
		    {
			if ('\\' == residue[pos])
			{
			    escapeFlag = true;
			}
			else
			{
			    result += residue[pos];
			}
		    }
		}
	    }
	    else
	    {
		if (' ' != residue[pos])
		{
		    if ('"' == residue[pos])
		    {
			inQuotedString = true;
		    }
		    else
		    {
			if (';' == residue[pos])
			{
			    result += _T("\" \"");
			}
			else
			{
			    if ('=' == residue[pos])
			    {
				result += _T("\" \"");
			    }
			    else
			    {
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

// FetchSubpartEnvelope returns the envelope data of an RFC 822 message subpart.
// The envelope data is a parenthesized list with the following fields
// in this order:
// date (string -- taken from m_csDateLine), subject (string -- taken from m_csSubject),
// from (parenthesized list of addresses from m_csFromLine), sender (parenthesized list
// of addresses from m_csSenderLine), reply-to (parenthesized list of addresses from 
// m_csReplyToLine), to (parenthesized list of addresses from m_csToLine), cc (parenthesized
// list of addresses from m_csCcLine), bcc (parenthesized list of addresses from m_csBccLine)
// in-reply-to (string from m_csInReplyTo), and message-id (string from m_csMessageId)
static CStdString FetchSubpartEnvelope(const MESSAGE_BODY &mbBody)
{
    CStdString result(_T("("));
    HEADER_FIELDS::const_iterator hfiField = mbBody.hfFieldList.find("date");
    if (mbBody.hfFieldList.end() != hfiField)
    {
	result += QuotifyString(hfiField->second) + _T(" ");
    }
    else 
    {
	result += _T("NIL ");
    }
    hfiField = mbBody.hfFieldList.find("subject");
    if (mbBody.hfFieldList.end() != hfiField)
    {
	result += QuotifyString(hfiField->second) + _T(" ");
    }
    else
    {
	result += _T("NIL ");
    }
    hfiField = mbBody.hfFieldList.find("from");
    CStdString csFrom;
    if (mbBody.hfFieldList.end() != hfiField)
    {
	csFrom = ParseAddressList(hfiField->second) + _T(" ");
    }
    else
    {
	csFrom = _T("NIL ");
    }
    result += csFrom;
    hfiField = mbBody.hfFieldList.find("sender");
    if (mbBody.hfFieldList.end() != hfiField)
    {
	result += ParseAddressList(hfiField->second) + _T(" ");
    }
    else
    {
	result += csFrom;
    }
    hfiField = mbBody.hfFieldList.find("reply-to");
    if (mbBody.hfFieldList.end() != hfiField)
    {
        result += ParseAddressList(hfiField->second) + _T(" ");
    }
    else
    {
	result += csFrom;
    }
    hfiField = mbBody.hfFieldList.find("to");
    if (mbBody.hfFieldList.end() != hfiField)
    {
	result += ParseAddressList(hfiField->second) + _T(" ");
    }
    else
    {
	result += _T("NIL ");
    }
    hfiField = mbBody.hfFieldList.find("cc");
    if (mbBody.hfFieldList.end() != hfiField)
    {
	result += ParseAddressList(hfiField->second) + _T(" ");
    }
    else
    {
	result += _T("NIL ");
    }
    hfiField = mbBody.hfFieldList.find("bcc");
    if (mbBody.hfFieldList.end() != hfiField)
    {
	result += ParseAddressList(hfiField->second) + _T(" ");
    }
    else
    {
	result += _T("NIL ");
    }
    hfiField = mbBody.hfFieldList.find("in-reply-to");
    if (mbBody.hfFieldList.end() != hfiField)
    {
	result += QuotifyString(hfiField->second) + _T(" ");
    }
    else
    {
	result += _T("NIL ");
    }
    hfiField = mbBody.hfFieldList.find("message-id");
    if (mbBody.hfFieldList.end() != hfiField)
    {
	result += QuotifyString(hfiField->second);
    }
    else
    {
	result += _T("NIL");
    }
    result += _T(")");
    return result;
}

// FetchResponseBodyStructure builds a bodystructure response.  The body structure is a representation
// of the MIME structure of the message.  It starts at the main part and goes from there.
// The results for each part are a parenthesized list of values.  For non-multipart parts, the
// fields are the body type (derived from the csContentTypeLine), the body subtype (derived from the
// csContentTypeLine), the body parameter parenthesized list (derived from the csContentTypeLine), the
// body id (from csContentIdLine), the body description (from csContentDescriptionLine), the body
// encoding (from csContentEncodingLine) and the body size (from csText.GetLength()).  Text sections
// also have the number of lines (taken from dwBodyLines)
//
// Multipart parts are the list of subparts followed by the subtype (from csContentTypeLine)
// Note that even though I'm passing a parameter about whether or not I want to include the 
// extension data, I'm not actually putting extension data in the response
//
// Message parts are formatted just like text parts except they have the envelope structure
// and the body structure before the number of lines.
static CStdString FetchResponseBodyStructureHelper(const MESSAGE_BODY &body, bool bIncludeExtensionData)
{
    CStdString result(_T("("));
    switch(body.mmtBodyMediaType)
    {
    case MIME_TYPE_MULTIPART:
	if (NULL != body.subparts) {
	    for (int i=0; i<body.subparts->GetSize(); ++i)
	    {
		MESSAGE_BODY part = (*body.subparts)[i];
		result += FetchResponseBodyStructureHelper(part, bIncludeExtensionData);
	    }
	}
	result += _T(" ") + ParseBodySubtype(body.csContentTypeLine, body.mmtBodyMediaType);
	break;

    case MIME_TYPE_MESSAGE:
	result += ParseBodyType(body.csContentTypeLine);
	result += _T(" ") + ParseBodySubtype(body.csContentTypeLine, body.mmtBodyMediaType);
	{
	    CStdString uncommented = RemoveRfc822Comments(body.csContentTypeLine);
	    int pos = uncommented.Find(';');
	    if (-1 == pos)
	    {
		result += _T(" NIL");
	    }
	    else
	    {
		result += _T(" ") + ParseBodyParameters(body.csContentTypeLine, body.mmtBodyMediaType);
	    }
	}
	if (0 == body.csContentIdLine.GetLength())
	{
	    result += _T(" NIL");
	}
	else
	{
	    result += _T(" \"") + body.csContentIdLine + _T("\"");
	}
	if (0 == body.csContentDescriptionLine.GetLength())
	{
	    result += _T(" NIL");
	}
	else
	{
	    result += _T(" \"") + body.csContentDescriptionLine + _T("\"");
	}
	if (0 == body.csContentEncodingLine.GetLength())
	{
	    result += _T(" \"7BIT\"");
	}
	else
	{
	    result += _T(" \"") + body.csContentEncodingLine + _T("\"");
	}
	if ((NULL != body.subparts) && (0<body.subparts->GetSize()))
	{
	    result.AppendFormat(_T(" %d"), body.dwBodyOctets - body.dwHeaderOctets);
	    MESSAGE_BODY part = (*body.subparts)[0];
	    result += _T(" ") + FetchSubpartEnvelope(part);
	    result += _T(" ") + FetchResponseBodyStructureHelper(part, bIncludeExtensionData);
	}
	break;

    default:
	result += ParseBodyType(body.csContentTypeLine);
	result += _T(" ") + ParseBodySubtype(body.csContentTypeLine, body.mmtBodyMediaType);
	result += _T(" ") + ParseBodyParameters(body.csContentTypeLine, body.mmtBodyMediaType);
	if (0 == body.csContentIdLine.GetLength())
	{
	    result += _T(" NIL");
	}
	else
	{
	    result += _T(" \"") + body.csContentIdLine + _T("\"");
	}
	if (0 == body.csContentDescriptionLine.GetLength())
	{
	    result += _T(" NIL");
	}
	else
	{
	    result += _T(" \"") + body.csContentDescriptionLine + _T("\"");
	}
	if (0 == body.csContentEncodingLine.GetLength())
	{
	    result += _T(" \"7BIT\"");
	}
	else
	{
	    result += _T(" \"") + body.csContentEncodingLine + _T("\"");
	}
	result.AppendFormat(_T(" %d"), body.dwBodyOctets - body.dwHeaderOctets);
	break;
    }
    if ((MIME_TYPE_TEXT == body.mmtBodyMediaType) ||
	(MIME_TYPE_MESSAGE == body.mmtBodyMediaType))
    {
	result.AppendFormat(_T(" %d"), body.dwBodyLines - body.dwHeaderLines);
    }
    result += _T(")");
    return result;
}

void ImapSession::FetchResponseBodyStructure(const CMailMessage &message)
{
    CStdString result(_T("BODYSTRUCTURE "));
    result +=  FetchResponseBodyStructureHelper(message.GetMessageBody(), true);
    Send((void *)result.c_str(), result.GetLength());
}

void ImapSession::FetchResponseBody(const CMailMessage &message)
{
    CStdString result(_T("BODY "));
    result +=  FetchResponseBodyStructureHelper(message.GetMessageBody(), false);
    Send((void *)result.c_str(), result.GetLength());
}

void ImapSession::FetchResponseUid(DWORD uid)
{
    CStdString result;
    result.Format(_T("UID %d"), uid);
    Send((void *)result.c_str(), result.GetLength());
}

#define SendBlank() (nBlankLen?Send((void *)_T(" "),1):0)

IMAP_RESULTS ImapSession::FetchHandlerExecute(bool bUsingUid)
{
    IMAP_RESULTS final_result = IMAP_OK;

    DWORD execute_pointer = (DWORD) strlen((char *)m_pParseBuffer) + 1;
    execute_pointer += (DWORD) strlen((char *)&m_pParseBuffer[execute_pointer]) + 1;
    SEARCH_RESULT srVector;

    if (bUsingUid)
    {
	execute_pointer += (DWORD) strlen((char *)&m_pParseBuffer[execute_pointer]) + 1;
	UidSequenceSet(srVector, execute_pointer);
    }
    else
    {
	if (!MsnSequenceSet(srVector, execute_pointer))
	{
	    strncpy(m_pResponseText, _T("Invalid Message Sequence Number"), MAX_RESPONSE_STRING_LENGTH);
	    final_result = IMAP_NO;
	}
    }

    for (int i = 0; (IMAP_BAD != final_result) && (i < srVector.GetSize()); ++i)
    {
	int nBlankLen = 0;
	CMailMessage message;

	CMailMessage::MAIL_MESSAGE_RESULT message_read_result = message.parse(m_msStore, srVector[i]);
	DWORD dwUid = srVector[i];
	if (CMailMessage::SUCCESS == message_read_result)
	{
	    IMAP_RESULTS result = IMAP_OK;

	    bool bSeenFlag = false;
	    DWORD specification_base;
// v-- This part syntax-checks the fetch request
	    specification_base = execute_pointer + strlen((char *)&m_pParseBuffer[execute_pointer]) + 1;
	    if (0 == strcmp(_T("("), (char *)&m_pParseBuffer[specification_base]))
	    {
		specification_base += (DWORD) strlen((char *)&m_pParseBuffer[specification_base]) + 1;
	    }
	    while ((IMAP_OK == result) && (specification_base < m_dwParsePointer))
	    {
		FETCH_NAME_T::iterator which = sFetchSymbolTable.find((char *)&m_pParseBuffer[specification_base]);
		if (sFetchSymbolTable.end() == which)
		{
		    // This may be an extended BODY name, so I have to check for that.
		    char *temp;
		    temp = strchr((char *)&m_pParseBuffer[specification_base], '[');
		    if (NULL != temp)
		    {
			*temp = '\0';
			FETCH_NAME_T::iterator which = sFetchSymbolTable.find((char *)&m_pParseBuffer[specification_base]);
			specification_base += (DWORD) strlen((char *)&m_pParseBuffer[specification_base]) + 1;
			*temp = '[';
			if (sFetchSymbolTable.end() != which)
			{
			    switch(which->second)
			    {
			    case FETCH_BODY:
			    case FETCH_BODY_PEEK:
				break;

			    default:
				strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
				result = IMAP_BAD;
				break;
			    }
			}
			else
			{
			    strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
			    result = IMAP_BAD;
			}
		    }
		    else
		    {
			strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_BAD;
		    }
		    MESSAGE_BODY body = message.GetMessageBody();
		    while ((IMAP_OK == result) && isdigit(m_pParseBuffer[specification_base]))
		    {
			char *end;
			DWORD section = strtoul((char *)&m_pParseBuffer[specification_base], &end, 10);
			// This part here is because the message types have two headers, one for the
			// subpart the message is in, one for the message that's contained in the subpart
			// if I'm looking at a subpart of that subpart, then I have to skip the subparts
			// header and look at the message's header, which is handled by the next four
			// lines of code.
			if ((body.mmtBodyMediaType == MIME_TYPE_MESSAGE) && (1 == (DWORD)body.subparts->GetSize()))
			{
			    body = (*body.subparts)[0];
			}
			if (NULL != body.subparts)
			{
			    if ((0 < section) && (section <= ((DWORD)body.subparts->GetSize())))
			    {
				body = (*body.subparts)[section-1];
				specification_base += (DWORD) (end - ((char *)&m_pParseBuffer[specification_base]));
				if ('.' == *end)
				{
				    ++specification_base;
				}
			    }
			    else
			    {
				strncpy(m_pResponseText, _T("Invalid Subpart"), MAX_RESPONSE_STRING_LENGTH);
				result = IMAP_BAD;
			    }
			}
			else
			{
			    if ((1 == section) && (']' == *end))
			    {
				specification_base += (DWORD) (end - ((char *)&m_pParseBuffer[specification_base]));
			    }
			    else
			    {
				strncpy(m_pResponseText, _T("Invalid Subpart"), MAX_RESPONSE_STRING_LENGTH);
				result = IMAP_BAD;
			    }
			}
		    }
		    // When I get here, and if everything's okay, then I'm looking for a possibly nonzero length of characters
		    // representing the section-msgtext
		    FETCH_BODY_PARTS which_part;
		    if ((IMAP_OK == result) && (m_dwParsePointer > specification_base))
		    {
			// If I'm out of string and the next string begins with ']', then I'm returning the whole body of 
			// whatever subpart I'm at
			if (']' == m_pParseBuffer[specification_base])
			{
			    which_part = FETCH_BODY_BODY;
			}
			else
			{
			    CStdString part((char *)&m_pParseBuffer[specification_base]);
			    // I'm looking for HEADER[.FIELDS[.NOT]], TEXT, or MIME
			    part.ToUpper();
			    if (part.Left(6).Equals(_T("header")))
			    {
				if (']' == part[6])
				{
				    specification_base += 6;
				    which_part = FETCH_BODY_HEADER;
				}
				else
				{
				    if (part.Mid(6,7).Equals(_T(".FIELDS")))
				    {
					if (13 == part.GetLength())
					{
					    specification_base +=  (DWORD) strlen((char *)&m_pParseBuffer[specification_base]) + 1;
					    which_part = FETCH_BODY_FIELDS;
					}
					else
					{
					    if (part.Mid(13).Equals(_T(".NOT")))
					    {
						specification_base +=  (DWORD) strlen((char *)&m_pParseBuffer[specification_base]) + 1;
						which_part = FETCH_BODY_NOT_FIELDS;
					    }
					    else
					    {
						strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
						result = IMAP_BAD;
					    }
					}
				    }
				    else
				    {
					strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
					result = IMAP_BAD;
				    }
				}
			    }
			    else
			    {
				if (part.Left(5).Equals(_T("TEXT]")))
				{
				    specification_base += 4;
				    which_part = FETCH_BODY_TEXT;
				}
				else
				{
				    if (part.Left(5).Equals(_T("MIME]")))
				    {
					specification_base += 4;
					which_part = FETCH_BODY_MIME;
				    }
				    else
				    {
					strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
					result = IMAP_BAD;
				    }
				}
			    }
			}
		    }
		    // Now look for the list of headers to return
		    CSimArray<CStdString> field_list;
		    if ((IMAP_OK == result) && (m_dwParsePointer > specification_base) &&
			((FETCH_BODY_FIELDS == which_part) || (FETCH_BODY_NOT_FIELDS == which_part)))
		    {
			if (0 == strcmp((char *)&m_pParseBuffer[specification_base], _T("(")))
			{
			    specification_base +=  (DWORD) strlen((char *)&m_pParseBuffer[specification_base]) + 1;
			    while((IMAP_BUFFER_LEN > specification_base) &&
				  (0 != strcmp((char *)&m_pParseBuffer[specification_base], _T(")"))))
			    {
				specification_base +=  (DWORD) strlen((char *)&m_pParseBuffer[specification_base]) + 1;
			    }
			    if (0 == strcmp((char *)&m_pParseBuffer[specification_base], _T(")")))
			    {
				specification_base +=  (DWORD) strlen((char *)&m_pParseBuffer[specification_base]) + 1;
			    }
			    else
			    {
				strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
				result = IMAP_BAD;
			    }
			}
			else
			{
			    strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
			    result = IMAP_BAD;
			}
		    }

		    // Okay, look for the end square bracket
		    if ((IMAP_OK == result) && (']' == m_pParseBuffer[specification_base]))
		    {
			specification_base++;
		    }
		    else
		    {
			strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_BAD;
		    }

		    // Okay, get the limits, if any.
		    DWORD first_byte = 0, max_length = ~0;
		    if ((IMAP_OK == result) && (m_dwParsePointer > specification_base))
		    {
			if ('<' == m_pParseBuffer[specification_base])
			{
			    ++specification_base;
			    char *end;
			    first_byte = strtoul((char *)&m_pParseBuffer[specification_base], &end, 10);
			    if ('.' == *end)
			    {
				max_length = strtoul(end+1, &end, 10);
			    }
			    if ('>' != *end)
			    {
				strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
				result = IMAP_BAD;
			    }
			}
		    }

		    if (IMAP_OK == result)
		    {
			switch(which_part)
			{
			case FETCH_BODY_BODY:
			case FETCH_BODY_MIME:
			case FETCH_BODY_HEADER:
			case FETCH_BODY_FIELDS:
			case FETCH_BODY_NOT_FIELDS:
			case FETCH_BODY_TEXT:
			    break;

			default:
			    strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
			    result = IMAP_BAD;
			    break; 
			}

		    }
		}
		specification_base += (DWORD) strlen((char *)&m_pParseBuffer[specification_base]) + 1;
		if (0 == strcmp(_T(")"), (char *)&m_pParseBuffer[specification_base]))
		{
		    specification_base += (DWORD) strlen((char *)&m_pParseBuffer[specification_base]) + 1;
		}
	    }
// ^-- This part syntax-checks the fetch request
// v-- This part executes the fetch request
	    if (IMAP_OK == result)
	    {
		specification_base = execute_pointer + strlen((char *)&m_pParseBuffer[execute_pointer]) + 1;
		if (0 == strcmp(_T("("), (char *)&m_pParseBuffer[specification_base]))
		{
		    specification_base += (DWORD) strlen((char *)&m_pParseBuffer[specification_base]) + 1;
		}
		CStdString fetch_result;
		fetch_result.Format(_T("* %u FETCH ("), message.GetMsn());
		Send((void *)fetch_result.c_str(), fetch_result.GetLength());
	    }
	    while ((IMAP_OK == result) && (specification_base < m_dwParsePointer))
	    {
		SendBlank();
		FETCH_NAME_T::iterator which = sFetchSymbolTable.find((char *)&m_pParseBuffer[specification_base]);
		if (sFetchSymbolTable.end() != which)
		{
		    nBlankLen = 1;
		    switch(which->second)
		    {
		    case FETCH_ALL:
			FetchResponseFlags(message.GetMessageFlags(), message.IsRecent());
			SendBlank();
			FetchResponseInternalDate(message);
			SendBlank();
			FetchResponseRfc822Size(message);
			SendBlank();
			FetchResponseEnvelope(message);
			break;

		    case FETCH_FAST:
			FetchResponseFlags(message.GetMessageFlags(), message.IsRecent());
			SendBlank();
			FetchResponseInternalDate(message);
			SendBlank();
			FetchResponseRfc822Size(message);
			break;

		    case FETCH_FULL:
			FetchResponseFlags(message.GetMessageFlags(), message.IsRecent());
			SendBlank();
			FetchResponseInternalDate(message);
			SendBlank();
			FetchResponseRfc822Size(message);
			SendBlank();
			FetchResponseEnvelope(message);
			SendBlank();
			FetchResponseBody(message);
			break;

		    case FETCH_BODY_PEEK:
		    case FETCH_BODY:
			FetchResponseBody(message);
			break;

		    case FETCH_BODYSTRUCTURE:
			FetchResponseBodyStructure(message);
			break;

		    case FETCH_ENVELOPE:
			FetchResponseEnvelope(message);
			break;

		    case FETCH_FLAGS:
			FetchResponseFlags(message.GetMessageFlags(), message.IsRecent());
			break;

		    case FETCH_INTERNALDATE:
			FetchResponseInternalDate(message);
			break;

		    case FETCH_RFC822:
			FetchResponseRfc822(dwUid, message);
			bSeenFlag = true;
			break;
						
		    case FETCH_RFC822_HEADER:
			FetchResponseRfc822Header(dwUid, message);
			break;

		    case FETCH_RFC822_SIZE:
			FetchResponseRfc822Size(message);
			break;

		    case FETCH_RFC822_TEXT:
			FetchResponseRfc822Text(dwUid, message);
			bSeenFlag = true;
			break;

		    case FETCH_UID:
			FetchResponseUid(dwUid);
			break;

		    default:
			strncpy(m_pResponseText, _T("Internal Logic Error"), MAX_RESPONSE_STRING_LENGTH);
//    					result = IMAP_NO;
			break;
		    }
		}
		else
		{
		    // This may be an extended BODY name, so I have to check for that.
		    char *temp;
		    temp = strchr((char *)&m_pParseBuffer[specification_base], '[');
		    if (NULL != temp)
		    {
			*temp = '\0';
			FETCH_NAME_T::iterator which = sFetchSymbolTable.find((char *)&m_pParseBuffer[specification_base]);
			specification_base += (DWORD) strlen((char *)&m_pParseBuffer[specification_base]) + 1;
			*temp = '[';
			if (sFetchSymbolTable.end() != which)
			{
			    switch(which->second)
			    {
			    case FETCH_BODY:
				bSeenFlag = true;
				// NOTE NO BREAK!
			    case FETCH_BODY_PEEK:
				Send((void *)_T("BODY["), 5);
				break;

			    default:
				strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
				result = IMAP_BAD;
				break;
			    }
			}
			else
			{
			    strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
			    result = IMAP_BAD;
			}
		    }
		    else
		    {
			strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_BAD;
		    }
		    MESSAGE_BODY body = message.GetMessageBody();
		    // I need a part number flag because, for a single part message, body[1] is
		    // different from body[], but later on, when I determine what part to fetch,
		    // I won't know whether I've got body[1] or body[], and bPartNumberFlag 
		    // records the difference
		    bool bPartNumberFlag = false;
		    while ((IMAP_OK == result) && isdigit(m_pParseBuffer[specification_base]))
		    {
			bPartNumberFlag = true;
			char *end;
			DWORD section = strtoul((char *)&m_pParseBuffer[specification_base], &end, 10);
			// This part here is because the message types have two headers, one for the
			// subpart the message is in, one for the message that's contained in the subpart
			// if I'm looking at a subpart of that subpart, then I have to skip the subparts
			// header and look at the message's header, which is handled by the next four
			// lines of code.
			if ((body.mmtBodyMediaType == MIME_TYPE_MESSAGE) && (1 == (DWORD)body.subparts->GetSize()))
			{
			    body = (*body.subparts)[0];
			}
			if (NULL != body.subparts)
			{
			    if ((0 < section) && (section <= ((DWORD)body.subparts->GetSize())))
			    {
				CStdString csSection;
				csSection.AppendFormat(_T("%d"), section);
				Send((void *)csSection.c_str(), csSection.GetLength());
				body = (*body.subparts)[section-1];
				specification_base += (DWORD) (end - ((char *)&m_pParseBuffer[specification_base]));
				if ('.' == *end)
				{
				    Send((void *)_T("."), 1);
				    ++specification_base;
				}
			    }
			    else
			    {
				strncpy(m_pResponseText, _T("Invalid Subpart"), MAX_RESPONSE_STRING_LENGTH);
				result = IMAP_NO;
			    }
			}
			else
			{
			    CStdString csSection;
			    csSection.AppendFormat(_T("%d"), section);
			    Send((void *)csSection.c_str(), csSection.GetLength());
			    specification_base += (DWORD) (end - ((char *)&m_pParseBuffer[specification_base]));
			}
		    }
		    // When I get here, and if everything's okay, then I'm looking for a possibly nonzero length of characters
		    // representing the section-msgtext
		    FETCH_BODY_PARTS which_part;
		    if ((IMAP_OK == result) && (m_dwParsePointer > specification_base))
		    {
			// If I'm out of string and the next string begins with ']', then I'm returning the whole body of 
			// whatever subpart I'm at
			if (']' == m_pParseBuffer[specification_base])
			{
			    // Unless it's a single part message -- in which case I return the text
			    if (!bPartNumberFlag || (NULL != body.subparts))
			    {
				which_part = FETCH_BODY_BODY;
			    }
			    else
			    {
				which_part = FETCH_BODY_TEXT;
			    }
			}
			else
			{
			    CStdString part((char *)&m_pParseBuffer[specification_base]);
			    // I'm looking for HEADER[.FIELDS[.NOT]], TEXT, or MIME
			    part.ToUpper();
			    if (part.Left(6).Equals(_T("header")))
			    {
				if (']' == part[6])
				{
				    specification_base += 6;
				    Send((void *)_T("HEADER"), 6);
				    which_part = FETCH_BODY_HEADER;
				}
				else
				{
				    if (part.Mid(6,7).Equals(_T(".FIELDS")))
				    {
					if (13 == part.GetLength())
					{
					    specification_base +=  (DWORD) strlen((char *)&m_pParseBuffer[specification_base]) + 1;
					    Send((void *)_T("HEADER.FIELDS"), 13);
					    which_part = FETCH_BODY_FIELDS;
					}
					else
					{
					    if (part.Mid(13).Equals(_T(".NOT")))
					    {
						specification_base +=  (DWORD) strlen((char *)&m_pParseBuffer[specification_base]) + 1;
						Send((void *)_T("HEADER.FIELDS.NOT"), 17);
						which_part = FETCH_BODY_NOT_FIELDS;
					    }
					    else
					    {
						strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
						result = IMAP_BAD;
					    }
					}
				    }
				    else
				    {
					strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
					result = IMAP_BAD;
				    }
				}
			    }
			    else
			    {
				if (part.Left(5).Equals(_T("TEXT]")))
				{
				    specification_base += 4;
				    Send((void *)_T("TEXT"), 4);;
				    which_part = FETCH_BODY_TEXT;
				}
				else
				{
				    if (part.Left(5).Equals(_T("MIME]")))
				    {
					specification_base += 4;
					Send((void *)_T("MIME"), 4);
					which_part = FETCH_BODY_MIME;
				    }
				    else
				    {
					strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
					result = IMAP_BAD;
				    }
				}
			    }
			}
		    }
		    // Now look for the list of headers to return
		    CSimArray<CStdString> field_list;
		    if ((IMAP_OK == result) && (m_dwParsePointer > specification_base) &&
			((FETCH_BODY_FIELDS == which_part) || (FETCH_BODY_NOT_FIELDS == which_part)))
		    {
			if (0 == strcmp((char *)&m_pParseBuffer[specification_base], _T("(")))
			{
			    Send((void *)_T(" ("), 2);
			    specification_base +=  (DWORD) strlen((char *)&m_pParseBuffer[specification_base]) + 1;
			    int nBlankLen = 0;
			    while((IMAP_BUFFER_LEN > specification_base) &&
				  (0 != strcmp((char *)&m_pParseBuffer[specification_base], _T(")"))))
			    {
				int len = (int) strlen((char *)&m_pParseBuffer[specification_base]);
				SendBlank();
				Send((void *)&m_pParseBuffer[specification_base], len);
				nBlankLen = 1;
				field_list.Add((char*)&m_pParseBuffer[specification_base], CompareStringsNoCase);
				specification_base +=  (DWORD) len + 1;
			    }
			    if (0 == strcmp((char *)&m_pParseBuffer[specification_base], _T(")")))
			    {
				Send((void *)_T(")"), 1);
				specification_base +=  (DWORD) strlen((char *)&m_pParseBuffer[specification_base]) + 1;
			    }
			    else
			    {
				strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
				result = IMAP_BAD;
			    }
			}
			else
			{
			    strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
			    result = IMAP_BAD;
			}
		    }

		    // Okay, look for the end square bracket
		    if ((IMAP_OK == result) && (']' == m_pParseBuffer[specification_base]))
		    {
			Send((void *)_T("]"), 1);
			specification_base++;
		    }
		    else
		    {
			strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_BAD;
		    }

		    // Okay, get the limits, if any.
		    DWORD first_byte = 0, max_length = ~0;
		    if ((IMAP_OK == result) && (m_dwParsePointer > specification_base))
		    {
			if ('<' == m_pParseBuffer[specification_base])
			{
			    ++specification_base;
			    char *end;
			    first_byte = strtoul((char *)&m_pParseBuffer[specification_base], &end, 10);
			    CStdString csLimit;
                            csLimit.Format(_T("<%d>"), first_byte);
			    Send((void *)csLimit.c_str(), csLimit.GetLength());
			    if ('.' == *end)
			    {
				max_length = strtoul(end+1, &end, 10);
			    }
			    if ('>' != *end)
			    {
				strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
				result = IMAP_BAD;
			    }
			}
		    }

		    if (IMAP_OK == result)
		    {
			DWORD length;

			switch(which_part)
			{
			case FETCH_BODY_BODY:
			    // This returns all the data header and text, but it's limited by first_byte and max_length
			    if (first_byte < body.dwBodyOctets)
			    {
				length = MIN(body.dwBodyOctets - first_byte, max_length);
				Send((void *)_T(" "), 1);
				SendMessageChunk(dwUid, first_byte + body.dwBodyStartOffset, length);
			    }
			    else
			    {
				Send((void *)_T(" {0}\r\n"), 6);
			    }
			    break;

			case FETCH_BODY_HEADER:
			    if ((body.mmtBodyMediaType == MIME_TYPE_MESSAGE) && (1 == (DWORD)body.subparts->GetSize()))
			    {
				body = (*body.subparts)[0];
			    }
			    // NOTE NO BREAK!  IT FALLS THROUGH!
			case FETCH_BODY_MIME:
			    if (first_byte < body.dwHeaderOctets)
			    {
				length = MIN(body.dwHeaderOctets - first_byte, max_length);
				Send((void *)_T(" "), 1);
				SendMessageChunk(dwUid, first_byte + body.dwBodyStartOffset, length);
			    }
			    else
			    {
				Send((void *)_T(" {0}\r\n"), 6);
			    }
			    break;

			case FETCH_BODY_FIELDS:
			    if ((body.mmtBodyMediaType == MIME_TYPE_MESSAGE) && (1 == (DWORD)body.subparts->GetSize()))
			    {
				body = (*body.subparts)[0];
			    }
			    {
				CStdString interesting;

				// For each element in field_list, look up the corresponding entries in body.hfFieldList
				// and display
				for(int i=0; i<field_list.GetSize(); ++i)
				{
				    for (HEADER_FIELDS::const_iterator iter = body.hfFieldList.lower_bound(field_list[i]);
					 body.hfFieldList.upper_bound(field_list[i]) != iter; ++iter)
				    {
					interesting += iter->first + _T(": ") + iter->second + _T("\r\n");
				    }
				}
				interesting += _T("\r\n");
				if (first_byte < (DWORD) interesting.GetLength())
				{
				    length = MIN(interesting.GetLength() - first_byte, max_length);
				    CStdString csLiteral;
				    CStdString csHeaderFields;
				    csHeaderFields.Format(_T(" {%d}\r\n"), length);
				    csHeaderFields += interesting.Mid(first_byte, length);
				    Send((void *)csHeaderFields.c_str(), csHeaderFields.GetLength());
				}
				else
				{
				    Send((void *)_T(" {0}\r\n"), 6);
				}
			    }
			    break;

			case FETCH_BODY_NOT_FIELDS:
			    if ((body.mmtBodyMediaType == MIME_TYPE_MESSAGE) && (1 == (DWORD)body.subparts->GetSize()))
			    {
				body = (*body.subparts)[0];
			    }
			    {
				CStdString interesting;

				for (HEADER_FIELDS::const_iterator i = body.hfFieldList.begin(); 
				     body.hfFieldList.end() != i; ++i)
				{
				    int dummy;
				    if (!field_list.Find(i->first, dummy, CompareStringsNoCase))
				    {
					interesting += i->first + _T(": ") + i->second + _T("\r\n");
				    }
				}
				interesting += _T("\r\n");
				if (first_byte < (DWORD) interesting.GetLength())
				{
				    length = MIN(interesting.GetLength() - first_byte, max_length);
				    CStdString csLiteral;
				    CStdString csHeaderFields;
				    csHeaderFields.Format(_T(" {%d}\r\n"), length);
				    csHeaderFields += interesting.Mid(first_byte, length);
				    Send((void *)csHeaderFields.c_str(), csHeaderFields.GetLength());
				}
				else
				{
				    Send((void *)_T(" {0}\r\n"), 6);
				}
			    }
			    break;

			case FETCH_BODY_TEXT:
			    if ((body.mmtBodyMediaType == MIME_TYPE_MESSAGE) && (1 == (DWORD)body.subparts->GetSize()))
			    {
				body = (*body.subparts)[0];
			    }
			    if (0 < body.dwHeaderOctets)
			    {
				CStdString result;
				DWORD length;
				if (body.dwBodyOctets > body.dwHeaderOctets)
				{
				    length = body.dwBodyOctets - body.dwHeaderOctets;
				}
				else
				{
				    length = 0;
				}
				if (first_byte < length)
				{
				    length = MIN(length - first_byte, max_length);
				    Send((void *)_T(" "), 1);
				    SendMessageChunk(dwUid, first_byte + body.dwBodyStartOffset + body.dwHeaderOctets, length);
				}
				else
				{
				    Send((void *)_T(" {0}\r\n"), 6);
				}
			    }
			    else
			    {
				Send((void *)_T(" {0}\r\n"), 6);
			    }
			    break;

			default:
			    strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
			    result = IMAP_BAD;
			    break; 
			}
		    }
		}
		nBlankLen = 1; 
		specification_base += (DWORD) strlen((char *)&m_pParseBuffer[specification_base]) + 1;
		if (0 == strcmp(_T(")"), (char *)&m_pParseBuffer[specification_base]))
		{
		    specification_base += (DWORD) strlen((char *)&m_pParseBuffer[specification_base]) + 1;
		}
	    }
	    // Here, I update the SEEN flag, if that is necessary, and send the flags if that flag has
	    // changed state
	    if (IMAP_OK == result)
	    {
		if (bSeenFlag && (0 == (IMAP_MESSAGE_SEEN & message.GetMessageFlags())))
		{
		    DWORD updated_flags;
		    if (CMailStore::SUCCESS == m_msStore->MessageUpdateFlags(srVector[i], ~0, IMAP_MESSAGE_SEEN, updated_flags))
		    {
			SendBlank();
			FetchResponseFlags(updated_flags, message.IsRecent());
			nBlankLen = 1;
		    }
		}
		if (bUsingUid)
		{
		    SendBlank();
		    FetchResponseUid(dwUid);
		    nBlankLen = 1;
		}
		Send((void *)_T(")\r\n"), 3);
		nBlankLen = 0;
	    }
	    else
	    {
		final_result = result;
	    }
	}
	else
	{
	    final_result = IMAP_NO;
	    if (CMailMessage::MESSAGE_DOESNT_EXIST != message_read_result)
	    {
		strncpy(m_pResponseText, _T("Internal Mailstore Error"), MAX_RESPONSE_STRING_LENGTH);
	    }
	    else
	    {
		strncpy(m_pResponseText, _T("Message Doesn't Exist"), MAX_RESPONSE_STRING_LENGTH);
	    }
	}
// ^-- This part executes the fetch request
    }
    return final_result;
}

IMAP_RESULTS ImapSession::FetchHandlerInternal(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt, bool bUsingUid)
{
    IMAP_RESULTS result = IMAP_OK;

    m_bUsesUid = bUsingUid;
    // The first thing up is a sequence set.  This is a sequence of digits, commas, and colons, followed by
    // a space
    size_t len = strspn(((char *)pData)+r_dwParsingAt, _T("0123456789,:*"));
    if ((0 < len) && (dwDataLen >= (r_dwParsingAt + len + 2)) && (' ' == pData[r_dwParsingAt+len]) && 
	(IMAP_BUFFER_LEN > (m_dwParsePointer+len+1)))
    {
	AddToParseBuffer(&pData[r_dwParsingAt], (DWORD)len);
	r_dwParsingAt += (DWORD) len + 1;

	m_dwParseStage = 0;
	if ('(' == pData[r_dwParsingAt])
	{
	    AddToParseBuffer(&pData[r_dwParsingAt], 1);
	    ++r_dwParsingAt;
	    m_dwParseStage = 1;
	    while ((IMAP_OK == result) && (dwDataLen > r_dwParsingAt) && (0 < m_dwParseStage))
	    {
		if((dwDataLen > r_dwParsingAt) && (' ' == pData[r_dwParsingAt]))
		{
		    ++r_dwParsingAt;
		}
		if((dwDataLen > r_dwParsingAt) && ('(' == pData[r_dwParsingAt]))
		{
		    AddToParseBuffer(&pData[r_dwParsingAt], 1);
		    ++r_dwParsingAt;
		    ++m_dwParseStage;
		}
		switch (astring(pData, dwDataLen, r_dwParsingAt, true, NULL))
		{
		case ImapStringGood:
		    result = IMAP_OK;
		    break;

		case ImapStringBad:
		    strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_BAD;
		    break;

		case ImapStringPending:
		    m_eInProgress = ImapCommandFetch;
		    strncpy(m_pResponseText, _T("Ready for Literal"), MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_NOTDONE;
		    break;

		default:
		    strncpy(m_pResponseText, _T("Failed"), MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_BAD;
		    break;
		}
		if ((dwDataLen > r_dwParsingAt) && (')' == pData[r_dwParsingAt]))
		{
		    AddToParseBuffer(&pData[r_dwParsingAt], 1);
		    ++r_dwParsingAt;
		    --m_dwParseStage;
		}
	    }
	    if ((IMAP_OK == result) && (0 == m_dwParseStage))
	    {
		if (dwDataLen == r_dwParsingAt)
		{
		    result = FetchHandlerExecute(bUsingUid);
		}
		else
		{
		    strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_BAD;
		}
	    }
	}
	else
	{
	    while ((IMAP_OK == result) && (dwDataLen > r_dwParsingAt))
	    {
		if (' ' == pData[r_dwParsingAt])
		{
		    ++r_dwParsingAt;
		}
		if ((dwDataLen > r_dwParsingAt) && ('(' == pData[r_dwParsingAt]))
		{
		    AddToParseBuffer(&pData[r_dwParsingAt], 1);
		    ++r_dwParsingAt;
		}
		if ((dwDataLen > r_dwParsingAt) && (')' == pData[r_dwParsingAt]))
		{
		    AddToParseBuffer(&pData[r_dwParsingAt], 1);
		    ++r_dwParsingAt;
		}
		switch (astring(pData, dwDataLen, r_dwParsingAt, true, NULL))
		{
		case ImapStringGood:
		    break;

		case ImapStringBad:
		    strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_BAD;
		    break;

		case ImapStringPending:
		    m_eInProgress = ImapCommandFetch;
		    strncpy(m_pResponseText, _T("Ready for Literal"), MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_NOTDONE;
		    break;

		default:
		    strncpy(m_pResponseText, _T("Failed"), MAX_RESPONSE_STRING_LENGTH);
		    result = IMAP_BAD;
		    break;
		}
	    }
	    if (IMAP_OK == result)
	    {
		result = FetchHandlerExecute(bUsingUid);
	    }
	}
    }
    else
    {
	strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
	result = IMAP_BAD;
    }
    return result;
}

IMAP_RESULTS ImapSession::FetchHandler(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt)
{
    return FetchHandlerInternal(pData, dwDataLen, r_dwParsingAt, false);
}

IMAP_RESULTS ImapSession::StoreHandlerInternal(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt, bool bUsingUid)
{
    IMAP_RESULTS result = IMAP_OK;
    DWORD execute_pointer = m_dwParsePointer;
    int update = '=';

    while((r_dwParsingAt < dwDataLen) && (' ' != pData[r_dwParsingAt]))
    {
	AddToParseBuffer(&pData[r_dwParsingAt++], 1, false);
    }
    AddToParseBuffer(NULL, 0);
    if ((r_dwParsingAt < dwDataLen) && (' ' == pData[r_dwParsingAt]))
    {
	++r_dwParsingAt;
    }
    else
    {
	strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
	result = IMAP_BAD;
    }

    if ((IMAP_OK == result) && (('-' == pData[r_dwParsingAt]) || ('+' == pData[r_dwParsingAt])))
    {
	update = pData[r_dwParsingAt++];
    }
    while((IMAP_OK == result) && (r_dwParsingAt < dwDataLen) && (' ' != pData[r_dwParsingAt]))
    {
	byte temp = toupper(pData[r_dwParsingAt++]);
	AddToParseBuffer(&temp, 1, false);
    }
    AddToParseBuffer(NULL, 0);
    if ((r_dwParsingAt < dwDataLen) && (' ' == pData[r_dwParsingAt]))
    {
	++r_dwParsingAt;
    }
    else
    {
	strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
	result = IMAP_BAD;
    }

    DWORD my_dwDataLen = dwDataLen;
    if ((IMAP_OK == result) && (r_dwParsingAt < dwDataLen) && ('(' == pData[r_dwParsingAt]))
    {
	++r_dwParsingAt;
	if (')' == pData[dwDataLen-1])
	{
	    --my_dwDataLen;
	}
	else
	{
	    strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
	    result = IMAP_BAD;
	}
    }

    while((IMAP_OK == result) && (r_dwParsingAt < my_dwDataLen))
    {
	// These can only be spaces, backslashes, or alpha
	if (isalpha(pData[r_dwParsingAt]) || (' ' == pData[r_dwParsingAt]) || ('\\' == pData[r_dwParsingAt]))
	{
	    if (' ' == pData[r_dwParsingAt])
	    {
		++r_dwParsingAt;
		AddToParseBuffer(NULL, 0);
	    }
	    else
	    {
		byte temp = toupper(pData[r_dwParsingAt++]);
		AddToParseBuffer(&temp, 1, false);
	    }
	}
	else
	{
	    strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
	    result = IMAP_BAD;
	}
    }
    AddToParseBuffer(NULL, 0);
    if (r_dwParsingAt != my_dwDataLen)
    {
	strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
	result = IMAP_BAD;
    }

    if (IMAP_OK == result)
    {
	SEARCH_RESULT srVector;
	bool bSequenceOk;
	if (bUsingUid)
	{
	    bSequenceOk = UidSequenceSet(srVector, execute_pointer);
	}
	else
	{
	    bSequenceOk = MsnSequenceSet(srVector, execute_pointer);
	}
	if (bSequenceOk)
	{
	    execute_pointer += (DWORD) strlen((char *)&m_pParseBuffer[execute_pointer]) + 1;
	    if (0 == strncmp((char *)&m_pParseBuffer[execute_pointer], _T("FLAGS"), 5))
	    {
		bool bSilentFlag = false;
		// I'm looking for "FLAGS.SILENT and the "+5" is because FLAGS is five characters long
		if ('.' == m_pParseBuffer[execute_pointer+5])
		{
		    if (0 == strcmp((char *)&m_pParseBuffer[execute_pointer+6], _T("SILENT")))
		    {
			bSilentFlag = true;
		    }
		    else
		    {
			strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_BAD;
		    }
		}
		if (IMAP_OK == result)
		{
		    DWORD flagSet = 0;

		    execute_pointer += (DWORD) strlen((char *)&m_pParseBuffer[execute_pointer]) + 1;
		    while((IMAP_OK == result) && (m_dwParsePointer > execute_pointer))
		    {
			if ('\\' == m_pParseBuffer[execute_pointer])
			{
			    FLAG_SYMBOL_T::iterator found = sFlagSymbolTable.find((char *)&m_pParseBuffer[execute_pointer+1]);
			    if (sFlagSymbolTable.end() != found)
			    {
				flagSet |= found->second;
			    }
			    else
			    {
				strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
				result = IMAP_BAD;
			    }
			}
			else
			{
			    strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
			    result = IMAP_BAD;
			}
			execute_pointer += (DWORD) strlen((char *)&m_pParseBuffer[execute_pointer]) + 1;
		    }
		    if (IMAP_OK == result)
		    {
			DWORD dwAndMask, dwOrMask;
			switch (update)
			{
			case '+':
			    // Add these flags
			    dwAndMask = ~0;
			    dwOrMask = flagSet;
			    break;
	
			case '-':
			    // Remove these flags
			    dwAndMask = ~flagSet;
			    dwOrMask = 0;
			    break;
	
			default:
			    // Set these flags
			    dwAndMask = 0;
			    dwOrMask = flagSet;
			    break;
			}
			for (int i=0; i < srVector.GetSize(); ++i)
			{
			    if (0 != srVector[i])
			    {
				DWORD flags;
				switch (m_msStore->MessageUpdateFlags(srVector[i], dwAndMask, dwOrMask, flags))
				{
				case CMailStore::SUCCESS:
				    if (!bSilentFlag)
				    {
					CStdString fetch;
					fetch.AppendFormat(_T("* %d FETCH ("), m_msStore->MailboxUidToMsn(srVector[i]));
					Send((void *)fetch.c_str(), (int)fetch.GetLength());
					FetchResponseFlags(flags, srVector[i] >= m_msStore->GetFirstRecentUid());
					if (bUsingUid)
					{
					    Send((void *)_T(" "), 1);
					    FetchResponseUid(srVector[i]);
					}
					Send((void *)_T(")\r\n"), 3);
				    }
				    break;

				case CMailStore::MAILBOX_READ_ONLY:
				    strncpy(m_pResponseText, _T("Mailbox Read Only"), MAX_RESPONSE_STRING_LENGTH);
				    result = IMAP_NO;
				    break;

				case CMailStore::MESSAGE_NOT_FOUND:
				    strncpy(m_pResponseText, _T("Message Not Found"), MAX_RESPONSE_STRING_LENGTH);
				    result = IMAP_NO;
				    break;

				default:
				    strncpy(m_pResponseText, _T("Unable to set flags"), MAX_RESPONSE_STRING_LENGTH);
				    result = IMAP_NO;
				    break;
				}
			    }
			}
		    }
		}
	    }
	    else
	    {
		strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
		result = IMAP_BAD;
	    }
	}
	else
	{
	    strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
	    result = IMAP_BAD;
	}
    }
    return result;
}

IMAP_RESULTS ImapSession::StoreHandler(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt)
{
    return StoreHandlerInternal(pData, dwDataLen, r_dwParsingAt, false);
}

IMAP_RESULTS ImapSession::CopyHandlerExecute(bool bUsingUid)
{
    IMAP_RESULTS result = IMAP_OK;
    DWORD execute_pointer = 0;

    // Skip the first two tokens that are the tag and the command
    execute_pointer += (DWORD) strlen((char *)m_pParseBuffer) + 1;
    execute_pointer += (DWORD) strlen((char *)&m_pParseBuffer[execute_pointer]) + 1;

    // I do this once again if bUsingUid is set because the command in that case is UID
    if (bUsingUid)
    {
	execute_pointer += (DWORD) strlen((char *)&m_pParseBuffer[execute_pointer]) + 1;
    }

    SEARCH_RESULT srVector;
    bool bSequenceOk;
    if (bUsingUid)
    {
	bSequenceOk = UidSequenceSet(srVector, execute_pointer);
    }
    else
    {
	bSequenceOk = MsnSequenceSet(srVector, execute_pointer);
    }
    if (bSequenceOk)
    {
	NUMBER_LIST nlMessageUidsAdded;

	execute_pointer += (DWORD) strlen((char *)&m_pParseBuffer[execute_pointer]) + 1;
	CStdString MailboxName((char *)&m_pParseBuffer[execute_pointer]);
	MailboxName.Replace('.', '\\');
	for (int i=0; (IMAP_OK == result) && (i < srVector.GetSize()); ++i)
	{
	    CMailMessage message;

	    CMailMessage::MAIL_MESSAGE_RESULT message_read_result = message.parse(m_msStore, srVector[i]);
	    if (CMailMessage::SUCCESS == message_read_result)
	    {
		DWORD new_uid;
		CSimpleDate when = message.GetInternalDate();

		CSimFile infile;
		DWORD dwDummy;
		CSimpleDate sdDummy(MMDDYYYY, true);
		bool bDummy;
		CStdString csPhysicalFileName = m_msStore->GetMessagePhysicalPath(srVector[i], dwDummy, sdDummy, bDummy);

		if (infile.Open(csPhysicalFileName, CSimFile::modeRead))
		{
		    char *buffer;
		    DWORD bufferLen = message.GetMessageBody().dwBodyOctets;
		    buffer = new char[bufferLen];

		    DWORD dwAmountRead;
		    dwAmountRead = infile.Read(buffer, bufferLen);
		    infile.Close();
		    CMailStore::MAIL_STORE_RESULT mr = m_msStore->AddMessageToMailbox(MailboxName, buffer, dwAmountRead,
										      when, 0, &new_uid);
		    delete[] buffer;

		    switch(mr)
		    {
		    case CMailStore::SUCCESS:
			nlMessageUidsAdded.Add(new_uid);
			break;

		    case CMailStore::MAILBOX_NOT_SELECTABLE:
		    case CMailStore::MAILBOX_DOES_NOT_EXIST:
			strncpy(m_pResponseText, _T("Mailbox Doesn't Exist"), MAX_RESPONSE_STRING_LENGTH);
			strncpy(m_pResponseCode, _T("[TRYCREATE]"), MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_NO;
			break;

		    default:
			strncpy(m_pResponseText, _T("General Error Copying"), MAX_RESPONSE_STRING_LENGTH);
			result = IMAP_NO;
			break;
		    }
		}
		else
		{
		    strncpy(m_pResponseText, _T("Unable to Read Message"), MAX_RESPONSE_STRING_LENGTH);
		    Log("Copy Handler unable to read user %d's mail file \"%s\\message.%08x\"\n", m_pUser->m_nUserID,
			m_msStore->GetMailboxUserPath().c_str(), srVector[i]);
		    result = IMAP_NO;
		    break;
		}
	    }
	}
	if (IMAP_OK != result)
	{
	    for (int i=0; i<nlMessageUidsAdded.GetSize(); ++i)
	    {
		m_msStore->DeleteMessage(MailboxName, nlMessageUidsAdded[i]);
	    }
	}
    }
    else
    {
	strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
	result = IMAP_BAD;
    }
    return result;
}

IMAP_RESULTS ImapSession::CopyHandlerInternal(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt, bool bUsingUid)
{
    IMAP_RESULTS result = IMAP_OK;

    while((r_dwParsingAt < dwDataLen) && (' ' != pData[r_dwParsingAt]))
    {
	AddToParseBuffer(&pData[r_dwParsingAt++], 1, false);
    }
    AddToParseBuffer(NULL, 0);
    if ((r_dwParsingAt < dwDataLen) && (' ' == pData[r_dwParsingAt]))
    {
	++r_dwParsingAt;
    }
    else
    {
	strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
	result = IMAP_BAD;
    }

    if (IMAP_OK == result)
    {
	switch (astring(pData, dwDataLen, r_dwParsingAt, true, NULL))
	{
	case ImapStringGood:
	    result = CopyHandlerExecute(bUsingUid);
	    break;

	case ImapStringBad:
	    strncpy(m_pResponseText, _T("Malformed Command"), MAX_RESPONSE_STRING_LENGTH);
	    result = IMAP_BAD;
	    break;

	case ImapStringPending:
	    result = IMAP_NOTDONE;
	    m_bUsesUid = bUsingUid;
	    m_eInProgress = ImapCommandCopy;
	    strncpy(m_pResponseText, _T("Ready for Literal"), MAX_RESPONSE_STRING_LENGTH);
	    break;

	default:
	    strncpy(m_pResponseText, _T("Failed"), MAX_RESPONSE_STRING_LENGTH);
	    break;
	}
    }
    return result;
}

IMAP_RESULTS ImapSession::CopyHandler(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt)
{
    return CopyHandlerInternal(pData, dwDataLen, r_dwParsingAt, false);
}
#endif // 0
