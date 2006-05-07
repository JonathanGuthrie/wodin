#if !defined(_MAILSTORE_HPP_INCLUDED_)
#define _MAILSTORE_HPP_INCLUDED_

#include <list>
#include <vector>
#include <string>

typedef struct
{
    std::string name;
    uint32_t	attributes;  
} MAILBOX_NAME; 

typedef std::list<MAILBOX_NAME> MAILBOX_LIST;
typedef std::list<unsigned> NUMBER_LIST;
typedef std::vector<unsigned> MSN_TO_UID;
typedef std::vector<unsigned> SEARCH_RESULT;

class MailStore {
public:
    typedef enum {
	SUCCESS = 0,
	GENERAL_FAILURE,
	MAILBOX_ALREADY_EXISTS,
	MAILBOX_DOES_NOT_EXIST,
	MAILBOX_IS_ACTIVE,
	MAILBOX_IS_NOT_LEAF,
	MAILBOX_UNABLE_TO_UPDATE_CATALOG,
	MAILBOX_NOT_SELECTABLE,
	MAILBOX_READ_ONLY,
	MAILBOX_NOT_OPEN,
	MESSAGE_UNABLE_TO_UPDATE_CATALOG,
	MESSAGE_FILE_OPEN_FAILED,
	MESSAGE_FILE_WRITE_FAILED,
	MESSAGE_NOT_FOUND
    } MAIL_STORE_RESULT;

    typedef enum {
	IMAP_MESSAGE_ANSWERED = 0x01,
	IMAP_MESSAGE_FLAGGED = 0x02,
	IMAP_MESSAGE_DELETED = 0x04,
	IMAP_MESSAGE_SEEN = 0x08,
	IMAP_MESSAGE_DRAFT = 0x10
    } MAIL_STORE_FLAGS;

    typedef enum {
	IMAP_MBOX_NOSELECT = 0x01,
	// This might look a little strange, but there are three possible
	// states:  The server knows the mailbox is "interesting", the
	// server knows the mailbox is "uninteresting" and the server
	// can't tell.  Note that if the IMAP_MBOX_MARKED flag is set,
	// the state of IMAP_MBOX_UNMARKED will be ignored
	IMAP_MBOX_MARKED = 0x02,
	IMAP_MBOX_UNMARKED = 0x04,
	IMAP_MBOX_HASCHILDREN = 0x08,
	IMAP_MBOX_NOINFERIORS = 0x10
    } MAIL_BOX_FLAGS;

    MailStore();
    virtual MAIL_STORE_RESULT CreateMailbox(const std::string &MailboxName) = 0;
    virtual MAIL_STORE_RESULT MailboxClose() = 0;
    virtual unsigned GetSerialNumber() = 0;
    virtual unsigned GetNextSerialNumber() = 0;
    virtual unsigned GetUidValidityNumber() = 0;
    virtual unsigned MailboxMessageCount(const std::string &MailboxName) = 0;
    virtual unsigned MailboxRecentCount(const std::string &MailboxName) = 0;
    virtual unsigned MailboxFirstUnseen(const std::string &MailboxName) = 0;
    virtual unsigned MailboxMessageCount() = 0;
    virtual unsigned MailboxRecentCount() = 0;
    virtual unsigned MailboxFirstUnseen() = 0;
    virtual std::string GetMailboxUserPath() const = 0;
    virtual MAIL_STORE_RESULT MailboxUpdateStats(NUMBER_LIST *nowGone) = 0;
    // If BuildMailboxList fails, it returns nothing, which is fine for IMAP
    // If bListAll is true, it will return all matching mailboxes, otherwise, it will return 
    // the mailboxes using the IMAP LSUB semantics, which may not be what you expect.
    // You have been warned.
    virtual void BuildMailboxList(const char *ref, const char *pattern, MAILBOX_LIST *result, bool listAll) = 0;
    virtual ~MailStore();
};

#endif // _MAILSTORE_HPP_INCLUDED_
