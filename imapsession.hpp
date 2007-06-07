#if !defined(_IMAPSESSION_HPP_INCLUDED_)
#define _IMAPSESSION_HPP_INCLUDED_

#include <iostream>

#include <map>
#include <string>

#include <stdint.h>

#include "socket.hpp"
#include "imapuser.hpp"
#include "namespace.hpp"
#include "sasl.hpp"
#include "insensitive.hpp"
#include "mailsearch.hpp"
#include "mailmessage.hpp"

class ImapServer;
class SessionDriver;
class MailSearch;
class MailMessage;

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
    IMAP_NO_WITH_PAUSE,
    IMAP_MBOX_ERROR
} IMAP_RESULTS;

#define MAX_RESPONSE_STRING_LENGTH	80
#define IMAP_BUFFER_LEN			8192

typedef struct {
    bool levels[ImapLogoff+1];
    IMAP_RESULTS (ImapSession::*handler)(uint8_t *data, const size_t dataLen, size_t &parsingAt);
} symbol;

typedef std::map<insensitiveString, symbol> IMAPSYMBOLS;

class ImapSession
{
public:
    ImapSession(Socket *sock, ImapServer *server, SessionDriver *driver, unsigned failedLoginPause = 5);
    ~ImapSession();
    static void BuildSymbolTables(void);
    int ReceiveData(uint8_t* pData, size_t dwDataLen );
    ImapServer *GetServer(void) const { return m_server; }
    time_t GetLastCommandTime() const { return m_lastCommandTime; }
    Socket *GetSocket(void) const { return m_s; }
    ImapState GetState(void) const { return m_state; }
    const ImapUser *GetUser(void) const { return m_userData; }

private:
    // These are configuration items
    bool m_LoginDisabled;
    static bool m_anonymousEnabled;
    unsigned m_failedLoginPause;
    SessionDriver *m_driver;
    MailStore::MAIL_STORE_RESULT m_mboxErrorCode;

    Socket *m_s;
    // These constitute the session's state
    enum ImapState m_state;
    enum ImapCommandInProgress m_inProgress;

    uint32_t m_mailFlags;
    uint8_t *m_lineBuffer;			// This buffers the incoming data into lines to be processed
    uint32_t m_lineBuffPtr, m_lineBuffLen;
    // As lines come in from the outside world, they are processed into m_bBuffer, which holds
    // them until the entire command has been received at which point it can be processed, often
    // by the *Execute() methods.
    uint8_t  *m_parseBuffer;
    uint32_t m_parseBuffLen;
    uint32_t m_parsePointer; 			// This points past the end of what's been put into the parse buffer

    // This is associated with handling appends
    uint32_t m_appendingUid;
    Namespace *m_store;
    char m_responseCode[MAX_RESPONSE_STRING_LENGTH+1];
    char m_responseText[MAX_RESPONSE_STRING_LENGTH+1];
    uint32_t m_commandString, m_arguments;
    uint32_t m_parseStage;
    uint32_t m_literalLength;
    bool m_usesUid;

    // These are used to send live updates in case the mailbox is accessed by multiple client simultaneously
    uint32_t m_currentNextUid, m_currentMessageCount;
    uint32_t m_currentRecentCount, m_currentUnseen, m_currentUidValidity;

    static IMAPSYMBOLS m_symbols;

    // ReceiveData processes data as it comes it.  It's primary job is to chop the incoming data into
    // lines and to pass each line to HandleOneLine, which is the core command processor for the system
    int HandleOneLine(uint8_t *pData, size_t dwDataLen);
    // This creates a properly formatted capability string based on the current state and configuration
    // of the IMAP server 
    std::string BuildCapabilityString(void);
    // This sends the untagged responses associated with selecting a mailbox
    void SendSelectData(const std::string &mailbox, bool isReadWrite);
    // This appends data to the Parse Buffer, extending the parse buffer as necessary.
    void AddToParseBuffer(const uint8_t *data, size_t length, bool bNulTerminate = true);

    IMAP_RESULTS LoginHandlerExecute();
    IMAP_RESULTS SearchHandlerInternal(uint8_t *data, const size_t dataLen, size_t &parsingAt, bool usingUid);
    IMAP_RESULTS SearchKeyParse(uint8_t *data, const size_t dataLen, size_t &parsingAt);
    IMAP_RESULTS StoreHandlerInternal(uint8_t *data, const size_t dataLen, size_t &parsingAt, bool usingUid);
    IMAP_RESULTS FetchHandlerInternal(uint8_t *data, const size_t dataLen, size_t &parsingAt, bool usingUid);
#if 0
    IMAP_RESULTS CopyHandlerInternal(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt, bool bUsingUid);
#endif // 0
    size_t ReadEmailFlags(uint8_t *data, const size_t dataLen, size_t &parsingAt, bool &okay);
    bool UpdateSearchTerms(MailSearch &searchTerm, size_t &tokenPointer, bool isSubExpression);
    bool MsnSequenceSet(SEARCH_RESULT &r_srVector, size_t &tokenPointer);
    bool UidSequenceSet(SEARCH_RESULT &r_srVector, size_t &tokenPointer);
    bool UpdateSearchTerm(MailSearch &searchTerm, size_t &tokenPointer);

    IMAP_RESULTS SelectHandlerExecute(bool isReadWrite = true);
    IMAP_RESULTS CreateHandlerExecute();
    IMAP_RESULTS DeleteHandlerExecute();
    IMAP_RESULTS RenameHandlerExecute();
    IMAP_RESULTS SubscribeHandlerExecute(bool isSubscribe);
    IMAP_RESULTS ListHandlerExecute(bool listAll);
    IMAP_RESULTS StatusHandlerExecute(uint8_t *data, const size_t dataLen, size_t parsingAt);
    IMAP_RESULTS AppendHandlerExecute(uint8_t *data, const size_t dataLen, size_t &parsingAt);
    IMAP_RESULTS SearchHandlerExecute(bool usingUid);
    IMAP_RESULTS FetchHandlerExecute(bool usingUid);
#if 0
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
 
    IMAP_RESULTS NamespaceHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt);
    IMAP_RESULTS SelectHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt);
    IMAP_RESULTS ExamineHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt);
    IMAP_RESULTS CreateHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt);
    IMAP_RESULTS DeleteHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt);
    IMAP_RESULTS RenameHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt);
    IMAP_RESULTS SubscribeHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt);
    IMAP_RESULTS UnsubscribeHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt);
    IMAP_RESULTS ListHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt);
    IMAP_RESULTS LsubHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt);
    IMAP_RESULTS StatusHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt);
    IMAP_RESULTS AppendHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt);

    IMAP_RESULTS CheckHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt);
    IMAP_RESULTS CloseHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt);
    IMAP_RESULTS ExpungeHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt);
    IMAP_RESULTS SearchHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt);
    IMAP_RESULTS FetchHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt);
    IMAP_RESULTS StoreHandler(uint8_t *data, const size_t dataLen, size_t &parsingAt);
#if 0
    IMAP_RESULTS CopyHandler(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt);
    IMAP_RESULTS UidHandler(byte *pData, const DWORD dwDataLen, DWORD &r_dwParsingAt);
#endif // 0

    // These are for fetches, which are special because they can generate arbitrarily large responses
    void FetchResponseFlags(uint32_t flags);
    void FetchResponseInternalDate(const MailMessage *message);
    void FetchResponseRfc822(unsigned long uid, const MailMessage *message);
    void FetchResponseRfc822Header(unsigned long uid, const MailMessage *message);
    void FetchResponseRfc822Size(const MailMessage *message);
    void FetchResponseRfc822Text(unsigned long uid, const MailMessage *message);
    void FetchResponseEnvelope(const MailMessage *message);
    void FetchResponseBodyStructure(const MailMessage *message);
    void FetchResponseBody(const MailMessage *message);
    void FetchResponseUid(unsigned long uid);
    void SendMessageChunk(unsigned long uid, size_t offset, size_t length);
    ImapUser *m_userData;
    ImapServer *m_server;
    Sasl *m_auth;
    time_t m_lastCommandTime;
};

#endif //_IMAPSESSION_HPP_INCLUDED_
