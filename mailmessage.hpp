#if !defined(_MAILMESSAGE_H_INCLUDED_)
#define _MAILMESSAGE_H_INCLUDED_

#include <list>
#include <map>
#include <vector>

#include <clotho/insensitive.hpp>

#include "datetime.hpp"

class MailStore;

struct MessageBody;
typedef struct MessageBody MESSAGE_BODY;

typedef std::multimap<insensitiveString, insensitiveString> HEADER_FIELDS;
typedef std::vector<MESSAGE_BODY> *BODY_PARTS;

typedef enum
{
    MIME_TYPE_UNKNOWN,
    MIME_TYPE_TEXT,
    MIME_TYPE_IMAGE,
    MIME_TYPE_AUDIO,
    MIME_TYPE_VIDEO,
    MIME_TYPE_APPLICATION,
    MIME_TYPE_MULTIPART,
    MIME_TYPE_MESSAGE
} MIME_MEDIA_TYPES;

struct MessageBody {
    HEADER_FIELDS fieldList;
    insensitiveString contentTypeLine;
    insensitiveString contentEncodingLine;
    insensitiveString contentIdLine;
    insensitiveString contentDescriptionLine;
    MIME_MEDIA_TYPES bodyMediaType;
    size_t bodyLines, bodyStartOffset, bodyOctets;
    size_t headerOctets, headerLines;
    BODY_PARTS subparts;
};

class MailMessage {
public:
    typedef enum
    {
	SUCCESS = 0,
	MESSAGE_DOESNT_EXIST,
	GENERAL_FAILURE,
	MESSAGE_FILE_OPEN_FAILED,
	MESSAGE_FILE_READ_FAILED,
	MESSAGE_MALFORMED_NO_BODY
    } MAIL_MESSAGE_RESULT;
    MailMessage(unsigned long uid, unsigned long msn);
    ~MailMessage();
    MAIL_MESSAGE_RESULT Parse(MailStore *store, bool readBody = true, bool loadBinaryParts = true);
    static void BuildSymbolTable();
    MAIL_MESSAGE_RESULT GetStatus() const { return m_messageStatus; }
    const insensitiveString &GetSubject() const { return m_subject; }
    const insensitiveString &GetCc() const { return m_ccLine; }
    const insensitiveString &GetBcc() const { return m_bccLine; }
    const insensitiveString &GetTo() const { return m_toLine; }
    const insensitiveString &GetFrom() const { return m_fromLine; }
    unsigned short GetTextLines() const { return m_textLines; }
    const HEADER_FIELDS &GetHeaderList() const { return m_mainBody.fieldList; }
    const MESSAGE_BODY &GetMessageBody() const { return m_mainBody; }
    const DateTime &GetMessageTime() const { return m_date; }
    uint32_t GetMessageFlags() const { return m_flagsWhenRead; }
    unsigned long GetUid() const { return m_uid; }
    unsigned long GetMsn() const { return m_msn; }
    const insensitiveString &GetDateLine() const { return m_dateLine; }
    const insensitiveString &GetSender() const { return m_senderLine; }
    const insensitiveString &GetReplyTo() const { return m_replyToLine; }
    const insensitiveString &GetInReplyTo() const { return m_inReplyTo; }
    const insensitiveString &GetMessageId() const { return m_messageId; }
    void SetMessageFlags(uint32_t newFlags) { m_flagsWhenRead = newFlags; }
    
private:
    bool ProcessHeaderLine(const insensitiveString &line);
    void ParseBodyParts(bool loadBinaryParts, MESSAGE_BODY &parentBody, char *messageBuffer, size_t &parsePointer,
			const char *parentSeparator, int nestingDepth);
    DateTime m_date;
    insensitiveString m_dateLine, m_subject, m_inReplyTo, m_messageId;
    insensitiveString m_fromLine, m_senderLine, m_replyToLine, m_toLine, m_ccLine, m_bccLine;
    unsigned long m_uid, m_msn, m_textLines;
    MAIL_MESSAGE_RESULT m_messageStatus;
    MESSAGE_BODY m_mainBody;
    static const int m_lineBufferLen = 100;
    char m_lineBuffer[m_lineBufferLen];
    int m_lineBuffPtr, m_lineBuffEnd;

    // These are actually not part of the message itself, but are cached here so I don't have to
    // screw around while processing IMAP fetches
    uint32_t m_flagsWhenRead;
};

#endif // _MAILMESSAGE_HPP_INCLUDED_
