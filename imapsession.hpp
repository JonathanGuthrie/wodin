#if !defined(_IMAPSESSION_HPP_INCLUDED_)
#define _IMAPSESSION_HPP_INCLUDED_

#include <iostream>

#include <map>
#include <string>

#include <stdint.h>

#include "socket.hpp"
#include "imapuser.hpp"
#include "mailstore.hpp"
#include "sasl.hpp"
#include "insensitive.hpp"

class ImapServer;

enum ImapState
{
    ImapNotAuthenticated = 0,
    ImapAuthenticated,
    ImapSelected,
    ImapLogoff
};

enum ImapCommandInProgress {
    ImapCommandNone,
    ImapCommandAuthenticate,
    ImapCommandAppend,
    ImapCommandLogin,
    ImapCommandSelect,
    ImapCommandExamine,
    ImapCommandCreate,
    ImapCommandDelete,
    ImapCommandRename,
    ImapCommandSubscribe,
    ImapCommandUnsubscribe,
    ImapCommandList,
    ImapCommandLsub,
    ImapCommandStatus,
    ImapCommandSearch,
    ImapCommandFetch,
    ImapCommandCopy
};

enum ImapStringState
{
    ImapStringGood,
    ImapStringBad,
    ImapStringPending
};

/*--------------------------------------------------------------------------------------*/
//* Imap											*/
/*--------------------------------------------------------------------------------------*/

class ImapSession;

typedef enum
{
    IMAP_OK,
    IMAP_NO,
    IMAP_BAD,
    IMAP_NOTDONE,
    IMAP_NO_WITH_PAUSE
} IMAP_RESULTS;

#define MAX_RESPONSE_STRING_LENGTH	80
#define IMAP_BUFFER_LEN			8192

typedef struct {
    bool levels[ImapLogoff+1];
    IMAP_RESULTS (ImapSession::*handler)(uint8_t *data, const size_t dataLen, size_t &parsingAt);
} symbol;

// SYZYGY This should actually be defined like this:
// typedef std::map<std::string, symbol, less_no_case> IMAPSYMBOLS;
// where less_no_case is a function that does not distinguish case
typedef std::map<insensitiveString, symbol> IMAPSYMBOLS;

class ImapSession
{
public:
    ImapSession(Socket *sock, ImapServer *server, unsigned failedLoginPause = 5);
    ~ImapSession();
    static void BuildSymbolTables(void);
    int ReceiveData(uint8_t* pData, size_t dwDataLen );
    ImapServer *GetServer(void) const { return server; }
    time_t GetLastCommandTime() const { return lastCommandTime; }
    Socket *GetSocket(void) const { return s; }
    ImapState GetState(void) const { return state; }

private:
    // These are configuration items
    bool m_LoginDisabled;
    static bool anonymousEnabled;
    unsigned failedLoginPause;

    Socket *s;
    // These constitute the session's state
    enum ImapState state;
    enum ImapCommandInProgress inProgress;

    uint32_t mailFlags;
    uint8_t *lineBuffer;			// This buffers the incoming data into lines to be processed
    uint32_t lineBuffPtr, lineBuffLen;
    // As lines come in from the outside world, they are processed into m_bBuffer, which holds
    // them until the entire command has been received at which point it can be processed, often
    // by the *Execute() methods.
    uint8_t  *parseBuffer;
    uint32_t parseBuffLen;
    uint32_t parsePointer; 			// This points past the end of what's been put into the parse buffer

    // This is associated with handling appends
    uint32_t m_dwAppendingUid;
    MailStore *store;
    char responseCode[MAX_RESPONSE_STRING_LENGTH+1];
    char responseText[MAX_RESPONSE_STRING_LENGTH+1];
    uint32_t commandString, arguments;
    uint32_t parseStage;
    uint32_t literalLength;
    bool m_bUsesUid;

    // These are used to send live updates in case the mailbox is accessed by multiple client simultaneously
    uint32_t currentNextUid, currentMessageCount;
    uint32_t currentRecentCount, currentUnseen, currentUidValidity;

    static IMAPSYMBOLS symbols;

    // ReceiveData processes data as it comes it.  It's primary job is to chop the incoming data into
    // lines and to pass each line to HandleOneLine, which is the core command processor for the system
    int HandleOneLine(uint8_t *pData, size_t dwDataLen);
    // This creates a properly formatted capability string based on the current state and configuration
    // of the IMAP server 
    std::string BuildCapabilityString(void);
#if 0
    // This sends the untagged responses associated with selecting a mailbox
    void SendSelectData(const CStdString &mailbox, bool bIsReadWrite);
#endif // 0
    // This appends data to the Parse Buffer, extending the parse buffer as necessary.
    void AddToParseBuffer(const uint8_t *data, size_t length, bool bNulTerminate = true);

    IMAP_RESULTS LoginHandlerExecute();
#if 0
    IMAP_RESULTS SearchHandlerInternal(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt, bool bUsingUid);
    IMAP_RESULTS SearchKeyParse(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt);
    IMAP_RESULTS StoreHandlerInternal(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt, bool bUsingUid);
    IMAP_RESULTS FetchHandlerInternal(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt, bool bUsingUid);
    IMAP_RESULTS CopyHandlerInternal(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt, bool bUsingUid);
    DWORD ReadEmailFlags(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt, bool &r_okay);
    bool UpdateSearchTerms(CMailSearch &searchTerm, DWORD &r_dwTokenPointer, bool isSubExpression);
    bool MsnSequenceSet(SEARCH_RESULT &r_srVector, DWORD &r_dwTokenPointer);
    bool UidSequenceSet(SEARCH_RESULT &r_srVector, DWORD &r_dwTokenPointer);
    bool UpdateSearchTerm(CMailSearch &searchTerm, DWORD &r_dwTokenPointer);
    IMAP_RESULTS SelectHandlerExecute(bool isReadWrite = true);
#endif // 0
    IMAP_RESULTS CreateHandlerExecute();
#if 0
    IMAP_RESULTS DeleteHandlerExecute();
    IMAP_RESULTS RenameHandlerExecute();
#endif // 0
    IMAP_RESULTS SubscribeHandlerExecute(bool isSubscribe);
    IMAP_RESULTS ListHandlerExecute(bool listAll);
#if 0
    IMAP_RESULTS StatusHandlerExecute(byte *pData, const DWORD dwDataLen, DWORD dwParsingAt);
    IMAP_RESULTS AppendHandlerExecute(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt);
    IMAP_RESULTS SearchHandlerExecute(bool bUsingUid);
    IMAP_RESULTS FetchHandlerExecute(bool bUsingUid);
    IMAP_RESULTS CopyHandlerExecute(bool bUsingUid);
#endif // 0

    void atom(uint8_t *data, const size_t dataLen, size_t &parsingAt); 
    enum ImapStringState astring(uint8_t *pData, const size_t dataLen, size_t &parsingAt, bool makeUppercase,
				 const char *additionalChars);
    std::string FormatTaggedResponse(IMAP_RESULTS status);

    // This is what is called if a command is unrecognized or if a command is not implemented
    IMAP_RESULTS UnimplementedHandler();

    // What follows are the handlers for the various IMAP command.  These are grouped similarly to the
    // way the commands are grouped in RFC-3501.  They all have identical function signatures so that
    // they can be called indirectly through a function pointer returned from the command map.
    IMAP_RESULTS CapabilityHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt);
    IMAP_RESULTS NoopHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt);
    IMAP_RESULTS LogoutHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt);

    IMAP_RESULTS LoginHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt);
    IMAP_RESULTS StarttlsHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt);
    IMAP_RESULTS AuthenticateHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt);
 
#if 0
    IMAP_RESULTS NamespaceHandler(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt);
    IMAP_RESULTS SelectHandler(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt);
    IMAP_RESULTS ExamineHandler(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt);
#endif // 0
    IMAP_RESULTS CreateHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt);
#if 0
    IMAP_RESULTS DeleteHandler(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt);
    IMAP_RESULTS RenameHandler(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt);
#endif // 0
    IMAP_RESULTS SubscribeHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt);
    IMAP_RESULTS UnsubscribeHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt);
    IMAP_RESULTS ListHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt);
    IMAP_RESULTS LsubHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt);
#if 0
    IMAP_RESULTS StatusHandler(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt);
    IMAP_RESULTS AppendHandler(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt);

    IMAP_RESULTS CheckHandler(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt);
    IMAP_RESULTS CloseHandler(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt);
    IMAP_RESULTS ExpungeHandler(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt);
    IMAP_RESULTS SearchHandler(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt);
    IMAP_RESULTS FetchHandler(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt);
    IMAP_RESULTS StoreHandler(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt);
    IMAP_RESULTS CopyHandler(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt);
    IMAP_RESULTS UidHandler(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt);

    // These are for fetches, which are special because they can generate arbitrarily large responses
    void FetchResponseFlags(DWORD flags, bool isRecent);
    void FetchResponseInternalDate(CMailMessage &message);
    void FetchResponseRfc822(DWORD uid, const CMailMessage &message);
    void FetchResponseRfc822Header(DWORD uid, const CMailMessage &message);
    void FetchResponseRfc822Size(const CMailMessage &message);
    void FetchResponseRfc822Text(DWORD uid, const CMailMessage &message);
    void FetchResponseEnvelope(const CMailMessage &message);
    void FetchResponseBodyStructure(const CMailMessage &message);
    void FetchResponseBody(const CMailMessage &message);
    void FetchResponseUid(DWORD uid);
    void SendMessageChunk(DWORD uid, DWORD offset, DWORD length);
#endif // 0
    ImapUser *userData;
    ImapServer *server;
    Sasl *auth;
    time_t lastCommandTime;
};

#endif //_IMAPSESSION_HPP_INCLUDED_
